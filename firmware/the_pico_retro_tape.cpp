////////////////////////////////////////////////////////
//                                                    //
// ThePicoRetroTape                                   //
// von Thorsten Kattanek                              //
//                                                    //
// #file: the_pico_retro_tape.c                       //
//                                                    //
// https://github.com/ThKattanek/the_pico_retro_tape  //
//                                                    //
//                                                    //
//////////////////////////////////////////////////////// 

#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/timer.h>
#include "sd_card.h"
#include "ff.h"

// SD Card
char buf[100];
char filename[] = "test02.txt";

FRESULT fr;
FATFS fs;
FIL fil;
int ret;

bool    sd_card_is_ready;

// Keys
#define KEY_WAIT 10000          // Tasten Entprellen
#define PLAY_BUTTON_GPIO        20

///// for commodore datasette 1530 support

#define C1530_TAPE_READ_GPIO      2
#define C1530_TAPE_WRITE_GPIO     4
#define C1530_TAPE_SENSE_GPIO     3
#define C1530_TAPE_MOTOR_GPIO     5

#include "./c1530.h"
C1530Class c1530;


void CheckKeys();
int InitSDCard();

int main()
{
     stdio_init_all();

    //  PlayButton is Input an set pull down
    gpio_init(PLAY_BUTTON_GPIO);
    gpio_set_dir(PLAY_BUTTON_GPIO, false);
    gpio_set_pulls(PLAY_BUTTON_GPIO, false, true);

    c1530.init_gpios(C1530_TAPE_READ_GPIO, C1530_TAPE_WRITE_GPIO, C1530_TAPE_SENSE_GPIO, C1530_TAPE_MOTOR_GPIO);
    gpio_put(C1530_TAPE_SENSE_GPIO, true);

    if(InitSDCard() == 0)
        sd_card_is_ready = true;
    else
        sd_card_is_ready = false;

    while (true) 
    {
        CheckKeys();

        // sendbuffer fill with new data
        if(!c1530.is_tap_end())
            c1530.update();
    }
}

void CheckKeys()
{
    static int key_wait_counter = 0;
    static bool play_button_new_state = false;
    static bool play_button_old_state = false;

    if(key_wait_counter == KEY_WAIT)
    {
        key_wait_counter = 0;

        play_button_new_state = gpio_get(PLAY_BUTTON_GPIO);
        if(play_button_new_state == true && play_button_old_state == false)
        {
            printf("Play Button is pressed!\n");
            gpio_put(C1530_TAPE_SENSE_GPIO, false);    
            c1530.read_start();
        }
        play_button_old_state = play_button_new_state;
    }

    key_wait_counter++;
}

int InitSDCard()
{
    printf("Init SD Card\r\n");

    // Initialize SD card
    if(!sd_init_driver())
    {
        printf("\rERROR: Could not initialize SD card\r\n");
        return -1;
    }

    // Mount drive
    fr = f_mount(&fs, "0:", 1);
    if(fr != FR_OK)
    {
        printf("ERROR: Could not mount filesystem (%d)\r\n", fr);
        return -2;
    }

    return 0;
}