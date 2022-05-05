// IMPORTANT: LCDWIKI_KBV LIBRARY MUST BE SPECIFICALLY
// CONFIGURED FOR EITHER THE TFT SHIELD OR THE BREAKOUT BOARD.

//This program is a demo of how to show a bmp picture from SD card.

//Set the pins to the correct ones for your development shield or breakout board.
//the 16bit mode only use in Mega.you must modify the mode in the file of lcd_mode.h
//when using the BREAKOUT BOARD only and using these 16 data lines to the LCD,
//pin usage as follow:
//             CS  CD  WR  RD  RST  D0  D1  D2  D3  D4  D5  D6  D7  D8  D9  D10  D11  D12  D13  D14  D15 
//Arduino Mega 40  38  39  /   41   37  36  35  34  33  32  31  30  22  23  24   25   26   27   28   29

//Remember to set the pins to suit your display module!

/**********************************************************************************
* @attention
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, QD electronic SHALL NOT BE HELD LIABLE FOR ANY
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE 
* CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
**********************************************************************************/

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

#define PIXEL_NUMBER  (s_width/4)

const char * main_file = "565.bmp";

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

void draw_bmp_picture(File fp)
{
  #define increment 1
  uint16_t *bpm_data = malloc(increment*sizeof(uint16_t)*s_width);
  //uint16_t bpm_color[PIXEL_NUMBER];
  fp.seek(bmp_offset);
  for (int y = 0; y < s_heigh; y+=increment) {
    fp.read(bpm_data,increment*sizeof(uint16_t)*s_width);
    my_lcd.Draw_Bit_Map(0, y, s_width, increment, bpm_data, 1);
  }
  free(bpm_data);
  //for (int i = 0; i < s_heigh; i++) {
  //  for (int j = 0; j < s_width; j++) {
  //    fp.read(&bpm_data,sizeof(uint16_t));
  //    my_lcd.Set_Draw_color(bpm_data);
  //    my_lcd.Draw_Pixel(j,i);
  //  }
  //}
    
}

char * sbuf = malloc(200);

void setup() 
{
  Serial.begin(9600);
   my_lcd.Init_LCD();
   my_lcd.Fill_Screen(BLACK);
   //s_width = my_lcd.Get_Display_Width();  
   //s_heigh = my_lcd.Get_Display_Height();
   //PIXEL_NUMBER = my_lcd.Get_Display_Width()/4;
   
   //strcpy(file_name,"01.bmp");
  //Init SD_Card
   pinMode(53, OUTPUT);
   
    if (!SD.begin(53)) 
    {
      my_lcd.Set_Text_Back_colour(BLUE);
      my_lcd.Set_Text_colour(WHITE);    
      my_lcd.Set_Text_Size(1);
      my_lcd.Print_String("SD Card Init fail!",0,0);
    }
}

void loop() 
{
    int i = 0;
    File bmp_file;
       bmp_file = SD.open(main_file);
       if(!bmp_file)
       {
            my_lcd.Set_Text_Back_colour(BLUE);
            my_lcd.Set_Text_colour(WHITE);    
            my_lcd.Set_Text_Size(1);
            sprintf(sbuf,"didnt find BMPimage! %s", main_file);
            my_lcd.Print_String(sbuf,0,10);
            while(1);
        }
        if(!analysis_bpm_header(bmp_file))
        {  
            my_lcd.Set_Text_Back_colour(BLUE);
            my_lcd.Set_Text_colour(WHITE);    
            my_lcd.Set_Text_Size(1);
            my_lcd.Print_String("bad bmp picture!",0,0);
            return;
        }
          draw_bmp_picture(bmp_file);
         bmp_file.close(); 
         delay(2000);
        while (1);
     
}