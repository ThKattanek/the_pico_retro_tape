////////////////////////////////////////////////////////
//                                                    //
// ThePicoRetroTape                                   //
// von Thorsten Kattanek                              //
//                                                    //
// #file: c1530.c                                     //
//                                                    //
// https://github.com/ThKattanek/the_pico_retro_tape  //
//                                                    //
//                                                    //
//////////////////////////////////////////////////////// 

// before commit send_prg
// - fix load error -> is now fixed !
// - kernal header add prg filename 
// - testing with many prg files and more c64

#include <stdio.h>
#include <pico/stdlib.h>
#include "sd_card.h"
#include "ff.h"
#include "./c1530.h"
#include <string.h>
#include <time.h>
#include <ctype.h>

bool timer_callback_send_data(__unused struct repeating_timer *t) 
{
    C1530Class* c1530 = static_cast<C1530Class*>(t->user_data);

    if (c1530->motor_state)
    {
        gpio_put(c1530->read_gpio, c1530->send_buffer_read_pos & 1);
        c1530->timer.delay_us = c1530->send_buffer[c1530->send_buffer_read_pos++];    
    }

    return true;
}

C1530Class::C1530Class()
    : read_gpio(-1), write_gpio(-1), sense_gpio(-1), motor_gpio(-1),
      send_buffer_read_pos(0), motor_state(false), tap_image_pos(0),
      buffer0_is_ready(false), buffer1_is_ready(false), tap_image_is_end(false),
      motor_new_state(false), motor_old_state(false), is_tape_insert(false)
{
    for(int i=0; i<256; i++)
        send_buffer[i] = -100;
}

void C1530Class::init_gpios(int read_gpio, int write_gpio, int sense_gpio, int motor_gpio)
{
    // read_gpio is for transfer from here to c64                   OUTPUT
    // write_gpio is for transfer from c64 to here                  INPUT
    // sense_gpio is the singal to the c64 when a key is pressed    OUTPUT
    // motor_gpio is the signal from the c64 when motor must on     INPUT

    // output
    gpio_init(read_gpio);
    gpio_init(sense_gpio);
    gpio_set_dir(read_gpio, true);
    gpio_set_dir(sense_gpio, true);
    gpio_put(sense_gpio, false);

    // input
    gpio_init(write_gpio);
    gpio_init(motor_gpio);
    gpio_set_dir(write_gpio, false);
    gpio_set_dir(motor_gpio, false);
    gpio_set_pulls(write_gpio, false, false);
    gpio_set_pulls(motor_gpio, false, false);

    this->read_gpio = read_gpio;
    this->write_gpio = write_gpio;
    this->sense_gpio = sense_gpio;
    this->motor_gpio = motor_gpio;
}

bool C1530Class::open_tap_image(char* filename)
{
    FRESULT fr;
    UINT read_bytes;

    if(filename == nullptr)
        return false;

    fr = f_open(&file, filename, FA_READ);
    if(fr == FR_OK)
    {
        // prüfen ob es sich um ein TAP-File handelt
        fr = f_read(&file, &tap_header, sizeof(TAPHeader), &read_bytes);
        if(fr != FR_OK || read_bytes != sizeof(TAPHeader))
        {
            printf("Error reading TAP-Header\n");
            f_close(&file);
            return false;
        }
        else
        {
            if(0 != strncmp(tap_header.magic_id, "C64-TAPE-RAW", 12))
            {
                printf("Error: Not a TAP-File\n");
                f_close(&file);
                return false;
            }
            else
            {
                image_source = IMAGE_SOURCE::SDCARD;
                image_type = IMAGE_TYPE::TAP;
                tap_image_pos = 0;
                is_tape_insert = true;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

bool C1530Class::open_tap_image(const uint8_t* image_buffer, u_int image_buffer_size, IMAGE_TYPE type)
{
    if(image_buffer == nullptr)
        return false;

    switch (type)
    {
    case IMAGE_TYPE::TAP:
        tap_data_buffer = image_buffer;
        tap_data_buffer_size = image_buffer_size;
        image_source = IMAGE_SOURCE::MEMORY;
        image_type = type;
        is_tape_insert = true;
        break;
    
    default:
        return false;
        break;
    }
    
    return true;
}

bool C1530Class::open_prg_image(char* filename)
{
    FRESULT fr;
    UINT read_bytes;

    if(filename == nullptr)
        return false;

    fr = f_open(&file, filename, FA_READ);
    if(fr == FR_OK)
    {
        image_source = IMAGE_SOURCE::SDCARD;
        image_type = IMAGE_TYPE::PRG;
        
        prg_send_pos = 0;
        prg_send_state = 0;
        sync_pulse_counter = 0; 
        countdown_sequence = 0x09;
        kernal_header_block_pos = 0;
        one_byte_pulse_buffer_pos = 0;

        is_tape_insert = true;

        prg_size = f_size(&file);
        prg_size -= 2; // 2 Bytes for the start address

        f_read(&file, &kernal_header_block.start_address_low, 1, &read_bytes);
        f_read(&file, &kernal_header_block.start_address_high, 1, &read_bytes);
        
        f_lseek(&file, 2);

        uint16_t temp_address = (kernal_header_block.start_address_low | (kernal_header_block.start_address_high << 8));
        uint16_t end_adress = static_cast<uint16_t>(temp_address);
        end_adress += static_cast<uint16_t>(prg_size);
        kernal_header_block.end_address_low = static_cast<uint8_t>(end_adress & 0x00FF);
        kernal_header_block.end_address_high = static_cast<uint8_t>((end_adress >> 8) & 0x00FF);

        kernal_header_block.header_type = 0x01; // Kernal Header Block
        memset(kernal_header_block.filename_dispayed, 0x20, sizeof(kernal_header_block.filename_dispayed));
        memset(kernal_header_block.filename_not_displayed, 0x20, sizeof(kernal_header_block.filename_not_displayed));

        // Extract the filename from the full path and convert to uppercase
        const char* base_filename = strrchr(filename, '/');
        base_filename = (base_filename != nullptr) ? base_filename + 1 : filename;

        // Remove the ".prg" extension if present
        char temp_filename[sizeof(kernal_header_block.filename_dispayed) + 1] = {0};
        strncpy(temp_filename, base_filename, sizeof(temp_filename) - 1);
        char* dot_position = strrchr(temp_filename, '.');
        if (dot_position && strcmp(dot_position, ".prg") == 0) {
            *dot_position = '\0'; // Remove the extension
        }

        // Copy the modified filename to the Kernal Header and convert to uppercase
        for (size_t i = 0; i < sizeof(kernal_header_block.filename_dispayed) && temp_filename[i] != '\0'; i++) {
            kernal_header_block.filename_dispayed[i] = toupper(temp_filename[i]);
        }

        // Calculate checksum
        checksum = 0;
        for (int i = 0; i < sizeof(kernal_header_block) - 1; i++)
        {
            checksum ^= ((uint8_t*)&kernal_header_block)[i];
        }
        kernal_header_block.crc_checksum = checksum;
        checksum = 0;

        return true;
    }
    return false;
}

void C1530Class::close_image()
{
    is_tape_insert = false;
    
    for(int i=0; i<256; i++)
        send_buffer[i] = -100;

    if(image_source == IMAGE_SOURCE::SDCARD)
        f_close(&file);
}

void C1530Class::update()
{
    if(!is_tape_insert)
        return;

    switch (image_source)
    {
    case IMAGE_SOURCE::MEMORY:
        fill_send_buffer_from_memory();
        break;
    
    case IMAGE_SOURCE::SDCARD:
        fill_send_buffer_from_file();
        break;  

    default:
        break;
    }

    // motor state
    motor_new_state = gpio_get(motor_gpio);
    if(motor_new_state == true && motor_old_state == false)
    {
        motor_state = true;
        printf("Motor is ON\n");
    }

    if(motor_new_state == false && motor_old_state == true)
    {
        motor_state = false;
        printf("Motor is OFF\n");
    }
    motor_old_state = motor_new_state;
}

void C1530Class::read_start()
{
    for(int i=0; i<256; i++)
        send_buffer[i] = -100;

    send_buffer_read_pos = 0;

    if(image_source == IMAGE_SOURCE::SDCARD)
    {
        //tap_image_pos = 0;
    }
    else
        tap_image_pos = 0x14;
            
    gpio_put(read_gpio, true);
    gpio_put(sense_gpio, true);  
    add_repeating_timer_us(-1000, timer_callback_send_data, this, &timer);

    buffer0_is_ready = false;
    buffer1_is_ready = false;
    tap_image_is_end = false;
}

void C1530Class::stop()
{
    cancel_repeating_timer(&timer);
    gpio_put(read_gpio, true);
    gpio_put(sense_gpio, false);  
}

bool C1530Class::is_tap_end()
{
    return tap_image_is_end;
}

double C1530Class::calculate_tap_runtime()
{
    clock_t start_time = clock(); // Startzeit erfassen

    uint32_t total_cycles = 0;
    uint32_t cycles;
    UINT read_bytes;
    uint8_t read_byte;

    if(image_source != IMAGE_SOURCE::SDCARD && is_tape_insert == false)
        return 0;

    // Set file position to the start of the tape data
    f_lseek(&file, 0x14);

    while(f_read(&file, &read_byte, 1, &read_bytes) == FR_OK && read_bytes == 1)
    {
        cycles = read_byte;
        if(cycles == 0)
        {
            unsigned char tmp[3];
            f_read(&file, tmp, 3, &read_bytes);
            cycles = ((tmp[2] << 16) | (tmp[1] << 8) | tmp[0]);
        }
        total_cycles += cycles << 3;
    }

        // Set file position to the start of the tape data
        f_lseek(&file, 0x14);

    // Convert cycles to seconds (PAL system: 985248 Hz)
    double runtime_seconds = total_cycles / 985248.0;

    clock_t end_time = clock(); // Endzeit erfassen
    double elapsed_time = double(end_time - start_time) / CLOCKS_PER_SEC; // Berechnungszeit in Sekunden

    printf("Berechnungszeit: %.2f Sekunden\n", elapsed_time); // Berechnungszeit ausgeben

    return runtime_seconds;
}

void C1530Class::fill_send_buffer_from_memory()
{
    switch (image_type)
    {
    case IMAGE_TYPE::TAP:

    if(send_buffer_read_pos == 0 && !buffer1_is_ready)
    {
        // Puffer 1 füllen

        for(int i=128; i<256; i+=2)
        {
            int32_t pulse = get_next_tap_us_pulse();
            if(tap_image_pos >= tap_data_buffer_size)
            {
                stop();
                tap_image_is_end = true;
                break;
            }

            send_buffer[i+0] = pulse;
            send_buffer[i+1] = pulse;
        }

        buffer1_is_ready = true;
        buffer0_is_ready = false;
        sleep_us(1);
    }

    if(send_buffer_read_pos == 128 && !buffer0_is_ready)
    {
        // Puffer 0 füllen

        for(int i=0; i<128; i+=2)
        {
            int32_t pulse = get_next_tap_us_pulse();
            if(tap_image_pos >= tap_data_buffer_size)
            {
                stop();
                tap_image_is_end = true;
                break;
            }

            send_buffer[i+0] = pulse;
            send_buffer[i+1] = pulse;
        }

        buffer0_is_ready = true;
        buffer1_is_ready = false;
        sleep_us(1);
    }
        break;
    
    case IMAGE_TYPE::PRG:
        break;

    case IMAGE_TYPE::T64:
        break;


    default:
        break;
    }
}

void C1530Class::fill_send_buffer_from_file()
{    
    switch (image_type)
    {
    case IMAGE_TYPE::TAP:
    // TAP-File
    if(send_buffer_read_pos == 0 && !buffer1_is_ready)
    {
        // Puffer 1 füllen
        for(int i=128; i<256; i+=2)
        {
            int32_t pulse = get_next_tap_us_pulse_from_file();
            if(tap_image_pos >= tap_header.data_length)
            {
                pulse = 0;
            }
                send_buffer[i+0] = pulse;
                send_buffer[i+1] = pulse;
        }

        buffer1_is_ready = true;
        buffer0_is_ready = false;
        //sleep_us(1);
    }

    if(send_buffer_read_pos == 128 && !buffer0_is_ready)
    {
        // Puffer 0 füllen
        for(int i=0; i<128; i+=2)
        {
            int32_t pulse = get_next_tap_us_pulse_from_file();
            if(tap_image_pos >= tap_header.data_length)
            {
                pulse = 0;
            }
            
            send_buffer[i+0] = pulse;
            send_buffer[i+1] = pulse;
            
        }

        buffer0_is_ready = true;
        buffer1_is_ready = false;
        //sleep_us(1);
    }
        break;
    
    case IMAGE_TYPE::PRG:
    // PRG-File    
    if(send_buffer_read_pos == 0 && !buffer1_is_ready)
    {
        // Puffer 1 füllen
        for(int i=128; i<256; i+=2)
        {
            int32_t pulse = get_next_prg_us_pulse_from_file();
            send_buffer[i+0] = pulse;
            send_buffer[i+1] = pulse;
        }

        buffer1_is_ready = true;
        buffer0_is_ready = false;
        //sleep_us(1);
    }

    if(send_buffer_read_pos == 128 && !buffer0_is_ready)
    {
        // Puffer 0 füllen
        for(int i=0; i<128; i+=2)
        {
            int32_t pulse = get_next_prg_us_pulse_from_file();
            send_buffer[i+0] = pulse;
            send_buffer[i+1] = pulse;
        }

        buffer0_is_ready = true;
        buffer1_is_ready = false;
        //sleep_us(1);
    }
        break;

    case IMAGE_TYPE::T64:
        break;

    default:
        break;
    }
}

int32_t C1530Class::get_next_tap_us_pulse_from_file()
{
    uint32_t cycles;
    uint32_t pulse;
    UINT read_bytes;

    uint8_t read_byte;

    f_read(&file, &read_byte, 1, &read_bytes);
    cycles = read_byte;
    if(cycles != 0)
    {
        pulse = cycles << 2;
        tap_image_pos++;
    }
    else 
    {
        // Wenn 0 dann die nächsten 3Bytes in unsigned int (32Bit) wandeln
        // Mit Hilfe eines Zeigers auf cycles
        unsigned char tmp[3];
        f_read(&file, tmp, 3, &read_bytes);
        cycles = (tmp[2] << 16) | (tmp[1] << 8) | tmp[0];
        pulse = cycles >> 1;
        tap_image_pos += 4;
    }

    return pulse;
}

int32_t C1530Class::get_next_tap_us_pulse()
{
        uint32_t cycles = tap_data_buffer[tap_image_pos];
        uint32_t pulse;

        if(cycles != 0)
        {
            pulse = cycles << 2;
            tap_image_pos++;
        }
        else 
        {
            // Wenn 0 dann die nächsten 3Bytes in unsigned int (32Bit) wandeln
            // Mit Hilfe eines Zeigers auf cycles
            unsigned char *tmp = (unsigned char*) & cycles;
            tmp[0] = tap_data_buffer[tap_image_pos + 1];
            tmp[1] = tap_data_buffer[tap_image_pos + 2];
            tmp[2] = tap_data_buffer[tap_image_pos + 3];
            pulse = cycles >> 1;    // 1/2
            tap_image_pos += 4;
        }

        return pulse;
}

int32_t C1530Class::get_next_prg_us_pulse_from_file()
{
    int32_t pulse = 0;
    uint8_t send_byte;
    UINT read_bytes;

    switch(prg_send_state)
    {
        case 0:
            // Sync send short pulse
            pulse = SHORT_PULSE >> 1;   // 1/2
            sync_pulse_counter++;
            if(sync_pulse_counter > 27135/5)
            {
                sync_pulse_counter = 0;
                prg_send_state++;
            }
            break;
        case 1:
            // Countdown sequence 0x89 - 0x81
            if(one_byte_pulse_buffer_pos == 0)
            {
                // fill the buffer with the next pulse for the next byte
                send_byte = countdown_sequence--;
                send_byte |= 0x80;
                conv_byte_to_pulses(send_byte, one_byte_pulse_buffer);
            }

            pulse = one_byte_pulse_buffer[one_byte_pulse_buffer_pos++];
            if(one_byte_pulse_buffer_pos == 20)
            {
                one_byte_pulse_buffer_pos = 0;
                if(countdown_sequence == 0x00)
                {
                    countdown_sequence = 0x09;
                    prg_send_state++;
                }
            }
            break;
        case 2:
            // Send Kernal Header Block
            if(one_byte_pulse_buffer_pos == 0)
            {
                // fill the buffer with the next pulse for the next byte
                send_byte = ((uint8_t*)&kernal_header_block)[kernal_header_block_pos++];
                conv_byte_to_pulses(send_byte, one_byte_pulse_buffer);
            }

            pulse = one_byte_pulse_buffer[one_byte_pulse_buffer_pos++];
            if(one_byte_pulse_buffer_pos == 20)
            {
                one_byte_pulse_buffer_pos = 0;
                if(kernal_header_block_pos == sizeof(kernal_header_block))
                {
                    kernal_header_block_pos = 0;
                    prg_send_state++;
                }
            }
            break;
        case 3:
            // EndOfData Maker (LS)
            pulse = LONG_PULSE >> 1; // long pulse
            prg_send_state++;
            break;
        case 4:
            // EndOfData Maker (LS)
            pulse = SHORT_PULSE >> 1; // short pulse
            prg_send_state++;
            break;
        case 5:
            // Sync send short pulse
            pulse = SHORT_PULSE >> 1;   // 1/2
            sync_pulse_counter++;
            if(sync_pulse_counter > 79)
            {
                sync_pulse_counter = 0;
                prg_send_state++;
            }
            break;
        case 6:
            // Countdown sequence 0x09 - 0x01
            if(one_byte_pulse_buffer_pos == 0)
            {
                // fill the buffer with the next pulse for the next byte
                send_byte = countdown_sequence--;
                conv_byte_to_pulses(send_byte , one_byte_pulse_buffer);
            }

            pulse = one_byte_pulse_buffer[one_byte_pulse_buffer_pos++];
            if(one_byte_pulse_buffer_pos == 20)
            {
                one_byte_pulse_buffer_pos = 0;
                if(countdown_sequence == 0x00)
                {
                    countdown_sequence = 0x09;
                    prg_send_state++;
                }
            }
            break;
        case 7:
            // Send Kernal Header Block
            if(one_byte_pulse_buffer_pos == 0)
            {
                // fill the buffer with the next pulse for the next byte
                send_byte = ((uint8_t*)&kernal_header_block)[kernal_header_block_pos++];
                conv_byte_to_pulses(send_byte, one_byte_pulse_buffer);
            }

            pulse = one_byte_pulse_buffer[one_byte_pulse_buffer_pos++];
            if(one_byte_pulse_buffer_pos == 20)
            {
                one_byte_pulse_buffer_pos = 0;
                if(kernal_header_block_pos == sizeof(kernal_header_block))
                {
                    kernal_header_block_pos = 0;
                    prg_send_state++;
                }
            }
            break;
        case 8:
            // Sync send short pulse
            pulse = SHORT_PULSE >> 1;   // 1/2
            sync_pulse_counter++;
            if(sync_pulse_counter > 5671)
            {
                checksum = 0;
                sync_pulse_counter = 0;
                prg_send_state++;
            }
            break;
        case 9:
            // Countdown sequence 0x89 - 0x81
            if(one_byte_pulse_buffer_pos == 0)
            {
                // fill the buffer with the next pulse for the next byte
                send_byte = countdown_sequence--;
                send_byte |= 0x80;
                conv_byte_to_pulses(send_byte, one_byte_pulse_buffer);
            }

            pulse = one_byte_pulse_buffer[one_byte_pulse_buffer_pos++];
            if(one_byte_pulse_buffer_pos == 20)
            {
                one_byte_pulse_buffer_pos = 0;
                if(countdown_sequence == 0x00)
                {
                    countdown_sequence = 0x09;
                    prg_send_state++;
                }
            }
            break;
        case 10:
            // Send Data
            if(one_byte_pulse_buffer_pos == 0)
            {
                // fill the buffer with the next pulse for the next byte
                f_read(&file, &send_byte, 1, &read_bytes);
                if(read_bytes == 1)
                {
                    checksum ^= send_byte;
                    conv_byte_to_pulses(send_byte , one_byte_pulse_buffer);
                    prg_send_pos++;
                }
                else
                {
                    pulse = 0;
                    prg_send_state = 100;
                }
            }

            pulse = one_byte_pulse_buffer[one_byte_pulse_buffer_pos++];
            if(one_byte_pulse_buffer_pos == 20)
            {
                one_byte_pulse_buffer_pos = 0;
                if(prg_send_pos == prg_size)
                {
                    conv_byte_to_pulses(checksum , one_byte_pulse_buffer);
                    prg_send_pos = 0;
                    prg_send_state++;
                }
            }

            break;
        case 11:
            pulse = one_byte_pulse_buffer[one_byte_pulse_buffer_pos++];
            if(one_byte_pulse_buffer_pos == 20)
            {
                f_lseek(&file, 2); // Set file position to the start of the tape data
                one_byte_pulse_buffer_pos = 0;
                checksum = 0;
                prg_send_state++;
            }   
            break;
        case 12:
            // EndOfData Maker (LS)
            pulse = LONG_PULSE >> 1; // long pulse
            prg_send_state++;
            break;
        case 13:
            // EndOfData Maker (LS)
            pulse = SHORT_PULSE >> 1; // short pulse
            prg_send_state++;
            break;
        case 14:
            // Sync send short pulse
            pulse = SHORT_PULSE >> 1;   // 1/2
            sync_pulse_counter++;
            if(sync_pulse_counter > 79)
            {
                sync_pulse_counter = 0;
                prg_send_state++;
            }
            break;
        case 15:
            // Countdown sequence 0x09 - 0x01
            if(one_byte_pulse_buffer_pos == 0)
            {
                // fill the buffer with the next pulse for the next byte
                send_byte = countdown_sequence--;
                conv_byte_to_pulses(send_byte , one_byte_pulse_buffer);
            }

            pulse = one_byte_pulse_buffer[one_byte_pulse_buffer_pos++];
            if(one_byte_pulse_buffer_pos == 20)
            {
                one_byte_pulse_buffer_pos = 0;
                if(countdown_sequence == 0x00)
                {
                    countdown_sequence = 0x09;
                    prg_send_state++;
                }
            }
            break;
        case 16:
            // Send Data
            if(one_byte_pulse_buffer_pos == 0)
            {
                // fill the buffer with the next pulse for the next byte
                f_read(&file, &send_byte, 1, &read_bytes);
                if(read_bytes == 1)
                {
                    checksum ^= send_byte;
                    conv_byte_to_pulses(send_byte , one_byte_pulse_buffer);
                    prg_send_pos++;
                }
                else
                {
                    pulse = 0;
                    prg_send_state = 100;
                }
            }

            pulse = one_byte_pulse_buffer[one_byte_pulse_buffer_pos++];
            if(one_byte_pulse_buffer_pos == 20)
            {
                one_byte_pulse_buffer_pos = 0;
                if(prg_send_pos == prg_size)
                {
                    conv_byte_to_pulses(checksum , one_byte_pulse_buffer);
                    prg_send_pos = 0;
                    prg_send_state++;
                }
            }

            break;
        case 17:
            pulse = one_byte_pulse_buffer[one_byte_pulse_buffer_pos++];
            if(one_byte_pulse_buffer_pos == 20)
            {
                f_lseek(&file, 2); // Set file position to the start of the tape data
                checksum = 0;
                prg_send_state++;
            }   
            break;
        case 18:
            // EndOfData Maker (LS)
            pulse = LONG_PULSE >> 1; // long pulse
            prg_send_state++;
            break;
        case 19:
            // EndOfData Maker (LS)
            pulse = SHORT_PULSE >> 1; // short pulse
            prg_send_state = 100;
            break;
        case 100:
            printf("End of PRG-File\n");
            break;
    }

    return pulse;
}

inline void C1530Class::conv_byte_to_pulses(uint32_t byte, uint32_t *pulses)
{
    int pos = 0;

    // ByteMaker
    pulses[pos++] = LONG_PULSE >> 1;     // long pulse
    pulses[pos++] = MEDIUM_PULSE >> 1;   // medium pulse

    // Write the bits of the byte (LSB first)
    uint8_t parity_bit = 1;
    for (int i = 0; i < 8; ++i)     
    {
        if (byte & (1 << i)) 
        {
            // Bit is 1
            pulses[pos++] = MEDIUM_PULSE >> 1; // medium pulse
            pulses[pos++] = SHORT_PULSE >> 1;  // short pulse
            parity_bit ^= 1;
        } else 
        {
            // Bit is 0
            pulses[pos++] = SHORT_PULSE >> 1;  // short pulse
            pulses[pos++] = MEDIUM_PULSE >> 1; // medium pulse
        }
    }

    // Write the parity bit (odd parity)
    if (parity_bit == 1) 
    {
        // Even number of 1 bits
        // Parity Bit = 1
        pulses[pos++] = MEDIUM_PULSE >> 1; // medium pulse
        pulses[pos++] = SHORT_PULSE >> 1;  // short pulse
    } else 
    {
        // Odd number of 1 bits
        // Parity Bit = 0
        pulses[pos++] = SHORT_PULSE >> 1;  // short pulse
        pulses[pos++] = MEDIUM_PULSE >> 1; // medium pulse
    }
}
