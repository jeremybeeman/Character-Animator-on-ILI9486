//Description: This code compresses a series of BMP images into binary files which can be played in sequence.
//Shrinks the amount used for the animation down based on what image was shown last 
 
//First 2 bytes are "AR" to verify the file is accurate 
//Then, next four bytes are the size of the pertinent data (number of entries). 
//Finally, one byte to specify which type of encoding
//Encode of 1:
//  Holds data in this form: [x_location16, y_location16, r5g6b5]. This way for all entries
//Stores the data into .arf files (animation rendering format)


//NOTES:
//5/11/2022: This code is currently only made to work on Linux WSL

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
#define sizeof_BMP_name 4

#define buffer_animation_files 3 //0th for first file, 1st last file buffer, 2nd for current file
#define buffer_first_file 0
#define buffer_last_file 1
#define buffer_curr_file 2

#define input_dir_argv  1
#define output_dir_argv 2

//Output File Organization: 
//     Extension: .bin 
//     Each Entry: 
//              4-bytes: 2 for width position, 2 for height position
//              2-bytes: R5G6B5 for that position
#define offset2widthpos(offset) offset % s_width
#define offset2heightpos(offset) (int32_t)(offset / s_width)

//Testing Code:
//gcc -Wall -Werror animate_compress.cpp -o animate_compress.exe
//animate_compress.exe "D:\jjbee\OneDrive\projects\Art\Cotton Candy\Pink_Cotton_Candy\Blinking\BMP" this 
//valgrind --leak-check=yes --track-origins=yes  ./animate_compress "Pictures" Output

//Verifies that the file being read in is actually a BMP for this system
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

//combines the directory and the file name together and outputs it
void directory_file_combine(char * input_file_str, char * input_dir_str, char * input_file_name) {
        strcpy(input_file_str, input_dir_str);
        strcat(input_file_str, "/"); //need to change for windows. On WSL
        strcat(input_file_str, input_file_name);
}

void combine_file_names(char * name_of_new_file, char * last_file_str, char * curr_file_str) {
    strcpy(name_of_new_file, last_file_str);
    name_of_new_file[strlen(name_of_new_file)-sizeof_BMP_name] = '\0';
    strcat(name_of_new_file, "2");
    strcat(name_of_new_file, curr_file_str);
    //.arf stands for animation rendering file
    strcpy(name_of_new_file+strlen(name_of_new_file)-sizeof_BMP_name, ".arf");
}

void file_name2output_dir(char * output_file_str, char * name_of_new_file, char * output_dir) {
    strcpy(output_file_str, output_dir);
    strcat(output_file_str, "/");
    strcat(output_file_str, name_of_new_file);
}

void decompress_r5g6b5(int16_t* compressed_color, char colors_out[3]) {
    colors_out[0] = (*compressed_color >> (5+6)) & 0x1F; 
    colors_out[1] = (*compressed_color & 0x7E0) >> 5;
    colors_out[2] = *compressed_color & 0x1F;
}

void compress_r5g6b5(int16_t* compressed_color, char colors_in[3]) {
    *compressed_color = colors_in[0] << (5+6);
    *compressed_color |= colors_in[1] << 5;
    *compressed_color |= colors_in[2];
}

//sets up the arf file with the 2-byte start and allocates the 4-byte arf location for the pertinent info size
void setup_arf(FILE * arf_file, char encode_type) {
    char arf_title [2] = {'A', 'R'};
    fwrite(arf_title, 2, 1, arf_file); //Stores the "AR" title
    int temp_blank_space = 0;
    fwrite(&temp_blank_space, sizeof(int), 1, arf_file); //Init stores size gap for the size
    fwrite(&encode_type, sizeof(char), 1, arf_file); //Write out the encoding type
}

//loads the output binary file with the pixels different between the last slide and current slide
//outputs the number of entries into the file (actual file size is entries*6bytes+6)
int load_output_arf(int16_t* last_BMP_pixel_arr, int16_t* curr_BMP_pixel_arr, int bmp_pixel_arr_size, FILE * output_file) {
    int16_t pos_val;
    int count_change = 0;
    for(int i = (bmp_pixel_arr_size)/2 -1; i >= 0; i--){
        //If the file's value is not the same, isolate and store
        if (last_BMP_pixel_arr[i] != curr_BMP_pixel_arr[i]) {
            //fprintf(stdout, "diff: [%x, %x]\n", BMP_pixel_array_buffer[buffer_last_file][i], BMP_pixel_array_buffer[buffer_curr_file][i]);
            
            //Find the width and height positions for the differing pixels 
            pos_val = offset2widthpos(i);
            fwrite(&pos_val, 2, 1, output_file);
            pos_val = offset2heightpos(i);
            fwrite(&pos_val, 2, 1, output_file);
            //Write the int16_t value out
            fwrite(curr_BMP_pixel_arr+i, 2, 1, output_file);
            count_change++;
        }
    }
    fprintf(stdout, "Count Changes: %d\n", count_change);
    return count_change;
}

//load the arf file with the number of entries in the file. Used to know when at the end of the file
void load_arf_num_entries(FILE * arf_file, int num_entries) {
    fseek(arf_file, 0x2, SEEK_SET);
    fwrite(&num_entries, sizeof(int), 1, arf_file);
}

//The main function runs through and analyzes the information 
int main(int argc, char *argv[])
{
    char encode_type = 1;
    char input_dir_str[100];
    char output_file_str[512];
    char last_file_str[256];
    char first_file_name[256];
    int first_bmp_size; 
    int first_bmp_offset;
    char *name_of_new_file = (char *)malloc(512); 
   int last_byte_size = 0;

    if (argc < 3)
        printf("Usage: (animate_compress.exe in_directory out_directory)\n");
    else {
        if (argc >= 4) {
            if (strcmp(argv[3], "xyrgb") == 0)
                encode_type = 1;
        }
        else {
            encode_type = 1;
        }
        int16_t** BMP_pixel_array_buffer = (int16_t **)malloc(sizeof(int16_t *)*buffer_animation_files); 
        strcpy(input_dir_str, argv[input_dir_argv]);
        DIR * FD; 
        if (NULL == (FD = opendir(argv[input_dir_argv]))) {
            fprintf(stderr, "ERROR: Couldn't open directory\n");
            free(BMP_pixel_array_buffer);
            return 1;
        }
    char input_file_str[200];
    struct dirent* fd_file_name;
    int file_count = 0;
    int bmp_size = 0;
    int bmp_offset = 0; 
    while((fd_file_name = readdir(FD))) {
        //printf("File: %s\n", fd_file_name->d_name);
        if (!strcmp(fd_file_name->d_name, "."))
            continue; 
        if (!strcmp(fd_file_name->d_name, ".."))
            continue; 
        //verify the name is a .bmp extension 
        if (!strcmp(fd_file_name->d_name + strlen(fd_file_name->d_name)-sizeof_BMP_name-1, ".bmp")) {
            fprintf(stderr, "ERROR, File Extension isn't exactly (.bmp). Case sensitive\n");
            for (int i = 0; i < 2*(file_count > 0)+(file_count>1); i++) {
                free(BMP_pixel_array_buffer[i]);
            }
            free(BMP_pixel_array_buffer);
            closedir(FD);
            return 1;
        }

        directory_file_combine(input_file_str, input_dir_str, fd_file_name->d_name);
        //printf("Data: %s\n", input_file_str);
        FILE * curr_file = fopen(input_file_str, "rb"); 
        //if failed to open the current file, release info
        if (curr_file == NULL) {
            fprintf(stderr, "ERROR, Failed to open file [%s]\n", fd_file_name->d_name);
            for (int i = 0; i < 2*(file_count > 0)+(file_count>1); i++) {
                free(BMP_pixel_array_buffer[i]);
            }
            free(BMP_pixel_array_buffer);
            closedir(FD);
            return 1;
        }
        //if the file isn't a valid bmp, free
        if(!verify_bmp(curr_file, &bmp_size, &bmp_offset)) {
            fprintf(stderr, "Exiting Due to Failed BMP...\n");
            for (int i = 0; i < 2*(file_count > 0)+(file_count>1); i++) {
                free(BMP_pixel_array_buffer[i]);
            }
            free(BMP_pixel_array_buffer);
            closedir(FD);
            return 1;
        }
        
        //when on the first file, it gets stored into the 0th part of the buffer        
        if (file_count == 0) {
            fseek(curr_file, bmp_offset, SEEK_SET);
            //Allocate the first file and last file values and store in the values
            BMP_pixel_array_buffer[buffer_first_file] = (int16_t*)malloc((bmp_size-bmp_offset)); 
            BMP_pixel_array_buffer[buffer_last_file]  = (int16_t*)malloc((bmp_size-bmp_offset)); 
            fprintf(stdout, "Bmp Offset: %x; BMP size: %x; BMP diff: %x\n", bmp_offset, bmp_size, bmp_size-bmp_offset);
            fread(BMP_pixel_array_buffer[buffer_first_file], sizeof(int16_t), (bmp_size-bmp_offset)/2, curr_file);
            fseek(curr_file, bmp_offset, SEEK_SET);
            fread(BMP_pixel_array_buffer[buffer_last_file], sizeof(int16_t), (bmp_size-bmp_offset)/2, curr_file);
            //if there was an error in loading the first and last files
            if (memcmp(BMP_pixel_array_buffer[buffer_first_file], (BMP_pixel_array_buffer[buffer_last_file]), (bmp_size-bmp_offset)) != 0) {
                fprintf(stdout, "First file and Last file not identical\n");
            }
            //store the first file info
            first_bmp_size = bmp_size; 
            first_bmp_offset = bmp_offset;

            strcpy(first_file_name, fd_file_name->d_name); //store the first file name for transitions from the last to first
            strcpy(last_file_str, fd_file_name->d_name); 
            last_byte_size = (bmp_size-bmp_offset);
        }
        //On the second file 
        else if (file_count == 1){
            fseek(curr_file, bmp_offset, SEEK_SET);
            //Verifies that the space to be allocated is valid
            if (last_byte_size != (bmp_size-bmp_offset)) {
                fprintf(stderr, "Byte Size of Images are Different. Files: [%s->%s] Exiting...\n", last_file_str, fd_file_name->d_name);
                free(BMP_pixel_array_buffer[buffer_first_file]);
                free(BMP_pixel_array_buffer[buffer_last_file]);
                free(BMP_pixel_array_buffer);
                closedir(FD);
                return 1;
            }
            //Now that the size is valid, allocate the space and analyze it next to the first image
            BMP_pixel_array_buffer[buffer_curr_file] = (int16_t*)malloc((bmp_size-bmp_offset)); 
            fread(BMP_pixel_array_buffer[buffer_curr_file], sizeof(int16_t), (bmp_size-bmp_offset)/2, curr_file);
            
            //Now make the file that will store the cross between the last file and the current one. 
            combine_file_names(name_of_new_file, last_file_str, fd_file_name->d_name);
            file_name2output_dir(output_file_str, name_of_new_file, argv[output_dir_argv]);
            fprintf(stdout, "New File Name: %s\n", output_file_str);

            //Now, can finally create the output file and analyze it next to the original buffer
            FILE * output_file = fopen(output_file_str, "wb");

            //Load the output file binary
            setup_arf(output_file, encode_type);
            int num_entries = load_output_arf(BMP_pixel_array_buffer[buffer_last_file], BMP_pixel_array_buffer[buffer_curr_file], (bmp_size-bmp_offset), output_file);
            load_arf_num_entries(output_file, num_entries);

            fclose(output_file);
            //change the last file to what was the current file 
            free(BMP_pixel_array_buffer[buffer_last_file]);
            BMP_pixel_array_buffer[buffer_last_file] = BMP_pixel_array_buffer[buffer_curr_file];
            strcpy(last_file_str, fd_file_name->d_name); 
            last_byte_size = (bmp_size-bmp_offset);
        }
        //for all other files after the first two 
        else {
            //Verifies that the space to be allocated is valid
            if (last_byte_size != (bmp_size-bmp_offset)) {
                fprintf(stderr, "Byte Size of Images are Different. Files: [%s->%s] Exiting...\n", last_file_str, fd_file_name->d_name);
                for (int i = 0; i < buffer_animation_files; i++) {
                    free(BMP_pixel_array_buffer[i]);
                }
                free(BMP_pixel_array_buffer);
                closedir(FD);
                return 1;
            }
            fseek(curr_file, bmp_offset, SEEK_SET);
            BMP_pixel_array_buffer[buffer_curr_file] = (int16_t*)malloc((bmp_size-bmp_offset)); 
            fread(BMP_pixel_array_buffer[buffer_curr_file], sizeof(int16_t), (bmp_size-bmp_offset)/2, curr_file);
            //Now make the file that will store the cross between the last file and the current one. 
            combine_file_names(name_of_new_file, last_file_str, fd_file_name->d_name);
            file_name2output_dir(output_file_str, name_of_new_file, argv[output_dir_argv]);
            fprintf(stdout, "New File Name: %s\n", output_file_str);

            //Now, can finally create the output file and analyze it next to the original buffer
            FILE * output_file = fopen(output_file_str, "wb");
            //Load the output file with the changing values
            setup_arf(output_file, encode_type);
            int num_entries = load_output_arf(BMP_pixel_array_buffer[buffer_last_file], BMP_pixel_array_buffer[buffer_curr_file], (bmp_size-bmp_offset), output_file);
            load_arf_num_entries(output_file, num_entries);

            fclose(output_file);
            //change the last file to what was the current file 
            free(BMP_pixel_array_buffer[buffer_last_file]);
            BMP_pixel_array_buffer[buffer_last_file] = BMP_pixel_array_buffer[buffer_curr_file];
            //Now, can finally create the output file and analyze it next to the original buffer
            strcpy(last_file_str, fd_file_name->d_name); 
            last_byte_size = (bmp_size-bmp_offset);
        }
        
        fclose(curr_file);
        file_count++;
    }
    closedir(FD);
    //If there was only one file in the folder, fail code. 
    if (file_count == 1) {
        fprintf(stderr, "There was only one file in the folder, so no animation was possible.\n");
        for (int i = 0; i < buffer_animation_files-1; i++) {
            free(BMP_pixel_array_buffer[i]);
        }
        free(BMP_pixel_array_buffer);
        return 1;
    }
    //Now that all files have been processed, wrap from the last to the first image for seamless transition
    combine_file_names(name_of_new_file, last_file_str, first_file_name);
    file_name2output_dir(output_file_str, name_of_new_file, argv[output_dir_argv]);
    fprintf(stdout, "New File Name: %s\n", output_file_str);
    //Now, can finally create the output file and analyze it next to the original buffer
    FILE * output_file = fopen(output_file_str, "wb");
    //Load the output with the changing BMP pixels
    setup_arf(output_file, encode_type);
    int num_entries = load_output_arf(BMP_pixel_array_buffer[buffer_last_file], BMP_pixel_array_buffer[buffer_first_file], (first_bmp_size-first_bmp_offset), output_file);
    load_arf_num_entries(output_file, num_entries);

    fclose(output_file);

    //Free all of the buffers 
    for (int i = 0; i < buffer_animation_files-1; i++) {
            free(BMP_pixel_array_buffer[i]);
    }
    free(BMP_pixel_array_buffer);
  }
  free(name_of_new_file);
  return 0;
}