////////////////////////////////////////////////////////
//                                                    //
// ThePicoRetroTape                                   //
// von Thorsten Kattanek                              //
//                                                    //
// #file: file_browser.cpp                            //
//                                                    //
// https://github.com/ThKattanek/the_pico_retro_tape  //
//                                                    //
//                                                    //
//////////////////////////////////////////////////////// 

#include "file_browser.h"

FileBrowser::FileBrowser(ST7735_TFT* tft, const char* root_path)
    : dir_entrys_pos(0), dir_entrys_pos_old(0) , tft(tft), root_path((char*)root_path), current_path((char*)root_path)
{
    DrawPage();
}   

FileBrowser::~FileBrowser()
{
}

void FileBrowser::ReadDirEntrys()
{
    FRESULT res;
    DIR dir;
    FILINFO fno;
    int nfile, ndir;

    char str0[266];

    printf("List all directory items [%s]\r\n", current_path);

    res = f_opendir(&dir, current_path);
    if(res == FR_OK)
    { 
        nfile = ndir = 0;
        for(;;)
        {
            res = f_readdir(&dir, &fno);
            if(res != FR_OK || fno.fname[0] == 0) break;
            if(fno.fattrib & AM_DIR)
            {
                // Directory
                sprintf(str0, "<DIR>  %s", fno.fname);
                printf(str0);
                strncpy(dir_entrys[ndir + nfile], str0, 266);
                dir_entrys_is_dir[ndir + nfile] = true;
                ndir++;
            }
            else
            {
                // File
                sprintf(str0, "%s", fno.fname);
                printf(str0);
                strncpy(dir_entrys[ndir + nfile], str0, 266);
                dir_entrys_is_dir[ndir + nfile] = false;
                nfile++;
            }
            if((nfile + ndir) == MAX_DIR_ENTRYS) break;
        }

        f_closedir(&dir);
        printf("\r\n%d dirs, %d files.\r\n", ndir, nfile);
    }   
    else
    {
        printf("Failed to open \"%s\"\r\n", current_path);
    }
}

void FileBrowser::DrawPage()
{
    ReadDirEntrys();
    tft->TFTfillScreen(0x0000);

    char str0[chars_per_line+1];

    uint16_t current_bg_color;

    for(int i=0; i<MAX_DIR_ENTRYS; i++)
    {
        if(i == dir_entrys_pos)
        {
            current_bg_color = sel_color;
            tft->TFTfillRect(0, i * 10 , 128, 8, sel_color);
        }
        else
        {
            current_bg_color = bg_color;
        }

        strncpy(str0, dir_entrys[i], chars_per_line);
        str0[chars_per_line] = 0;
        
        tft->TFTdrawText(0, i * 10, str0, fg_color, current_bg_color, 1);
    }
}

void FileBrowser::Up()
{
    char str0[chars_per_line+1];

    strncpy(str0, dir_entrys[dir_entrys_pos], chars_per_line);
    str0[chars_per_line] = 0;

    tft->TFTfillRect(0, dir_entrys_pos * 10 , 128, 8, bg_color);
    tft->TFTdrawText(0, dir_entrys_pos * 10, str0, fg_color, bg_color, 1);

    dir_entrys_pos--;
    if(dir_entrys_pos < 0)
        dir_entrys_pos = 0;

    strncpy(str0, dir_entrys[dir_entrys_pos], chars_per_line);
    str0[chars_per_line] = 0;

    tft->TFTfillRect(0, dir_entrys_pos * 10 , 128, 8, sel_color);
    tft->TFTdrawText(0, dir_entrys_pos * 10, str0, fg_color, sel_color, 1);
}

void FileBrowser::Down()
{
    char str0[chars_per_line+1];

    strncpy(str0, dir_entrys[dir_entrys_pos], chars_per_line);
    str0[chars_per_line] = 0;

    tft->TFTfillRect(0, dir_entrys_pos * 10 , 128, 8, bg_color);
    tft->TFTdrawText(0, dir_entrys_pos * 10, str0, fg_color, bg_color, 1);

    dir_entrys_pos++;
    if(dir_entrys_pos > MAX_DIR_ENTRYS-1)
        dir_entrys_pos = MAX_DIR_ENTRYS-1;

    strncpy(str0, dir_entrys[dir_entrys_pos], chars_per_line);
    str0[chars_per_line] = 0;

    tft->TFTfillRect(0, dir_entrys_pos * 10 , 128, 8, sel_color);
    tft->TFTdrawText(0, dir_entrys_pos * 10, str0, fg_color, sel_color, 1);
}

void FileBrowser::Enter()
{

}