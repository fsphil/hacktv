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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "video.h"

#define ECM  0
#define HASH 1

#define EC_M 0
#define EC_S 3

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
	{ "rdv",     EC_S, { 0xFE, 0x6D, 0x9A, 0xBB, 0xEB, 0x97, 0xFB }, { 0x00, 0x2D, 0x93 }, { 0x22, 0x70, 0xFF, 0x00 } },
	{ "tvs",     EC_S, { 0x5C, 0x8B, 0x11, 0x2F, 0x99, 0xA8, 0x2C }, { 0x00, 0x2B, 0x50 }, { 0x7A, 0x14, 0x00, 0x01 } },
	{ "ctvs",    EC_S, { 0x22, 0xDE, 0x81, 0xF5, 0xCA, 0x4D, 0x4A }, { 0x00, 0x2B, 0x20 }, { 0x7A, 0x14, 0x00, 0x01 } },
	{ "ctv",     EC_M, { 0x84, 0x66, 0x30, 0xE4, 0xDA, 0xFA, 0x23 }, { 0x00, 0x04, 0x38 }, { 0x21, 0x65, 0xFF, 0x00 } },
	{ "tvplus",  EC_M, { 0x12, 0x06, 0x28, 0x3A, 0x4B, 0x1D, 0xE2 }, { 0x00, 0x2C, 0x08 }, { 0x21, 0x65, 0x04, 0x00 } },
	{ "tv1000",  EC_M, { 0x48, 0x63, 0xC5, 0xB3, 0xDA, 0xE3, 0x29 }, { 0x00, 0x04, 0x18 }, { 0x21, 0x65, 0x05, 0x04 } },
	{ "filmnet", EC_M, { 0x21, 0x12, 0x31, 0x35, 0x8A, 0xC3, 0x4F }, { 0x00, 0x28, 0x08 }, { 0x21, 0x15, 0x05, 0x00 } },
	{ "nrk",     EC_S, { 0xE7, 0x19, 0x5B, 0x7C, 0x47, 0xF4, 0x66 }, { 0x47, 0x52, 0x00 }, { 0x21, 0x15, 0x05, 0x00 } },
	{ NULL }
};

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

static void _eurocrypt(const uint8_t *in, uint8_t *out, const uint8_t *key, int hash, int emode)
{
	int i;
	uint64_t r, l, c, d, s;
	
	memcpy(out, in, 8);
	
	/* Initial permutation for Eurocrypt S2 */
	if(emode)
	{
		_permute_ec(out, _ip, 64);
	}
	
	/* Key preparation. Split key into two halves */
	c = ((uint64_t) key[0] << 20)
	  ^ ((uint64_t) key[1] << 12)
	  ^ ((uint64_t) key[2] << 4)
	  ^ ((uint64_t) key[3] >> 4);
	
	d = ((uint64_t) (key[3] & 0x0F) << 24)
	  ^ ((uint64_t) key[4] << 16)
	  ^ ((uint64_t) key[5] << 8)
	  ^ ((uint64_t) key[6] << 0);
	
	/* Control word preparation. Split CW into two halves. */
	l = ((uint64_t) out[0] << 24)
	  ^ ((uint64_t) out[1] << 16)
	  ^ ((uint64_t) out[2] << 8)
	  ^ ((uint64_t) out[3] << 0);
	
	r = ((uint64_t) out[4] << 24)
	  ^ ((uint64_t) out[5] << 16)
	  ^ ((uint64_t) out[6] << 8)
	  ^ ((uint64_t) out[7] << 0);
	
	/* 16 iterations */
	for(i = 0; i < 16; i++)
	{
		uint64_t r3;
		uint8_t k2[8];
		int j, k;
		
		/* Key shift rotation */
		if(!emode || (emode && hash))
		{
			for(j = 0; j < _lshift[i]; j++)
			{
				c = ((c << 1) ^ (c >> 27)) & 0xFFFFFFFL;
				d = ((d << 1) ^ (d >> 27)) & 0xFFFFFFFL;
			}
		}
		
		/* Key expansion */
		for(j = 0, k = 0; j < 8; j++)
		{
			int t;
			
			k2[j] = 0;
			
			for(t = 0; t < 6; t++, k++)
			{
				if(_pc2[k] < 29)
				{
					k2[j] |= (c >> (28 - _pc2[k]) & 1) << (5 - t);
				}
				else
				{
					k2[j] |= (d >> (56 - _pc2[k]) & 1) << (5 - t);
				}
			}
		}
		
		/* One decryption round */
		s = _ec_des_f(r, k2);
		
		if(emode && !hash)
		{
			for(j = 0; j < _lshift[15 - i]; j++)
			{
				c = ((c >> 1) ^ (c << 27)) & 0xFFFFFFFL;
				d = ((d >> 1) ^ (d << 27)) & 0xFFFFFFFL;
			}
		}
		
		/* Swap first two bytes if it's a hash routine */
		if(hash && !emode)
		{
			s = ((s >> 8) & 0xFF0000L) | ((s << 8) & 0xFF000000L) | (s & 0x0000FFFFL);
		}
		
		/* Rotate halves around */
		r3 = l ^ s;
		l = r;
		r = r3;
	}
	
	/* Put everything together */
	out[0] = r >> 24;
	out[1] = r >> 16;
	out[2] = r >> 8;
	out[3] = r;
	out[4] = l >> 24;
	out[5] = l >> 16;
	out[6] = l >> 8;
	out[7] = l;
	
	if(emode)
	{
		_permute_ec(out, _ipp, 64);
	}
}

static void _ecm_hash(uint8_t *hash, const uint8_t *src, const ec_mode_t *mode)
{
	uint8_t msg[32];
	int msglen, i;
	
	/* Clear hash memory */
	memset(hash, 0, 8);
	
	/* Build the hash message */
	msglen = 0;
	
	if(mode->emode == EC_S)
	{
		/* EC S2 */
		
		/* Copy PPID */
		memcpy(msg, src + 2, 3);
		msglen += 3;
		
		/* Copy other data */
		memcpy(msg + msglen, src + 10, 5);
		msglen += 5;
		
		/* Copy CWs */
		memcpy(msg + msglen, src + 16, 16);
		msglen += 16;
	}
	else
	{
		/* EC M */
		memcpy(msg, src + 5, 27);
		msglen += 27;
	}
	
	/* Iterate through data */
	for(i = 0; i < msglen; i++)
	{
		hash[(i % 8)] ^= msg[i];
		
		if(i % 8 == 7)
		{
			_eurocrypt(hash, hash, mode->key, HASH, mode->emode);
		}
	}
	
	/* Final interation - EC M only */
	if(mode->emode == EC_M)
	{
		_eurocrypt(hash, hash, mode->key, HASH, mode->emode);
	}
}

static void _update_ecm_packet(eurocrypt_t *e, int t)
{
	int x;
	uint16_t b;
	uint8_t *pkt = e->ecm_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES);
	
	/* PT - always 0x00 for ECM */
	x = 0;
	pkt[x++] = 0x00;
	
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
	
	/* CTRL */
	pkt[x++] = 0xE0; /* PI */
	pkt[x++] = 0x01; /* LI */
	pkt[x++] = 0x00; /* This is the default, can this be removed? */
	
	/* CDATE + THEME/LEVEL */
	pkt[x++] = 0xE1; /* PI */
	pkt[x++] = 0x04; /* LI */
	memcpy(&pkt[x], e->mode->cdate, 4); x += 4;
	
	/* ECW/OCW */
	pkt[x++] = 0xEA; /* PI */
	pkt[x++] = 0x10; /* LI */
	memcpy(&pkt[x], e->ecw[0], 8); x += 8; /* ECW */
	memcpy(&pkt[x], e->ecw[1], 8); x += 8; /* OCW */
	
	/* HASH */
	pkt[x++] = 0xF0; /* PI */
	pkt[x++] = 0x08; /* LI */
	_ecm_hash(&pkt[x], &pkt[3], e->mode); x += 8;
	
	/* Update the CI command length */
	pkt[2] = x - 3;
	
	/* Test if the data is too large for a single packet */
	if(x > 45)
	{
		fprintf(stderr, "Warning: ECM packet too large (%d)\n", x);
	}
	
	/* Golay encode the payload */
	mac_golay_encode(pkt + 1, 30);
}

static uint64_t _update_cw(eurocrypt_t *e, int t)
{
	uint64_t cw;
	int i;
	
	/* Fetch the next active CW */
	for(cw = i = 0; i < 8; i++)
	{
		cw = (cw << 8) | e->cw[t][i];
	}
	
	/* Generate a new CW */
	t ^= 1;
	
	for(i = 0; i < 8; i++)
	{
		e->ecw[t][i] = rand() & 0xFF;
	}
	
	_eurocrypt(e->ecw[t], e->cw[t], e->mode->key, ECM, e->mode->emode);
	
	return(cw);
}

void eurocrypt_next_frame(vid_t *vid)
{
	eurocrypt_t *e = &vid->mac.ec;
	
	/* Update the CW at the beginning of frames FCNT == 1 */
	if((vid->frame & 0xFF) == 1)
	{
		int t = (vid->frame >> 8) & 1;
		
		/* Fetch and update next CW */
		vid->mac.cw = _update_cw(e, t);
		
		/* Update the ECM packet */
		_update_ecm_packet(e, t);
		
		/* Print ECM */
		if(vid->conf.showecm)
		{
			int i;
			fprintf(stderr, "\nEurocrypt ECM In:\t");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->ecw[0][i]);
			fprintf(stderr, "| ");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->ecw[1][i]);
			fprintf(stderr,"\nEurocrypt ECM Out:\t");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->cw[0][i]);
			fprintf(stderr, "| ");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->cw[1][i]);
			fprintf(stderr,"\nUsing CW (%s):  \t%s", t ? "odd" : "even", t ? "                          " : "");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", (uint8_t) e->cw[t][i]);
			fprintf(stderr,"\nHash:\t\t\t");
			for(i = 73; i < 91; i+=6) 
			{
				/* Remove Golay bits before printing */
				fprintf(stderr, "%02X %02X ", e->ecm_pkt[i], (e->ecm_pkt[i + 1] & 0x0F) | ((e->ecm_pkt[i + 3] & 0x0F) << 4));
				if(i != 85) fprintf(stderr, "%02X ", ((e->ecm_pkt[i + 4] << 4) & 0xF0) | (e->ecm_pkt[i + 3] >> 4));
			}
			fprintf(stderr,"\n");
		}
	}
	
	/* Send an ECM packet every 12 frames - ~0.5s */
	if(vid->frame % 12 == 0)
	{
		mac_write_packet(vid, 0, e->ecm_addr, 0, e->ecm_pkt, 0);
	}
}

int eurocrypt_init(vid_t *vid, const char *mode)
{
	eurocrypt_t *e = &vid->mac.ec;
	
	memset(e, 0, sizeof(eurocrypt_t));
	
	/* Find the mode */
	for(e->mode = _ec_modes; e->mode->id != NULL; e->mode++)
	{
		if(strcmp(mode, e->mode->id) == 0) break;
	}
	
	if(e->mode->id == NULL)
	{
		fprintf(stderr, "Unrecognised Eurocrypt mode.\n");
		return(VID_ERROR);
	}
	
	/* ECM/EMM address */
	e->ecm_addr = 346;
	
	/* Generate initial even and odd encrypted CWs */
	_update_cw(e, 0);
	_update_cw(e, 1);
	
	/* Generate initial packet */
	_update_ecm_packet(e, 0);
	
	return(VID_OK);
}

