/*
This file is part of the simple utf16 compression/decompression application.  

It reads data from standard input and writes to standard output. If it is
executed without a parameter it displays simple help and exits.
 
This source code is public-domain. You can use, modify, and distribute the
source code and executable programs based on the source code.

This software is provided "as is" without express or implied warranty.
*/


#ifndef __utf16_compress_h_ 
#define __utf16_compress_h_ 

/**
* hash table length 
*/
#define DICT_LEN 16384

/**
* utf16 character
*/
typedef unsigned short u16; 

/**
* one byte 
*/
typedef unsigned char u8; 

/**
* 32 bit unsigned integer
*/
typedef unsigned int u32;

/**
* utf16 Compress function. This function implements described compression algorithm.
*
* @param input - array of input utf16 characters
* @param length - number of input utf16 characters
* @param output - output buffer
* @return number of output bytes
*/
int utf16_compress(u16 *input, int length, u8 *output);

/**
* utf16 Decompress function. This function implements described decompression algorithm.
*
* @param input - input compressed data
* @param len - length of input
* @param output - output buffer
* @return number of extracted utf16 characters
*/
int utf16_decompress(u8 *input, int length, u16 *output);

#endif

