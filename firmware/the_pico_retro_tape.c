#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/timer.h>

#include "./ggs.h"

#define PLAY_BUTTON_GPIO        16

#define C64_TAPE_READ_GPIO      2
#define C64_TAPE_SENSE_GPIO     3

#define C64_TAPE_WRITE_GPIO     4
#define C64_TAPE_MOTOR_GPIO     5

#define SHORT_PULSE     176     // 176µs
#define MEDIUM_PULSE    256     // 256µs
#define LONG_PULSE      336     // 336µs

struct repeating_timer timer;
volatile int32_t send_buffer[256];
volatile uint8_t send_buffer_read_pos;    

volatile uint32_t tap_image_pos = 0x14;

struct C64_HEADER
{
    uint8_t HeaderType;
    uint8_t StartAddressLowByte;
    uint8_t StartAddressHighByte;
    uint8_t EndAddressLowByte;
    uint8_t EndAddressHighByte;
    uint8_t Filename1[16];  // displayed in the FOUND message
    uint8_t Filename2[171]; // not displayed in the FOUND message
};

void init_tape_ports_c64()
{
    // output
    gpio_init(C64_TAPE_READ_GPIO);
    gpio_init(C64_TAPE_SENSE_GPIO);
    gpio_set_dir(C64_TAPE_READ_GPIO, true);
    gpio_set_dir(C64_TAPE_SENSE_GPIO, true);

    // input
    gpio_init(C64_TAPE_WRITE_GPIO);
    gpio_init(C64_TAPE_MOTOR_GPIO);
    gpio_set_dir(C64_TAPE_WRITE_GPIO, false);
    gpio_set_dir(C64_TAPE_MOTOR_GPIO, false);
    gpio_set_pulls(C64_TAPE_WRITE_GPIO, false, false);
    gpio_set_pulls(C64_TAPE_MOTOR_GPIO, false, false);
}

void sync_10sek()
{
    for(int i=0; i<2840*10; i++)
    {
        gpio_put(C64_TAPE_READ_GPIO, true);
        sleep_us(SHORT_PULSE);
        gpio_put(C64_TAPE_READ_GPIO, false);
        sleep_us(SHORT_PULSE);
    }
}

void send_byte(uint8_t byte)
{
    // ByteMaker
    gpio_put(C64_TAPE_READ_GPIO, true);
    sleep_us(LONG_PULSE);
    gpio_put(C64_TAPE_READ_GPIO, false);
    sleep_us(LONG_PULSE);
    gpio_put(C64_TAPE_READ_GPIO, true);
    sleep_us(MEDIUM_PULSE);
    gpio_put(C64_TAPE_READ_GPIO, false);
    sleep_us(MEDIUM_PULSE);

    // Byte
    uint8_t bit_mask = 1;
    for(int i=0; i<8; i++)
    {
        if(byte & bit_mask)
        {
            // 1-bit
            gpio_put(C64_TAPE_READ_GPIO, true);
            sleep_us(MEDIUM_PULSE);
            gpio_put(C64_TAPE_READ_GPIO, false);
            sleep_us(MEDIUM_PULSE);
            gpio_put(C64_TAPE_READ_GPIO, true);
            sleep_us(SHORT_PULSE);
            gpio_put(C64_TAPE_READ_GPIO, false);
            sleep_us(SHORT_PULSE);
        }
        else
        {
            // 0-bit
            gpio_put(C64_TAPE_READ_GPIO, true);
            sleep_us(SHORT_PULSE);
            gpio_put(C64_TAPE_READ_GPIO, false);
            sleep_us(SHORT_PULSE);
            gpio_put(C64_TAPE_READ_GPIO, true);
            sleep_us(MEDIUM_PULSE);
            gpio_put(C64_TAPE_READ_GPIO, false);
            sleep_us(MEDIUM_PULSE);
        }

        bit_mask <<= 1;
    }

    if((byte % 2) == 0)
    {
        // gerade anzahl von 1 Bits
        // Parity Bit = 1    
        gpio_put(C64_TAPE_READ_GPIO, true);
        sleep_us(MEDIUM_PULSE);
        gpio_put(C64_TAPE_READ_GPIO, false);
        sleep_us(MEDIUM_PULSE);
        gpio_put(C64_TAPE_READ_GPIO, true);
        sleep_us(SHORT_PULSE);
        gpio_put(C64_TAPE_READ_GPIO, false);
        sleep_us(SHORT_PULSE);
    }
    else
    {
        // ungerade anzahl von 1 Bits
        // Parity Bit = 0
        gpio_put(C64_TAPE_READ_GPIO, true);
        sleep_us(SHORT_PULSE);
        gpio_put(C64_TAPE_READ_GPIO, false);
        sleep_us(SHORT_PULSE);
        gpio_put(C64_TAPE_READ_GPIO, true);
        sleep_us(MEDIUM_PULSE);
        gpio_put(C64_TAPE_READ_GPIO, false);
        sleep_us(MEDIUM_PULSE);

    }
}

void send_test()
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
    for(int i=0; i<sizeof(header); i++)
    {
        checksum ^= buffer[i];
        send_byte(buffer[i]);
    }

    // send the checksum
    send_byte(checksum);

    // long pulse
    gpio_put(C64_TAPE_READ_GPIO, true);
    sleep_us(LONG_PULSE);
    gpio_put(C64_TAPE_READ_GPIO, false);
    sleep_us(LONG_PULSE);

    // 60 Sync Short Pulse
    for(int i=0; i<30; i++)
    {
        gpio_put(C64_TAPE_READ_GPIO, true);
        sleep_us(SHORT_PULSE);
        gpio_put(C64_TAPE_READ_GPIO, false);
        sleep_us(SHORT_PULSE);
    }

     // 2. Countdown sequence
    for(uint8_t c_byte = 0x09; c_byte >= 0x01; c_byte--)
        send_byte(c_byte);

    // send 192 bytes data
    checksum = 0;
    for(int i=0; i<sizeof(header); i++)
    {
        checksum ^= buffer[i];
        send_byte(buffer[i]);
    }

    // send the checksum
    send_byte(checksum);

    // End-of-data MArke
    gpio_put(C64_TAPE_READ_GPIO, true);
    sleep_us(LONG_PULSE);
    gpio_put(C64_TAPE_READ_GPIO, false);
    sleep_us(LONG_PULSE);
    gpio_put(C64_TAPE_READ_GPIO, true);
    sleep_us(SHORT_PULSE);
    gpio_put(C64_TAPE_READ_GPIO, false);
    sleep_us(SHORT_PULSE);

    // long pulse
    gpio_put(C64_TAPE_READ_GPIO, true);
    sleep_us(LONG_PULSE);
    gpio_put(C64_TAPE_READ_GPIO, false);
    sleep_us(LONG_PULSE);

    // 60 Sync Short Pulse
    for(int i=0; i<30; i++)
    {
        gpio_put(C64_TAPE_READ_GPIO, true);
        sleep_us(SHORT_PULSE);
        gpio_put(C64_TAPE_READ_GPIO, false);
        sleep_us(SHORT_PULSE);
    }
}

void send_tap_image()
{
    int counter = 0x14;
    int pulse;
    uint32_t cycles;

    for(int i=0; i<sizeof(TAPE_DATA); i++)
    {
         cycles = TAPE_DATA[counter] << 3;

        if(cycles != 0)
        {
            pulse = cycles * 0.5182846f; 
            counter++;
        }
        else 
        {
            // Wenn 0 dann die nächsten 3Bytes in unsigned int (32Bit) wandeln
            // Mit Hilfe eines Zeigers auf cycles
            cycles = 0;
            unsigned char *tmp = (unsigned char*) & cycles;
            tmp[0] = TAPE_DATA[counter + 1];
            tmp[1] = TAPE_DATA[counter + 2];
            tmp[2] = TAPE_DATA[counter + 3];
            pulse = cycles * 0.5182846f;
            counter += 4;
        }

        gpio_put(C64_TAPE_READ_GPIO, true);
        sleep_us(pulse);
        gpio_put(C64_TAPE_READ_GPIO, false);
        sleep_us(pulse);        
    }
}

int32_t get_next_tap__us_pulse(const uint8_t *tap_image, uint32_t *tap_image_pos_1)
{
        uint32_t cycles = tap_image[tap_image_pos];
        uint32_t pulse;

        if(cycles != 0)
        {
            pulse = (cycles << 3) * 0.50748644f; 
            tap_image_pos++;
        }
        else 
        {
            // Wenn 0 dann die nächsten 3Bytes in unsigned int (32Bit) wandeln
            // Mit Hilfe eines Zeigers auf cycles
            unsigned char *tmp = (unsigned char*) & cycles;
            tmp[0] = tap_image[tap_image_pos + 1];
            tmp[1] = tap_image[tap_image_pos + 2];
            tmp[2] = tap_image[tap_image_pos + 3];
            pulse = cycles * 0.50748644f;
            tap_image_pos += 4;
        }

       // printf("Pos: %d, DATA: %d\n", tap_image_pos, pulse);

        return pulse * -1;
}

bool repeating_timer_callback(__unused struct repeating_timer *t) 
{
    bool out = gpio_get(C64_TAPE_READ_GPIO);
    gpio_put(C64_TAPE_READ_GPIO, !out);

    int32_t pulse = send_buffer[send_buffer_read_pos];
    
/*
    if(out)
        timer.delay_us = pulse;
    else
        timer.delay_us = pulse;
*/
    timer.delay_us = pulse;

    
    
    send_buffer_read_pos++;
    
    return true;
}

int main()
{
    stdio_init_all();

    //  PlayButton is Input an set pull down
    gpio_init(PLAY_BUTTON_GPIO);
    gpio_set_dir(PLAY_BUTTON_GPIO, false);
    gpio_set_pulls(PLAY_BUTTON_GPIO, false, true);

    init_tape_ports_c64();

    gpio_put(C64_TAPE_SENSE_GPIO, true);

    bool play_button_new_state = false;
    bool play_button_old_state = false;

    bool c64_motor_new_state = false;
    bool c64_motor_old_state = false;

    while (true) 
    {
        
        play_button_new_state = gpio_get(PLAY_BUTTON_GPIO);
        if(play_button_new_state == true && play_button_old_state == false)
        {
            printf("Play Button is pressed!\n");
            gpio_put(C64_TAPE_SENSE_GPIO, false);

            //send_test();
            //send_byte(0x00);
            //send_tap_image();

            // Hardware Timer aktivieren
            // GPIO OUT Low
    
            for(int i=0; i<256; i++)
                send_buffer[i] = -100;

            send_buffer_read_pos = 0;
            
            gpio_put(C64_TAPE_READ_GPIO, true);
            add_repeating_timer_us(-1000, repeating_timer_callback, NULL, &timer);

            bool buffer0_is_ready = false;
            bool buffer1_is_ready = false;
            
            bool tap_image_is_end = false;
            while(!tap_image_is_end)
            {
                if(send_buffer_read_pos == 0 && !buffer1_is_ready)
                {
                    // Puffer 1 füllen

                    for(int i=128; i<256; i+=2)
                    {
                        int32_t pulse = get_next_tap__us_pulse(TAPE_DATA, &tap_image_pos);
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
                    printf("Buffer 1 is ready.\n");
                }

                if(send_buffer_read_pos == 128 && !buffer0_is_ready)
                {
                    // Puffer 0 füllen

                    for(int i=0; i<128; i+=2)
                    {
                        int32_t pulse = get_next_tap__us_pulse(TAPE_DATA, &tap_image_pos);
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
                    printf("Buffer 0 is ready.\n");
                }
            }
            printf("tap image completed.\n");
        }
        

        play_button_old_state = play_button_new_state;

        c64_motor_new_state = gpio_get(C64_TAPE_MOTOR_GPIO);

        if(c64_motor_new_state == true && c64_motor_old_state == false)
        {
            // motor on
            printf("Motor is on.\n");
        }

        if(c64_motor_new_state == false && c64_motor_old_state == true)
        {
            // motor off
            printf("Motor is off.\n");
        }

        c64_motor_old_state = c64_motor_new_state;

        sleep_ms(100);
    }
}
