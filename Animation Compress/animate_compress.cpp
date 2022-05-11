//Description: This code compresses a series of BMP images into binary files which can be played in sequence.
//Shrinks the amount used for the animation down based on what image was shown last 
//Holds data in this form: [x_location16, y_location16, r5g6b5]


//NOTES:
//5/10/2022: This code is currently only made to work on Windows

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>

#define s_width 320
#define s_height 480
#define SEEK_CURR 1
//max bytes for Arduino Mega2560
#define max_SRAM_bytes 1024*8
#define max_Flash_bytes 1024*256

#define start_expected_animation_files 1 //CHANGE TO A MORE REASONABLE NUMBER AFTER TESTING
#define grow_files_multiplier 2


//gcc -Wall -Werror animate_compress.cpp -o animate_compress.exe
//animate_compress.exe "D:\jjbee\OneDrive\projects\Art\Cotton Candy\Pink_Cotton_Candy\Blinking\BMP" this 


bool verify_bmp(FILE* curr_file, int* bmp_size, int* bmp_offset)
{
    //See below website for BMP guidance:
    //https://en.wikipedia.org/wiki/BMP_file_format
    uint16_t two_byte_store; 
    fread(&two_byte_store, 2, 1, curr_file);
    if(two_byte_store != 0x4D42) //0x4D42 is the BMP signature, stating it is a BMP
    {
      fprintf(stderr, "Not a valid BMP \n");
      return false;  
    }
    
    //extracts the BMP's size 
    fread(bmp_size, 4, 1, curr_file);
    //skip the next four bytes because they're application specific
    fseek(curr_file, 0x4, SEEK_CURR);
    //get offset to where pixel info is
    fread(bmp_offset, 4, 1, curr_file);
    //skip Size of Header information
    fseek(curr_file, 0x4, SEEK_CURR);
    //get width and heigh information
    uint32_t bmp_width; 
    uint32_t bmp_height; 
    fread(&bmp_width, 4, 1, curr_file);
    fread(&bmp_height, 4, 1, curr_file);

    if((bmp_width != s_width) || (bmp_height != s_height))
    {
      fprintf(stderr, "BMP Width and Height is not [%d, %d]. Instead, it is [%d, %d] \n", s_width, s_height, bmp_width, bmp_height);
      return false; 
    }
    //Read the number of color panes (must be 1)
    fread(&two_byte_store, 2, 1, curr_file);
    if(two_byte_store != 1) 
    {
        fprintf(stderr, "Number of Color Panes isn't 1\n");
        return false;
    }
    //The number of bits per pixel
    fread(&two_byte_store, 2, 1, curr_file);
    if (two_byte_store != 16) {
      fprintf(stderr, "Color needs to be 16-bit color\n");
      return false;
    }

    fread(&two_byte_store, 2, 1, curr_file);
    //make sure image is set to 3 for 565 images
    if(two_byte_store != 3)
    {
      fprintf(stderr, "Color needs to be R5G6B5\n");
      return false; 
     }
    
    return true;
}



int main(int argc, char *argv[])
{
    char input_dir_str[100];
    int max_file_count = start_expected_animation_files;
    int16_t** BMP_animation_files = (int16_t **)malloc(sizeof(int16_t *)*start_expected_animation_files); 
    //long running_byte_count = 0;//add eventually. Will keep track of bytes

    if (argc < 3)
        printf("Usage: (animate_compress.exe in_directory out_directory)\n");
    else {
        strcpy(input_dir_str, argv[1]);
        DIR * FD; 
        if (NULL == (FD = opendir(argv[1]))) {
            fprintf(stderr, "ERROR: Couldn't open directory\n");
            return 1;
        }
    char input_file_str[200];
    struct dirent* fd_file_name;
    int file_count = 0;
    while((fd_file_name = readdir(FD))) {
        if (file_count >= max_file_count) {
            BMP_animation_files = (int16_t **)realloc(BMP_animation_files, sizeof(int16_t *)*max_file_count*grow_files_multiplier);
            max_file_count = max_file_count*grow_files_multiplier;
        }
        //printf("File: %s\n", fd_file_name->d_name);
        if (!strcmp(fd_file_name->d_name, "."))
            continue; 
        if (!strcmp(fd_file_name->d_name, ".."))
            continue; 

        strcpy(input_file_str, input_dir_str);
        strcat(input_file_str, "\\");
        strcat(input_file_str, fd_file_name->d_name);
        //printf("Data: %s\n", input_file_str);
        FILE * curr_file = fopen(input_file_str, "rb"); 
        if (curr_file == NULL) {
            fprintf(stderr, "ERROR, Failed to open file [%s]\n", fd_file_name->d_name);
            return 1;
        }

        int bmp_size;
        int bmp_offset; 
        
        if(!verify_bmp(curr_file, &bmp_size, &bmp_offset)) {
            fprintf(stderr, "Exiting Due to Failed BMP...\n");
            return 1;
        }
        
        fprintf(stdout, "File Name: %s; bmp size: %x; bmp offset: %x; \n", fd_file_name->d_name, bmp_size, bmp_offset);
        
        
        fclose(curr_file);
        file_count++;
    }
    closedir(FD);
  }
  return 0;
}