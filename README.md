# Character Animation on the ILI9486 

This repo handles the animation of 2D characters on the ILI9486. The current state of the repo uses PlatformIO, the Arduino Mega with the ILI9486 to display a character and have that character blink, move its eyes, and move its mouth. It is very limited to the Arduino Mega's slow clock speed and minimal RAM. Othr extended hardware or a different board entirely needs to be used for expedient screen reaction. 

## Table of Contents
- [Current Features](#features)
- [Future Expansion](#future-modifications)
- [Notes on Usage](#notes-on-usage)
- [How to Use in Current Build](#how-to-use)
- [ARF File Explanation](#arf-file)
  * [Introduction](#introduction)
  * [Header](#header)
  * [Encoding Type 1](#encoding-type-1)
  * [Encoding Type 2](#encoding-type-2)
## Current Features 

The current repo's state has two parts:
 1. The BMP and ARF handler code to be written to the Arduino Mega 
 2. The Animation Compression C/C++ code which compresses the animation expected into more efficient animation files. 
 
(#1) uses the LCDWIKI_KBV included with this repo (I don't remember where I got it, but it does work). It then uses the BMP example included in the KBV folder and modifies it for extra speed (through reading in more of a file per SD communication). The code created is in animate_handler.cpp. This code can allow for drawing BMPs and ARFs in the desired direction, instead of in on set direction.

(#2) Takes in a list of files to animate and then finds the similarities between frames. Then, depending on the encoding type, creates ARF files (animation rendering files) which compact the data given for faster display of the data at hand.

Usage: animate_compress.exe <animate_file_specs.txt> <output_folder_name> <encode_number>

## Future Modifications 

 1. Make the specification .txt file more robust so that everything can be specified within. Also fix the problem where a new line is required at the end of the file in order for the program to run properly.
 2. Add a Python application for easier usage.
 3. Allow for more control of timing on each animation.
 4. Add an encoding type or file type which can directly be read from the SD card, treating the SD card as a DMA (direct memory access).
 5. Add a nodal system for linking animations together via an event framework so that animations can change mid animation and can react to changing events more dynamically.

## Notes on Usage
Before you start using this code, FIRST look at the NOTES in animate_compress.cpp. This gives a list of the current limitations of the current build. 

## How to Use
(5-29-22) In the current build:
 1. Make all of your images as .bmp files with R5G6B5 setup (can do in GIMP [see this link](https://gist.github.com/solsarratea/c9fcaeee1fd264613520801743ae37cf)) Make sure all images are in the upright position and are of size 320X480. 
 2. Put a list of your files into a .txt file, along with the direction at which you want the animation to go. 
 3. Run the animate_compress.exe specifying the .txt file's location, the output folder location, and the encoding type desired for the .arf files 
 4. Download all of the .arf files created and place them into the desired SD card. 
 5. Then use the functions in animate_handler.cpp to generate the desired animation.
 6. Download the code and you're good to go.

## ARF File
### Introduction 
The ARF file (or Animation Rendering File) is a file type used to store the TFT LCD animation screens. These can be created with the animation_compress.exe file. 
### Header 
| Data Value (Explanation)                        | Offset   | Bytes Used   |
|:-----------------------------------------------:|:--------:|:-------------|
| "AR" (Denotes a .arf file)                      | 0x0      |   2          |
| Entries number or byte count                    | 0x2      |   4          |
| Draw Direction (up=0, down=1, left=2, right=3)  | 0x6      |   1          |
| Encoding type (states organization of the file) | 0x7      |   1          |

### Encoding Type 1
This encoding type uses the Entries number stored at a 0x4 offset in order to formulate a pixel-based image. All entries draw singular pixels. These pixels were chosen as the colors that were changing from the last frame. This is a basic method and can be slower than other encoding types, mainly because it waits to draw singular pixels instead of drawing groups. This file type can also be bigger than a standard BMP, because it adds the width and height locations on top of the colors. 

How each Entry is arranged:
| Data Value                                      | Offset from Start of Entry  | Bytes Used   |
|:-----------------------------------------------:|:---------------------------:|:-------------|
| width x location                                | 0x0                         |   2          |
| height y location                               | 0x2                         |   2          |
| color                                           | 0x4                         |   2          |

### Encoding Type 2
This encoding type is created for lines. Its whole type finds similar colors and then creates lines based on the similar colors. This works well for the animations that I am working with (it has been significantly faster with the LCDWiki I am using). However, the file sizes can be very large if there are a lot of color changes. This is great for cartoons that are pretty consistent in coloring and slightly changes colors per frame. 

The organization of the data is split into 2 parts: The row header and the column entries. The row header is the same 4 byte size for all lines. The column entries, however, are flexible in size and vary based on how much colors are changing on a row. 

|          | Row Header |                            |                |    Column Entries (Shown are Bytes per Entry) |                |
|:--------:|:----------:|:--------------------------:|:--------------:|:---------------------------------------------:|:--------------:|
|          | y location |number of color lines in row| color          |                              start x location | end x location |
|Byte Count|   2        |         2                  |     2          |       2                                       |       2        |

