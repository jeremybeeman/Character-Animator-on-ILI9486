# Character Animation on the ILI9486 

This repo handles the animation of 2D characters on the ILI9486. The repo uses the Arduino Mega with the ILI9486 to display a character and have that character blink, move its eyes, and move its mouth. It is very limited to the Arduino Mega's slow clock speed and minimal RAM. Othr extended hardware or a different board entirely needs to be used for expedient screen reaction. 

## Table of Contents
- [ARF File Explanation](#arf-file)
  * [Introduction](#introduction)
  * [Header](#header)
  * [Encoding Type 1](#encoding-type-1)

## ARF File
### Introduction 
The ARF file (or Animation Rendering File) is a file type used to store the TFT LCD animation screens. These can be created with the animation_compress.exe file. 
### Header 
| Data Value (Explanation)                        | Offset   | Bytes Used   |
|:-----------------------------------------------:|:--------:|:-------------|
| "AR" (Denotes a .arf file)                      | 0x0      |   4          |
| Entries number or byte count                    | 0x4      |   4          |
| Draw Direction (up=0, down=1, left=2, right=3)  | 0x8      |   1          |
| Encoding type (states organization of the file) | 0x9      |   1          |

### Encoding Type 1
This encoding type uses the Entries number stored at a 0x4 offset in order to formulate a pixel-based image. All entries draw singular pixels. These pixels were chosen as the colors that were changing from the last frame. This is a basic method and can be slower than other encoding types, mainly because it waits to draw singular pixels instead of drawing groups. It also can have larger file sizes as compared to other methods. This file type can also be bigger than a standard BMP, because it adds the width and height locations on top of the colors. 

How each Entry is arranged:
| Data Value                                      | Offset   | Bytes Used   |
|:-----------------------------------------------:|:--------:|:-------------|
| width x location                                | 0x0      |   2          |
| height y location                               | 0x2      |   2          |
| color                                           | 0x4      |   2          |
