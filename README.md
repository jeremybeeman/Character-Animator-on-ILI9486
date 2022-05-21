# Character Animation on the ILI9486 

This repo handles the animation of 2D characters on the ILI9486. The repo uses the Arduino Mega with the ILI9486 to display a character and have that character blink, move its eyes, and move its mouth. It is very limited to the Arduino Mega's slow clock speed and minimal RAM. Othr extended hardware or a different board entirely needs to be used for expedient screen reaction. 

## Table of Contents
- [ARF File Explanation](#arf-file)
  * [Introduction](#introduction)
  * [Header](#header)
  * [Encoding Type 1](#encoding-type-1)
  * [Encoding Type 2](#encoding-type-2)

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
| Data Value                                      | Offset   | Bytes Used   |
|:-----------------------------------------------:|:--------:|:-------------|
| width x location                                | 0x0      |   2          |
| height y location                               | 0x2      |   2          |
| color                                           | 0x4      |   2          |

### Encoding Type 2
This encoding type is created for lines. Its whole type finds similar colors and then creates lines based on the similar colors. This works well for the animations that I am working with (it has been significantly faster with the LCDWiki I am using). However, the file sizes can be very large if there are a lot of color changes. This is great for cartoons that are pretty consistent in coloring and slightly changes colors per frame. 

The organization of the data is split into 2 parts: The row header and the column entries. The row header is the same 4 byte size for all lines. The column entries, however, are flexible in size and vary based on how much colors are changing on a row. 

|          | Row Header |                            |                |    Column Entries |                |
|:--------:|:----------:|:--------------------------:|:--------------:|:-----------------:|:--------------:|
|          | y location |number of color lines in row| color          |  start x location | end x location |
|Byte Count|   2        |         2                  |     2          |       2           |       2        |

