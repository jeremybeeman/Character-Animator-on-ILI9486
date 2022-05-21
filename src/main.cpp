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

char * sbuf = (char*)malloc(300);

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
    //open the BMP
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
      //verify the BMP to be valid
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

bool verify_arf(File arf_fp, uint32_t* arf_num_entries, char* draw_dir, char* encode_type) {
  if (read_16(arf_fp) != 0x5241) { //0x5241 is "AR"
    Serial.println("Non valid ARF file. Doesn't have correct header format.");
    return false; 
  }

  *arf_num_entries = read_32(arf_fp); //read in the number of entries 
  *draw_dir = arf_fp.read();
  *encode_type = arf_fp.read(); //read in the encoding type (How entries arranged)
  //Serial.print("NE: ");
  //Serial.print(*arf_num_entries); 
  //Serial.print(" ET: "); 
  //Serial.println((int)*encode_type);
  return true;
  
}

void print_arf_dir_encode1(File arf_file, uint32_t arf_num_entries, char draw_dir) {
    int16_t* entries_buff = (int16_t*)malloc(sizeof(int16_t)*3); //3 because 2-byte xpos, 2-byte y-pos, 2-byte rgb
    //loop through all of the entries, reading them in and printing their values to the screen
    int16_t last_color = 0x00; 
    my_lcd.Set_Draw_color(last_color);
    for (uint32_t i = 0; i < arf_num_entries; i++) {
      arf_file.read(entries_buff, sizeof(int16_t)*3); 
      //sprintf(sbuf, "E#%d, [%d, %d, %x]", i, entries_buff[0], entries_buff[1], entries_buff[2]);
      //Serial.println(sbuf);
      my_lcd.Draw_Pixe(entries_buff[0], entries_buff[1], entries_buff[2]);
    }
    free(entries_buff);
}

void print_arf_dir_encode2(File arf_file, uint32_t arf_num_entries, char draw_dir) {
    int16_t* entries_buff = (int16_t*)malloc(sizeof(int16_t)*3); //3 because 2-byte xpos, 2-byte y-pos, 2-byte rgb
    int16_t curr_row;
    int16_t curr_entries_on_row; 
    if (draw_dir < 2) {
      //loop through all of the entries, reading them in and printing their values to the screen
      int16_t last_color = 0x00; 
      my_lcd.Set_Draw_color(last_color);
      for (uint32_t i = 0; i < arf_num_entries; i++) {
        arf_file.read(&curr_row, sizeof(int16_t)); 
        arf_file.read(&curr_entries_on_row, sizeof(int16_t)); 
        for (int row_entry = 0; row_entry < curr_entries_on_row; row_entry++) {
            arf_file.read(entries_buff, sizeof(int16_t)*3); 
            my_lcd.Set_Draw_color(entries_buff[0]);
            my_lcd.Draw_Fast_HLine(entries_buff[1], curr_row, entries_buff[2]-entries_buff[1]+1);
        }
      }
      free(entries_buff);
    }
}

//.arf stands for animation rendering file
void display_arf(const char* file_name) {
    File arf_file; 
    uint32_t arf_num_entries; 
    char draw_dir;
    char encode_type;
    unsigned long start = millis();
    arf_file = SD.open(file_name);
    if (!arf_file) {
      Serial.println("Failed to open ARF");
      return;
    }

    if (!verify_arf(arf_file, &arf_num_entries, &draw_dir, &encode_type)) {
      Serial.println("Failed to verify ARF file");
      arf_file.close(); 
      return;
    }

    switch(encode_type) {
      case 1: //when the encoding type is xyrgb
        print_arf_dir_encode1(arf_file, arf_num_entries, draw_dir);
      break;
      case 2:
        print_arf_dir_encode2(arf_file, arf_num_entries, draw_dir);
      break;
    }

    sprintf(sbuf,"Draw ARF Time: %lu", millis()-start);
    Serial.println(sbuf);
    arf_file.close(); 
}

//assumes format of .bmp, then all .arf after
bool draw_animation(const char ** animation_files, int num_files, bool* already_blinked) {
  if (!*already_blinked) {
    display_bmp(animation_files[0], down2up); 
    *already_blinked = true;
  }
  for (int i = 1; i < num_files; i++) {
    display_arf(animation_files[i]);
  }
  return true;
}

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