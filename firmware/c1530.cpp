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

#include <stdio.h>
#include <pico/stdlib.h>
#include "sd_card.h"
#include "ff.h"
#include "./c1530.h"
#include <string.h>
#include <time.h>

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

bool C1530Class::open_image(char* filename)
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
        if(fr != FR_OK && read_bytes != sizeof(TAPHeader))
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

bool C1530Class::open_image(const uint8_t* image_buffer, int image_buffer_size, IMAGE_TYPE type)
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
    }

    if(motor_new_state == false && motor_old_state == true)
    {
        motor_state = false;
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
        tap_image_pos = 0;
    }
    else
        tap_image_pos = 0x14;
            
    gpio_put(read_gpio, true);
    add_repeating_timer_us(-1000, timer_callback_send_data, this, &timer);

    buffer0_is_ready = false;
    buffer1_is_ready = false;
    tap_image_is_end = false;
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

    if(send_buffer_read_pos == 0 && !buffer1_is_ready)
    {
        // Puffer 1 füllen

        for(int i=128; i<256; i+=2)
        {
            int32_t pulse = get_next_tap_us_pulse_from_file();
            if(tap_image_pos >= tap_header.data_length)
            {
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
            int32_t pulse = get_next_tap_us_pulse_from_file();
            if(tap_image_pos >= tap_header.data_length)
            {
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
        unsigned char tmp[3];
        f_read(&file, tmp, 3, &read_bytes);
        cycles = (tmp[2] << 16) | (tmp[1] << 8) | tmp[0];
        pulse = cycles >> 1;
        tap_image_pos += 4;
    }

    return pulse * -1;
}

int32_t C1530Class::get_next_tap_us_pulse()
{
        uint32_t cycles = tap_data_buffer[tap_image_pos];
        uint32_t pulse;

        if(cycles != 0)
        {
            //pulse = (cycles << 3) * 0.50748644f; 
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
            //pulse = cycles * 0.50748644f;
            pulse = cycles >> 1;
            tap_image_pos += 4;
        }

        return pulse * -1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// Experimental

void C1530Class::sync_10sek()
{
    for(int i=0; i<2840*10; i++)
    {
        gpio_put(read_gpio, false);
        sleep_us(SHORT_PULSE);
        gpio_put(read_gpio, true);
        sleep_us(SHORT_PULSE);
    }
}

void C1530Class::send_byte(uint8_t byte)
{
    // ByteMaker
    gpio_put(read_gpio, false);
    sleep_us(LONG_PULSE);
    gpio_put(read_gpio, true);
    sleep_us(LONG_PULSE);
    gpio_put(read_gpio, false);
    sleep_us(MEDIUM_PULSE);
    gpio_put(read_gpio, true);
    sleep_us(MEDIUM_PULSE);

    // Byte
    uint8_t bit_mask = 1;
    for(int i=0; i<8; i++)
    {
        if(byte & bit_mask)
        {
            // 1-bit
            gpio_put(read_gpio, false);
            sleep_us(MEDIUM_PULSE);
            gpio_put(read_gpio, true);
            sleep_us(MEDIUM_PULSE);
            gpio_put(read_gpio, false);
            sleep_us(SHORT_PULSE);
            gpio_put(read_gpio, true);
            sleep_us(SHORT_PULSE);
        }
        else
        {
            // 0-bit
            gpio_put(read_gpio, false);
            sleep_us(SHORT_PULSE);
            gpio_put(read_gpio, true);
            sleep_us(SHORT_PULSE);
            gpio_put(read_gpio, false);
            sleep_us(MEDIUM_PULSE);
            gpio_put(read_gpio, true);
            sleep_us(MEDIUM_PULSE);
        }

        bit_mask <<= 1;
    }

    if((byte % 2) == 0)
    {
        // gerade anzahl von 1 Bits
        // Parity Bit = 1    
        gpio_put(read_gpio, false);
        sleep_us(MEDIUM_PULSE);
        gpio_put(read_gpio, true);
        sleep_us(MEDIUM_PULSE);
        gpio_put(read_gpio, false);
        sleep_us(SHORT_PULSE);
        gpio_put(read_gpio, true);
        sleep_us(SHORT_PULSE);
    }
    else
    {
        // ungerade anzahl von 1 Bits
        // Parity Bit = 0
        gpio_put(read_gpio, false);
        sleep_us(SHORT_PULSE);
        gpio_put(read_gpio, true);
        sleep_us(SHORT_PULSE);
        gpio_put(read_gpio, false);
        sleep_us(MEDIUM_PULSE);
        gpio_put(read_gpio, true);
        sleep_us(MEDIUM_PULSE);
    }
}

void C1530Class::send_test()
{
    struct C64_HEADER header;

    header.HeaderType = 0x01;
    header.StartAddressHighByte = 0x08;
    header.StartAddressLowByte = 0x01;
    header.EndAddressHighByte = 0x10;
    header.EndAddressLowByte = 0x00;
    
    for(int i=0; i<16; i++)
        header.Filename1[i] = 0x20;

    header.Filename1[0] = 'A';

    uint8_t *buffer = (uint8_t*)&header;

    // 10sec Sync
    sync_10sek();

    // 1. Countdown sequence
    for(uint8_t c_byte = 0x89; c_byte >= 0x81; c_byte--)
        send_byte(c_byte);

    // send 192 bytes data
    uint8_t checksum = 0;
    for(int i=0; i < (int)sizeof(header); i++)
    {
        checksum ^= buffer[i];
        send_byte(buffer[i]);
    }

    // send the checksum
    send_byte(checksum);

    // long pulse
    gpio_put(read_gpio, false);
    sleep_us(LONG_PULSE);
    gpio_put(read_gpio, true);
    sleep_us(LONG_PULSE);

    // 60 Sync Short Pulse
    for(int i=0; i<30; i++)
    {
        gpio_put(read_gpio, false);
        sleep_us(SHORT_PULSE);
        gpio_put(read_gpio, true);
        sleep_us(SHORT_PULSE);
    }

     // 2. Countdown sequence
    for(uint8_t c_byte = 0x09; c_byte >= 0x01; c_byte--)
        send_byte(c_byte);

    // send 192 bytes data
    checksum = 0;
    for(int i=0; i < (int)sizeof(header); i++)
    {
        checksum ^= buffer[i];
        send_byte(buffer[i]);
    }

    // send the checksum
    send_byte(checksum);

    // End-of-data MArke
    gpio_put(read_gpio, false);
    sleep_us(LONG_PULSE);
    gpio_put(read_gpio, true);
    sleep_us(LONG_PULSE);
    gpio_put(read_gpio, false);
    sleep_us(SHORT_PULSE);
    gpio_put(read_gpio, true);
    sleep_us(SHORT_PULSE);

    // long pulse
    gpio_put(read_gpio, false);
    sleep_us(LONG_PULSE);
    gpio_put(read_gpio, true);
    sleep_us(LONG_PULSE);

    // 60 Sync Short Pulse
    for(int i=0; i<30; i++)
    {
        gpio_put(read_gpio, false);
        sleep_us(SHORT_PULSE);
        gpio_put(read_gpio, true);
        sleep_us(SHORT_PULSE);
    }
}

