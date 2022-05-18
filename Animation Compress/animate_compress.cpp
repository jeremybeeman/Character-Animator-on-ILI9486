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


//TO DO:
//1. Still need to allow for horizontal animations and rotating the images
//2. Allow for the .txt file to specify comments, output file, encoding type

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
#define offset2widthpos(offset) (int16_t)(offset % s_width)
#define offset2heightpos(offset) (int16_t)(offset / s_width)

//Testing Code:
//gcc -Wall -Werror animate_compress.cpp -o animate_compress
//animate_compress.exe "D:\jjbee\OneDrive\projects\Art\Cotton Candy\Pink_Cotton_Candy\Blinking\BMP" this 
//valgrind --leak-check=yes --track-origins=yes  ./animate_compress "Output/test.txt" Output

/**************************************************************************************************************
 *                  BMP Handling 
 **************************************************************************************************************/
#define first_BMP_attr 0
#define last_BMP_attr  1
#define curr_BMP_attr  2 
#define total_BMP_attr 3
enum image_orientation {horizontal, vertical};
enum draw_direction {up=0, down=1, left=2, right=3, invalid=0xff};
struct BMP_attributes {
    FILE* BMP_file;
    char* file_name;
    int width;
    int height;
    int offset;
    int size;
    enum image_orientation orientation;
    enum draw_direction animate_dir;
    int16_t* BMP_pixel_array;
};

//Extracts the extension after the ".". Only works if there are no other "."
char * extract_extension(char * file_in) {
    const char period = '.';
    return strchr(file_in, period);
}

//Verifies that the file extension is ".bmp". Case sensitive
bool verify_bmp_file_name(char* file2check) {
    char* bmp_ext = (char*)malloc(sizeof_BMP_name+1);
    memcpy(bmp_ext, extract_extension(file2check), sizeof_BMP_name);
    bmp_ext[sizeof_BMP_name] = '\0';
    bool out_bool = strcmp(bmp_ext, ".bmp") == 0;
    free(bmp_ext);
    return out_bool;
}

//Verifies that the file being read in is actually a BMP for this system
//Also loads the BMP attribute struct while verifying
bool verify_bmp(struct BMP_attributes* BMP_handler)
{   
    //See below website for BMP guidance:
    //https://en.wikipedia.org/wiki/BMP_file_format
    uint16_t two_byte_store; 
    fread(&two_byte_store, 2, 1, BMP_handler->BMP_file);
    if(two_byte_store != 0x4D42) //0x4D42 is the BMP signature, stating it is a BMP
    {
      fprintf(stderr, "Not a valid BMP \n");
      return false;  
    }
    //extracts the BMP's size 
    fread(&(BMP_handler->size), 4, 1, BMP_handler->BMP_file);
    //skip the next four bytes because they're application specific
    fseek(BMP_handler->BMP_file, 0x4, SEEK_CURR);
    //get offset to where pixel info is
    fread(&(BMP_handler->offset), 4, 1, BMP_handler->BMP_file);
    //skip Size of Header information
    fseek(BMP_handler->BMP_file, 0x4, SEEK_CURR);
    //read width and height
    fread(&(BMP_handler->width), 4, 1, BMP_handler->BMP_file);
    fread(&(BMP_handler->height), 4, 1, BMP_handler->BMP_file);

    //Check the width and height and determine the orientation
    if (BMP_handler->width == s_width && BMP_handler->height == s_height) {
        BMP_handler->orientation = vertical;
    }
    else if (BMP_handler->width == s_height && BMP_handler->height == s_width) {
        BMP_handler->orientation = horizontal;
    }
    else {
      fprintf(stderr, "BMP Width and Height are invalid");
      return false;    
    }

    //Read the number of color panes (must be 1)
    fread(&two_byte_store, 2, 1, BMP_handler->BMP_file);
    if(two_byte_store != 1) 
    {
        fprintf(stderr, "Number of Color Panes isn't 1\n");
        return false;
    }
    //The number of bits per pixel
    fread(&two_byte_store, 2, 1, BMP_handler->BMP_file);
    if (two_byte_store != 16) {
      fprintf(stderr, "Color needs to be 16-bit color\n");
      return false;
    }

    fread(&two_byte_store, 2, 1, BMP_handler->BMP_file);
    //make sure image is set to 3 for 565 images
    if(two_byte_store != 3)
    {
      fprintf(stderr, "Color needs to be R5G6B5\n");
      return false; 
     }
    return true;
}

void free_BMP_attr(BMP_attributes* BMP_handler) {
    free(BMP_handler->file_name); 
    free(BMP_handler->BMP_pixel_array);
    free(BMP_handler);
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
//Combines the output file directory with the file name
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
enum draw_direction draw_dir2num(char* draw_dir) {
    const char* total_dirs[4] = {
        "up", 
        "down", 
        "left", 
        "right",
    };
    int total_dir_sz[4] = {2, 4, 4, 5};

    for (int i = 0; i < 4; i++) {
        if (strcmp_sz((char*)total_dirs[i], draw_dir, total_dir_sz[i])) {
            return (enum draw_direction)i;
        }
    }
    fprintf(stderr, "Invalid draw direction. Draw direction HAS to be lowercase and just the word and a new line character.");
    return (enum draw_direction)0xff; //-1 if fail
}
//sets up the arf file with the 2-byte start and allocates the 4-byte arf location for the pertinent info size
void setup_arf(FILE * arf_file, enum draw_direction animate_dir, char encode_type) {
    char arf_title [2] = {'A', 'R'};
    fwrite(arf_title, 2, 1, arf_file); //Stores the "AR" title
    int temp_blank_space = 0;
    fwrite(&temp_blank_space, sizeof(int), 1, arf_file); //Init stores size gap for the size
    char animate_char = (char)animate_dir;
    fwrite(&animate_char, sizeof(char), 1, arf_file);//Writes the direction to draw at
    fwrite(&encode_type, sizeof(char), 1, arf_file); //Write out the encoding type
}

//loads the output binary file with the pixels different between the last slide and current slide
//outputs the number of entries into the file (actual file size is entries*6bytes+6)
//uses the encoding type 1
int load_arf_encode1(struct BMP_attributes* last_BMP, struct BMP_attributes* curr_BMP, FILE* output_file){//int16_t* last_BMP_pixel_arr, int16_t* curr_BMP_pixel_arr, int bmp_pixel_arr_size, char* draw_dir, FILE * output_file) {
    int16_t pos_val;
    int count_change = 0;
    enum draw_direction draw_dir = curr_BMP->animate_dir;
    fseek(output_file, 0x8, SEEK_SET);
    switch(draw_dir) {
        case up:
            for(int i = 0; i < (curr_BMP->size-curr_BMP->offset)/2; i++){
                //If the file's value is not the same, isolate and store
                if (last_BMP->BMP_pixel_array[i] != curr_BMP->BMP_pixel_array[i]) {
                    //fprintf(stdout, "diff: [%x, %x]\n", BMP_pixel_array_buffer[buffer_last_file][i], BMP_pixel_array_buffer[buffer_curr_file][i]);
                    //Find the width and height positions for the differing pixels 
                    pos_val = offset2widthpos(i);
                    fwrite(&pos_val, 2, 1, output_file);
                    pos_val = offset2heightpos(i);
                    fwrite(&pos_val, 2, 1, output_file);
                    //Write the int16_t value out
                    fwrite((curr_BMP->BMP_pixel_array)+i, 2, 1, output_file);
                    count_change++;
                }
            }
        break;
        case down: //down
        for(int i = (curr_BMP->size-curr_BMP->offset)/2 -1; i >= 0; i--){
            //If the file's value is not the same, isolate and store
            if (last_BMP->BMP_pixel_array[i] != curr_BMP->BMP_pixel_array[i]) {
                //fprintf(stdout, "diff: [%x, %x]\n", BMP_pixel_array_buffer[buffer_last_file][i], BMP_pixel_array_buffer[buffer_curr_file][i]);
                //Find the width and height positions for the differing pixels 
                pos_val = offset2widthpos(i);
                fwrite(&pos_val, 2, 1, output_file);
                pos_val = offset2heightpos(i);
                fwrite(&pos_val, 2, 1, output_file);
                //Write the int16_t value out
                fwrite((curr_BMP->BMP_pixel_array)+i, 2, 1, output_file);
                count_change++;
            }
        }
        break;
        default: 

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
//Taking in BMP attributes, creates the output files and spits out data to them
void files2arf(struct BMP_attributes* last_BMP, struct BMP_attributes* curr_BMP, char* output_dir, char encode_type) {
    char name_of_output_file[512];
    char output_file_str[512];
    //First create the name of the output file
    combine_file_names(name_of_output_file, last_BMP->file_name, curr_BMP->file_name);
    file_name2output_dir(output_file_str, name_of_output_file, output_dir);
    fprintf(stdout, "New File Name: %s\n", output_file_str);

    //Now, can finally create the output file and analyze it next to the original buffer
    FILE * output_file = fopen(output_file_str, "wb");

    //Load the output file binary
    setup_arf(output_file, curr_BMP->animate_dir, encode_type);
    int num_entries = load_arf_encode1(last_BMP, curr_BMP, output_file);
    load_arf_num_entries(output_file, num_entries);

    fclose(output_file);
}
/**************************************************************************************************************
 *                 END ARF File Handler
 **************************************************************************************************************/
//Free the BMP pixel array based on how many files have been processed so far. Greater than 1 will free all 3 of array
void free_BMP_arr(int curr_file_count, int16_t** BMP_pixel_arr) {
    for (int i = 0; i < 2*(curr_file_count > 0)+(curr_file_count>1); i++) {
        free(BMP_pixel_arr[i]);
    }
    free(BMP_pixel_arr);
}

//The main function runs through and analyzes the information 
int main(int argc, char *argv[])
{   
    char encode_type;
    char input_dir_file_str[512];


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
        //Store the input file's directory
        strcpy(input_dir_file_str, argv[input_dir_argv]);
        int num_lines_in_file;
        char** cmd_file_data; 
        //Read in the cmd file
        if (NULL == (cmd_file_data = read_cmd_file(&num_lines_in_file, input_dir_file_str))) {
            //failed to parse the file.
            return 1;
        }
        //Now time to analyze the cmd file data
        int file_count = 0;
        struct BMP_attributes BMP_handler[total_BMP_attr]; 
        //struct BMP_attributes* BMP_attr_point; 
        //serach through all of the data in the setup file 
        for (int curr_file_num = 0; curr_file_num < num_lines_in_file; curr_file_num+=2) {
            //verify the name is a .bmp extension 
            if (!verify_bmp_file_name(extract_file_name(cmd_file_data[curr_file_num]))) {
                fprintf(stderr, "ERROR, File Extension isn't exactly (.bmp). Case sensitive\n");
                return 1;
            }

            //this switch sets up BMP files and analyzes them
            switch(file_count) {
                case 0: 
                //the first BMP file. Set up the first BMP file and the last file*
                //also fills up the pixel array
                /////////////////////////////////////////////
                    BMP_handler[first_BMP_attr].file_name = extract_file_name(cmd_file_data[curr_file_num]);
                    BMP_handler[first_BMP_attr].BMP_file = fopen(cmd_file_data[curr_file_num], "rb");
                    if (BMP_handler[first_BMP_attr].BMP_file == NULL) {
                        fprintf(stderr, "ERROR, Failed to open file [%s]\n", cmd_file_data[curr_file_num]);
                        return 1;
                    }
                    //if the file isn't a valid bmp, free
                    if(!verify_bmp(&BMP_handler[first_BMP_attr])) {
                        fprintf(stderr, "Exiting Due to Failed BMP...\n");
                        return 1;
                    }
                    //Fills the values of the BMP pixel array
                    BMP_handler[first_BMP_attr].BMP_pixel_array = (int16_t*)malloc((BMP_handler[first_BMP_attr].size-BMP_handler[first_BMP_attr].offset)); 
                    fseek(BMP_handler[first_BMP_attr].BMP_file, BMP_handler[first_BMP_attr].offset, SEEK_SET);
                    fread(BMP_handler[first_BMP_attr].BMP_pixel_array, sizeof(int16_t), (BMP_handler[first_BMP_attr].size-BMP_handler[first_BMP_attr].offset)/2, BMP_handler[first_BMP_attr].BMP_file);
                    //Free up the BMP file
                    fclose(BMP_handler[first_BMP_attr].BMP_file);
                    //Can't fill the direction to draw in until the last file has been hit

                    //First file and last file are the same data, so copy/paste
                    memcpy(&BMP_handler[last_BMP_attr], &BMP_handler[first_BMP_attr], sizeof(struct BMP_attributes));
                break;
                default: //sets up the current File*
                /////////////////////////////////////////////
                    BMP_handler[curr_BMP_attr].file_name = extract_file_name(cmd_file_data[curr_file_num]);
                    BMP_handler[curr_BMP_attr].BMP_file = fopen(cmd_file_data[curr_file_num], "rb");
                    if (BMP_handler[curr_BMP_attr].BMP_file == NULL) {
                        fprintf(stderr, "ERROR, Failed to open file [%s]\n", cmd_file_data[curr_file_num]);
                        return 1;
                    }
                    //if the file isn't a valid bmp, free
                    if(!verify_bmp(&BMP_handler[curr_BMP_attr])) {
                        fprintf(stderr, "Exiting Due to Failed BMP...\n");
                        return 1;
                    }
                    //Fills the values of the BMP pixel array
                    BMP_handler[curr_BMP_attr].BMP_pixel_array = (int16_t*)malloc((BMP_handler[curr_BMP_attr].size-BMP_handler[curr_BMP_attr].offset)); 
                    fseek(BMP_handler[curr_BMP_attr].BMP_file, BMP_handler[curr_BMP_attr].offset, SEEK_SET);
                    fread(BMP_handler[curr_BMP_attr].BMP_pixel_array, sizeof(int16_t), (BMP_handler[curr_BMP_attr].size-BMP_handler[curr_BMP_attr].offset)/2, BMP_handler[curr_BMP_attr].BMP_file);
                    //Fill in the draw direction
                    BMP_handler[curr_BMP_attr].animate_dir = draw_dir2num(cmd_file_data[curr_file_num-1]);

                    //Now that the files have been properly loaded in, now they can be analyzed
                    files2arf(&BMP_handler[last_BMP_attr], &BMP_handler[curr_BMP_attr], argv[output_dir_argv], encode_type);
                    //Free up the BMP file
                    fclose(BMP_handler[curr_BMP_attr].BMP_file);
                    //allow for reallocation of data
                    if (file_count != 1)
                        free(BMP_handler[last_BMP_attr].BMP_pixel_array);
                    
                    BMP_handler[last_BMP_attr].BMP_pixel_array = NULL;
                    //Now move the data from the current file to the last file 
                    memcpy(&BMP_handler[last_BMP_attr], &BMP_handler[curr_BMP_attr], sizeof(struct BMP_attributes));
                break;
            }
            file_count++;
        }
        //If there was only one file in the folder, fail code. 
        if (file_count == 1) {
            fprintf(stderr, "There was only one file in the folder, so no animation was possible.\n");
            return 1;
        }
        
        //Creates the final looping animation based off of the first and last BMPs
        BMP_handler[first_BMP_attr].animate_dir = draw_dir2num(cmd_file_data[file_count*2-1]);
        files2arf(&BMP_handler[last_BMP_attr], &BMP_handler[first_BMP_attr], argv[output_dir_argv], encode_type);
        
        //Free up the final values
        free(BMP_handler[first_BMP_attr].BMP_pixel_array);
        free(BMP_handler[last_BMP_attr].BMP_pixel_array);
        //Free up the setup file read in 
        free_files_charpp(cmd_file_data, num_lines_in_file);
    }

    return 0;
}