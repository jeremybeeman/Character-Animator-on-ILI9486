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
//5/16/2022: Modifying from just folder input and folder output to i/o in multiple ways
//5/16/2022: For verifying the bmp, case sensitive (has to be lower case), also can't have . in any names of folders

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
//     Encode Type: 1
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

/**************************************************************************************************************
 *                  BMP Handling 
 **************************************************************************************************************/
char * extract_extension(char * file_in) {
    const char period = '.';
    return strchr(file_in, period);
}

bool verify_bmp_file_name(char* file2check) {
    char* bmp_ext = (char*)malloc(sizeof_BMP_name+1);
    memcpy(bmp_ext, extract_extension(file2check), sizeof_BMP_name);
    bmp_ext[sizeof_BMP_name] = '\0';
    bool out_bool = strcmp(bmp_ext, ".bmp") == 0;
    free(bmp_ext);
    return out_bool;
}

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
/**************************************************************************************************************
 *                  END BMP Handling 
 **************************************************************************************************************/

/**************************************************************************************************************
 *                  File Handling 
 **************************************************************************************************************/
//Compares two strings for a specific number of desired characters to compare. 
bool strcmp_sz(char* str1, char* str2, int sz) {
    for (int i = 0; i < sz; i++) {
        if (str1[i] != str2[i]) {
            return false;
        }
    }
    return true;
}
//extracts the file name from a char* with the file name and file extension
char* extract_file_name(char* file_dir_name) {
    char slash = '/';//for WSL Linux for now
    char* file_loc = NULL;
    char* next_file_loc = strchr(file_dir_name, slash);
    while (next_file_loc) {
        file_loc = ++next_file_loc;
        next_file_loc = strchr(next_file_loc, slash);
    }
    return file_loc;
}

//combines the directory and the file name together and outputs it
void directory_file_combine(char * input_file_str, char * input_dir_str, char * input_file_name) {
        strcpy(input_file_str, input_dir_str);
        strcat(input_file_str, "/"); //need to change for windows. On WSL
        strcat(input_file_str, input_file_name);
}

//Combines two names for files and spits out a .arf file
void combine_file_names(char * name_of_new_file, char * last_file_str, char * curr_file_str) {
    strcpy(name_of_new_file, last_file_str);
    name_of_new_file[strlen(name_of_new_file)-sizeof_BMP_name] = '\0';
    strcat(name_of_new_file, "2");
    strcat(name_of_new_file, curr_file_str);
    //.arf stands for animation rendering file
    strcpy(name_of_new_file+strlen(name_of_new_file)-sizeof_BMP_name, ".arf");
}

void file_name2output_dir(char * output_file_str, char * name_of_file, char * output_dir) {
    strcpy(output_file_str, output_dir);
    strcat(output_file_str, "/");//ONLY on WSL
    strcat(output_file_str, name_of_file);
}
/**************************************************************************************************************
 *                  END File Handling 
 **************************************************************************************************************/

/**************************************************************************************************************
 *                  File Input/Output 
 **************************************************************************************************************/
//Takes in the name of the folder and then outputs the files in that folder. Returns the full directory for all files
char** in_folder_dir_out_files(int* num_files_out, char* folder_dir_in) {
    DIR * FD; 
    struct dirent* fd_file_name;
    char max_file_size = 100;
    char growth_each_realloc = 2;
    char ** out_files = (char **)malloc(max_file_size*sizeof(char *));
    int out_file_pointer = 0;

    if (NULL == (FD = opendir(folder_dir_in))) {
        fprintf(stderr, "ERROR: Couldn't open directory\n");
        return NULL;
    }
    //read through all files in the folder and export them
    while((fd_file_name = readdir(FD))) {
        //skip the unused . and ..
        if (!strcmp(fd_file_name->d_name, "."))
            continue; 
        if (!strcmp(fd_file_name->d_name, ".."))
            continue; 
        if (out_file_pointer >= max_file_size) {
            max_file_size *= growth_each_realloc;
            out_files = (char **)realloc(out_files, max_file_size*sizeof(char*));
        }
        out_files[out_file_pointer] = (char *)malloc(strlen(folder_dir_in) + strlen(fd_file_name->d_name)+1+1);//1 for '/' 1 for '\0'
        file_name2output_dir(out_files[out_file_pointer], fd_file_name->d_name, folder_dir_in);
        out_file_pointer++;
    }
    closedir(FD);
    out_files = (char **)realloc(out_files, out_file_pointer*sizeof(char*)); //resize to acutal size of files
    *num_files_out = out_file_pointer;
    return out_files;
}
//frees a file char**
void free_files_charpp(char ** file_charpp, int num_files) {
    for (int i = 0; i < num_files; i++) {
        free(file_charpp[i]);
    }
    free(file_charpp);
}

//send a group of char** file names to a file. Just listing names
void charpp2file(char* out_file_name_dir, char** files_in, int num_files, int add_newline) {
    FILE * curr_file;
    if (NULL == (curr_file = fopen(out_file_name_dir, "w"))) {
        fprintf(stderr, "File Open Failiure\n");
        return;
    }
    for (int i = 0; i < num_files; i++) {
        fwrite(files_in[i], sizeof(char), strlen(files_in[i]), curr_file);
        if (add_newline)
            fwrite("\n", sizeof(char), 1, curr_file);
    }
    fclose(curr_file);
}
/**************************************************************************************************************
 *                  END File Input/Output 
 **************************************************************************************************************/
/**************************************************************************************************************
 *                  Decompress and Compress R5G6B5
 **************************************************************************************************************/
//Takes a 16-bit color and splits it up into the three different colors
void decompress_r5g6b5(int16_t* compressed_color, char colors_out[3]) {
    colors_out[0] = (*compressed_color >> (5+6)) & 0x1F; 
    colors_out[1] = (*compressed_color & 0x7E0) >> 5;
    colors_out[2] = *compressed_color & 0x1F;
}
//Takes in an array of 3 colors and spits out a 16-bit color
void compress_r5g6b5(int16_t* compressed_color, char colors_in[3]) {
    *compressed_color = colors_in[0] << (5+6);
    *compressed_color |= colors_in[1] << 5;
    *compressed_color |= colors_in[2];
}
/**************************************************************************************************************
 *                  END Decompress and Compress R5G6B5
 **************************************************************************************************************/
/**************************************************************************************************************
 *                  Parse Input File (format file name, then draw direction)
 **************************************************************************************************************/
//parses the input cmd file (file name, then draw_direction). In ASCII format. Also removes \n and anything past file . extension
char** read_cmd_file(int* num_lines_out, char* input_cmd_file_dir) {
    FILE * cmd_file; 
    int max_lines = 100;
    int grow_lines_realloc = 2;
    int num_lines = 0;
    char** cmd_file_out = (char**)malloc(max_lines*sizeof(char*)); 

    if (NULL == (cmd_file = fopen(input_cmd_file_dir, "r"))) {
        fprintf(stderr, "File Open Failiure\n");
        return NULL;
    }
    char *line = NULL;
    size_t len_line = 0;
    ssize_t read_line_chars;
    //parse each line
    while ((read_line_chars = getline(&line, &len_line, cmd_file)) != -1) {
        //reallocate when too small
        if (num_lines >= max_lines) {
            max_lines *= grow_lines_realloc; 
            cmd_file_out = (char**)realloc(cmd_file_out, max_lines*sizeof(char*));
        }
        cmd_file_out[num_lines] = (char*)malloc(len_line);
        strcpy(cmd_file_out[num_lines], line);
        //when even, remove after the .bmp
        if (num_lines % 2 == 0) {
            char* bmp_ext = extract_extension(cmd_file_out[num_lines]); 
            bmp_ext+=sizeof_BMP_name;
            *bmp_ext = '\0';
        }
        //When odd, remove the new line character
        else {
            char new_line = '\n';
            char* end_line = strchr(cmd_file_out[num_lines], new_line);
            *end_line = '\0';
        }
        num_lines++;
    }
    free(line);
    //reallocate to exact size
    cmd_file_out = (char**)realloc(cmd_file_out, num_lines*sizeof(char*));
    *num_lines_out = num_lines;
    fclose(cmd_file);
    return cmd_file_out;

}
/**************************************************************************************************************
 *                  END Parse Input File (format file name, then draw direction)
 **************************************************************************************************************/
/**************************************************************************************************************
 *                  ARF File Handler
 **************************************************************************************************************/
//input the draw direction as a char pointer and output a number. 
//(up = 0), (down = 1), (left = 2), (right = 3)
char draw_dir2num(char* draw_dir) {
    const char* total_dirs[4] = {
        "up", 
        "down", 
        "left", 
        "right",
    };
    int total_dir_sz[4] = {2, 4, 4, 5};

    for (int i = 0; i < 4; i++) {
        if (strcmp_sz((char*)total_dirs[i], draw_dir, total_dir_sz[i])) {
            fprintf(stdout, "Draw dir:%s\n", draw_dir);
            return i;
        }
    }

    fprintf(stderr, "Invalid draw direction. Draw direction HAS to be lowercase and just the word and a new line character.");
    return 0xff; //-1 if fail
}
//sets up the arf file with the 2-byte start and allocates the 4-byte arf location for the pertinent info size
void setup_arf(FILE * arf_file, char* direction_draw, char encode_type) {
    char arf_title [2] = {'A', 'R'};
    fwrite(arf_title, 2, 1, arf_file); //Stores the "AR" title
    int temp_blank_space = 0;
    fwrite(&temp_blank_space, sizeof(int), 1, arf_file); //Init stores size gap for the size
    char dirnum = draw_dir2num(direction_draw);
    fwrite(&dirnum, sizeof(char), 1, arf_file);//Writes the direction to draw at
    fwrite(&encode_type, sizeof(char), 1, arf_file); //Write out the encoding type
}

//loads the output binary file with the pixels different between the last slide and current slide
//outputs the number of entries into the file (actual file size is entries*6bytes+6)
//uses the encoding type 1
int load_arf_encode1(int16_t* last_BMP_pixel_arr, int16_t* curr_BMP_pixel_arr, int bmp_pixel_arr_size, char* draw_dir, FILE * output_file) {
    int16_t pos_val;
    int count_change = 0;
    char draw_num = draw_dir2num(draw_dir);
    switch(draw_num) {
        case 0: //up 
            for(int i = 0; i < (bmp_pixel_arr_size)/2; i++){
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
        break;
        case 1: //down
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
        break;
    }
    fprintf(stdout, "Count Changes: %d\n", count_change);
    return count_change;
}

//load the arf file with the number of entries in the file. Used to know when at the end of the file
void load_arf_num_entries(FILE * arf_file, int num_entries) {
    fseek(arf_file, 0x2, SEEK_SET);
    fwrite(&num_entries, sizeof(int), 1, arf_file);
}
/**************************************************************************************************************
 *                 END ARF File Handler
 **************************************************************************************************************/

//The main function runs through and analyzes the information 
int main(int argc, char *argv[])
{   
    char encode_type = 1;
    char input_dir_file_str[512];
    char output_file_str[512];
    char last_file_str[256];
    char first_file_name[256];
    int first_bmp_size; 
    int first_bmp_offset;
    char *name_of_new_file = (char *)malloc(512); 
    int last_byte_size = 0;

    if (argc < 3)
        printf("Usage: (animate_compress.exe in_setup_file.txt out_directory)\n");
    else {
        if (argc >= 4) {
            if (strcmp(argv[3], "1") == 0)
                encode_type = 1;
        }
        else {
            encode_type = 1;
        }
        //Allocate the BMP buffer
        int16_t** BMP_pixel_array_buffer = (int16_t **)malloc(sizeof(int16_t *)*buffer_animation_files); 
        //Store the input file's directory
        strcpy(input_dir_file_str, argv[input_dir_argv]);
        int num_lines_in_file;
        char** cmd_file_data; 
        //Read in the cmd file
        if (NULL == (cmd_file_data = read_cmd_file(&num_lines_in_file, input_dir_file_str))) {
            //failed to parse the file.
            free(BMP_pixel_array_buffer);
            return 1;
        }
        //Now time to analyze the cmd file data
        int file_count = 0;
        int bmp_size = 0;
        int bmp_offset = 0; 
        //serach through all of the files
        for (int curr_file_num = 0; curr_file_num < num_lines_in_file; curr_file_num+=2) {
            //verify the name is a .bmp extension 
            if (!verify_bmp_file_name(extract_file_name(cmd_file_data[curr_file_num]))) {
                fprintf(stderr, "ERROR, File Extension isn't exactly (.bmp). Case sensitive\n");
                for (int i = 0; i < 2*(file_count > 0)+(file_count>1); i++) {
                    free(BMP_pixel_array_buffer[i]);
                }
                free(BMP_pixel_array_buffer);
                return 1;
            }

            //After the bmp file name has been verified, then verify the bmp itself 
            FILE * curr_file = fopen(cmd_file_data[curr_file_num], "rb"); 
            //if failed to open the current file, release info
            if (curr_file == NULL) {
                fprintf(stderr, "ERROR, Failed to open file [%s]\n", cmd_file_data[curr_file_num]);
                for (int i = 0; i < 2*(file_count > 0)+(file_count>1); i++) {
                    free(BMP_pixel_array_buffer[i]);
                }
                free(BMP_pixel_array_buffer);
                return 1;
            }
            //if the file isn't a valid bmp, free
            if(!verify_bmp(curr_file, &bmp_size, &bmp_offset)) {
                fprintf(stderr, "Exiting Due to Failed BMP...\n");
                for (int i = 0; i < 2*(file_count > 0)+(file_count>1); i++) {
                    free(BMP_pixel_array_buffer[i]);
                }
                free(BMP_pixel_array_buffer);
                
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

                strcpy(first_file_name, extract_file_name(cmd_file_data[curr_file_num])); //store the first file name for transitions from the last to first
                strcpy(last_file_str, extract_file_name(cmd_file_data[curr_file_num])); 
                last_byte_size = (bmp_size-bmp_offset);
            }
            //On the second file 
            else if (file_count == 1){
                fseek(curr_file, bmp_offset, SEEK_SET);
                //Verifies that the space to be allocated is valid
                if (last_byte_size != (bmp_size-bmp_offset)) {
                    fprintf(stderr, "Byte Size of Images are Different. Files: [%s->%s] Exiting...\n", last_file_str, cmd_file_data[curr_file_num]);
                    free(BMP_pixel_array_buffer[buffer_first_file]);
                    free(BMP_pixel_array_buffer[buffer_last_file]);
                    free(BMP_pixel_array_buffer);
                    
                    return 1;
                }
                //Now that the size is valid, allocate the space and analyze it next to the first image
                BMP_pixel_array_buffer[buffer_curr_file] = (int16_t*)malloc((bmp_size-bmp_offset)); 
                fread(BMP_pixel_array_buffer[buffer_curr_file], sizeof(int16_t), (bmp_size-bmp_offset)/2, curr_file);

                //Now make the file that will store the cross between the last file and the current one. 
                combine_file_names(name_of_new_file, last_file_str, extract_file_name(cmd_file_data[curr_file_num]));
                file_name2output_dir(output_file_str, name_of_new_file, argv[output_dir_argv]);
                fprintf(stdout, "New File Name: %s\n", output_file_str);

                //Now, can finally create the output file and analyze it next to the original buffer
                FILE * output_file = fopen(output_file_str, "wb");

                //Load the output file binary
                setup_arf(output_file, cmd_file_data[curr_file_num-1], encode_type);
                int num_entries = load_arf_encode1(BMP_pixel_array_buffer[buffer_last_file], BMP_pixel_array_buffer[buffer_curr_file], (bmp_size-bmp_offset), cmd_file_data[curr_file_num-1], output_file);
                load_arf_num_entries(output_file, num_entries);

                fclose(output_file);
                //change the last file to what was the current file 
                free(BMP_pixel_array_buffer[buffer_last_file]);
                BMP_pixel_array_buffer[buffer_last_file] = BMP_pixel_array_buffer[buffer_curr_file];
                strcpy(last_file_str, extract_file_name(cmd_file_data[curr_file_num])); 
                last_byte_size = (bmp_size-bmp_offset);
            }
            //for all other files after the first two 
            else {
                //Verifies that the space to be allocated is valid
                if (last_byte_size != (bmp_size-bmp_offset)) {
                    fprintf(stderr, "Byte Size of Images are Different. Files: [%s->%s] Exiting...\n", last_file_str, cmd_file_data[curr_file_num]);
                    for (int i = 0; i < buffer_animation_files; i++) {
                        free(BMP_pixel_array_buffer[i]);
                    }
                    free(BMP_pixel_array_buffer);
                    
                    return 1;
                }
                fseek(curr_file, bmp_offset, SEEK_SET);
                BMP_pixel_array_buffer[buffer_curr_file] = (int16_t*)malloc((bmp_size-bmp_offset)); 
                fread(BMP_pixel_array_buffer[buffer_curr_file], sizeof(int16_t), (bmp_size-bmp_offset)/2, curr_file);
                //Now make the file that will store the cross between the last file and the current one. 
                combine_file_names(name_of_new_file, last_file_str, extract_file_name(cmd_file_data[curr_file_num]));
                file_name2output_dir(output_file_str, name_of_new_file, argv[output_dir_argv]);
                fprintf(stdout, "New File Name: %s\n", output_file_str);

                //Now, can finally create the output file and analyze it next to the original buffer
                FILE * output_file = fopen(output_file_str, "wb");
                //Load the output file with the changing values
                setup_arf(output_file, cmd_file_data[curr_file_num-1], encode_type);
                int num_entries = load_arf_encode1(BMP_pixel_array_buffer[buffer_last_file], BMP_pixel_array_buffer[buffer_curr_file], (bmp_size-bmp_offset), cmd_file_data[curr_file_num-1], output_file);
                load_arf_num_entries(output_file, num_entries);

                fclose(output_file);
                //change the last file to what was the current file 
                free(BMP_pixel_array_buffer[buffer_last_file]);
                BMP_pixel_array_buffer[buffer_last_file] = BMP_pixel_array_buffer[buffer_curr_file];
                //Now, can finally create the output file and analyze it next to the original buffer
                strcpy(last_file_str, extract_file_name(cmd_file_data[curr_file_num])); 
                last_byte_size = (bmp_size-bmp_offset);
            }

            fclose(curr_file);
            file_count++;
        }
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
        setup_arf(output_file, cmd_file_data[file_count*2-1], encode_type);
        int num_entries = load_arf_encode1(BMP_pixel_array_buffer[buffer_last_file], BMP_pixel_array_buffer[buffer_first_file], (first_bmp_size-first_bmp_offset), cmd_file_data[file_count*2-1], output_file);
        load_arf_num_entries(output_file, num_entries);

        fclose(output_file);

        //Free all of the buffers 
        for (int i = 0; i < buffer_animation_files-1; i++) {
                free(BMP_pixel_array_buffer[i]);
        }
        free(BMP_pixel_array_buffer);
        free_files_charpp(cmd_file_data, num_lines_in_file);
    }
    free(name_of_new_file);
    return 0;
}