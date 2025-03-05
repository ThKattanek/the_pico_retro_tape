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
#include <hardware/clocks.h>
#include "sd_card.h"
#include "ff.h"

//#include "tap_images/ggs.h"           // the great giana sisters
//#include "tap_images/aargh_tap.h"     // aargh!
//#include "tap_images/goonies.h"         // the goonies
//#include "tap_images/hoh.h"           // head over heels

// SD Card
char buf[100];
char filename[] = "/c64_tap/Demos [ABC].tap";

const char* allowed_extensions[] = {".tap", ".t64", ".prg"};

FATFS fs;
int ret;

bool    sd_card_is_ready;

// Keys
#define KEY_WAIT 10000          // Tasten Entprellen
#define PLAY_BUTTON_GPIO   20
#define KEY_UP_GPIO        6
#define KEY_DOWN_GPIO      7
#define KEY_ENTER_GPIO     8

///// for commodore datasette 1530 support

#define C1530_TAPE_READ_GPIO      2
#define C1530_TAPE_WRITE_GPIO     4
#define C1530_TAPE_SENSE_GPIO     3
#define C1530_TAPE_MOTOR_GPIO     5

#include "./c1530.h"
C1530Class c1530;

#include "st7735/ST7735_TFT.hpp"
ST7735_TFT tft;

#include "file_browser.h"
FileBrowser* file_browser;

void CheckKeys();
int InitSDCard();
void InitTFTDisplay(ST7735_TFT *tft);
void ReleaseSDCard();

int main()
{
    // Set system clock to 200 MHz
    // set_sys_clock_khz(220000, true);
    
    stdio_init_all();

    // Überprüfen Sie die tatsächliche Taktfrequenz
    uint32_t freq = clock_get_hz(clk_sys);
    printf("System clock set to %lu Hz\n", freq);

    //  PlayButton is Input an set pull down
    gpio_init(PLAY_BUTTON_GPIO);
    gpio_set_dir(PLAY_BUTTON_GPIO, false);
    gpio_set_pulls(PLAY_BUTTON_GPIO, false, true);

    //  KeyUp is Input an set pull down
    gpio_init(KEY_UP_GPIO);
    gpio_set_dir(KEY_UP_GPIO, false);
    gpio_set_pulls(KEY_UP_GPIO, false, true);

    //  KeyDown is Input an set pull down
    gpio_init(KEY_DOWN_GPIO);
    gpio_set_dir(KEY_DOWN_GPIO, false);
    gpio_set_pulls(KEY_DOWN_GPIO, false, true);

    //  KeyEnter is Input an set pull down
    gpio_init(KEY_ENTER_GPIO);
    gpio_set_dir(KEY_ENTER_GPIO, false);
    gpio_set_pulls(KEY_ENTER_GPIO, false, true);


    c1530.init_gpios(C1530_TAPE_READ_GPIO, C1530_TAPE_WRITE_GPIO, C1530_TAPE_SENSE_GPIO, C1530_TAPE_MOTOR_GPIO);
    gpio_put(C1530_TAPE_SENSE_GPIO, true);

    if(InitSDCard() == 0)
        sd_card_is_ready = true;
    else
        sd_card_is_ready = false;

    InitTFTDisplay(&tft);
    
    tft.TFTFontNum(TFT_FONT_TYPE_e::TFTFont_Default);
	tft.TFTfillScreen(ST7735_BLACK);
	tft.setTextColor(0xffff, 0x0000);
	tft.TFTsetCursor(0,0);
	tft.TFTsetScrollDefinition(0,160,1);

    file_browser = new FileBrowser(&tft, "/", allowed_extensions, 3);

    // Open a tap image with the c1530 class and print corresponding message
    if (c1530.open_image(filename)) {
        printf("Successfully opened 1530 image \"%s\"\n", filename);
    } else {
        printf("Failed to open 1530 image \"%s\"\n", filename);
    }

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
    static bool up_button_new_state = false;
    static bool up_button_old_state = false;
    static bool down_button_new_state = false;
    static bool down_button_old_state = false;
    static bool enter_button_new_state = false;
    static bool enter_button_old_state = false;

    if(key_wait_counter == KEY_WAIT)
    {
        key_wait_counter = 0;

        play_button_new_state = gpio_get(PLAY_BUTTON_GPIO);
        if(play_button_new_state == true && play_button_old_state == false)
        {
            printf("Play Button is pressed!\n");  
            c1530.read_start();
        }
        play_button_old_state = play_button_new_state;

        up_button_new_state = gpio_get(KEY_UP_GPIO);
        if(up_button_new_state == true && up_button_old_state == false)
        {
            printf("Up Button is pressed!\n");
            file_browser->Up();
        }
        up_button_old_state = up_button_new_state;

        down_button_new_state = gpio_get(KEY_DOWN_GPIO);
        if(down_button_new_state == true && down_button_old_state == false)
        {
            printf("Down Button is pressed!\n");
            file_browser->Down();
        }
        down_button_old_state = down_button_new_state;

        enter_button_new_state = gpio_get(KEY_ENTER_GPIO);
        if(enter_button_new_state == true && enter_button_old_state == false)
        {
            printf("Enter Button is pressed!\n");
            if(!file_browser->Enter())
            {
                // Open file
                char* file = file_browser->GetCurrentFile();
                printf("Open file %s\n", file);
                char path[256];
                sprintf(path, "/c64_tap/%s", file);
                if (c1530.open_image(path)) {
                    printf("Successfully opened 1530 image \"%s\"\n", path);
                    c1530.stop();
                } else {
                    printf("Failed to open 1530 image \"%s\"\n", path);
                }
            }

            /*
            if(dir_entrys_is_dir[dir_entrys_pos])
            {
                char path[256];
                sprintf(path, "/tap/%s", dir_entrys[dir_entrys_pos]);
                ReadDirEntrys(path);
                DrawFileBrowser();
            }
            else
            {
                printf("Open file %s\n", dir_entrys[dir_entrys_pos]);
                char path[256];
                sprintf(path, "/c64_tap/%s", dir_entrys[dir_entrys_pos]);
                if (c1530.open_image(path)) {
                    printf("Successfully opened 1530 image \"%s\"\n", path);
                } else {
                    printf("Failed to open 1530 image \"%s\"\n", path);
                }
            }
            */
        }
        enter_button_old_state = enter_button_new_state;
    }

    key_wait_counter++;
}

int InitSDCard()
{
    FRESULT fr;

    printf("Init SD Card...");

    // Initialize SD card
    if(!sd_init_driver())
    {
        printf("\rERROR: Could not initialize SD card\r\n");
        return -1;
    }

    // Mount drive
    fr = f_mount(&fs, "", 1);
    if(fr != FR_OK)
    {
        printf("ERROR: Could not mount filesystem (%d)\r\n", fr);
        return -2;
    }

    printf("OK\r\n");

    return 0;
}

void ReleaseSDCard()
{
    printf("Release SD Card\r\n");
    f_unmount("");
}

void InitTFTDisplay(ST7735_TFT *tft)
{
	sleep_ms(20);	// wait to tft 20ms

//*************** USER OPTION 0 SPI_SPEED + TYPE ***********
	bool bhardwareSPI = true; // true for hardware spi,

	if (bhardwareSPI == true) { // hw spi
		uint32_t TFT_SCLK_FREQ =  40000 ; // Spi freq in KiloHertz , 1000 = 1Mhz
		tft->TFTInitSPIType(TFT_SCLK_FREQ, spi1);
	} else { // sw spi
		tft->TFTInitSPIType();
	}

// ******** USER OPTION 1 GPIO *********
// NOTE if using Hardware SPI clock and data pins will be tied to
// the chosen interface eg Spi0 CLK=18 DIN=19)
	int8_t SDIN_TFT = 11;
	int8_t SCLK_TFT = 10;
	int8_t DC_TFT = 15;
	int8_t CS_TFT = 14 ;
	int8_t RST_TFT = 13;
	tft->TFTSetupGPIO(RST_TFT, DC_TFT, CS_TFT, SCLK_TFT, SDIN_TFT);

// ****** USER OPTION 2 Screen Setup ******
	uint8_t OFFSET_COL = 0;  // 2, These offsets can be adjusted for any issues->
	uint8_t OFFSET_ROW = 0; // 3, with screen manufacture tolerance/defects
    uint16_t TFT_WIDTH = 128;// Screen width in pixels
    uint16_t TFT_HEIGHT = 160; // Screen height in pixels
    tft->TFTInitScreenSize(OFFSET_COL, OFFSET_ROW , TFT_WIDTH , TFT_HEIGHT);

// ******** USER OPTION 3 PCB_TYPE  **************************
	tft->TFTInitPCBType(TFT_PCBtype_e::TFT_ST7735S_Black); // pass enum,4 choices,see README
}