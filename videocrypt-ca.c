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

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "videocrypt-ca.h"

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

static uint8_t _rotate_left(x)
{
	return (((x) << 1) | ((x) >> 7)) & 0xFF;
}

const uint8_t tac_key[] = {
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
const uint8_t sky07_key[] = {
   0x65, 0xe7, 0x71, 0x1a, 0xb4, 0x88, 0xd7, 0x76,
   0x28, 0xd0, 0x4c, 0x6e, 0x86, 0x8c, 0xc8, 0x43,
   0xa9, 0xec, 0x60, 0x42, 0x05, 0xf2, 0x3d, 0x1c,
   0x6c, 0xbc, 0xaf, 0xc3, 0x2b, 0xb5, 0xdc, 0x90,
   0xf9, 0x05, 0xea, 0x51, 0x46, 0x9d, 0xe2, 0x60,
   0x70, 0x52, 0x67, 0x26, 0x61, 0x49, 0x42, 0x09,
   0x50, 0x99, 0x90, 0xa2, 0x36, 0x0e, 0xfd, 0x39 
};

/* Videocrypt key used for Sky 09 series cards */
const uint8_t sky09_key[] = {
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

/* Key used by Multichoice Central Europe in Videocrypt 2 */
const uint8_t vc2_key[] = {
    0x58, 0x6B, 0x4D, 0x05, 0xB0, 0x69, 0x83, 0x16,
    0xA6, 0x48, 0xDE, 0x5E, 0x0B, 0xAA, 0x49, 0xA9,
    0xC6, 0xE5, 0x93, 0x1A, 0xBE, 0x56, 0x73, 0x20,
    0xFB, 0xF8, 0xCA, 0x08, 0x34, 0x29, 0x8A, 0x9B
};

static const uint32_t xtea_key[4]= {
	0x00112233, 0x44556677, 0x8899aabb, 0xccddeeff
};

/* Reverse nibbles in a byte */
static inline uint8_t _rnibble(uint8_t a)
{
	return((a >> 4) | (a << 4));
}

void _rand_vc_seed(uint8_t *message)
{
	for(int i = 12; i < 27; i++) message[i] = rand() + 0xFF;
}

/* Reverse calculated control word */
uint64_t _rev_cw(uint64_t in[8])
{
	int i;
	uint64_t cw;
	
	/* Mask high nibble of last byte as it's not used */
	in[7] &= 0x0F;
	
	for(i = 0, cw = 0; i < 8; i++)
	{
		cw = in[i] << (i * 8) | cw;
	}
	
	return(cw);
}

/* XOR "round" function to obfuscate card serial number */
void _xor_serial(uint8_t *message, int cmd, uint32_t cardserial, int byte)
{
	int i;
	uint8_t a, b, xor[4];
	
	/* XOR round function */
	a = message[1] ^ message[2];
	a = _rnibble(a);
	b = message[2];

	for (i=0; i < 4;i++)
	{
		b = _rotate_left(b);
		b += a;
		xor[i] = b;
	}
	
	message[3] =  cmd  ^ xor[0];
	message[7] =  byte ^ xor[0];
	message[8] =  ((cardserial >> 24) & 0xFF) ^ xor[1];
	message[9] =  ((cardserial >> 16) & 0xFF) ^ xor[2];
	message[10] = ((cardserial >> 8)  & 0xFF) ^ xor[3];
	message[11] = ((cardserial >> 0)  & 0xFF);
	for(i = 12; i < 27; i++) message[i] = message[11];
}

void _vc_kernel07(uint64_t *out, int *oi, const uint8_t in, int offset, int ca)
{
	uint8_t b, c;
	
	uint8_t key[32];
	
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
	c = _rotate_left(c) + in;
	c = _rotate_left(c);
	c = _rnibble(c);
	*oi = (*oi + 1) & 7;
	out[*oi] ^= c;
}

void _vc_process_p07_msg(uint8_t *message, uint64_t *cw, int ca)
{
	int offset = 0;
	int i;
	int oi = 0;	
	uint8_t b;
	
	if(ca == VC_TAC2)
	/* TAC key offsets */
	{
		if (message[1] > 0x3A) offset = 0x20;
		if (message[1] > 0x48) offset = 0x40;
	}
	else if (ca == VC_SKY7)
	/* Sky 07 key offsets */
	{
		if (message[1] > 0x32) offset = 0x08;
		if (message[1] > 0x3A) offset = 0x18;
	}
	
	/* Change date code for old TAC cards */
	if(ca == VC_TAC1) message[1] = 0x29;
	
	/* Reset answers */
	for (i = 0; i < 8; i++) cw[i] = 0;
	
	/* Run through kernel */
	for (i = 0; i < 27; i++) _vc_kernel07(cw, &oi, message[i], offset, ca);
	
	/* Calculate signature */
	for (i = 27, b = 0; i < 31; i++)
	{
		_vc_kernel07(cw, &oi, b, offset, ca);
		_vc_kernel07(cw, &oi, b, offset, ca);
		b = message[i] = cw[oi];
		oi = (oi + 1) & 7;
	}
	
	/* Generate checksum */
	message[31] = _crc(message);
	
	/* Iterate through _vc_kernel07 64 more times (99 in total)
	   Odd bug(?) in newer TAC card where checksum is always 0x0d */
	for (i = 0; i < 64; i++) _vc_kernel07(cw, &oi, (ca == VC_TAC2) ? 0x0d : message[31], offset, ca);
}

void _vc_seed_p03(_vc_block_t *s)
{
	/* Generate checksum */
	s->messages[5][31] = _crc(s->messages[5]);
}

void _vc_seed_p07(_vc_block_t *s, int ca)
{
	uint64_t cw[8];

	/* Random seed for bytes 12 to 26 */
	_rand_vc_seed(s->messages[5]);
	
	/* Process Videocrypt message */
	_vc_process_p07_msg(s->messages[5], cw, ca);
	
	/* Reverse calculated control word */
	s->codeword = _rev_cw(cw);
}

void _vc_emm_p07(_vc_block_t *s, int cmd, uint32_t cardserial)
{
	int i;
	uint64_t cw[8];
	
	int emmdata[7] = { 0xE0, 0x3F, 0x3E, 0xEC, 0x1C, 0x60, 0x0F };
	
	/* Copy EMM data into message block */
	for(i = 0; i < 7; i++) s->messages[2][i] = emmdata[i];
	
	/* Obfuscate card serial */
	_xor_serial(s->messages[2], cmd, cardserial, 0xA7);
	
	/* Process Videocrypt message */
	_vc_process_p07_msg(s->messages[2], cw, VC_SKY7);
}


void _vc_seed_vc2(_vc2_block_t *s, int ca)
{
	uint64_t cw[8];
	
	/* Random seed for bytes 12 to 26 */
	_rand_vc_seed(s->messages[5]);
	
	/* Process Videocrypt message */
	_vc_process_p07_msg(s->messages[5], cw, ca);
	
	/* Reverse calculated control word */
	s->codeword = _rev_cw(cw);
}

void _vc2_emm(_vc2_block_t *s, int cmd, uint32_t cardserial, int ca)
{
	int i;
	uint64_t cw[8];
	
	int emmdata[7] = { 0xE1,0x81,0x36,0x00,0xFF,0xFF,0xB4 };
	
	/* Copy EMM data into message block */
	for(i = 0; i < 7; i++) s->messages[2][i] = emmdata[i];
	
	/* Obfuscate card serial */
	_xor_serial(s->messages[2], cmd, cardserial, 0x81);
	
	/* Process Videocrypt message */
	_vc_process_p07_msg(s->messages[2], cw, VC2_MC);
}

void _vc_kernel09(const uint8_t in, uint64_t *out)
{
	uint8_t a, b, c, d, temp[8];
	uint16_t m;
	int i;
	
	for(i = 0; i < 8; i++) temp[i] = out[i];
	
	a = in;
	for (i = 0; i <= 4; i += 2)
	{
		b = temp[i] & 0x3F;
		b =  sky09_key[b] ^ sky09_key[b + 0x98];
		c = a + b - temp[i + 1];
		d = ((uint8_t) (temp[i] - temp[i + 1])) ^ a;
		m = d * c;
		temp[i + 2] ^= (m & 0xFF);
		temp[i + 3] += m >> 8;
		a = _rotate_left(a) + 0x49;
	}
	
	m = temp[6] * temp[7];
	a = (m & 0xFF) + temp[0];
	if (a < temp[0]) a++;
	temp[0] = a + 0x39;
	a = (m >> 8) + temp[1];
	if (a < temp[1]) a++;
	temp[1] = a + 0x8F;
	
	for(i = 0; i < 8; i++) out[i] = temp[i];
}

void _vc_process_p09_msg(uint8_t *message, uint64_t *cw)
{
	int i;
	uint8_t b;
	
	/* Reset CW */
	for (i = 0; i < 8; i++) cw[i] = 0;
	
	for (i = 0; i < 27; i++) _vc_kernel09(message[i], cw);
	
	/* Calculate signature */
	for (i = 27, b = 0; i < 31; i++)
	{
		_vc_kernel09(b, cw);
		_vc_kernel09(b, cw);
		b = message[i] = cw[7];
	}
	
	/* Generate checksum */
	message[31] = _crc(message);
	
	/* Iterate through _vc_kernel09 64 more times (99 in total)*/
	for (i = 0; i < 64; i++) _vc_kernel09(message[31], cw);
	
	/* Mask high nibble of last byte as it's not used */
	cw[7] &= 0x0F;
}

void _vc_seed_p09(_vc_block_t *s)
{
	uint64_t cw[8];
	
	/* Random seed for bytes 12 to 26 */
	_rand_vc_seed(s->messages[5]);
	
	/* Process Videocrypt message */
	_vc_process_p09_msg(s->messages[5], cw);
	
	/* Reverse calculated control word */
	s->codeword = _rev_cw(cw);
}

void _vc_emm_p09(_vc_block_t *s, int cmd, uint32_t cardserial)
{
	int i;
	uint64_t cw[8];
	
	int emmdata[7] = { 0xE1, 0x52, 0x01, 0x25, 0x80, 0xFF, 0x20 };
	
	/* Copy EMM data into message block */
	for(i = 0; i < 7; i++) s->messages[2][i] = emmdata[i];
	
	/* Obfuscate card serial */
	_xor_serial(s->messages[2], cmd, cardserial, 0xA9);
	
	/* Process Videocrypt message */
	_vc_process_p09_msg(s->messages[2], cw);
}

void _vc_seed_xtea(_vc_block_t *s)
{
	/* Random seed for bytes 11 to 31 */
	for(int i=11; i < 32; i++) s->messages[5][i] = rand() + 0xFF;
	
	int i;
	uint32_t v0, v1, sum = 0;
	uint32_t delta = 0x9E3779B9;
	
	s->messages[5][6] = 0x63;
	
	memcpy(&v1, &s->messages[5][11], 4);
	memcpy(&v0, &s->messages[5][15], 4);
	
	for (i = 0; i < 32;i++)
	{
		v0  += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + xtea_key[sum & 3]);
		sum += delta;
		v1  += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + xtea_key[(sum >> 11) & 3]);
	
		if(i == 7)
		{
			memcpy(&s->messages[5][19], &v1, 4);
			memcpy(&s->messages[5][23], &v0, 4);
		}
	}
	
	/* Reverse calculated control word */
	s->codeword = ((uint64_t) v0 << 32 | v1) & 0x0FFFFFFFFFFFFFFFUL;
}

/* Code below is to support seed generation for "dumb"/memory card */
/* Thanks to Phil Pemberton for providing the required information */
/* https://github.com/philpem/hacktv */

/* Code table at address 0x1421 from verifier */
const uint8_t tab_1421[8] = {
	0x59, 0x2B, 0x71, 0x22, 0xCF, 0xB7, 0x33, 0x4F	
};

/* The four moduli and also a 256-byte data table */
const uint8_t moduli[256] = {
	0xB1, 0xFD, 0x91, 0x2C, 0x6D, 0xB8, 0xB6, 0xBE,
	0x15, 0x08, 0x0D, 0xE2, 0x83, 0xB1, 0xE8, 0x0B,
	0x36, 0xB0, 0x47, 0xEA, 0xA1, 0x10, 0xA7, 0x8E,
	0xAA, 0x2E, 0x94, 0xC8, 0x47, 0x41, 0xFE, 0x87,
	0x7E, 0xEC, 0x67, 0x45, 0xAB, 0x89, 0x84, 0xA5,
	0xEF, 0xCD, 0x23, 0x01, 0x67, 0x45, 0x2D, 0x46,
	0xAB, 0xA9, 0xEF, 0xCD, 0x24, 0x93, 0x02, 0x67,
	0x1B, 0x4F, 0x81, 0x95, 0xA7, 0x01, 0x00, 0x01,
	
	0x29, 0x9F, 0xC9, 0x85, 0x19, 0xB9, 0x53, 0x53,
	0x92, 0x52, 0x90, 0x5A, 0x44, 0x2D, 0xCA, 0xD4,
	0x90, 0x8D, 0x3A, 0xAD, 0xFB, 0x2B, 0x00, 0x9D,
	0xE4, 0x0C, 0xB8, 0x81, 0x28, 0xBF, 0xE9, 0x0B,
	0x85, 0x7C, 0xAD, 0x90, 0x41, 0xE7, 0x7A, 0xBA,
	0x9D, 0xEF, 0x7E, 0x83, 0x82, 0x0D, 0x0A, 0xCE,
	0x64, 0x77, 0x83, 0x1E, 0x1D, 0x80, 0x26, 0xF5,
	0x48, 0xA4, 0x39, 0x6E, 0xC3, 0x01, 0x00, 0x01,
	
	0x0D, 0x2D, 0xC9, 0x25, 0x51, 0x4A, 0xA3, 0x85,
	0x8B, 0xDC, 0xC7, 0x25, 0x40, 0x0C, 0xB8, 0x61,
	0x0C, 0xF9, 0xC1, 0x21, 0xBD, 0x3D, 0x57, 0x6D,
	0x6C, 0x71, 0x2F, 0xA4, 0xCC, 0x93, 0x40, 0x37,
	0xDE, 0x32, 0x39, 0x65, 0xC1, 0x8D, 0x63, 0x6A,
	0x49, 0xB6, 0xE1, 0xD0, 0x73, 0x5E, 0xDE, 0x9C,
	0x12, 0xA7, 0xC3, 0x34, 0x5E, 0x38, 0x8C, 0x73,
	0x05, 0x4E, 0x63, 0x41, 0x0A, 0x01, 0x00, 0x01,
	
	0xE5, 0x20, 0x5B, 0xD5, 0x56, 0xD1, 0x9B, 0xA9,
	0xA5, 0x54, 0xB7, 0x83, 0x16, 0xDE, 0x36, 0x0B,
	0xD6, 0x03, 0x58, 0x1B, 0xE0, 0x0D, 0x36, 0x72,
	0xAD, 0x6B, 0x69, 0xDA, 0xD9, 0x99, 0x16, 0xBC,
	0xCB, 0x24, 0xF6, 0x65, 0xB4, 0x45, 0xA6, 0xBB,
	0xED, 0x53, 0x3E, 0xB0, 0xF7, 0xB8, 0xF5, 0xEA,
	0xA6, 0xB7, 0xAF, 0x64, 0xED, 0xA2, 0xE7, 0xFE,
	0xC2, 0x57, 0xC4, 0xD1, 0x0B, 0x01, 0x00, 0x01
};

void _hash_ppv(uint64_t *answ, size_t len)
{
	int i, j, m;
	/* Generate seed */	
	for (i = 0; i < 8; i++) 
	{
		for (j = 1; j != len; j++) 
		{
			m = tab_1421[i] + answ[j - 1] & 0xFF;
			answ[j] = _rotate_left(answ[j] ^ moduli[m]);
		}
		answ[0] ^= answ[len-1];
	}
}

void _vc_seed_ppv(_vc_block_t *s, uint8_t _ppv_card_data[7])
{
	int i;
	
	/* Temporary buffers */
	uint64_t   msg[32];
	uint64_t serial[5];
	
	/* Random bytes */
	s->messages[0][21] = rand() + 0xFF;
	s->messages[0][22] = rand() + 0xFF;
	
	/* Copy data into buffers */
	for(i = 0; i < 31; i++)    msg[i] = s->messages[0][i];
	for(i = 0; i <  5; i++) serial[i] = _ppv_card_data[i];
	
	_hash_ppv(serial, 5);
	
	msg[1] ^= serial[0] ^ _ppv_card_data[5];
	msg[2] ^= serial[1] ^ _ppv_card_data[6];
	
	_hash_ppv(&msg[1], 22);
	
	/* Mask high nibble of last byte as it's not used */
	msg[8] &= 0x0F;
	
	/* Reverse calculated control word */
	for(i = 0, s->codeword = 0; i < 8; i++)	s->codeword = msg[i + 1] << (i * 8) | s->codeword;
}