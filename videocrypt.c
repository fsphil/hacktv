/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2017 Philip Heron <phil@sanslogic.co.uk>                    */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */

/* -=== Videocrypt encoder ===-
 * 
 * This is a Videocrypt I/II encoder. It scrambles the image using a technique
 * called "line cut-and-rotate", and inserts the necessary data into the
 * VBI area of the image to activate the Videocrypt hardware unscrambler.
 * 
 * THANKS
 * 
 * Markus Kuhn and William Andrew Steer for their detailed descriptions
 * and examples of how Videocrypt works:
 * 
 * https://www.cl.cam.ac.uk/~mgk25/tv-crypt/
 * http://www.techmind.org/vdc/
 * 
 * Ralph Metzler for the details of how the VBI data is encoded:
 * 
 * http://src.gnu-darwin.org/ports/misc/vbidecode/work/bttv/apps/vbidecode/vbidecode.cc
 * 
 * Alex L. James for providing an active Sky subscriber card, VBI samples,
 * Videocrypt 2 information and testing.
 *
 * Marco Wabbel for xtea algo and Funcard (ATMEL based) hex files - needed for xtea.
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "video.h"

/* Experimental - Sky 06/07 mode channel ID */
#define SKY07_CHID 0x00	

/* Packet header sequences */
static const uint8_t _sequence[8] = {
	0x87,0x96,0xA5,0xB4,0xC3,0xD2,0xE1,0x87,
};
  
static const uint8_t _sequence2[8] = {
 	0x80,0x91,0xA2,0xB3,0xC4,0xD5,0xE6,0xF7,
};

/* Hamming codes */
static const uint8_t _hamming[16] = {
	0x15,0x02,0x49,0x5E,0x64,0x73,0x38,0x2F,
	0xD0,0xC7,0x8C,0x9B,0xA1,0xB6,0xFD,0xEA,
};

/* Blocks for VC1 free-access decoding */
static _vc_block_t _fa_blocks[] = { { 0x05, VC_PRBS_CW_FA } };

/* Blocks for VC1 conditional-access sample, taken from Sky Movies and modified, */
/* requires an active Sky 07 card to decode */
static _vc_block_t _sky07_blocks[] = {
	{
		0x07, 0,
		{
 			{ 0x20 },
 			{ }, { }, { }, { },	{ },
 			{ 0xE8,0x29,0x3E,0xED,0xF0,0x1C,0x01,0x6F,0xE9,0x06,0xD6 }
		},
	},
	{
		0x07, 0,
		{
 			{ 0x20,0x00,0x77,0x20,0x20,0x20,0x48,0x41,0x43,0x4b,0x54,0x56,0x20,0x20,0x20,0x20,0x53,0x4b,0x59,0x30,0x37,0x20,0x4d,0x4f,0x44,0x45 },
 			{ }, { }, { }, { },	{ },
 			{ 0xE8,0x29,0x3E,0xED,0xF0,0x1C,0x01,0x6F,0xE9,0x06,0xD6 }
		},
	},
};

/* Blocks for VC1 conditional-access sample, taken from Sky Movies and modified, */
/* requires an active Sky 09 card to decode */
static _vc_block_t _sky09_blocks[] = {
	{
		0x07, 0,
		{
 			{ 0x20 }, 
			 { }, { }, { }, { },{ },
 			{ 0xe8,0x43,0x0a,0x88,0x82,0x61,0x0c,0x29,0xe4,0x03,0xf6 }
		},
	},
	{
		0x07, 0,
		{
 			{ 0x20,0x00,0x77,0x20,0x20,0x20,0x48,0x41,0x43,0x4b,0x54,0x56,0x20,0x20,0x20,0x20,0x53,0x4b,0x59,0x30,0x39,0x20,0x4d,0x4f,0x44,0x45 },
 			{ }, { }, { }, { },	{ },
 			{ 0xe8,0x43,0x0a,0x88,0x82,0x61,0x0c,0x29,0xe4,0x03,0xf6 }
		},
	},
};

/* Blocks for VC1 conditional-access sample, taken from Tyson fight and modified, */
/* requires an active Sky 10 (0A) series card to decode enabled */
static _vc_block_t _sky10_blocks[] = {
	{
		0x07, 0x16DEC6F6FD37145BUL,
		{
 			{ 0x20 },
 			{ }, { }, { }, { },	{ },
 			{ 0xF8,0x91,0x45,0x24,0xE9,0xEB,0x8F,0x00,0xE8,0x78,0x86,0x13,0x63,0x92,0xB3,0x14,0x73,0x32,0xBA,0xC3,0xCE,0x7B,0x41,0xE6,0x4B,0x53,0xB7,0x76,0x82,0x8B,0xFD }
		},
	},
	{
		0x07, 0x7DE3581091A306E5UL,
		{
 			{ 0x20,0x00,0x77,0x20,0x20,0x20,0x48,0x41,0x43,0x4b,0x54,0x56,0x20,0x20,0x20,0x20,0x53,0x4b,0x59,0x31,0x30,0x20,0x4d,0x4f,0x44,0x45 },
 			{ }, { }, { }, { },	{ },
 			{ 0xF8,0x91,0x54,0x24,0x4B,0xFE,0x9F,0xA7,0x26,0x75,0x11,0x65,0xE5,0x11,0x70,0x75,0xDA,0xF1,0x0B,0x99,0x6B,0x95,0x0A,0x53,0xC0,0xCA,0xAE,0x76,0xEF,0x5D,0xD8 }	
		},
	},	
};

/* Blocks for VC1 conditional-access sample, taken from Tyson fight and modified, */
/* requires an active Sky 10 (0A) series card with PPV enabled to decode */

/*
This packet sets up credits and events
you wish to purchase. The 12h/34h bytes are
the first program/credit pair. The 56h/78h
bytes are the second program/credit pair.
The six bytes 01h-06h are the six event
numbers to enable.

53 86 01 00 2D 
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 12
34 56 78 01 02 03 04 05 06 00 00 00 00 

For these CWs, the following needs to be sent to the 
card to activate it for event 66.

53 86 01 00 2D
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 42
34 00 00 42 00 00 00 00 00 00 00 00 00 

This packet forces the new Pay Per View
data into the card without authorization.

53 74 01 00 20
C0 8F 40 00 00 2E AB F5 19 26 98 B5 46 77 BB E3
32 12 ED 50 49 FF 57 B7 52 C0 0A 02 02 02 02 02
*/

static _vc_block_t _sky10ppv_blocks[] = {
	{
		0x07, 0x7298D7112703C5D9UL,
		{
 			{ 0x20 },
			/* Packet below removes PPV credits - uncomment at your own risk! */
 			//{ 0xC0,0x8F,0x40,0x00,0x00,0x2E,0xAB,0xF5,0x19,0x26,0x98,0xB5,0x46,0x77,0xBB,0xE3,0x32,0x12,0xED,0x50,0x49,0xFF,0x57,0xB7,0x52,0xC0,0x0A,0x02,0x02,0x02,0x02,0x02 }, 
			/* Comment out below line is above is enabled */
			{ }, 
			{ }, { }, { },	{ },
 			{ 0xf8,0x94,0x9c,0xf0,0x42,0xe6,0x2f,0x2a,0xb7,0x25,0xbc,0x62,0xd8,0x95,0x1e,0x93,0x7c,0x55,0xb8,0x71,0xd5,0x2f,0x29,0x65,0xa2,0x1e,0x3a,0x63,0xf6,0x59,0xf6 }
		},
	},
	{
		0x07, 0x20222974682EFF97UL,
		{
 			{ 0x20,0x00,0x77,0x20,0x20,0x20,0x48,0x41,0x43,0x4b,0x54,0x56,0x20,0x20,0x20,0x20,0x53,0x4b,0x59,0x31,0x30,0x20,0x4d,0x4f,0x44,0x45 },
 			{ }, { }, { }, { }, { },
			{ 0xf8,0x94,0x8e,0xf0,0x42,0x7e,0x13,0xcc,0x19,0xdd,0x87,0x60,0x5a,0x78,0x61,0x0b,0x66,0x44,0x51,0x87,0xae,0x01,0xbc,0x4d,0x47,0x31,0x03,0x80,0x7f,0x8d,0x74 }	
		},
	},
};

/* Blocks for VC1 conditional-access sample, taken from MTV UK and modified, */
/* requires an active Sky 11 series card to decode */
static _vc_block_t _sky11_blocks[] = {
	{
		0x07, 0xB2DD55A7BCE178EUL,
		{
 			{ 0x20 },
 			{ }, { }, { }, { },	{ },
 			{ 0xF8,0x19,0x10,0x83,0x20,0x85,0x60,0xAF,0x8F,0xF0,0x49,0x34,0x86,0xC4,0x6A,0xCA,0xC3,0x21,0x4D,0x44,0xB3,0x24,0x36,0x57,0xEC,0xA7,0xCE,0x12,0x38,0x91,0x3E }
		},
	},
	{
		0x07, 0xF9885DA50770B80UL,
		{
 			{ 0x20,0x00,0x77,0x20,0x20,0x20,0x48,0x41,0x43,0x4b,0x54,0x56,0x20,0x20,0x20,0x20,0x53,0x4b,0x59,0x31,0x31,0x20,0x4d,0x4f,0x44,0x45 },
 			{ }, { }, { }, { },	{ },
 			{ 0xF8,0x19,0x10,0x83,0x20,0xD1,0xB5,0xA9,0x1F,0x82,0xFE,0xB3,0x6B,0x0A,0x82,0xC3,0x30,0x7B,0x65,0x9C,0xF2,0xBD,0x5C,0xB0,0x6A,0x3B,0x64,0x0F,0xA2,0x66,0xBB }	
		},
	},
};

/* Blocks for VC1 conditional-access sample, taken from Sky One and modified, */
/* requires an active Sky 12 series card to decode */
static _vc_block_t _sky12_blocks[] = {
	{
		0x07, 0xC73805F6EF5A1AAB,
		{
 			{ 0x20 },
 			{ }, { }, { }, { },	{ },
 			{ 0xE8,0xCD,0xA7,0x83,0xD1,0x21,0xE5,0x42,0x30,0xF1,0x09,0xBD,0x74,0xB5,0x24,0xC5,0xBF,0x62,0x08,0x1F,0x43,0xE9,0x17,0xA9,0x69,0xE0,0xF6,0x3A,0x35,0x88,0x61 }
		},
	},
	{
		0x07, 0xc389AED26500336B,
		{
 			{ 0x20,0x00,0x77,0x20,0x20,0x20,0x48,0x41,0x43,0x4b,0x54,0x56,0x20,0x20,0x20,0x20,0x53,0x4b,0x59,0x31,0x32,0x20,0x4d,0x4f,0x44,0x45 },
 			{ }, { }, { }, { },	{ },
 			{ 0xE8,0xCD,0xA7,0x83,0xD1,0x31,0x3F,0xF3,0xB5,0x71,0x01,0x9C,0xF9,0xAE,0xB9,0x8D,0x9C,0x19,0xB1,0x75,0xFA,0xCC,0x90,0x33,0x1E,0xDC,0x38,0xCA,0x58,0x10,0xFA }
		},
	},
};

/* Sequence for Conditional-access sample, taken from The Adult Channel and modified. */
/* Requires a PIC16F84 based card flashed with supplied hex file or an active TAC card. */
static _vc_block_t _tac_blocks[] = {
	{
		0x07, 0,
		{
		 	{ 0x20 },
 			{ }, { }, { }, { },	{ },
 			{ 0xE8,0x49,0x09,0x92,0x1F,0x93,0x20,0x74,0xCE,0x62,0xE4 }
		}
	},
	{
		0x07, 0,
		{
			{ 0x20,0x00,0x76,0x20,0x20,0x20,0x48,0x41,0x43,0x4b,0x54,0x56,0x20,0x20,0x20,0x20,0x20,0x54,0x41,0x43,0x20,0x4d,0x4f,0x44,0x45 },
 			{ }, { }, { }, { },	{ },
 			{ 0xE8,0x49,0x09,0x92,0x1F,0x93,0x20,0x74,0xCE,0x62,0xE4 }
		}
	},
};

static _vc_block_t _xtea_blocks[] = {
	{
	    0x07, 0x1A298F7C70F4F65UL,
	    {
	        { 0x20 },
	        { }, { }, { }, { }, { },
	        { 0xE8,0x49,0x09,0x92,0x1F,0x93,0x20,0x74,0xCE,0x62,0xE4,0xEA,0xB3,0xB3,0xBF,0x8A,0xE3,0xDF,0x26,0x99,0x5E,0x2F,0x0B,0xFB,0x8E,0x45,0x22,0x94,0x4A,0x1A,0x8E }
	    } 
	},
	{
	    0x07, 0xD491B336D3D54BAUL,
	    {
	        { 0x20,0x00,0x79,0x20,0x20,0x20,0x48,0x41,0x43,0x4b,0x54,0x56,0x20,0x20,0x20,0x54,0x41,0x43,0x2f,0x58,0x54,0x45,0x41,0x20,0x4d,0x4f,0x44,0x45 },
	        { }, { }, { }, { }, { },
	        { 0xE8,0x49,0x09,0x92,0x1F,0x93,0x20,0x74,0xCE,0x62,0xE4,0xD5,0x04,0x5E,0x8E,0x54,0x91,0x58,0xFF,0x44,0x32,0x72,0xBB,0xEF,0x29,0x8E,0x3C,0x98,0x41,0xD1,0x33 }
	    }
	},
};

/* Blocks for VC2 conditional-access sample, taken from Discovery and modified. */
/* Requires a PIC16F84 based card flashed with supplied hex file or an active Multichoice card.*/
static _vc2_block_t _vc2_blocks[] = {
	{
		0xA0, 0,
		{
			{ 0x21,0x02,0x6B,0x20,0x48,0x41,0x43,0x4b,0x54,0x56,0x20,0x56,0x43,0x32 },
			{ 0xE1 },
			{ 0xE1 },
			{ 0xE1 },
			{ 0xE1 },
			{ 0xF9,0x62,0x36,0x82,0x04,0xF7,0x87,0x00,0x00,0x5A,0x06 },
		}
	},
	{
		0xA0, 0,
		{
			{ 0x21,0x02 },
			{ 0xE1 },
			{ 0xE1 },
			{ 0xE1 },
			{ 0xE1 },
			{ 0xF9,0x62,0x56,0x82,0x04,0xF7,0x87,0x00,0x00,0xA5,0x06 },
		}
	},
};

/* Blocks for VC2 free-access decoding */
static _vc2_block_t _fa2_blocks[] = { { 0x9C, VC_PRBS_CW_FA } };

/* 
  * Videocrypt key used for Eurotica and The Adult Channel 
  * Sky 07 card used a 56-byte key with three possible key offsets 
  * depending on msg[1] byte value (month). TAC key has five 
  * different offsets. We only ever use one here.
  *
  * If this key is changed, wrong signature will be generated
  * and you will receive "THIS CHANNEL IS BLOCKED" message. You can 
  * update the key in hex file at address 0000 in EEPROM data.
*/
const unsigned char tac_key[] = {
	0xd9, 0x45, 0x08, 0xdb, 0x7c, 0xf9, 0x56, 0xf7,
	0x58, 0x18, 0x22, 0x54, 0x38, 0xcd, 0x3d, 0x94,
	0x09, 0xe6, 0x8e, 0x0d, 0x9a, 0x86, 0xfc, 0x1c,
	0xa0, 0x19, 0x8f, 0xbc, 0xfd, 0x8d, 0xd1, 0x57,
	0x56, 0xf2, 0xb6, 0x4f, 0xc9, 0xbd, 0x2a, 0xb3,
	0x9d, 0x81, 0x5d, 0xe0, 0x05, 0xb5, 0xb9, 0x26,
	0x67, 0x3c, 0x65, 0xa0, 0xba, 0x39, 0xc7, 0xaf,
	0x33, 0x24, 0x47, 0xa6, 0x20, 0x1e, 0x14, 0x6f,
	0x48, 0x9b, 0x4d, 0xa6, 0xf9, 0xd9, 0xdf, 0x6e,
	0xac, 0x84, 0xfa, 0x8b, 0x2e, 0xb6, 0x76, 0x19,
	0xc1, 0xb0, 0xa3, 0xbb, 0x0c, 0xfd, 0x70, 0x72,
	0xca, 0x55, 0xef, 0xa0, 0x7f, 0xbf, 0x59, 0xad
};

/* Videocrypt key used for Sky 07 series cards */
const unsigned char sky07_key[] = {
   0x65, 0xe7, 0x71, 0x1a, 0xb4, 0x88, 0xd7, 0x76,
   0x28, 0xd0, 0x4c, 0x6e, 0x86, 0x8c, 0xc8, 0x43,
   0xa9, 0xec, 0x60, 0x42, 0x05, 0xf2, 0x3d, 0x1c,
   0x6c, 0xbc, 0xaf, 0xc3, 0x2b, 0xb5, 0xdc, 0x90,
   0xf9, 0x05, 0xea, 0x51, 0x46, 0x9d, 0xe2, 0x60,
   0x70, 0x52, 0x67, 0x26, 0x61, 0x49, 0x42, 0x09,
   0x50, 0x99, 0x90, 0xa2, 0x36, 0x0e, 0xfd, 0x39 
};

/* Videocrypt key used for Sky 09 series cards */
const unsigned char sky09_key[216] = {
	0x91, 0x61, 0x9d, 0x53, 0xb3, 0x27, 0xd5, 0xd9,
	0x0F, 0x59, 0xa6, 0x6f, 0x73, 0xfb, 0x99, 0x4c,
	0xfb, 0x45, 0x54, 0x8e, 0x20, 0x5f, 0xb3, 0xb1,
	0x38, 0xd0, 0x6b, 0xa7, 0x40, 0x39, 0xed, 0x2a,
	0xda, 0x43, 0x8d, 0x51, 0x92, 0xd6, 0xe3, 0x61,
	0x65, 0x8c, 0x71, 0xe6, 0x84, 0x65, 0x87, 0x03,
	0x55, 0xbc, 0x64, 0x07, 0xbb, 0x79, 0x9e, 0x40,
	0x97, 0x89, 0xc4, 0x14, 0x8f, 0x8b, 0x41, 0x4d,
	0x2a, 0xaa, 0xe8, 0xe1, 0x08, 0xcd, 0x82, 0x43,
	0x8f, 0x6f, 0x36, 0x9b, 0x72, 0x47, 0xf2, 0xa4,
	0x49, 0xdd, 0x8b, 0x6e, 0x26, 0xc6, 0xbf, 0xb7,
	0xd8, 0x44, 0xc3, 0x70, 0xa3, 0x4c, 0xb6, 0xb2,
	0x37, 0x9b, 0x09, 0xdf, 0x32, 0x28, 0x24, 0x86,
	0x8d, 0x5f, 0xe6, 0x4b, 0x5d, 0xd0, 0x2f, 0xdb,
	0xac, 0x2e, 0x78, 0x1e, 0xcc, 0x52, 0xc1, 0x61,
	0xea, 0x82, 0xca, 0xb3, 0xf4, 0x8f, 0x63, 0x8e,
	0x6c, 0xbc, 0xaf, 0xc3, 0x2b, 0xb5, 0xdc, 0x90,
	0xf9, 0x05, 0xea, 0x51, 0x46, 0x9d, 0xe2, 0x60,
	0x01, 0x35, 0x59, 0x79, 0x00, 0x00, 0x55, 0x0F,
	0x00, 0x00, 0x00, 0x00, 0x10, 0x6e, 0x1c, 0xbd,
	0xfe, 0x44, 0xeb, 0x79, 0xf3, 0xab, 0x5d, 0x23,
	0xb3, 0x20, 0xd2, 0xe7, 0xfc, 0x00, 0x03, 0x6f,
	0xd8, 0xb7, 0xf7, 0xf3, 0x55, 0x72, 0x47, 0x13,
	0x7b, 0x0c, 0x08, 0x01, 0x8a, 0x2c, 0x70, 0x56,
	0x0a, 0x85, 0x18, 0x14, 0x43, 0xc9, 0x46, 0x64,
	0x6c, 0x9a, 0x99, 0x59, 0x0a, 0x6c, 0x40, 0xd5,
	0x17, 0xb3, 0x2c, 0x69, 0x41, 0xe8, 0xe7, 0x0e
};

/* Key used by Multichoice Central Europe broadcase in Videocrypt 2 */
const unsigned char vc2_key[] = {
    0x58,0x6B,0x4D,0x05,0xB0,0x69,0x83,0x16,
    0xA6,0x48,0xDE,0x5E,0x0B,0xAA,0x49,0xA9,
    0xC6,0xE5,0x93,0x1A,0xBE,0x56,0x73,0x20,
    0xFB,0xF8,0xCA,0x08,0x34,0x29,0x8A,0x9B
};

static const uint32_t xtea_key[4]= {
	0x00112233, 0x44556677, 0x8899aabb, 0xccddeeff
};

/* Calculate Videocrypt message CRC */
static uint8_t _crc(uint8_t *data)
{
	int x;
	uint8_t crc;

	for(crc = x = 0; x < 31; x++) 
	{
		crc += data[x];
	}
		
	return (~crc + 1);
}

/* Reverse bits in an 8-bit value */
static uint8_t _reverse(uint8_t b)
{
	b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
	b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
	b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
	return(b);
}

/* Reverse bits in an x-bit value */
static uint64_t _rev(uint64_t b, int x)
{
	uint64_t r = 0;
	
	while(x--)
	{
		r = (r << 1) | (b & 1);
		b >>= 1;
	}
	
	return(r);
}

/* Reverse nibbles in a byte */
static inline uint8_t _rnibble(uint8_t a)
{
	return((a >> 4) | (a << 4));
}

/* Generate IW for PRBS */
static uint64_t _generate_iw(uint64_t cw, uint8_t fcnt)
{
	uint64_t iw;
	
	/* FCNT is repeated 8 times, each time inverted */
	iw  = ((fcnt ^ 0xFF) << 8) | fcnt;
	iw |= (iw << 16) | (iw << 32) | (iw << 48);
	
	return((iw ^ cw) & VC_PRBS_CW_MASK);
}

/* Apply VBI frame interleaving */
static void _interleave(uint8_t *frame)
{
	int b, i, j;
	int offset[6] = { 0, 6, 12, 20, 26, 32 };
	uint8_t r[8];
	uint8_t m;
	
	for(b = 0; b < 6; b++)
	{
		uint8_t *s = frame + offset[b];
		
		s[0] = _reverse(s[0]);
		s[7] = _reverse(s[7]);
		
		for(i = 0, m = 0x80; i < 8; i++, m >>= 1)
		{
			r[i] = 0x00;
			for(j = 0; j < 8; j++)
			{
				r[i] |= ((m & s[j]) ? 1 : 0) << j;
			}
		}
		
		memcpy(s, r, 8);
	}
}

/* Encode VBI data */
static void _encode_vbi(uint8_t vbi[40], const uint8_t data[16], uint8_t a, uint8_t b)
{
	int x;
	uint8_t crc;
	
	crc = vbi[0] = a;
	for(x = 0; x < 8; x++)
	{
		crc += vbi[1 + x] = data[0 + x];
	}
	vbi[9] = crc;
	
	crc = vbi[10] = b;
	for(x = 0; x < 8; x++)
	{
		crc += vbi[11 + x] = data[8 + x];
	}
	vbi[19] = crc;
	
	/* Hamming code the VBI data */
	for(x = 19; x >= 0; x--)
	{
		vbi[x * 2 + 1] = _hamming[vbi[x] & 0x0F];
		vbi[x * 2 + 0] = _hamming[vbi[x] >> 4];
	}
	
	/* Interleave the VBI data */
	_interleave(vbi);
}
 

int vc_init(vc_t *s, vid_t *vid, const char *mode, const char *mode2)
{
	double f, l;
	int x;
	time_t t;
	srand((unsigned) time(&t));
	
	memset(s, 0, sizeof(vc_t));
	
	s->vid      = vid;
	s->counter  = 0;
	s->cw       = 0x0;
	
	/* Videocrypt I setup */
	if(mode == NULL)
	{
		s->blocks    = NULL;
		s->block_len = 0;
	}
	else if(strcmp(mode, "free") == 0)
	{
		s->blocks    = _fa_blocks;
		s->block_len = 1;
	}
	else if(strcmp(mode, "sky07") == 0)
	{
		s->blocks    = _sky07_blocks;
		s->block_len = 2;
		_vc_seed_sky07(&s->blocks[0], VC_SKY7);
		_vc_seed_sky07(&s->blocks[1], VC_SKY7);
	}
	else if(strcmp(mode, "sky09") == 0)
	{
		s->blocks    = _sky09_blocks;
		s->block_len = 2;
		_vc_seed_sky09(&s->blocks[0]);
		_vc_seed_sky09(&s->blocks[1]);
	}
	else if(strcmp(mode, "sky10") == 0)
	{
		s->blocks    = _sky10_blocks;
		s->block_len = 2;
	}			
	else if(strcmp(mode, "sky10ppv") == 0)
	{
		s->blocks    = _sky10ppv_blocks;
		s->block_len = 2;
	}
	else if(strcmp(mode, "sky11") == 0)
	{
		s->blocks    = _sky11_blocks;
		s->block_len = 2;
	}
	else if(strcmp(mode, "sky12") == 0)
	{
		s->blocks    = _sky12_blocks;
		s->block_len = 2;
	}
	else if (strcmp(mode, "tac1") == 0) 
	{
		s->blocks    = _tac_blocks;
		s->block_len = 2;
		_vc_seed_sky07(&s->blocks[0], VC_TAC1);
		_vc_seed_sky07(&s->blocks[1], VC_TAC1);
	}
	else if (strcmp(mode, "tac2") == 0) 
	{
		s->blocks    = _tac_blocks;
		s->block_len = 2;
		_vc_seed_sky07(&s->blocks[0], VC_TAC2);
		_vc_seed_sky07(&s->blocks[1], VC_TAC2);
	}
	else if (strcmp(mode, "xtea") == 0) 
	{
		s->blocks    = _xtea_blocks;
		s->block_len = 2;
		_vc_seed_xtea(&s->blocks[0]);
		_vc_seed_xtea(&s->blocks[1]);
	}
	else
	{
		fprintf(stderr, "Unrecognised Videocrypt I mode '%s'.\n", mode);
		return(VID_ERROR);
	}
	
	s->block = 0;
	
	/* Videocrypt II setup */
	if(mode2 == NULL)
	{
		s->blocks2    = NULL;
		s->block2_len = 0;
	}
	else if(strcmp(mode2, "free") == 0)
	{
		s->blocks2    = _fa2_blocks;
		s->block2_len = 1;
	}
	else if(strcmp(mode2, "conditional") == 0)
	{
		s->blocks2    = _vc2_blocks;
		s->block2_len = 2;
		_vc_seed_vc2(&s->blocks2[0]);
		_vc_seed_vc2(&s->blocks2[1]);
	}
	else
	{
		fprintf(stderr, "Unrecognised Videocrypt II mode '%s'.\n", mode2);
		return(VID_ERROR);
	}
	
	s->block2 = 0;
	
	/* Sample rate ratio */
	f = (double) s->vid->width / VC_WIDTH;
	
	/* Videocrypt timings appear to be calculated against the centre of the hsync pulse */
	l = (double) VC_SAMPLE_RATE * s->vid->conf.hsync_width / 2;
	
	/* Quick and dirty sample rate conversion array */
	for(x = 0; x < VC_WIDTH; x++)
	{
		s->video_scale[x] = round((l + x) * f);
	}
	
	/* Add one delay line */
	s->vid->olines += 1;
	
	return(VID_OK);
}

void vc_free(vc_t *s)
{
	/* Nothing */
}

void vc_render_line(vc_t *s, const char *mode, const char *mode2)
{
	int x;
	const uint8_t *bline = NULL;
	
	/* On the first line of each frame, generate the VBI data */
	if(s->vid->line == 1)
	{
		uint64_t iw;
		uint8_t crc;
		
		/* Videocrypt I */
		if(s->blocks)
		{
			if((s->counter & 7) == 0)
			{
				/* The active message is updated every 8th frame. The last
				 * message in the block is a duplicate of the first. */
				for(crc = x = 0; x < 31; x++)
				{
					crc += s->message[x] = s->blocks[s->block].messages[((s->counter >> 3) & 7) % 7][x];
				}
				
				s->message[x] = ~crc + 1;
			}
			
			if((s->counter & 4) == 0)
			{
				/* The first half of the message. Transmitted for 4 frames */
				_encode_vbi(
					s->vbi, s->message,
					_sequence[(s->counter >> 4) & 7],
					s->counter & 0xFF
				);
			}
			else
			{
				/* The second half of the message. Transmitted for 4 frames */
				_encode_vbi(
					s->vbi, s->message + 16,
					_rnibble(_sequence[(s->counter >> 4) & 7]),
					s->blocks[s->block].mode
				);
			}
		}
		
		/* Videocrypt II */
		if(s->blocks2)
		{
			if((s->counter & 1) == 0)
			{
				/* The active message is updated every 2nd frame */
				for(crc = x = 0; x < 31; x++)
				{
					crc += s->message2[x] = s->blocks2[s->block2].messages[(s->counter >> 1) & 7][x];
				}
				
				s->message2[x] = ~crc + 1;
			}
			
			if((s->counter & 1) == 0)
			{
				/* The first half of the message */
				_encode_vbi(
					s->vbi2, s->message2,
					_sequence2[(s->counter >> 1) & 7],
					s->counter & 0xFF
				);
			}
			else
			{
				/* The second half of the message */
				_encode_vbi(
					s->vbi2, s->message2 + 16,
					_rnibble(_sequence2[(s->counter >> 1) & 7]),
					(s->counter & 0x08 ? 0x00 : s->blocks2[s->block2].mode)
				);
			}
		}
		
		/* Reset the PRBS */
		iw = _generate_iw(s->cw, s->counter);
		s->sr1 = iw & VC_PRBS_SR1_MASK;
		s->sr2 = (iw >> 31) & VC_PRBS_SR2_MASK;
		
		s->counter++;
		
		/* After 64 frames, advance to the next VC1 block and codeword */
		if((s->counter & 0x3F) == 0)
		{
			/* Apply the current block codeword */
			if(s->blocks)
			{
				s->cw = s->blocks[s->block].codeword;
			}

			/* Generate new seeds */
			if(mode)
			{
				if(strcmp(mode,"tac1") == 0)  _vc_seed_sky07(&s->blocks[s->block], VC_TAC1);
				if(strcmp(mode,"tac2") == 0)  _vc_seed_sky07(&s->blocks[s->block], VC_TAC2);
				if(strcmp(mode,"sky07") == 0) _vc_seed_sky07(&s->blocks[s->block], VC_SKY7);
				if(strcmp(mode,"sky09") == 0) _vc_seed_sky09(&s->blocks[s->block]);
				if(strcmp(mode,"xtea") == 0)  _vc_seed_xtea(&s->blocks[s->block]);
			}

			/* Move to the next block */
			if(++s->block == s->block_len)
			{
				s->block = 0;
			}
		}

		/* After 16 frames, apply the codeword */
		if((s->counter & 0x0F) == 0)
		{
			/* Apply the current block codeword */
			if(s->blocks2)
			{
				s->cw = s->blocks2[s->block2].codeword;
			}

			if(mode2)
			{
				if(strcmp(mode2,"conditional") == 0) _vc_seed_vc2(&s->blocks2[s->block2]);
			}
			
			/* Move to the next block after 64 frames */
			if(((s->counter & 0x3F) == 0) && (++s->block2 == s->block2_len))
			{
				s->block2 = 0;
			}
		}
	}
	
	/* Calculate VBI line, or < 0 if not */
	if(s->blocks &&
	   s->vid->line >= VC_VBI_FIELD_1_START &&
	   s->vid->line < VC_VBI_FIELD_1_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Top VBI field */
		bline = &s->vbi[(s->vid->line - VC_VBI_FIELD_1_START) * VC_VBI_BYTES_PER_LINE];
	}
	else if(s->blocks &&
	        s->vid->line >= VC_VBI_FIELD_2_START &&
	        s->vid->line < VC_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Bottom VBI field */
		bline = &s->vbi[(s->vid->line - VC_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD) * VC_VBI_BYTES_PER_LINE];
	}
	else if(s->blocks2 &&
	        s->vid->line >= VC2_VBI_FIELD_1_START &&
	        s->vid->line < VC2_VBI_FIELD_1_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Top VBI field VC2 */
		bline = &s->vbi2[(s->vid->line - VC2_VBI_FIELD_1_START) * VC_VBI_BYTES_PER_LINE];
	}
	else if(s->blocks2 &&
	        s->vid->line >= VC2_VBI_FIELD_2_START &&
	        s->vid->line < VC2_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Bottom VBI field VC2 */
		bline = &s->vbi2[(s->vid->line - VC2_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD) * VC_VBI_BYTES_PER_LINE];
	}
	
	/* Render the VBI line if necessary */
	if(bline)
	{
		int b, c;
		
		x = s->video_scale[VC_VBI_LEFT];
		
		for(b = 0; b < VC_VBI_BITS_PER_LINE; b++)
		{
			c = (bline[b / 8] >> (b % 8)) & 1;
			c = c ? s->vid->white_level : s->vid->black_level;
			
			for(; x < s->video_scale[VC_VBI_LEFT + VC_VBI_SAMPLES_PER_BIT * (b + 1)]; x++)
			{
				s->vid->output[x * 2] = c;
			}
		}
		
		*s->vid->vbialloc = 1;
	}
	
	/* Scramble the line if necessary */
	x = -1;
	
	if((s->vid->line >= VC_FIELD_1_START && s->vid->line < VC_FIELD_1_START + VC_LINES_PER_FIELD) ||
	   (s->vid->line >= VC_FIELD_2_START && s->vid->line < VC_FIELD_2_START + VC_LINES_PER_FIELD))
	{
		int i;
		
		x = (s->c >> 8) & 0xFF;
		
		for(i = 0; i < 16; i++)
		{
			int a;
			
			/* Update shift registers */
			s->sr1 = (s->sr1 >> 1) ^ (s->sr1 & 1 ? 0x7BB88888UL : 0);
			s->sr2 = (s->sr2 >> 1) ^ (s->sr2 & 1 ? 0x17A2C100UL : 0);
			
			/* Load the multiplexer address */
			a = _rev(s->sr2, 29) & 0x1F;
			if(a == 31) a = 30;
			
			/* Shift into result register */
			s->c = (s->c >> 1) | (((_rev(s->sr1, 31) >> a) & 1) << 15);
		}
	}
	
	/* Hack to preserve WSS signal data */
	if(s->vid->line == 24) x = -1;
	
	if(x != -1)
	{
		int cut;
		int lshift;
		int y;
		int16_t *delay = s->vid->oline[s->vid->odelay - 1];
		
		cut = 105 + (0xFF - x) * 2;
		lshift = 710 - cut;
		
		y = s->video_scale[VC_LEFT + lshift];
		for(x = s->video_scale[VC_LEFT]; x < s->video_scale[VC_LEFT + cut]; x++, y++)
		{
			delay[x * 2] = s->vid->output[y * 2];
		}
		
		y = s->video_scale[VC_LEFT];
		for(; x < s->video_scale[VC_RIGHT + VC_OVERLAP]; x++, y++)
		{
			delay[x * 2] = s->vid->output[y * 2];
		}
	}
	
	vid_adj_delay(s->vid, 1);
}

void _vc_seed_sky07(_vc_block_t *s, int ca)
{
	int i;
	int oi = 0;	
	unsigned char b;
	uint64_t answ[8];
	int offset = 0;

	if(ca == VC_TAC2)
	/* TAC key offsets */
	{
		if (s->messages[6][1] > 0x33) offset = 0x08;
		if (s->messages[6][1] > 0x3a) offset = 0x32;
		if (s->messages[6][1] > 0x43) offset = 0x40;
		if (s->messages[6][1] > 0x4a) offset = 0x48;
	}
	else if (ca == VC_SKY7)
	/* Sky 07 key offsets */
	{
		s->messages[6][6] = SKY07_CHID;
		if (s->messages[6][1] > 0x32) offset = 0x08;
  		if (s->messages[6][1] > 0x3a) offset = 0x18;
	}

	/* Change date code for old TAC cards */
	if(ca == VC_TAC1) s->messages[6][1] = 0x29;

	/* Random seed for bytes 11 to 26 */
	for(int i=11; i < 27; i++) s->messages[6][i] = rand() + 0xFF;

	/* Reset answers */
	for (i = 0; i < 8; i++)  answ[i] = 0;
	
	for (i = 0; i < 27; i++) _vc_kernel07(answ, &oi, s->messages[6][i], offset, ca);

	/* Calculate signature */
	for (i = 27, b = 0; i < 31; i++)
	{
		_vc_kernel07(answ, &oi, b, offset, ca);
		_vc_kernel07(answ, &oi, b, offset, ca);
		b = s->messages[6][i] = answ[oi];
		oi = (oi + 1) & 7;
	}

	/* Generate checksum */
	s->messages[6][31] = _crc(s->messages[6]);

	/* Iterate through _vc_kernel07 64 more times (99 in total) 
	   Odd bug(?) in newer TAC card where checksum is always 0x0d */
	for (i = 0; i < 64; i++)
	{
		_vc_kernel07(answ, &oi, (ca == VC_TAC2 && s->messages[6][1] > 0x30) ? 0x0d : s->messages[6][31], offset, ca);
	}
	
	/* Mask high nibble of last byte as it's not used */
	answ[7] &= 0x0F;
	
	/* Reverse calculated control word */
	for(i = 0, s->codeword = 0; i < 8; i++)	s->codeword = answ[i] << (i * 8) | s->codeword;
}

void _vc_seed_vc2(_vc2_block_t *s)
{
	int i;
	int oi = 0;	
	unsigned char b;
	uint64_t answ[8];

	/* Random seed for bytes 11 to 26 */
	for(int i=11; i < 27; i++) s->messages[5][i] = rand() + 0xFF;

	/* Reset answers */
	for (i = 0; i < 8; i++)  answ[i] = 0;
	
	for (i = 0; i < 27; i++) _vc_kernel07(answ, &oi, s->messages[5][i], 0, VC2_MC);

	/* Calculate signature */
	for (i = 27, b = 0; i < 31; i++)
	{
		_vc_kernel07(answ, &oi, b, 0, VC2_MC);
		_vc_kernel07(answ, &oi, b, 0, VC2_MC);
		b = s->messages[5][i] = answ[oi];
		oi = (oi + 1) & 7;
	}

	/* Generate checksum */
	s->messages[5][31] = _crc(s->messages[5]);

	for (i = 0; i < 64; i++)
	{
		_vc_kernel07(answ, &oi, s->messages[5][31], 0, VC2_MC);
	}
	
	/* Mask high nibble of last byte as it's not used */
	answ[7] &= 0x0F;
	
	/* Reverse calculated control word */
	for(i = 0, s->codeword = 0; i < 8; i++)	
	{
		/* Random bytes 17 - 24 in OSD message 0x21 used in seed generation in Videocrypt II */
		s->messages[0][i + 17] = rand() + 0xFF;
		answ[i] ^= s->messages[0][i + 17];
		
		s->codeword = answ[i] << (i * 8) | s->codeword;
	}
}

void _vc_kernel07(uint64_t *out, int *oi, const unsigned char in, int offset, int ca)
{
	unsigned char b, c;

  	unsigned char key[32];

	if(ca == VC_SKY7) 
	{
		memcpy(key, sky07_key + offset, 32);
	}
	else if(ca == VC2_MC) 
	{
		memcpy(key, vc2_key, 32);
	}
	else
	{
		memcpy(key, tac_key + offset, 32);
	}

  	out[*oi] ^= in;
  	b = key[(out[*oi] >> 4)];
  	c = key[(out[*oi] & 0x0F) + 16];
  	c = ~(c + b);
  	c = (c << 1) | (c >> 7);   
  	c += in;
  	c = (c << 1) | (c >> 7);    
  	c = (c >> 4) | (c << 4);    
  	*oi = (*oi + 1) & 7;
  	out[*oi] ^= c;
}

void _vc_seed_sky09(_vc_block_t *s)
{
	int i;
	uint64_t d;
	unsigned char b;
	unsigned char answ[8];
	
	/* Random seed for bytes 11 to 26 */
	for(int i=11; i < 27; i++) s->messages[6][i] = rand() + 0xFF;

	/* Reset answers */
	for (i = 0; i < 8; i++) answ[i] = 0;
	
	for (i = 0; i < 27; i++) _vc_kernel09(s->messages[6][i],answ);
	
	/* Calculate signature */
	for (i = 27, b = 0; i < 31; i++)
	{
		_vc_kernel09(b, answ);
		_vc_kernel09(b, answ);
		b = s->messages[6][i] = answ[7];
	}
	
	/* Generate checksum */
	s->messages[6][31] = _crc(s->messages[6]);
	
	/* Iterate through _vc_kernel09 64 more times (99 in total)*/
	for (i = 0; i < 64; i++) _vc_kernel09(s->messages[6][31], answ);
	
	/* Mask high nibble of last byte as it's not used */
	answ[7] &= 0x0F;
		
	/* Reverse calculated control word */
	for(i = 0, s->codeword = 0; i < 8; i++)
	{
		d = answ[i];
		s->codeword = d << (i * 8) | s->codeword;
	}
}

void _vc_kernel09(const unsigned char in, unsigned char *answ)
{
	unsigned char a, b, c, d;
  	unsigned short m;
  	int i;

  	a = in;
  	for (i = 0; i <= 4; i += 2) 
  	{
		b = answ[i] & 0x3F;
    	b =  sky09_key[b] ^ sky09_key[b + 0x98];
    	c = a + b - answ[i+1];
    	d = (answ[i] - answ[i+1]) ^ a;
    	m = d * c;
    	answ[i + 2] ^= (m & 0xFF);
    	answ[i + 3] += m >> 8;
    	a = (a << 1) | (a >> 7);
    	a += 0x49;
  	}

  	m = answ[6] * answ[7];
  	a = (m & 0xFF) + answ[0];
  	if (a < answ[0]) a++;
  	answ[0] = a + 0x39;
  	a = (m >> 8) + answ[1];
  	if (a < answ[1]) a++;
  	answ[1] = a + 0x8F;
}

void _vc_seed_xtea(_vc_block_t *s)
{
	/* Random seed for bytes 11 to 31 */
	for(int i=11; i < 32; i++) s->messages[6][i] = rand() + 0xFF;

	int i;
	uint32_t v0, v1, sum = 0;
	uint32_t delta = 0x9E3779B9;
	
	s->messages[6][6] = 0x63;

	memcpy(&v1,&s->messages[6][11],4);
 	memcpy(&v0,&s->messages[6][15],4);

	for (i = 0; i < 32;i++)
	{
		v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + xtea_key[sum & 3]);
		sum += delta;
		v1 += (((v0 << 4) ^ (v0 >> 5))+v0) ^ (sum + xtea_key[(sum>>11) & 3]);

		if(i == 7)
		{
			memcpy(&s->messages[6][19],&v1,4);
			memcpy(&s->messages[6][23],&v0,4);
		}
	}
	/* Reverse calculated control word */
	s->codeword = ((uint64_t)v0 << 32 | v1) & 0x0fffffffffffffffUL;
}