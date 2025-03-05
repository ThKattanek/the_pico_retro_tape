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

// Constructor

FileBrowser::FileBrowser(ST7735_TFT* tft, const char* root_path, const char** allowed_extensions, int num_allowed_extensions)
    : dir_entrys_pos(0), dir_entrys_pos_old(0) , tft(tft), root_path((char*)root_path), current_path(NULL), dir_level(0)
{
    current_path = (char*)malloc(MAX_PATH_LENGTH);
    strcpy(current_path, root_path);    

    if(allowed_extensions != NULL)
    {
        this->allowed_extensions = allowed_extensions;
        this->num_allowed_extensions = num_allowed_extensions;
    }   

    DrawPage();
}   

// Destructor

FileBrowser::~FileBrowser()
{
}

// Public functions

void FileBrowser::DrawPage()
{
    if(ReadDirEntrys() == 0)
    {
        if(current_page > 0)
        {
            current_page--;
        }
        return; 
    }

    tft->TFTfillScreen(0x0000);

    for(int i=0; i<current_page_entries; i++)
    {
        if(i == dir_entrys_pos)
            DrawLine(i, true);
        else
            DrawLine(i, false);
    }
}

void FileBrowser::Up()
{
    DrawLine(dir_entrys_pos, false);

    dir_entrys_pos--;
    if(dir_entrys_pos < 0)
    {
        // previous page
        if(current_page > 0)
        {
            current_page--;
            DrawPage();
        }


        dir_entrys_pos = current_page_entries-1;
    }

    DrawLine(dir_entrys_pos, true);
}

void FileBrowser::Down()
{
    DrawLine(dir_entrys_pos, false);

    dir_entrys_pos++;
    if(dir_entrys_pos > current_page_entries-1)
    {
        // next page
        current_page++;
        DrawPage();

        dir_entrys_pos = 0;
    }   

    DrawLine(dir_entrys_pos, true);
}

bool FileBrowser::Enter()
{
    if(dir_entrys_is_dir[dir_entrys_pos])
    {
        if(strcmp(dir_entrys[dir_entrys_pos], "..") == 0)
        {
            // Go up one directory
            char* p = strrchr(current_path, '/');
            if(p != NULL)
            {
                *p = '\0'; // Entferne den letzten Teil des Pfads
                dir_level--;
            }
            if (strlen(current_path) == 0) {
                strcpy(current_path, "/");
            }
            current_page = 0;
        }
        else
        {
            // Go down one directory
            if(strcmp(current_path, "/") != 0)
                strcat(current_path, "/");
            
            strcat(current_path, dir_entrys[dir_entrys_pos]);
            dir_level++;
            current_page = 0;
        }

        dir_entrys_pos_old = dir_entrys_pos;
        dir_entrys_pos = 0;

        printf("Enter directory %s\r\n", current_path);

        DrawPage();
        return true;    // is a directory
    }
    else
    {
        return false;   // is a file
    }
}

void FileBrowser::Back()
{
    // Go up one directory
    if (strcmp(current_path, "/") != 0) {
        char* p = strrchr(current_path, '/');
        if(p != NULL)
        {
            *p = '\0'; // Entferne den letzten Teil des Pfads
            dir_level--;
            if (strlen(current_path) == 0) {
                strcpy(current_path, "/");
            }
            dir_entrys_pos = 0;
            DrawPage();
        }
    }
}

// Private functions

int FileBrowser::ReadDirEntrys()
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
        if(current_page > 0)
        {
            int files_to_skip;
            
            if(dir_level > 0)   
                files_to_skip = current_page * (MAX_DIR_ENTRYS -1);
            else
                files_to_skip = current_page * MAX_DIR_ENTRYS;

            for(int i=0; i<files_to_skip; i++)
            {
                res = f_readdir(&dir, &fno);
                if(res != FR_OK || fno.fname[0] == 0) break;
            }
        }

        nfile = ndir = 0;

        if(dir_level > 0)   // Not root directory
        {
            sprintf(str0, "..");
            printf(str0);
            strncpy(dir_entrys[ndir + nfile], str0, 266);
            dir_entrys_is_dir[ndir + nfile] = true;
            ndir++;
        }

        for(;;)
        {
            res = f_readdir(&dir, &fno);
            if(res != FR_OK || fno.fname[0] == 0) break;
            if(fno.fattrib & AM_DIR)
            {
                // Directory
                if(hide_dot_files && fno.fname[0] != '.')
                {
                    strncpy(dir_entrys[ndir + nfile], fno.fname, 255);
                    dir_entrys_is_dir[ndir + nfile] = true;
                    ndir++;
                }

            }
            else
            {
                // File
                bool allowed = false;

                if(num_allowed_extensions == 0)
                {
                    allowed = true;
                }
                else
                {
                    for (int i = 0; i < num_allowed_extensions; i++)
                    {
                        if (strstr(fno.fname, allowed_extensions[i]) != NULL)
                        {
                            allowed = true;
                            break;
                        }
                    }
                }

                if(hide_dot_files && fno.fname[0] != '.' && allowed)
                {
                    strncpy(dir_entrys[ndir + nfile], fno.fname, 255);
                    dir_entrys_is_dir[ndir + nfile] = false;
                    nfile++;
                }
            }
            if((nfile + ndir) == MAX_DIR_ENTRYS) break;
        }

        f_closedir(&dir);
        current_page_entries = nfile + ndir;

        printf("\r\n%d dirs, %d files.\r\n", ndir, nfile);

        int num_files = nfile + ndir;
        if(dir_level > 0)   
            num_files--;   

        return num_files;
    }   
    else
    {
        printf("Failed to open \"%s\"\r\n", current_path);
        return 0;
    }
}

void FileBrowser::DrawLine(int line, bool selected)
{
    char str0[chars_per_line+1];
    char str1[256+6+1];

    if(dir_entrys_is_dir[line])
        sprintf(str1, "<DIR> %s", dir_entrys[line]);
    else
        sprintf(str1, "%s", dir_entrys[line]);

    strncpy(str0, str1, chars_per_line);
    str0[chars_per_line] = 0;

    if(selected)
    {
        tft->TFTfillRect(0, line * 10 , 128, 8, sel_color);
        tft->TFTdrawText(0, line * 10, str0, fg_color, sel_color, 1);
    }
    else
    {
        tft->TFTfillRect(0, line * 10 , 128, 8, bg_color);
        tft->TFTdrawText(0, line * 10, str0, fg_color, bg_color, 1);
    }
}