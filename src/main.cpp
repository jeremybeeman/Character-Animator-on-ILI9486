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
#include "animate_handler.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <LCDWIKI_GUI.h> //Core graphics library
#include <LCDWIKI_KBV.h> //Hardware-specific library

const char * main_file = "565.bmp";
#define num_blinking_files 6
const char * blinking_animation[num_blinking_files] = {
      "vert/01.bmp", 
      "vert/02.bmp", 
      "vert/03.bmp", 
      "vert/04.bmp", 
      "vert/05.bmp", 
      "vert/b06.bmp", 

    };
const char * horiz_blinking_animation[num_blinking_files] = {
      "horiz/h01.bmp", 
      "horiz/h02.bmp", 
      "horiz/h03.bmp", 
      "horiz/h04.bmp", 
      "horiz/h05.bmp", 
      "horiz/h06.bmp", 

    };

#define blink_loop 7
const char * vert_blink_anim_compress[blink_loop] = {
      "vert/01.bmp", 
      "blinkarf/01206.arf", 
      "blinkarf/02203.arf", 
      "blinkarf/03204.arf", 
      "blinkarf/04205.arf", 
      "blinkarf/05206.arf", 
      "blinkarf/06201.arf", 
};

#define tobl_loop 5
const char * vert_tobl_loop[tobl_loop] = {
      "tobl/tobl1.bmp", 
      "tobl/01.arf", 
      "tobl/02.arf", 
      "tobl/03.arf", 
      "tobl/04.arf", 
};

#define t1 3
const char * t1_loop[t1] = {
      "tobl/tobl1.bmp", 
      "t1/01.arf", 
      "t1/02.arf", 
};

void setup() 
{
  Serial.begin(9600);
  init_SD_display();
}

bool already_blinked = false;
void loop() 
{

    draw_animation(t1_loop, t1, &already_blinked);
    //draw_animation(vert_tobl_loop, tobl_loop, &already_blinked);
    long start = millis();
    //my_lcd.Fill_Screen(BLACK);
    Serial.print("Fill Screen Time: ");
    Serial.println(millis()-start);
    delay(3000);
    //int i = 0;
    //for (i= 0; i < 12; i++) {
    //  //Serial.println(i);
    //  //display_bmp(horiz_blinking_animation[i % 6], right2left);
    //  //if (i == 0)
    //  //  display_bmp(blinking_animation[12*(i>=5) + (1-2*(i>=5))*i], up2down);
    //  //else if (i == 6)
    //  //  display_bmp(blinking_animation[5], up2down);
    //  //else
    //  //  display_bmp(blinking_animation[12*(i>=5) + (1-2*(i>=5))*i], down2up);
    //}    
}