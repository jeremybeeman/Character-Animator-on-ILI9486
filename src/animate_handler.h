// IMPORTANT: LCDWIKI_KBV LIBRARY MUST BE SPECIFICALLY
// CONFIGURED FOR EITHER THE TFT SHIELD OR THE BREAKOUT BOARD.
//Set the pins to the correct ones for your development shield or breakout board.
//the 16bit mode only use in Mega.you must modify the mode in the file of lcd_mode.h
//when using the BREAKOUT BOARD only and using these 16 data lines to the LCD,
//pin usage as follow:
//             CS  CD  WR  RD  RST  D0  D1  D2  D3  D4  D5  D6  D7  D8  D9  D10  D11  D12  D13  D14  D15 
//Arduino Mega 40  38  39  /   41   37  36  35  34  33  32  31  30  22  23  24   25   26   27   28   29

//Remember to set the pins to suit your display module!

#include "colors.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <LCDWIKI_GUI.h> //Core graphics library
#include <LCDWIKI_KBV.h> //Hardware-specific library

enum draw_direction {up2down, down2up, left2right, right2left};

uint16_t read_16(File fp);

uint32_t read_32(File fp);
 
bool analysis_bmp_header(File fp, uint32_t* bmp_width, uint32_t* bmp_height);

void draw_bmp_picture(File fp, char increment, enum draw_direction draw_dir, uint32_t bmp_width);

void display_bmp(const char* file_name, enum draw_direction draw_dir);

void init_SD_display();

bool verify_arf(File arf_fp, uint32_t* arf_num_entries, char* draw_dir, char* encode_type);

void print_arf_dir_encode1(File arf_file, uint32_t arf_num_entries, char draw_dir);

void print_arf_dir_encode2(File arf_file, uint32_t arf_num_entries, char draw_dir);

//.arf stands for animation rendering file
void display_arf(const char* file_name);

//assumes format of .bmp, then all .arf after
bool draw_animation(const char ** animation_files, int num_files, bool* already_blinked);
