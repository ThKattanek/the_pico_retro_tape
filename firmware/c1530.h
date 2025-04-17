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

struct TAPHeader {
    char     magic_id[12];    // "C64-TAPE-RAW"
    uint32_t version;      // 0x01000000
    uint32_t data_length;  // Length of following tape data
}__attribute__((packed));

class C1530Class
{
public:
    C1530Class();
    void init_gpios(int read_gpio, int write_gpio, int sense_gpio, int motor_gpio);
    void update();  // tap datas from memory
    bool open_tap_image(char* filename);   // from sd card
   
    /**
     * @brief Opens an image from a memory buffer.
     * 
     * @param image_buffer Pointer to the buffer containing the image data.
     * @param image_buffer_size Size of the image buffer in bytes.
     * @param type The type of the image.
     * @return true if the image was successfully opened, false otherwise.
     */
    bool open_tap_image(const uint8_t* image_buffer, UINT image_buffer_size, IMAGE_TYPE type);  // from memory
    
    /**
     * @brief Closes the currently open image file.
     *
     * This function is responsible for closing the image file that is currently open.
     * It ensures that all resources associated with the file are properly released.
     */
    void close_image();
    
    /**
     * @brief Initiates the read process.
     *
     * This function starts the process of reading data from the device.
     * It sets up necessary configurations and prepares the system for
     * data acquisition.
     */
    void read_start();
    void stop();
    bool is_tap_end();
    double calculate_tap_runtime();

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
    int32_t get_next_tap_us_pulse_from_file();
    void fill_send_buffer_from_memory();
    void fill_send_buffer_from_file();

    void sync_10sek();
    void send_byte(uint8_t byte);
    void send_test();

    const uint8_t *tap_data_buffer;
    UINT tap_data_buffer_size;

    bool buffer0_is_ready;
    bool buffer1_is_ready;
    bool tap_image_is_end;

    bool motor_new_state;
    bool motor_old_state;

    int image_source;
    int image_type;
    bool is_tape_insert;

    FIL file;
    TAPHeader tap_header;
};


#endif // C1530_CLASS_H