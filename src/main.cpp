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

//the definiens of 16bit mode as follow:
//if the IC model is known or the modules is unreadable,you can use this constructed function
LCDWIKI_KBV my_lcd(ILI9486,40,38,39,-1,41); //model,cs,cd,wr,rd,reset

uint32_t bmp_offset = 0;
uint16_t s_width = my_lcd.Get_Display_Width();  
uint16_t s_heigh = my_lcd.Get_Display_Height();
enum draw_direction {up2down, down2up};

#define PIXEL_NUMBER  (s_width/4)

const char * main_file = "565.bmp";
#define num_blinking_files 6
const char * blinking_animation[num_blinking_files] = {
      "01.bmp", 
      "02.bmp", 
      "03.bmp", 
      "04.bmp", 
      "05.bmp", 
      "06.bmp"
    };
char * sbuf = (char*)malloc(200);

uint16_t read_16(File fp)
{
    uint8_t low;
    uint16_t high;
    low = fp.read();
    high = fp.read();
    return (high<<8)|low;
}

uint32_t read_32(File fp)
{
    uint16_t low;
    uint32_t high;
    low = read_16(fp);
    high = read_16(fp);
    return (high<<8)|low;   
 }
 
bool analysis_bpm_header(File fp)
{
    if(read_16(fp) != 0x4D42)
    {
      return false;  
    }
    //get bpm size
    read_32(fp);
    //get creator information
    read_32(fp);
    //get offset information
    bmp_offset = read_32(fp);
    //get DIB infomation
    read_32(fp);
    //get width and heigh information
    uint32_t bpm_width = read_32(fp);
    uint32_t bpm_heigh = read_32(fp);
    if((bpm_width != s_width) || (bpm_heigh != s_heigh))
    {
      //Serial.println("bpm width height fail");
      return false; 
    }
    if(read_16(fp) != 1) 
    {
      //Serial.println("Must be 1 fail");
        return false;
    }
    if (read_16(fp) != 0x10) {
      //Serial.println("Must be 16 bit fail");
      return false;
    }

    //make sure image is set to 3 for 565 images
    if(read_16(fp) != 3)
    {
      //Serial.println("Must be rgb565 fail");
      return false; 
     }
    
    return true;
}

void draw_bmp_picture(File fp, char increment, enum draw_direction draw_dir)
{
  uint16_t *bpm_data = (uint16_t *)malloc(increment*sizeof(uint16_t)*s_width);
  //uint16_t bpm_color[PIXEL_NUMBER];
  fp.seek(bmp_offset);
  switch(draw_dir) {
    case (up2down):
      for (int y = s_heigh-1; y >=0; y-=increment) {
        fp.read(bpm_data,increment*sizeof(uint16_t)*s_width);
        my_lcd.Draw_Bit_Map(0, y, s_width, increment, bpm_data, 1);
      }
    break;
    case (down2up):
      for (int y = 0; y < s_heigh; y+=increment) {
        fp.read(bpm_data,increment*sizeof(uint16_t)*s_width);
        my_lcd.Draw_Bit_Map(0, y, s_width, increment, bpm_data, 1);
      }
    break;
  }

  free(bpm_data);
    
}

void display_bmp(const char* file_name, enum draw_direction draw_dir) {
    File bmp_file;
    unsigned long start = millis();
    bmp_file = SD.open(file_name);
    //sprintf(sbuf,"Open File Time: %u", millis()-start);
    //Serial.println(sbuf);
    if(!bmp_file)
    {
         my_lcd.Set_Text_Back_colour(BLUE);
         my_lcd.Set_Text_colour(WHITE);    
         my_lcd.Set_Text_Size(1);
         sprintf(sbuf,"didnt find BMPimage! %s", file_name);
         Serial.println(sbuf);
         my_lcd.Print_String(sbuf,0,10);
     }
     if(!analysis_bpm_header(bmp_file))
     {  
         my_lcd.Set_Text_Back_colour(BLUE);
         my_lcd.Set_Text_colour(WHITE);    
         my_lcd.Set_Text_Size(1);
         my_lcd.Print_String("bad bmp picture!",0,0);
         return;
     }
      start = millis();
      draw_bmp_picture(bmp_file, 1, draw_dir);
      sprintf(sbuf,"Draw BMP Time: %lu", millis()-start);
      Serial.println(sbuf);
      bmp_file.close(); 
}

void init_SD_display() {
   my_lcd.Init_LCD();
   my_lcd.Fill_Screen(BLACK);
   pinMode(53, OUTPUT);
   
    if (!SD.begin(53)) 
    {
      my_lcd.Set_Text_Back_colour(BLUE);
      my_lcd.Set_Text_colour(WHITE);    
      my_lcd.Set_Text_Size(1);
      my_lcd.Print_String("SD Card Init fail!",0,0);
    }
}

void setup() 
{
  Serial.begin(9600);
  init_SD_display();
}

void loop() 
{
    int i = 0;
    for (i= 0; i < 12; i++) {
      if (i == 0)
        display_bmp(blinking_animation[12*(i>=5) + (1-2*(i>=5))*i], up2down);
      else if (i == 6)
        display_bmp(blinking_animation[12*(i>=5) + (1-2*(i>=5))*i], up2down);
      else
        display_bmp(blinking_animation[12*(i>=5) + (1-2*(i>=5))*i], down2up);
    }    
}