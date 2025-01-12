#include <stdio.h>
#include "pico/stdlib.h"

#define PLAY_BUTTON_GPIO        16

#define C64_TAPE_READ_GPIO      2
#define C64_TAPE_SENSE_GPIO     3

void init_tape_ports_c64()
{
    gpio_init(C64_TAPE_READ_GPIO);
    gpio_init(C64_TAPE_SENSE_GPIO);

    gpio_set_dir(C64_TAPE_READ_GPIO, true);
    gpio_set_dir(C64_TAPE_SENSE_GPIO, true);
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

    while (true) 
    {
        play_button_new_state = gpio_get(PLAY_BUTTON_GPIO);
        
        if(play_button_new_state == true && play_button_old_state == false)
        {
            printf("Play Button is pressed!\n");
            gpio_put(C64_TAPE_SENSE_GPIO, false);
        }

        play_button_old_state = play_button_new_state;

        sleep_ms(100);
    }
}
