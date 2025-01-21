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

#include "./c1530.h"

//#include "./ggs.h"
#include "./aargh_tap.h"

bool timer_callback_send_data(__unused struct repeating_timer *t) 
{
    C1530Class* c1530 = (C1530Class*)t->user_data;

    if(c1530->send_buffer_read_pos & 1)
        gpio_put(c1530->read_gpio, true);
    else    
        gpio_put(c1530->read_gpio, false);

    c1530->timer.delay_us = c1530->send_buffer[c1530->send_buffer_read_pos++];    
    return true;
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

void C1530Class::read_start()
{
    for(int i=0; i<256; i++)
        send_buffer[i] = -100;

    send_buffer_read_pos = 0;
    tap_image_pos = 0x14;;
            
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

void C1530Class::fill_send_buffer()
{
        if(send_buffer_read_pos == 0 && !buffer1_is_ready)
        {
            // Puffer 1 füllen

            for(int i=128; i<256; i+=2)
            {
                int32_t pulse = get_next_tap_us_pulse();
                if(tap_image_pos >= sizeof(TAPE_DATA))
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
                if(tap_image_pos >= sizeof(TAPE_DATA))
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
}

int32_t C1530Class::get_next_tap_us_pulse()
{
        uint32_t cycles = TAPE_DATA[tap_image_pos];
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
            tmp[0] = TAPE_DATA[tap_image_pos + 1];
            tmp[1] = TAPE_DATA[tap_image_pos + 2];
            tmp[2] = TAPE_DATA[tap_image_pos + 3];
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