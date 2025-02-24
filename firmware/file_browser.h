////////////////////////////////////////////////////////
//                                                    //
// ThePicoRetroTape                                   //
// von Thorsten Kattanek                              //
//                                                    //
// #file: file_browser.h                              //
//                                                    //
// https://github.com/ThKattanek/the_pico_retro_tape  //
//                                                    //
//                                                    //
//////////////////////////////////////////////////////// 

#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#include <stdio.h>
#include <pico/stdlib.h>
#include <ff.h>
#include "st7735/ST7735_TFT.hpp"

#define MAX_DIR_ENTRYS 16

class FileBrowser {
public:
    FileBrowser(ST7735_TFT* tft, const char* root_path);
    ~FileBrowser();

    void set_bg_color(uint16_t color) { bg_color = color; }
    void set_fg_color(uint16_t color) { fg_color = color; }
    void set_sel_color(uint16_t color) { sel_color = color; }
    void set_chars_per_line(int chars) { chars_per_line = chars; }
    
    char* GetCurrentFile() { return dir_entrys[dir_entrys_pos]; }

    void DrawPage();
    void Up();
    void Down();
    void Enter();

private:
    void ReadDirEntrys();

    char    dir_entrys[MAX_DIR_ENTRYS][256];    // Directory entrys
    bool    dir_entrys_is_dir[16];              // is true if entry is a directory ; false if entry is a file
    int     dir_entrys_pos;                     // Position in the directory entrys
    int     dir_entrys_pos_old;                 // Old position in the directory entrys

    ST7735_TFT* tft;        // TFT Display

    char* root_path;        // Root path of the file browser
    char* current_path;     // Current path of the file browser

    int chars_per_line = 21;    

    uint16_t bg_color = 0x0000;
    uint16_t fg_color = 0xFFFF;
    uint16_t sel_color = 0xF800;
};

#endif // FILE_BROWSER_H