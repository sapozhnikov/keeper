#include "stdafx.h"
/*! \file utf16_compress.c
\brief This file is part of the simple utf16 compression/decompression application.  

It reads data from standard input and writes to standard output. If it is
executed without a parameter it displays simple help and exits.
 
This source code is public-domain. You can use, modify, and distribute the
source code and executable programs based on the source code.

This software is provided "as is" without express or implied warranty.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "utf16_compress.h"

/* Offset and length are encoded the same way. */
#define DecodeOffset(x) DecodeLength(x)

static int utf16_compressor_initialized = 0;
static u16 * dictionary[DICT_LEN];

/**
* Simple hash function. Takes two utf16 characters and enumerate
* their hash value. The value is used as dictionary array index.
* 
* @param first - utf16 character
* @param second - utf16 character
* @return hash value in interval from 0 to DICT_LEN
*/
static u16 hash(u16 char0, u16 char1)
{
  return ((char1 * 37 ^ ((char0 >> 7) * 5) ^ (char0)) * 33) & DICT_LEN;
}

/**
* Initialize the compressor. It is called from utf16_compress only.
*/
static void utf16_compressor_init(void)
{
  if (utf16_compressor_initialized)
   return;
  utf16_compressor_initialized = 1;
  /* clear the hash table. It needs to be called only once to prevent access
  unaligned pointer which causes crash on some architectures. */
  memset(dictionary, 0, sizeof(dictionary));
}

/**
* Decode length value. The offset is decoded the same way.
*
* @param input - pointer to array of bytes which encodes the length.
* @return the encoded length/offset value. *input pointer is shifted by the read length.  
*/
static int DecodeLength(u8 **input)
{
  int ret = 0;
  int shift = 6;

  u8 *b = *input;

  (*input)++;
  ret += *b & 0x3f;

  if ((*b & 0x40) == 0)
    return ret;

  do
  {
    b = *input;
    (*input)++;
    ret |= ((int)(*b & 0x7f)) << shift;
    shift+=7;
  } while ((*b & 0x80) != 0);

  return ret;
}

/**
* Writes one output byte. 
*
* @param out - output byte
* @param output - pointer to output array.
*/
static void OutputByte(u8 out, u8 **output)
{
  **output = out;
  *output += 1;
}

/**
* Encode pair of length and offset values. OutpurByte function is called on each output byte.
*
* @param offset - offset
* @param len - length
* @param pointer to output array
*/
static void OutputMatch(u16 offset, u16 len, u8 **output)
{
  OutputByte((u8)(len & 0x3F) | ((len > 0x3F) << 6) | 0x80, output);
  len >>= 6;

  while (len > 0) {
    OutputByte((u8)(len & 0x7F) | ((len > 0x7F) << 7), output);
    len >>= 7;
  }

  OutputByte((u8)(offset & 0x3F) | ((offset > 0x3F) << 6) | 0x80, output);
  offset >>= 6;
  
  while (offset > 0) {
    OutputByte((u8)(offset & 0x7F) | ((offset > 0x7F) << 7), output);
    offset >>= 7;
  }
}

/**
* Encode array of literals. OutpurByte function is called on each output byte.
*
* @param input - array of utf16 literals
* @param len - number of input utf16 literals
* @param output - pointer to output byte array.
*/

static void OutputLiteral(u16 *input, u16 len, u8 **output)
{
  u16 previous, l = len;
  int diff;

  /* most significant bit is 0 to mark a literal */
  OutputByte((u8)(len & 0x3F) | ((len > 0x3F) << 6), output);

  len >>= 6;

  while (len > 0) {
    OutputByte((u8)(len & 0x7F) | ((len > 0x7F) << 7), output);
    len >>= 7;
  }

  /* save the first Unicode character */
  previous = *input++;
  OutputByte(previous & 0xFF, output);
  OutputByte(previous >> 8, output);

  /* save differences between the characters */
  len = l - 1;
  while (len-- > 0) {
    diff = previous - *input;
    if ((diff & 0xFFFFFFC0) == 0 || (diff | 0x3F) == -1)
      OutputByte((u8)(diff & 0x7F), output);
    else {
      OutputByte((u8)((diff & 0x7F) | 0x80), output);
      diff >>= 7;
      if ((diff & 0xFFFFFFC0) == 0 || (diff | 0x3F) == -1)
        OutputByte((u8)(diff & 0x7F), output);
      else {
        OutputByte((u8)((diff & 0x7F) | 0x80), output);
        OutputByte(diff >> 7, output);
      }
    }
    previous = *input++;
  }
}

/**
* utf16 Compress function. This function implements described compression algorithm.
*
* @param input - array of input utf16 characters
* @param length - number of input utf16 characters
* @param output - output buffer
* @return number of output bytes
*/
int utf16_compress(u16 *input, int length, u8 *output)
{
  int i;
  u16 *input_orig = input;
  u8 *o = output;
  u16 *literal, *match, match_index;

  utf16_compressor_init();

  literal = input;
  while (length-- > 0) 
  {
    /* check for previous occurrence */
    match_index = hash(*input, *(input + 1));
    match = dictionary[match_index];
    dictionary[match_index] = input;
    if (match >= input_orig && match < input && *match == *input && *(match + 1) == *(input + 1))   
    {
      /* Previous occurrence was found. Encode literals...*/
      if (literal < input)
        OutputLiteral(literal, input - literal, &output);
      i = 2;
      /* ...find the occurrence length...*/
      while (*(match + i) == *(input + i))
        ++i;
      /* ...and encode the (offset, length) pair */
      OutputMatch(input - match, i, &output);
      input += i;
      length -= (i - 1);
      literal = input;
    } 
    else
      ++input;
  }
  /* if there are some remaining literals, encode them */
  if (literal < input)
    OutputLiteral(literal, input - literal, &output);

  return output - o;
}

/**
* utf16 Decompress function. This function implements described decompression algorithm.
*
* @param input - input compressed data
* @param len - length of input
* @param output - output buffer
* @return number of extracted utf16 characters
*/
int utf16_decompress(u8 *input, int len, u16 *output)
{
  u8 *o = (u8 *)output;
  int offset, length;
  u8 *end = input + len;
  int c; 

  while (input < end) {
  /* decite what to decode. Match or set of literals?*/
    if (*input & 0x80) 
    {  /* match */
      length = DecodeLength(&input);
      offset = DecodeOffset(&input);  // same algorithm as DecodeLength
      while (length-- > 0)
      {
        *output = *(output - offset);
        ++output;
      }
    }
    else 
    {  
      /* literal */
      length = DecodeLength(&input);
      *output = *input++;
      *output++ |= ((unsigned int)(*input++)) << 8;
      --length;

      while (length-- > 0) {
        c = *input & 0x7F;
        
        if (*input++ & 0x80) {    /* two bytes */
          c |= ((unsigned int)*input & 0x7F) << 7;
          
          if (*input++ & 0x80) {  /* three bytes */
            c |= *input++ << 14;
            
            if (c & 0x10000)      /* negative number */
              c |= 0xFFFF0000;
          }
          else if (c & 0x2000)    /* negative number */
            c |= 0xFFFFC000;
        }
        else if (c & 0x40)        /* negative number */
          c |= 0xFFFFFF80;

        *output = *(output - 1) - c;
        ++output;
      }
    }
  }
  return (u8 *)output - o;
}

