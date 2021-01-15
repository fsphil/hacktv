/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2020 Alex L. James                                          */
/* Copyright 2020 Philip Heron <phil@sanslogic.co.uk>                    */
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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "video.h"

#define ECM  0
#define HASH 1

#define EC_M 0
#define EC_S 3
#define EC_3DES 4

#define ROTATE_LEFT 1
#define ROTATE_RIGHT 2

#define IP_DIM 64
#define IPP_DIM 64
#define E_DIM 48
#define S_BOXES 8
#define S_DIM 64
#define P_DIM 32
#define PC2_DIM 48
#define LS_DIM 16

/* Data for EC controlled-access decoding */
const static ec_mode_t _ec_modes[] = {
	{ "rdv",      EC_S, { 0xFE, 0x6D, 0x9A, 0xBB, 0xEB, 0x97, 0xFB }, { 0x00, 0x2D, 0x93 }, { "01/11/1995" }, { 0xFF, 0x00 }, "RDV (S2)" },
	{ "tvs",      EC_S, { 0x5C, 0x8B, 0x11, 0x2F, 0x99, 0xA8, 0x2C }, { 0x00, 0x2B, 0x50 }, { "01/11/1995" }, { 0x00, 0x01 }, "TV-S (S2)" },
	{ "ctvs",     EC_S, { 0x17, 0x38, 0xFA, 0x8A, 0x84, 0x5A, 0x5E }, { 0x00, 0x2B, 0x20 }, { "01/11/1995" }, { 0x00, 0x01 }, "CTV (S)" },
	{ "ctv",      EC_M, { 0x84, 0x66, 0x30, 0xE4, 0xDA, 0xFA, 0x23 }, { 0x00, 0x04, 0x38 }, { "01/11/1995" }, { 0xFF, 0x00 }, "CTV (M)" },
	{ "tvplus",   EC_M, { 0x12, 0x06, 0x28, 0x3A, 0x4B, 0x1D, 0xE2 }, { 0x00, 0x2C, 0x08 }, { "01/11/1995" }, { 0x04, 0x00 }, "TV Plus (M)" },
	{ "tv1000",   EC_M, { 0x48, 0x63, 0xC5, 0xB3, 0xDA, 0xE3, 0x29 }, { 0x00, 0x04, 0x18 }, { "01/11/1995" }, { 0x05, 0x04 }, "TV1000 (M)" },
	{ "tv3update",EC_M, { 0xE9, 0xF3, 0x34, 0x36, 0xB0, 0xBB, 0xF8 }, { 0x00, 0x04, 0x0C }, { "01/11/1995" }, { 0x05, 0x04 }, "TV3 (AU - M)" },
	{ "filmnet",  EC_M, { 0x21, 0x12, 0x31, 0x35, 0x8A, 0xC3, 0x4F }, { 0x00, 0x28, 0x08 }, { "01/11/1995" }, { 0x05, 0x00 }, "FilmNet (M)" },
	{ "nrk",      EC_S, { 0xE7, 0x19, 0x5B, 0x7C, 0x47, 0xF4, 0x66 }, { 0x47, 0x52, 0x00 }, { "01/11/1995" }, { 0x00, 0x02 }, "NRK (S)" },
	{ "cplus", EC_3DES, { 0x34, 0x51, 0x85, 0xCE, 0x42, 0x07, 0x4B, 
	                      0xB4, 0xA0, 0xD9, 0x3B, 0x94, 0x28, 0xC9 }, { 0x00, 0x2B, 0x1C }, { "01/11/1995" }, { 0x00, 0x01 }, "Canal+ (3DES)" },
	{ NULL } 
};

/* Data for EC controlled-access EMMs */
const static em_mode_t _em_modes[] = {
	{ "tv3update",EC_M, { 0x99, 0xCF, 0xCA, 0x13, 0x7A, 0x53, 0x6D }, { 0x00, 0x04, 0x04 }, { 0x70, 0x31, 0x12 }, { 0, 0, 0, 0 }, EMMS },
	{ NULL } 
};

/* Initial permutation for Eurocrypt-S2/3DES */
static const uint8_t _ip[IP_DIM] = {
	58, 50, 42, 34, 26, 18, 10, 2,
	60, 52, 44, 36, 28, 20, 12, 4,
	62, 54, 46, 38, 30, 22, 14, 6,
	64, 56, 48, 40, 32, 24, 16, 8,
	57, 49, 41, 33, 25, 17,  9, 1,
	59, 51, 43, 35, 27, 19, 11, 3,
	61, 53, 45, 37, 29, 21, 13, 5,
	63, 55, 47, 39, 31, 23, 15, 7,
};

/* Inverse/final permutation for Eurocrypt-S2/3DES */
static const uint8_t _ipp[IPP_DIM] = {
	40, 8, 48, 16, 56, 24, 64, 32,
	39, 7, 47, 15, 55, 23, 63, 31,
	38, 6, 46, 14, 54, 22, 62, 30,
	37, 5, 45, 13, 53, 21, 61, 29,
	36, 4, 44, 12, 52, 20, 60, 28,
	35, 3, 43, 11, 51, 19, 59, 27,
	34, 2, 42, 10, 50, 18, 58, 26,
	33, 1, 41,  9, 49, 17, 57, 25,
};

static const uint8_t _exp[E_DIM] = {
	32,  1,  2,  3,  4,  5,
	 4,  5,  6,  7,  8,  9,
	 8,  9, 10, 11, 12, 13,
	12, 13, 14, 15, 16, 17,
	16, 17, 18, 19, 20, 21,
	20, 21, 22, 23, 24, 25,
	24, 25, 26, 27, 28, 29,
	28, 29, 30, 31, 32,  1
};

static const uint8_t _sb[S_BOXES][S_DIM] = {
	{ 0xE, 0x0, 0x4, 0xF, 0xD, 0x7, 0x1, 0x4,
	  0x2, 0xE, 0xF, 0x2, 0xB, 0xD, 0x8, 0x1,
	  0x3, 0xA, 0xA, 0x6, 0x6, 0xC, 0xC, 0xB,
	  0x5, 0x9, 0x9, 0x5, 0x0, 0x3, 0x7, 0x8,
	  0x4, 0xF, 0x1, 0xC, 0xE, 0x8, 0x8, 0x2,
	  0xD, 0x4, 0x6, 0x9, 0x2, 0x1, 0xB, 0x7,
	  0xF, 0x5, 0xC, 0xB, 0x9, 0x3, 0x7, 0xE,
	  0x3, 0xA, 0xA, 0x0, 0x5, 0x6, 0x0, 0xD
	},
	{ 0xF, 0x3, 0x1, 0xD, 0x8, 0x4, 0xE, 0x7,
	  0x6, 0xF, 0xB, 0x2, 0x3, 0x8, 0x4, 0xE,
	  0x9, 0xC, 0x7, 0x0, 0x2, 0x1, 0xD, 0xA,
	  0xC, 0x6, 0x0, 0x9, 0x5, 0xB, 0xA, 0x5,
	  0x0, 0xD, 0xE, 0x8, 0x7, 0xA, 0xB, 0x1,
	  0xA, 0x3, 0x4, 0xF, 0xD, 0x4, 0x1, 0x2,
	  0x5, 0xB, 0x8, 0x6, 0xC, 0x7, 0x6, 0xC,
	  0x9, 0x0, 0x3, 0x5, 0x2, 0xE, 0xF, 0x9
	},
	{ 0xA, 0xD, 0x0, 0x7, 0x9, 0x0, 0xE, 0x9,
	  0x6, 0x3, 0x3, 0x4, 0xF, 0x6, 0x5, 0xA,
	  0x1, 0x2, 0xD, 0x8, 0xC, 0x5, 0x7, 0xE,
	  0xB, 0xC, 0x4, 0xB, 0x2, 0xF, 0x8, 0x1,
	  0xD, 0x1, 0x6, 0xA, 0x4, 0xD, 0x9, 0x0,
	  0x8, 0x6, 0xF, 0x9, 0x3, 0x8, 0x0, 0x7,
	  0xB, 0x4, 0x1, 0xF, 0x2, 0xE, 0xC, 0x3,
	  0x5, 0xB, 0xA, 0x5, 0xE, 0x2, 0x7, 0xC
	},
	{ 0x7, 0xD, 0xD, 0x8, 0xE, 0xB, 0x3, 0x5,
	  0x0, 0x6, 0x6, 0xF, 0x9, 0x0, 0xA, 0x3,
	  0x1, 0x4, 0x2, 0x7, 0x8, 0x2, 0x5, 0xC,
	  0xB, 0x1, 0xC, 0xA, 0x4, 0xE, 0xF, 0x9,
	  0xA, 0x3, 0x6, 0xF, 0x9, 0x0, 0x0, 0x6,
	  0xC, 0xA, 0xB, 0x1, 0x7, 0xD, 0xD, 0x8,
	  0xF, 0x9, 0x1, 0x4, 0x3, 0x5, 0xE, 0xB,
	  0x5, 0xC, 0x2, 0x7, 0x8, 0x2, 0x4, 0xE
	},
	{ 0x2, 0xE, 0xC, 0xB, 0x4, 0x2, 0x1, 0xC,
	  0x7, 0x4, 0xA, 0x7, 0xB, 0xD, 0x6, 0x1,
	  0x8, 0x5, 0x5, 0x0, 0x3, 0xF, 0xF, 0xA,
	  0xD, 0x3, 0x0, 0x9, 0xE, 0x8, 0x9, 0x6,
	  0x4, 0xB, 0x2, 0x8, 0x1, 0xC, 0xB, 0x7,
	  0xA, 0x1, 0xD, 0xE, 0x7, 0x2, 0x8, 0xD,
	  0xF, 0x6, 0x9, 0xF, 0xC, 0x0, 0x5, 0x9,
	  0x6, 0xA, 0x3, 0x4, 0x0, 0x5, 0xE, 0x3
	},
	{ 0xC, 0xA, 0x1, 0xF, 0xA, 0x4, 0xF, 0x2,
	  0x9, 0x7, 0x2, 0xC, 0x6, 0x9, 0x8, 0x5,
	  0x0, 0x6, 0xD, 0x1, 0x3, 0xD, 0x4, 0xE,
	  0xE, 0x0, 0x7, 0xB, 0x5, 0x3, 0xB, 0x8,
	  0x9, 0x4, 0xE, 0x3, 0xF, 0x2, 0x5, 0xC,
	  0x2, 0x9, 0x8, 0x5, 0xC, 0xF, 0x3, 0xA,
	  0x7, 0xB, 0x0, 0xE, 0x4, 0x1, 0xA, 0x7,
	  0x1, 0x6, 0xD, 0x0, 0xB, 0x8, 0x6, 0xD
	},
	{ 0x4, 0xD, 0xB, 0x0, 0x2, 0xB, 0xE, 0x7,
	  0xF, 0x4, 0x0, 0x9, 0x8, 0x1, 0xD, 0xA,
	  0x3, 0xE, 0xC, 0x3, 0x9, 0x5, 0x7, 0xC,
	  0x5, 0x2, 0xA, 0xF, 0x6, 0x8, 0x1, 0x6,
	  0x1, 0x6, 0x4, 0xB, 0xB, 0xD, 0xD, 0x8,
	  0xC, 0x1, 0x3, 0x4, 0x7, 0xA, 0xE, 0x7,
	  0xA, 0x9, 0xF, 0x5, 0x6, 0x0, 0x8, 0xF,
	  0x0, 0xE, 0x5, 0x2, 0x9, 0x3, 0x2, 0xC
	},
	{ 0xD, 0x1, 0x2, 0xF, 0x8, 0xD, 0x4, 0x8,
	  0x6, 0xA, 0xF, 0x3, 0xB, 0x7, 0x1, 0x4,
	  0xA, 0xC, 0x9, 0x5, 0x3, 0x6, 0xE, 0xB,
	  0x5, 0x0, 0x0, 0xE, 0xC, 0x9, 0x7, 0x2,
	  0x7, 0x2, 0xB, 0x1, 0x4, 0xE, 0x1, 0x7,
	  0x9, 0x4, 0xC, 0xA, 0xE, 0x8, 0x2, 0xD,
	  0x0, 0xF, 0x6, 0xC, 0xA, 0x9, 0xD, 0x0,
	  0xF, 0x3, 0x3, 0x5, 0x5, 0x6, 0x8, 0xB
	}
};

static const uint8_t _perm[P_DIM] = {
	16,  7, 20, 21,
	29, 12, 28, 17,
	 1, 15, 23, 26,
	 5, 18, 31, 10,
	 2,  8, 24, 14,
	32, 27,  3,  9,
	19, 13, 30,  6,
	22, 11,  4, 25
};

static const uint8_t _pc2[PC2_DIM] = {
	14, 17, 11, 24,  1,  5,
	 3, 28, 15,  6, 21, 10,
	23, 19, 12,  4, 26,  8,
	16,  7, 27, 20, 13,  2,
	41, 52, 31, 37, 47, 55,
	30, 40, 51, 45, 33, 48,
	44, 49, 39, 56, 34, 53,
	46, 42, 50, 36, 29, 32
};

static const uint8_t _lshift[LS_DIM] = {
	1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

static void _permute_ec(uint8_t *data, const uint8_t *pr, int n)
{
	uint8_t pin[8];
	int i, j, k, t;
	
	for(i = 0, k = 0; k < n; i++)
	{
		int p = 0;
		
		for(j = 7; j >= 0; j--, k++)
		{
			t = pr[k] - 1;
			p = (p << 1) + ((data[t >> 3]) >> (7 - (t & 7)) & 1);
		}
		
		pin[i] = p;
	}
	
	memcpy(data, pin, 8);
}

uint16_t _get_ec_date(const char *dtm, int mode)
{
	int day, mon, year;
	
	uint16_t date;
	
	sscanf(dtm, "%d/%d/%d", &day, &mon, &year);
	
	/* EC-M and EC-S2/3DES have different date byte structures */
	if(mode == EC_M)
	{
		date  = (year - 1980) << 9; /* Year - first 7 bits */
		date |= mon << 5;           /* Month - next 4 bits */
		date |= day << 0;           /* Day - next 5 bits */
	}
	else
	{
		date  = (year - 1990) << 12; /* Year - first 4 bits */
		date |= mon << 8;            /* Month - next 4 bits */
		date |= day << 0;            /* Day - 8 bits */
	}
	
	return (date);
}

static uint64_t _ec_des_f(uint64_t r, uint8_t *k2)
{
	int i, k;
	uint64_t s = 0, result = 0;
	
	for(i = 0, k = 0; i < 8; i++)
	{
		int j;
		uint8_t v = 0;
		
		/* The expansion E (R1) */
		for(j = 0; j < 6; j++, k++)
		{
			v |= (r >> (32 - _exp[k]) & 1) << (5 - j);
		}
		
		/* Create R2 */
		v ^= k2[i];
		
		/* The S-boxes */
		s |= (uint64_t) _sb[i][v] << (28 - 4 * i);
	}
	
	/* The permutation P (R3) */
	for(i = 0; i < 32; i++)
	{
		result |= (s >> (32 - _perm[i]) & 1) << (31 - i);
	}
	
	return(result);
}

static void _key_rotate_ec(uint64_t *c, uint64_t *d, int dir, int iter)
{
	int i;
	
	/* Rotate left (decryption) */
	if(dir == ROTATE_LEFT)
	{
		for(i = 0; i < _lshift[iter]; i++)
		{
			*c = ((*c << 1) ^ (*c >> 27)) & 0xFFFFFFFL;
			*d = ((*d << 1) ^ (*d >> 27)) & 0xFFFFFFFL;
		}
	}
	/* Rotate right (encryption) */
	else
	{
		for(i = 0; i < _lshift[15 - iter]; i++)
		{
			*c = ((*c >> 1) ^ (*c << 27)) & 0xFFFFFFFL;
			*d = ((*d >> 1) ^ (*d << 27)) & 0xFFFFFFFL;
		}
	}
}

static void _key_exp(uint64_t *c, uint64_t *d, uint8_t *k2)
{
	int j, k;
	
	/* Key expansion */
	for(j = 0, k = 0; j < 8; j++)
	{
		int t;
		
		k2[j] = 0;
		
		for(t = 0; t < 6; t++, k++)
		{
			if(_pc2[k] < 29)
			{
				k2[j] |= (*c >> (28 - _pc2[k]) & 1) << (5 - t);
			}
			else
			{
				k2[j] |= (*d >> (56 - _pc2[k]) & 1) << (5 - t);
			}
		}
	}
}

static void _eurocrypt(uint8_t *data, const uint8_t *key, int desmode, int emode, int rnd)
{
	int i;
	uint64_t r, l, c, d, s;
	
	/* Key preparation. Split key into two halves */
	c = ((uint64_t) key[0] << 20)
	  ^ ((uint64_t) key[1] << 12)
	  ^ ((uint64_t) key[2] << 4)
	  ^ ((uint64_t) key[3] >> 4);
	
	d = ((uint64_t) (key[3] & 0x0F) << 24)
	  ^ ((uint64_t) key[4] << 16)
	  ^ ((uint64_t) key[5] << 8)
	  ^ ((uint64_t) key[6] << 0);
	
	/* Initial permutation for Eurocrypt S2/3DES  - always do this */
	if(emode)
	{
		_permute_ec(data, _ip, 64);
	}
	
	/* Control word preparation. Split CW into two halves. */
	for(i = 3, l = 0, r = 0; i >= 0; i--)
	{
		l ^= (uint64_t) data[3 - i] << (8 * i);
		r ^= (uint64_t) data[7 - i] << (8 * i);
	}
	
	/* 16 iterations */
	for(i = 0; i < 16; i++)
	{
		uint64_t r3;
		uint8_t k2[8];
		
		switch (emode) {
			/* If mode is not valid, abort -- this is a bug! */
			default:
				fprintf(stderr, "_eurocrypt: BUG: invalid encryption mode!!!\n");
				assert(0);
				break;

			/* EC-M */
			case EC_M:
				{
					if(desmode == HASH)
					{
						_key_rotate_ec(&c, &d, ROTATE_LEFT, i);
					}
					
					/* Key expansion */
					_key_exp(&c, &d, k2);
					
					/* One DES round */
					s = _ec_des_f(r, k2);
					
					if(desmode != HASH)
					{
						_key_rotate_ec(&c, &d, ROTATE_RIGHT, i);
					}
					
					/* Swap first two bytes if it's a hash routine */
					if(desmode == HASH)
					{
						s = ((s >> 8) & 0xFF0000L) | ((s << 8) & 0xFF000000L) | (s & 0x0000FFFFL);
					}
				}
				break;

			/* EC-S2 */
			case EC_S:
				{
					/* Key rotation */
					_key_rotate_ec(&c, &d, ROTATE_LEFT, i);
					
					/* Key expansion */
					_key_exp(&c, &d, k2);
					
					/* One DES round */
					s = _ec_des_f(r, k2);
					
				}
				break;

			/* EC-3DES */
			case EC_3DES:
				{
					/* Key rotation */
					if(rnd != 2)
					{
						_key_rotate_ec(&c, &d, ROTATE_LEFT, i);
					}
					
					/* Key expansion */
					_key_exp(&c, &d, k2);
					
					/* One DES round */
					s = _ec_des_f(r, k2);
					
					if(rnd == 2)
					{
						_key_rotate_ec(&c, &d, ROTATE_RIGHT, i);
					}
				}
				break;
		}
		
		/* Rotate halves around */
		r3 = l ^ s;
		l = r;
		r = r3;
	}
	
	/* Put everything together */
	for(i = 3; i >= 0; i--)
	{
		data[3 - i] = r >> (8 * i);
		data[7 - i] = l >> (8 * i);
	}
	
	/* Final permutation for Eurocrypt S2/3DES */
	if(emode)
	{
		_permute_ec(data, _ipp, 64);
	}
}

static void _calc_ec_hash(uint8_t *hash, uint8_t *msg, int mode, int msglen, const uint8_t *key)
{
	int i, r;
	
	/* Iterate through data */
	for(i = 0; i < msglen; i++)
	{
		hash[(i % 8)] ^= msg[i];
		
		if(i % 8 == 7)
		{
			/* Three rounds for 3DES mode, one round for others */
			for(r = 0; r < (mode != EC_3DES ? 1 : 3); r++) 
			{
				/* Use second key on second round in 3DES */
				_eurocrypt(hash, key + (r != 1 ? 0 : 7), HASH, mode, r + 1);
			}
		}
	}
	
	/* Final interation - EC-M only */
	if(mode == EC_M)
	{
		_eurocrypt(hash, key, HASH, mode, 1);
	}
}

static void _build_ecm_hash_data(uint8_t *hash, const eurocrypt_t *e, int x)
{
	uint8_t msg[MAC_PAYLOAD_BYTES];
	int msglen;
	
	/* Clear hash memory */
	memset(hash, 0, 8);
	
	/* Build the hash message */
	msglen = 0;
	
	/* EC-S2 and EC-3DES */
	if(e->mode->emode)
	{
		/* Copy PPID */
		memcpy(msg, e->ecm_pkt + 5, 3);
		msglen += 3;
		
		/* Third byte of PPUA contains key index, which needs to be masked for hashing */
		msg[2] &= 0xF0;
		
		/* Copy E1 04 data + 0xEA */
		memcpy(msg + msglen, e->ecm_pkt + (x - 24), 5);
		msglen += 5;
		
		/* Copy CWs */
		memcpy(msg + msglen, e->ecw[0], 8); msglen += 8;
		memcpy(msg + msglen, e->ecw[1], 8); msglen += 8;
	}
	else
	{
		/* EC-M */
		memcpy(msg, e->ecm_pkt + 8, x - 10);
		msglen += x - 10;
	}
	
	/* Calculate hash */
	_calc_ec_hash(hash, msg, e->mode->emode, msglen, e->mode->key);
}

static void _build_emmg_hash_data(uint8_t *hash, eurocrypt_t *e)
{
	uint8_t msg[MAC_PAYLOAD_BYTES];
	int msglen;
	
	/* Clear hash memory */
	memset(hash, 0, 8);
	
	/* Copy card's Shared Address into hash buffer */
	hash[5] = e->emmode->sa[2];
	hash[6] = e->emmode->sa[1];
	hash[7] = e->emmode->sa[0];
	
	/* Do the initial hashing of the buffer */
	_eurocrypt(hash, e->emmode->key, HASH, e->mode->emode, 1);
	
	/* Build the hash data */
	msglen = 0;

	/* Copy ADF into data buffer */
	msg[msglen++] = 0x9e;
	msg[msglen++] = 0x20;
	memcpy(msg + msglen, e->emms_pkt + 6, 32); msglen += 32;
	
	_calc_ec_hash(hash, msg, e->mode->emode, msglen, e->emmode->key);
	
	msglen = 0;
	
	/* Copy entitlements into data buffer */
	memcpy(msg + msglen, e->emmg_pkt + 8, 15); msglen += 15;
	
	_calc_ec_hash(hash, msg, e->mode->emode, msglen, e->emmode->key);
}

static void _build_emmu_hash_data(uint8_t *hash, eurocrypt_t *e, int x)
{
	uint8_t msg[MAC_PAYLOAD_BYTES];
	int msglen;
	
	/* Clear hash memory */
	memset(hash, 0, 8);
	
	/* Copy card's Unique Address into hash buffer */
	hash[4] = e->emmode->ua[3];
	hash[5] = e->emmode->ua[2];
	hash[6] = e->emmode->ua[1];
	hash[7] = e->emmode->ua[0];
	
	/* Do the initial hashing of the buffer */
	_eurocrypt(hash, e->emmode->key, HASH, e->mode->emode, 1);
	
	/* Build the hash data */
	msglen = 0;
	
	/* Copy entitlements into data buffer */
	memcpy(msg + msglen, e->emmu_pkt + 8, x - 10); msglen += x - 10;
	
	_calc_ec_hash(hash, msg, e->mode->emode, msglen, e->emmode->key);
}

static void _encrypt_opkey(uint8_t *data, eurocrypt_t *e)
{
	uint8_t *emm = malloc(sizeof(e->mode->key) / sizeof(uint8_t));
	
	memset(emm, 0, 8);
	memcpy(emm, e->mode->key, 7);
	
	_eurocrypt(emm, e->emmode->key, ECM, e->emmode->emode, 1);
	
	memcpy(data, emm, 8);
}

static uint8_t _update_ecm_packet(eurocrypt_t *e, int t)
{
	int x;
	uint16_t b;
	uint8_t *pkt = e->ecm_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES * 2);
	
	/* PT - always 0x00 for ECM */
	x = 0;
	pkt[x++] = ECM;
	
	/* Command Identifier, CI */
	b  = 0x20 << 2;		/* Crypto-algo type - always 0x20 for Eurocrypt PC2 implementation */
	b |= 1 << 1;		/* Format bit - always 1 */
	b |= t << 0;		/* Toggle bit */
	pkt[x++] = b;
	
	/* Command Length Indicator, CLI -- updated later */
	pkt[x++] = 0;
	
	/* PPID */
	pkt[x++] = 0x90; /* PI */
	pkt[x++] = 0x03; /* LI */
	memcpy(&pkt[x], e->mode->ppid, 3); x += 3;
	
	/* Undocumented meaning of this byte but appears in captured logs from live transmissions */
	pkt[x++] = 0xDF; /* PI */
	pkt[x++] = 0x00; /* LI */
	
	/* CDATE + THEME/LEVEL */
	pkt[x++] = 0xE1; /* PI */
	pkt[x++] = 0x04; /* LI */
	uint16_t d = _get_ec_date(e->mode->date, e->mode->emode);
	pkt[x++] = (d & 0xFF00) >> 8;
	pkt[x++] = (d & 0x00FF) >> 0;
	memcpy(&pkt[x], e->mode->theme, 2); x += 2;
	
	/* ECW/OCW */
	pkt[x++] = 0xEA; /* PI */
	pkt[x++] = 0x10; /* LI */
	memcpy(&pkt[x], e->ecw[0], 8); x += 8; /* ECW */
	memcpy(&pkt[x], e->ecw[1], 8); x += 8; /* OCW */
	
	/* HASH */
	pkt[x++] = 0xF0; /* PI */
	pkt[x++] = 0x08; /* LI */
	_build_ecm_hash_data(&pkt[x], e, x); x += 8;
	memcpy(e->ecm_hash, &pkt[x-8], 8);
	
	/* Update the CI command length */
	pkt[2] = x - 3;
	
	return (x / ECM_PAYLOAD_BYTES);
}

static void _update_emms_packet(eurocrypt_t *e)
{
	int x;
	uint16_t b;
	uint8_t *pkt = e->emms_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES);
	
	x = 0;
	
	/* Packet Type */
	pkt[x++] = EMMS;
	
	/* Shared Address - reversed */
	memcpy(&pkt[x], e->emmode->sa, 3); x += 3;
	
	/* Command Identifier, CI */
	b  = 0x20 << 2;		/* Crypto-algo type - always 0x20 for Eurocrypt PC2 implementation */
	b |= 0 << 1;		/* Format bit - 0: fixed, 1: variable */
	b |= 0 << 0;		/* ADF - clear */
	pkt[x++] = b;
	
	/* Command Length Indicator, CLI */
	pkt[x++] = 0x28;
	
	/* ADF */
	memset(&pkt[x], 0xFF, 32); x += 32;
	
	/* EMM hash */
	_build_emmg_hash_data(&pkt[x], e); x += 8;
	memcpy(e->emm_hash, &pkt[x - 8], 8);
	
	mac_golay_encode(pkt + 1, 30);
}

static uint8_t _update_emmg_packet(eurocrypt_t *e, int t)
{
	int x;
	uint16_t b;
	uint8_t *pkt = e->emmg_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES * 2);
	
	/* Packet Type */
	x = 0;
	pkt[x++] = EMMG;
	
	/* Command Identifier, CI */
	b  = 0x20 << 2;		/* Crypto-algo type - always 0x20 for Eurocrypt PC2 implementation */
	b |= 1 << 1;		/* Format bit - 0: fixed, 1: variable */
	b |= t << 0;		/* Toggle */
	pkt[x++] = b;
	
	/* Command Length Indicator, CLI -- updated later */
	pkt[x++] = 0;
	
	/* PPID */
	pkt[x++] = 0x90; /* PI */
	pkt[x++] = 0x03; /* LI */
	/* Provider ID and M-key to use for decryption of op-key */
	memcpy(&pkt[x], e->emmode->ppid, 3); x += 3;
	
	/* IDUP */
	pkt[x++] = 0xa1;
	pkt[x++] = 0x03;
	/* Provider ID and op-key to update */
	memcpy(&pkt[x], e->mode->ppid, 3); x += 3;
	
	pkt[x++] = 0xef;
	pkt[x++] = 0x08;
	_encrypt_opkey(&pkt[x], e); x += 8;
	
	/* Update the CI command length */
	pkt[2] = x - 3;
	
	return(x / ECM_PAYLOAD_BYTES);
}

static uint8_t _update_emmu_packet(eurocrypt_t *e, int t)
{
	int x;
	uint16_t b;
	uint8_t *pkt = e->emmu_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES * 2);
	
	/* Packet Type */
	x = 0;
	pkt[x++] = EMMU;

	/* Unique Address - reversed */
	memcpy(&pkt[x], e->emmode->ua, 4); x += 4;
	
	/* Command Identifier, CI */
	b  = 0x20 << 2;     /* Crypto-algo type - always 0x20 for Eurocrypt PC2 implementation */
	b |= 1 << 1;        /* Format bit - 0: fixed, 1: variable */
	b |= t << 0;        /* Toggle */
	pkt[x++] = b;
	
	/* Command Length Indicator, CLI -- updated later */
	pkt[x++] = 0;
	
	/* PPID */
	pkt[x++] = 0x90; /* PI */
	pkt[x++] = 0x03; /* LI */
	/* Provider ID and op-key to update */
	memcpy(&pkt[x], e->mode->ppid, 3); x += 3;
	
	/* IDUP */
	pkt[x++] = 0xa1;
	pkt[x++] = 0x03;
	/* Provider ID and op-key to update */
	memcpy(&pkt[x], e->emmode->ppid, 3); x += 3;
	
	/* CLE */
	pkt[x++] = 0xEF;
	pkt[x++] = 0x08;
	_encrypt_opkey(&pkt[x], e); x += 8;
	
	/* EMM hash */
	pkt[x++] = 0xF0;
	pkt[x++] = 0x08;
	_build_emmu_hash_data(&pkt[x], e, x); x += 8;
	memcpy(e->emm_hash, &pkt[x - 8], 8);
	
	/* Update the CI command length */
	pkt[6] = x - 7;
	
	return(x / ECM_PAYLOAD_BYTES);
}

static uint64_t _update_cw(eurocrypt_t *e, int t)
{
	uint64_t cw;
	int i, r;
	
	/* Fetch the next active CW */
	for(cw = i = 0; i < 8; i++)
	{
		cw = (cw << 8) | e->cw[t][i];
	}
	
	/* Generate a new CW */
	t ^= 1;
	
	for(i = 0; i < 8; i++)
	{
		e->cw[t][i] = e->ecw[t][i] = rand() & 0xFF;
	}
	
	/* Three rounds for 3DES mode, one round for others */
	for(r = 0; r < (e->mode->emode != EC_3DES ? 1 : 3); r++)
	{
		/* Use second key on second round in 3DES */
		_eurocrypt(e->ecw[t], e->mode->key + (r != 1 ? 0 : 7), ECM, e->mode->emode, r + 1);
	}
	
	return(cw);
}

void eurocrypt_next_frame(vid_t *vid, int frame)
{
	eurocrypt_t *e = &vid->mac.ec;
	
	/* Update the CW at the beginning of frames FCNT == 1 */
	if((frame & 0xFF) == 1)
	{
		int t = (frame >> 8) & 1;
		
		/* Fetch and update next CW */
		vid->mac.cw = _update_cw(e, t);
		
		/* Update the ECM packet */
		e->ecm_cont = _update_ecm_packet(e, t);
		
		/* Print ECM */
		if(vid->conf.showecm)
		{
			int i;
			fprintf(stderr, "\n\n***** ECM *****");
			fprintf(stderr, "\nOperational key [%02X]:\t", e->mode->ppid[2] & 0x0F);
			for(i = 0; i < 7; i++) fprintf(stderr, "%02X ", e->mode->key[i]);
			fprintf(stderr, "\nEurocrypt ECM   [in]:\t");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->ecw[0][i]);
			fprintf(stderr, "| ");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->ecw[1][i]);
			fprintf(stderr,"\nEurocrypt ECM   [out]:\t");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->cw[0][i]);
			fprintf(stderr, "| ");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->cw[1][i]);
			fprintf(stderr,"\nUsing CW        [%s]:\t%s", t ? "odd" : "even", t ? "                          " : "");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", (uint8_t) e->cw[t][i]);
			fprintf(stderr,"\nHash:\t\t\t");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", (uint8_t) e->ecm_hash[i]);
			fprintf(stderr,"\n");
		}
	}
	
	/* Send an ECM packet every 12 frames - ~0.5s */
	if(frame % 12 == 0)
	{
		uint8_t pkt[MAC_PAYLOAD_BYTES];
		memset(pkt, 0, MAC_PAYLOAD_BYTES);
		int i;
		
		/* Break up the ECM packet, if required */
		for(i = 0; i <= e->ecm_cont; i++)
		{
			memcpy(pkt, e->ecm_pkt + (i * ECM_PAYLOAD_BYTES), ECM_PAYLOAD_BYTES);
			
			/* Golay encode the payload */
			mac_golay_encode(pkt + 1 - i, 30);
			
			mac_write_packet(vid, 0, e->ecm_addr, i, pkt, 0);
		}	
	}

	/* Send EMMs every ~10 seconds, if available */
	if(e->emmode->id != NULL)
	{
		if((vid->frame & 0xFF) == 0x7F)
		{
			/* Generate EMM-Shared packet */
			if(e->emmode->emmtype == EMMS)
			{
				uint8_t pkt[MAC_PAYLOAD_BYTES];
				memset(pkt, 0, MAC_PAYLOAD_BYTES);
				int i;
				
				int t = (vid->frame >> 8) & 1;
				
				/* Shared EMM packet requires EMM-Global packet before it */
				e->emm_cont = _update_emmg_packet(e, t);
				
				/* Break up the EMM-G packet, if required */
				for(i = 0; i <= e->emm_cont; i++)
				{
					memcpy(pkt, e->emmg_pkt + (i * ECM_PAYLOAD_BYTES), ECM_PAYLOAD_BYTES);
					
					/* Golay encode the payload */
					mac_golay_encode(pkt + 1 - i, 30);
					
					mac_write_packet(vid, 0, e->emm_addr, i, pkt, 0);
				}
				
				/* Generate the EMM-S packet (always fixed length) */
				_update_emms_packet(e);
				
				mac_write_packet(vid, 0, e->emm_addr, 0, e->emms_pkt, 0);
				
				/* Print EMM */
				if(vid->conf.showecm)
				{
					int i;
					fprintf(stderr, "\n\n ***** EMM *****");
					fprintf(stderr, "\nShared address:\t\t");
					for(i = 0; i < 3; i++) fprintf(stderr, "%02X ", e->emmode->sa[2 - i]);
					fprintf(stderr, "\nManagement key   [%02X]:\t", e->emmode->ppid[2] & 0x0F);
					for(i = 0; i < 7; i++) fprintf(stderr, "%02X ", e->emmode->key[i]);
					fprintf(stderr, "\nDecrypted op key [%02X]:\t", e->mode->ppid[2] & 0x0F);
					for(i = 0; i < 7; i++) fprintf(stderr, "%02X ", e->mode->key[i]);
					fprintf(stderr, "\nEncrypted op key [%02X]:\t", e->mode->ppid[2] & 0x0F);
					for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->emmg_pkt[15 + i]);
					fprintf(stderr,"\nHash:\t\t\t");
					for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", (uint8_t) e->emm_hash[i]);
					fprintf(stderr,"\n");
				}
			}
		}
	}
}

int eurocrypt_init(vid_t *vid, const char *mode)
{
	eurocrypt_t *e = &vid->mac.ec;
	
	memset(e, 0, sizeof(eurocrypt_t));
	
	/* Find the ECM mode */
	for(e->mode = _ec_modes; e->mode->id != NULL; e->mode++)
	{
		if(strcmp(mode, e->mode->id) == 0) break;
	}
	
	if(e->mode->id == NULL)
	{
		fprintf(stderr, "Unrecognised Eurocrypt mode.\n");
		return(VID_ERROR);
	}
	
	/* Find the EMM mode */
	for(e->emmode = _em_modes; e->emmode->id != NULL; e->emmode++)
	{
		if(strcmp(mode, e->emmode->id) == 0) break;
	}
	
	if(e->emmode->id == NULL)
	{
		fprintf(stderr, "Cannot find a matching EMM mode.\n");
	}
	
	/* ECM/EMM address */
	e->ecm_addr = 346;
	e->emm_addr = 347;
	
	/* Generate initial even and odd encrypted CWs */
	_update_cw(e, 0);
	_update_cw(e, 1);
	
	/* Generate initial packet */
	e->ecm_cont = _update_ecm_packet(e, 0);
	
	return(VID_OK);
}

