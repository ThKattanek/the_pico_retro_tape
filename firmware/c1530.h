////////////////////////////////////////////////////////
//                                                    //
// ThePicoRetroTape                                   //
// von Thorsten Kattanek                              //
//                                                    //
// #file: c1530.h                                     //
//                                                    //
// https://github.com/ThKattanek/the_pico_retro_tape  //
//                                                    //
//                                                    //
//////////////////////////////////////////////////////// 

#ifndef C1530_CLASS_H
#define C1530_CLASS_H

#define SHORT_PULSE     176     // 176µs
#define MEDIUM_PULSE    256     // 256µs
#define LONG_PULSE      336     // 336µs 

enum IMAGE_TYPE {TAP, T64, PRG};
enum IMAGE_SOURCE {SDCARD, MEMORY};

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

class C1530Class
{
public:
    C1530Class();
    void init_gpios(int read_gpio, int write_gpio, int sense_gpio, int motor_gpio);
    void update();  // tap datas from memory
    bool open_image(char* filename);   // from sd card
   
    /**
     * @brief Opens an image from a memory buffer.
     * 
     * @param image_buffer Pointer to the buffer containing the image data.
     * @param image_buffer_size Size of the image buffer in bytes.
     * @param type The type of the image.
     * @return true if the image was successfully opened, false otherwise.
     */
    bool open_image(const uint8_t* image_buffer, int image_buffer_size, IMAGE_TYPE type);  // from memory
    void close_image();
    void read_start();
    bool is_tap_end();

    int read_gpio;
    int write_gpio;
    int sense_gpio;
    int motor_gpio;

    struct repeating_timer timer;
    int32_t send_buffer[256];
    uint8_t send_buffer_read_pos;  

    bool motor_state;
    uint32_t tap_image_pos;

private:
    int32_t get_next_tap_us_pulse();
    void fill_send_buffer();
    void fill_send_buffer(FIL* file);

    void sync_10sek();
    void send_byte(uint8_t byte);
    void send_test();

    const uint8_t *tap_data_buffer;
    int tap_data_buffer_size;

    bool buffer0_is_ready;
    bool buffer1_is_ready;
    bool tap_image_is_end;

    bool motor_new_state;
    bool motor_old_state;

    int image_source;
    int image_type;
    bool is_tape_insert;
};


#endif // C1530_CLASS_H