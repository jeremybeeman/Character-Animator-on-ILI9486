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
uint16_t s_height= my_lcd.Get_Display_Height();
enum draw_direction {up2down, down2up, left2right, right2left};

#define PIXEL_NUMBER  (s_width/4)

const char * main_file = "565.bmp";
#define num_blinking_files 6
const char * blinking_animation[num_blinking_files] = {
      "01.bmp", 
      "02.bmp", 
      "03.bmp", 
      "04.bmp", 
      "05.bmp", 
      "b06.bmp", 

    };
const char * horiz_blinking_animation[num_blinking_files] = {
      "h01.bmp", 
      "h02.bmp", 
      "h03.bmp", 
      "h04.bmp", 
      "h05.bmp", 
      "h06.bmp", 

    };
char * sbuf = (char*)malloc(200);

uint16_t read_16(File fp)
{
    uint16_t read_uint16;
    fp.read(&read_uint16, 2);
    return read_uint16;
}

uint32_t read_32(File fp)
{
    uint32_t read_uint32;
    fp.read(&read_uint32, 4);  
    return read_uint32;
 }
 
bool analysis_bmp_header(File fp, uint32_t* bmp_width, uint32_t* bmp_height)
{
  //See below website for BMP guidance:
  //https://en.wikipedia.org/wiki/BMP_file_formats
    if(read_16(fp) != 0x4D42)
    {
      return false;  
    }
    //get bmp size
    read_32(fp);
    //get creator information
    read_32(fp);
    //get offset information
    bmp_offset = read_32(fp);
    //get DIB infomation
    read_32(fp);
    //get width and height information
    *bmp_width = read_32(fp);
    *bmp_height = read_32(fp);

    //if the image is valid (as in the width/height do match up)
    if(!(((*bmp_width == s_width) && (*bmp_height == s_height)) || ((*bmp_width == s_height) || (*bmp_height == s_height))))
    {
      Serial.println("bmp width height fail");
      return false; 
    }
    //always suppossed to be 1
    if(read_16(fp) != 1) 
    {
      Serial.println("Must be 1 fail");
        return false;
    }
    //if the bmp is set to 16-bit color
    if (read_16(fp) != 0x10) {
      Serial.println("Must be 16 bit fail");
      return false;
    }

    //make sure image is set to 3 for 565 images
    if(read_16(fp) != 3)
    {
      Serial.println("Must be rgb565 fail");
      return false; 
     }
    
    return true;
}

void draw_bmp_picture(File fp, char increment, enum draw_direction draw_dir, uint32_t bmp_width)
{
  //when drawing, it is already assumed that the bmp already works. 
  uint16_t *bmp_data;
  if (bmp_width == s_height) {
    Serial.println("Going by Height");
    //if the height and the draw direction is not valid, just set it to a valid one
    if (draw_dir == up2down)
      draw_dir = left2right; 
    else if (draw_dir == down2up)
      draw_dir = right2left;

    bmp_data = (uint16_t *)malloc(increment*sizeof(uint16_t)*s_height);
  }
  else {
    //if the height and the draw direction is not valid, just set it to a valid one
    if (draw_dir == left2right)
      draw_dir = up2down; 
    else if (draw_dir == right2left)
      draw_dir = down2up;

    bmp_data = (uint16_t *)malloc(increment*sizeof(uint16_t)*s_width);
  }
  //uint16_t bmp_color[PIXEL_NUMBER];
  fp.seek(bmp_offset);
  //draw rows or columns, based on the orientation of the image.
  switch(draw_dir) {
    case (up2down):
      for (int y = s_height-1; y >=0; y-=increment) {
        fp.read(bmp_data,increment*sizeof(uint16_t)*s_width);
        my_lcd.Draw_Bit_Map(0, y, s_width, increment, bmp_data, 1);
      }
    break;
    case (down2up):
      for (int y = 0; y < s_height; y+=increment) {
        fp.read(bmp_data,increment*sizeof(uint16_t)*s_width);
        my_lcd.Draw_Bit_Map(0, y, s_width, increment, bmp_data, 1);
      }
    break;
    case (left2right):
      for (int x = s_width-1; x >=0; x-=increment) {
        fp.read(bmp_data,increment*sizeof(uint16_t)*s_height);
        my_lcd.Draw_Bit_Map(x, 0, increment, s_height, bmp_data, 1);
      }
    break;
    case (right2left):
      for (int x = 0; x < s_width; x+=increment) {
        fp.read(bmp_data,increment*sizeof(uint16_t)*s_height);
        my_lcd.Draw_Bit_Map(x, 0, increment, s_height, bmp_data, 1);
      }
    break;
  }

  free(bmp_data);
    
}

void display_bmp(const char* file_name, enum draw_direction draw_dir) {
    File bmp_file;
    uint32_t bmp_width; 
    uint32_t bmp_height;
    unsigned long start = millis();
    bmp_file = SD.open(file_name);
    if(!bmp_file)
    {
         my_lcd.Set_Text_Back_colour(BLUE);
         my_lcd.Set_Text_colour(WHITE);    
         my_lcd.Set_Text_Size(1);
         sprintf(sbuf,"didnt find BMPimage! ");
         Serial.print(sbuf);
         Serial.println(file_name);
         my_lcd.Print_String(sbuf,0,10);
         bmp_file.close();
         return;
     }
    else {
      if(!analysis_bmp_header(bmp_file, &bmp_width, &bmp_height))
       {  
           my_lcd.Set_Text_Back_colour(BLUE);
           my_lcd.Set_Text_colour(WHITE);    
           my_lcd.Set_Text_Size(1);
           sprintf(sbuf,"Bad BMP: %s", file_name);
           my_lcd.Print_String(sbuf,0,0);
           Serial.println(sbuf);
           return;
       }
    }
    start = millis();
    draw_bmp_picture(bmp_file, 1, draw_dir, bmp_width);
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
      //Serial.println(i);
      display_bmp(horiz_blinking_animation[i % 6], left2right);
      //if (i == 0)
      //  display_bmp(blinking_animation[12*(i>=5) + (1-2*(i>=5))*i], up2down);
      //else if (i == 6)
      //  display_bmp(blinking_animation[5], up2down);
      //else
      //  display_bmp(blinking_animation[12*(i>=5) + (1-2*(i>=5))*i], down2up);
    }    
}