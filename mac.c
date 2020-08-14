/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2018 Philip Heron <phil@sanslogic.co.uk>                    */
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

/* -=== D/D2-MAC encoder ===- */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video.h"
#include "nicam728.h"
#include "mac.h"

/* MAC sync codes */
#define MAC_CLAMP 0xEAF3927FUL
#define MAC_LSW   0x0BUL
#define MAC_CRI   0x55555555UL
#define MAC_FSW   0x65AEF3153F41C246ULL

/* Polynomial for PRBS generator */
#define _PRBS_POLY 0x7FFF

/* Hamming codes */
static const uint8_t _hamming[0x10] = {
	0x15, 0x02, 0x49, 0x5E, 0x64, 0x73, 0x38, 0x2F, 0xD0, 0xC7, 0x8C, 0x9B, 0xA1, 0xB6, 0xFD, 0xEA
};

/* Network origin and name */
static const char *_nwo    = "hacktv";
static const char *_nwname = "hacktv";

/* Service Reference */
static const char *_sname = "hacktv"; /* Service Name (max 32 characters) */

/* RDF sequence */
typedef struct {
	int tdmcid;
	int fln1;
	int lln1;
	int fln2;
	int lln2;
	int fcp;
	int lcp;
	int links;
} _rdf_t;

static _rdf_t _rdf_d2[] = {
	/* CID, FL1, LL1,  FL2,  LL2, FCP,  LCP */
	{ 0x01,   0, 622, 1023, 1023,   9,  205, 0 }, /* MPX 01 data burst (99 bits) */
	{ 0x10,  22, 309,  334,  621, 235,  583, 0 }, /* CDIFF colour difference signal */
	{ 0x11,  22, 309,  334,  621, 589, 1285, 0 }, /* LUM luminance signal */
	{ 0x20,   0,  21,  312,  333, 229, 1292, 0 }, /* FF Fixed Format teletext */
	{ 0x00, }, /* End of sequence */
};

static _rdf_t _rdf_d[] = {
	/* CID, FL1, LL1,  FL2,  LL2, FCP,  LCP */
	{ 0x01,   0, 622, 1023, 1023,   6,  104, 0 }, /* MPX 01 data burst (99 bits) */
	{ 0x02,   0, 622, 1023, 1023, 105,  203, 0 }, /* MPX 02 data burst (99 bits) */
	{ 0x10,  22, 309,  334,  621, 235,  583, 0 }, /* CDIFF colour difference signal */
	{ 0x11,  22, 309,  334,  621, 589, 1285, 0 }, /* LUM luminance signal */
	{ 0x20,   0,  21,  312,  333, 229, 1292, 0 }, /* FF Fixed Format teletext */
	{ 0x00, }, /* End of sequence */
};

static double _rrc(double x)
{
	return(x == 0 ? 1 : sin(M_PI * x) / (M_PI * x));
}

static int16_t *_duobinary_lut(int mode, int width, double level)
{
	double samples_per_symbol;
	double offset;
	int i, x, bits;
	double err;
	int ntaps, htaps;
	int16_t *lut, *p;
	
	bits = (mode == MAC_MODE_D2 ? 648 : 1296);
	samples_per_symbol = (double) width / bits;
	offset = width / 1296 * (mode == MAC_MODE_D2 ? -3 : -1);
	ntaps = (int) (samples_per_symbol * 16) | 1;
	htaps = ntaps / 2;
	
	lut = malloc(sizeof(int16_t) * ((ntaps + 1) * bits + 1));
	if(!lut)
	{
		return(NULL);
	}
	
	p = lut;
	
	*(p++) = ntaps;
	for(i = 0; i < bits; i++)
	{
		/* Calculate the error */
		*p = lround(offset + samples_per_symbol * i);
		err = offset + samples_per_symbol * i - *p;
		*(p++) -= htaps;
		
		for(x = 0; x < ntaps; x++)
		{
			*(p++) = lround(_rrc((double) (x - htaps - err) / samples_per_symbol) * level);
		}
	}
	
	return(lut);
}

static int _duobinary(vid_t *s, int bit)
{
	if(bit)
	{
		return(s->mac.polarity);
	}
	
	s->mac.polarity = -s->mac.polarity;
	
	return(0);
}

static void _render_duobinary(vid_t *s, uint8_t *data, int nbits)
{
	const int16_t *taps;
	int symbol;
	int ntaps;
	int x, xo;
	int l;
	int i;
	
	taps = s->mac.lut;
	ntaps = *(taps++);
	
	for(i = 0; i < nbits; i++, taps += ntaps + 1)
	{
		/* Read the next symbol */
		symbol = _duobinary(s, (data[i >> 3] >> (i & 7)) & 1);
		
		/* 0 bits don't need to be rendered */
		if(!symbol) continue;
		
		l = 0;
		xo = *taps;
		
		if(xo < 0)
		{
			l = -1;
			xo += s->width;
		}
		
		for(x = 1; x <= ntaps; x++, xo++)
		{
			int t;
			
			if(xo >= s->width)
			{
				xo -= s->width;
				l++;
			}
			
			t = s->oline[s->odelay + l][xo * 2] + (symbol == 1 ? taps[x] : -taps[x]);
			
			/* Don't let the duobinary signal clip */
			if(t < INT16_MIN) t = INT16_MIN;
			else if(t > INT16_MAX) t = INT16_MAX;
			
			s->oline[s->odelay + l][xo * 2] = t;
		}
	}
}

/* Pseudo-random binary sequence (PRBS) generator for spectrum shaping */
static int _prbs(uint16_t *x)
{
	int b;
	
	b = (*x ^ (*x >> 14)) & 1;
	*x = (*x >> 1) | (b << 14);
	
	return(b);
}

/* Generate IW for CA PRBS for video scrambling */
static uint64_t _prbs_generate_iw(uint64_t cw, uint8_t fcnt)
{
	uint64_t iw;
	
	/* FCNT is repeated 8 times, each time inverted */
	iw  = ((fcnt ^ 0xFF) << 8) | fcnt;
	iw |= (iw << 16) | (iw << 32) | (iw << 48);
	
	return((iw ^ cw) & MAC_PRBS_CW_MASK);
}

/* Reset CA PRBS */
static void _prbs1_reset(mac_t *s, uint8_t fcnt)
{
	uint64_t iw = _prbs_generate_iw(s->cw, fcnt);
	
	s->sr1 = iw & MAC_PRBS_SR3_MASK;
	s->sr2 = (iw >> 31) & MAC_PRBS_SR4_MASK;
}

static void _prbs2_reset(mac_t *s, uint8_t fcnt)
{
	uint64_t iw = _prbs_generate_iw(s->cw, fcnt);
	
	s->sr3 = iw & MAC_PRBS_SR3_MASK;
	s->sr4 = (iw >> 31) & MAC_PRBS_SR4_MASK;
}

/* Return first x LSBs in b in reversed order. TODO: Remove this */
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

/* Update CA PRBS1 */
static uint64_t _prbs1_update(mac_t *s)
{
	uint64_t code = 0;
	int i;
	
	for(i = 0; i < 61; i++)
	{
		uint32_t a, b;
		
		/* Load the multiplexer address */
		a  = (_rev(s->sr2, 29) << 0) & 0x03;
		a |= (_rev(s->sr1, 31) << 2) & 0x1C;
		
		/* Load the multiplexer data */
		b  = (_rev(s->sr2, 29) >> 2) & 0x000000FF;
		b |= (_rev(s->sr1, 31) << 5) & 0xFFFFFF00;
		
		/* Shift into result register */
		code = (code >> 1) | ((uint64_t) ((b >> a) & 1) << 60);
		
		/* Update shift registers */
		s->sr1 = (s->sr1 >> 1) ^ (s->sr1 & 1 ? 0x78810820UL : 0);
		s->sr2 = (s->sr2 >> 1) ^ (s->sr2 & 1 ? 0x17121100UL : 0);
	}
	
	return(code);
}

/* Update CA PRBS2 */
static uint16_t _prbs2_update(mac_t *s)
{
	uint16_t code = 0;
	int i;
	
	for(i = 0; i < 16; i++)
	{
		int a;
		
		/* Load the multiplexer address */
		a = _rev(s->sr4, 29) & 0x1F;
		if(a == 31) a = 30;
		
		/* Shift into result register */
		code = (code >> 1) | (((_rev(s->sr3, 31) >> a) & 1) << 15);
		
		/* Update shift registers */
		s->sr3 = (s->sr3 >> 1) ^ (s->sr3 & 1 ? 0x7BB88888UL : 0);
		s->sr4 = (s->sr4 >> 1) ^ (s->sr4 & 1 ? 0x17A2C100UL : 0);
	}
	
	return(code);
}

/* Pack bits into buffer LSB first */
static size_t _bits(uint8_t *data, size_t offset, uint64_t bits, size_t nbits)
{
	uint8_t b;
	
	for(; nbits; nbits--, offset++, bits >>= 1)
	{
		b = 1 << (offset & 7);
		if(bits & 1) data[offset >> 3] |= b;
		else data[offset >> 3] &= ~b;
	}
	
	return(offset);
}

/* Pack bits into buffer MSB first */
static size_t _rbits(uint8_t *data, size_t offset, uint64_t bits, size_t nbits)
{
	uint64_t m = (uint64_t) 1 << (nbits - 1);
	uint8_t b;
	
	for(; nbits; nbits--, offset++, bits <<= 1)
	{
		b = 1 << (offset & 7);
		if(bits & m) data[offset >> 3] |= b;
		else data[offset >> 3] &= ~b;
	}
	
	return(offset);
}

/* Pack bits from a byte array into buffer LSB first */
static size_t _bits_buf(uint8_t *data, size_t offset, const uint8_t *src, size_t nbits)
{
	for(; nbits >= 8; nbits -= 8)
	{
		offset = _bits(data, offset, *(src++), 8);
	}
	
	if(nbits)
	{
		offset = _bits(data, offset, *src, nbits);
	}
	
	return(offset);
}

/* Pack bits from a byte array into buffer LSB first, interleaved with PRNG bits */
static size_t _bits_buf_il(uint8_t *data, size_t offset, const uint8_t *src, size_t nbits, uint16_t *poly)
{
	int x;
	
	for(x = 0; x < nbits; x++)
	{
		_prbs(poly);
		offset = _bits(data, offset, (src[x >> 3] >> (x & 7)) & 1, 1);
		offset = _bits(data, offset, _prbs(poly), 1);
	}
	
	return(offset);
}

static inline uint8_t _parity(unsigned int value)
{
	uint8_t p = 0;
	
	while(value)
	{
		p ^= value & 1;
		value >>= 1;
	}
	
	return(p);
}

/* Reversed version of the CCITT CRC */
static uint16_t _crc16(const uint8_t *data, size_t length)
{
	uint16_t crc = 0x0000;
	const uint16_t poly = 0x8408;
	int b;
	
	while(length--)
	{
		crc ^= *(data++);
		
		for(b = 0; b < 8; b++)
		{
			crc = (crc & 1 ? (crc >> 1) ^ poly : crc >> 1);
		}
	}
	
	return(crc);
}

/* Calculate and append bits in *data with BCH codes.
 * 
 * data = pointer to bits, LSB first
 * n = Length of final code in bits (data + BCH codes)
 * k = Length of data in bits
*/
static void _bch_encode(uint8_t *data, int n, int k)
{
	unsigned int code = 0x0000;
	unsigned int g;
	int i, b;
	
	g = (n == 23 ? 0x0571 : 0x3BB0);
	
	for(i = 0; i < k; i++)
	{
		b = (data[i >> 3] >> (i & 7)) & 1;
		b = (b ^ code) & 1;
		
		code >>= 1;
		
		if(b) code ^= g;
	}
	
	_bits(data, k, code, n - k);
}

/* Golay(24,12) protection */
void mac_golay_encode(uint8_t *data, int blocks)
{
	uint8_t p[MAC_PAYLOAD_BYTES];
	uint8_t *dst = p, *src = data;
	int i;
	
	memset(p, 0, MAC_PAYLOAD_BYTES);
	
	for(i = 0; i < blocks; i += 2)
	{
		dst[0] = src[0];
		dst[1] = src[1] & 0x0F;
		dst[2]  = 0x00;
		_bch_encode(dst, 23, 12);
		dst[2] |= (_parity(dst[0] | (dst[1] << 8) | (dst[2] << 16)) ^ 1) << 7;
		dst += 3;
		
		dst[0]  = (src[2] << 4) | (src[1] >> 4);
		dst[1]  = src[2] >> 4;
		dst[2]  = 0x00;
		_bch_encode(dst, 23, 12);
		dst[2] |= (_parity(dst[0] | (dst[1] << 8) | (dst[2] << 16)) ^ 1) << 7;
		dst += 3;
		src += 3;
	}
	
	memcpy(data, p, blocks * 3);
}

static void _update_udt(uint8_t udt[25], time_t timestamp)
{
	struct tm tm;
	int i, mjd;
	
	/* Get the timezone offset */
	localtime_r(&timestamp, &tm);
	i = tm.tm_gmtoff / 1800;
	if(i < 0) i = -i | (1 << 5);
	
	/* Calculate Modified Julian Date */
	gmtime_r(&timestamp, &tm);
	mjd = 367.0 * (1900 + tm.tm_year)
	    - (int) (7.0 * (1900 + tm.tm_year + (int) ((1 + tm.tm_mon + 9.0) / 12.0)) / 4.0)
	    + (int) (275.0 * (1 + tm.tm_mon) / 9.0) + tm.tm_mday - 678987.0;
	
	/* Set the Unified Date and Time sequence */
	memset(udt, 0, 25);
	udt[ 0] = mjd / 10000 % 10;     /* MJD digit weight 10^4 */
	udt[ 1] = mjd / 1000 % 10;      /* MJD digit weight 10^3 */
	udt[ 2] = mjd / 100 % 10;       /* MJD digit weight 10^2 */
	udt[ 3] = mjd / 10 % 10;        /* MJD digit weight 10^1 */
	udt[ 4] = mjd / 1 % 10;         /* MJD digit weight 10^0 */
	udt[ 5] = tm.tm_hour / 10 % 10; /* UTC hours (tens)      */
	udt[ 6] = tm.tm_hour / 1 % 10;  /* UTC hours (units)     */
	udt[ 7] = tm.tm_min / 10 % 10;  /* UTC minutes (tens)    */
	udt[ 8] = tm.tm_min / 1 % 10;   /* UTC minutes (units)   */
	udt[ 9] = tm.tm_sec / 10 % 10;  /* UTC seconds (tens)    */
	udt[10] = tm.tm_sec / 1 % 10;   /* UTC seconds (units)   */
	udt[15] = (i >> 4) & 15;        /* Local Offset */
	udt[16] = i & 15;               /* Local Offset */
	
	/* Apply the chain code sequence */
	/* 0000101011101100011111001 */
	for(i = 0; i < 25; i++)
	{
		udt[i] |= ((0x13E3750 >> i) & 1) << 4;
	}
}

static void _interleave(uint8_t pkt[94])
{
	uint8_t tmp[94];
	int c, d, i;
	
	memcpy(tmp, pkt, 94);
	
	/* + 1 bit to ensure final byte is shifted correctly */
	for(d = i = 0; i < 751 + 1; i++)
	{
		c = i >> 3;
		
		pkt[d] = (pkt[d] >> 1) | (tmp[c] << 7);
		tmp[c] >>= 1;
		
		if(++d == 94) d = 0;
	}
}

static void _encode_packet(uint8_t *pkt, int address, int continuity, const uint8_t *data)
{
	int x;
	
	/* Generate packet header (address and continuity, MSB first) */
	x = _bits(pkt, 0, address & 0x3FF, 10);
	x = _bits(pkt, x, continuity & 3, 2);
	_bch_encode(pkt, 23, 12);
	
	/* Write the packet contents, or zero */
	for(x = 23; x < 751; x += 8)
	{
		_bits(pkt, x, data ? *(data++) : 0x00, 8);
	}
	
	/* Interleave the packet */
	_interleave(pkt);
}

static void _scramble_packet(uint8_t *pkt, uint64_t iw)
{
	int x;
	
	for(x = 1; x < MAC_PAYLOAD_BYTES; x++)
	{
		int i;
		uint8_t c = 0;
		
		/* PRBS3 */
		for(i = 0; i < 8; i++)
		{
			uint32_t a, b;
			
			/* Load the multiplexer address */
			a  = ((_rev(iw, 61) >>  4) & 1) << 0;
			a |= ((_rev(iw, 61) >>  9) & 1) << 1;
			a |= ((_rev(iw, 61) >> 14) & 1) << 2;
			a |= ((_rev(iw, 61) >> 19) & 1) << 3;
			a |= ((_rev(iw, 61) >> 24) & 1) << 4;
			
			/* Load the multiplexer data */
			b = (_rev(iw, 61) >> 29) & 0xFFFFFFFF;
			
			/* Shift into result */
			c = (c >> 1) | (((b >> a) & 1) << 7);
			
			/* Update shift registers */
			iw = (iw >> 1) ^ (iw & 1 ? 0x163D23594C934051UL : 0);
		}
		
		pkt[x] ^= c;
	}
}

/* Packet reader. Returns a dummy packet if the queue is empty */
static void _read_packet(mac_t *s, _mac_packet_queue_item_t *pkt, int subframe)
{
	mac_subframe_t *sf = &s->subframes[subframe];
	int x;
	
	if(sf->queue.len == 0)
	{
		/* The packet queue is empty, generate a dummy packet */
		pkt->address = 0x3FF;
		pkt->continuity = sf->dummy_continuity++;
		pkt->scramble = 0;
		memset(pkt->pkt, 0, MAC_PAYLOAD_BYTES);
		return;
	}
	
	x = sf->queue.p - sf->queue.len;
	if(x < 0) x += MAC_QUEUE_LEN;
	
	memcpy(pkt, &sf->queue.pkts[x], sizeof(_mac_packet_queue_item_t));
	
	sf->queue.len--;
}

static void _create_si_dg0_packet(mac_t *s, uint8_t pkt[MAC_PAYLOAD_BYTES])
{
	int x;
	uint16_t b;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES);
	
	/* PT Packet Type */
	pkt[0] = 0xF8;
	
	/* DGH (Data Group Header) */
	pkt[1] = _hamming[0];		/* TG data group type */
	pkt[2] = _hamming[0];		/* C  data group continuity */
	pkt[3] = _hamming[15];		/* R  data group repetition */
	pkt[4] = _hamming[0];		/* S1 MSB number of packets carrying the data group */
	pkt[5] = _hamming[1];		/* S2 LSB number of packets carrying the data group */
	pkt[6] = _hamming[0];		/* F1 MSB number of data group bytes in the last packet */
	pkt[7] = _hamming[0];		/* F2 LSB number of data group bytes in the last packet */
	pkt[8] = _hamming[1];		/* N  data group suffix indicator */
	
	pkt[9]  = 0x10;			/* CI Network Command (Medium Priority) */
	pkt[10] = 11;			/* LI Length (bytes, everything following up until the DGS) */
	x = 11;
	
	/* Parameter NWO */
	pkt[x++] = 0x10;		/* PI NWO (Network Origin) */
	pkt[x++] = 3 + strlen(_nwo);	/* LI Length (bytes, 3 + string length) */
	pkt[x++] = 0x00;		/* Channel number, BCD */
	pkt[x++] = 0x01;		/* First and second digit of satellite orbital position, BCD */
	pkt[x++] = 0x91;		/* Third digit of satellite orbital position, BCD and Polarisation */
	strcpy((char *) &pkt[x], _nwo);	/* Network Origin string */
	x += strlen(_nwo);
	
	/* Parameter NWNAME */
	pkt[x++] = 0x14;			/* PI NWNAME (Network Name) */
	pkt[x++] = strlen(_nwname);		/* LI Length (bytes, string length) */
	strcpy((char *) &pkt[x], _nwname);	/* Network Name string */
	x += strlen(_nwname);
	
	/* Parameter LISTX (TV) */
	pkt[x++] = 0x18;	/* PI LISTX (List of index values) */
	pkt[x++] = 0x04;	/* LI Length (4 bytes) */
	pkt[x++] = 0x01;	/* TV service */
	pkt[x++] = 0x01;	/* Index value 1 */
	
	b  = 3 << 12;			/* TV, detailed description = DG3 */
	b |= 1 << 10;			/* Subframe identification, TDMCID = 01 */
	b |= s->audio_channel;		/* Packet address of the main TV sound */
	pkt[x++] = (b & 0x00FF) >> 0;	/* TV config LSB */
	pkt[x++] = (b & 0xFF00) >> 8;	/* TV config MSB */
	
	/* Update the CI command length */
	pkt[10] = x - pkt[10];
	
	/* Generate the DGS CRC */
	b = _crc16(&pkt[9], pkt[10] + 2);
	pkt[x++] = (b & 0x00FF) >> 0;
	pkt[x++] = (b & 0xFF00) >> 8;
	
	/* Update the DGH length */
	x -= 1;
	pkt[6] = _hamming[(x & 0xF0) >> 4];
	pkt[7] = _hamming[(x & 0x0F) >> 0];
	
	/* Test if the data is too large for a single packet */
	if(x > 45 - 2)
	{
		fprintf(stderr, "SI DG0 packet overflow (%d/43 bytes)\n", x);
	}
	
	/* Generate the overall packet CRC (excludes PT and CRC) */
	x = MAC_PAYLOAD_BYTES;
	b = _crc16(&pkt[1], x - 3);
	pkt[x - 2] = (b & 0x00FF) >> 0;
	pkt[x - 1] = (b & 0xFF00) >> 8;
}

static void _create_si_dg3_packet(mac_t *s, uint8_t *pkt)
{
	int x;
	uint16_t b;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES);
	
	/* PT Packet Type */
	pkt[0] = 0xF8;
	
	/* DGH (Data Group Header) */
	pkt[1] = _hamming[3];           /* TG data group type */
	pkt[2] = _hamming[0];           /* C  data group continuity */
	pkt[3] = _hamming[15];          /* R  data group repetition */
	pkt[4] = _hamming[0];           /* S1 MSB number of packets carrying the data group */
	pkt[5] = _hamming[1];           /* S2 LSB number of packets carrying the data group */
	pkt[6] = _hamming[0];           /* F1 MSB number of data group bytes in the last packet */
	pkt[7] = _hamming[0];           /* F2 LSB number of data group bytes in the last packet */
	pkt[8] = _hamming[1];           /* N  data group suffix indicator */
	
	pkt[9]  = 0x90;                 /* CI TV Command (Medium Priority) */
	pkt[10] = 11;                   /* LI Length (bytes, everything following up until the DGS) */
	x = 11;
	
	/* Parameter SREF */
	pkt[x++] = 0x40;		/* PI Service Reference */
	pkt[x++] = 1 + strlen(_sname);	/* LI Length */
	pkt[x++] = 1;			/* Index value 1 */
	strcpy((char *) &pkt[x], _sname);
	x += strlen(_sname);
	
	if(s->eurocrypt)
	{
		/* Parameter ACCM */
		pkt[x++] = 0x88;
		pkt[x++] = 0x03;        /* Packet length = 3 */
		b  = 1 << 15;           /* 0: Absence of ECM, 1: Presence of ECM */
		b |= 0 << 14;           /* 0: CW derived 'by other means', 1: CW derived from CAFCNT */
		b |= 1 << 10;           /* Subframe related location - TDMCID 01 */
		b |= s->ec.ecm_addr;    /* Address 346 */
		pkt[x++] = (b & 0x00FF) >> 0;
		pkt[x++] = (b & 0xFF00) >> 8;
		pkt[x++] = 0x40;        /* Eurocrypt */
	}
	
	/* Parameter VCONF */
	pkt[x++] = 0x90;	/* PI Analogue TV picture (VCONF) */
	pkt[x++] = 1;		/* LI Length (1 byte) */
	
	b  = 1 << 5;		/* Always 001 */
	//b |= 0 << 4;		/* Aspect ratio: 0: 4:3, 1: 16:9 -- note: inverse of line 625 flag */
	b |= s->ratio << 4;
	b |= 0 << 3;		/* Compression ratio: always 0 for Cy = 3:2, Cu = 3:1 */
	/* VSAM Vision scrambling and access mode (3 bits) */
	//b |= 0 << 2;		/* Access type: 0: Free access, 1: Controlled access */
	//b |= 0 << 1;		/* Scramble type: 0: Double-cut rotation, 1: single-cut line rotation */
	//b |= 1 << 0;		/* Scrambled: 0: Scrambled, 1: Unscrambled */
	b |= s->vsam << 0;
	pkt[x++] = b;
	
	/* Parameter DCINF A4 */
	pkt[x++] = 0xA4;	/* PI TV original sound (DCINF A4) */
	pkt[x++] = 3;		/* LI Length (3 bytes */
	pkt[x++] = 0x09;	/* Language (see page 188) = English */
	
	b = 0x0400 | s->audio_channel;  /* 0 0 0 0 01 audio_channel */
	pkt[x++] = (b & 0x00FF) >> 0;
	pkt[x++] = (b & 0xFF00) >> 8;
	
	if(s->teletext)
	{
		/* Parameter DCINF F0 */
		pkt[x++] = 0xF0;        /* PI CCIR system B cyclic teletext */
		pkt[x++] = 3;           /* LI Length (3 bytes) */
		pkt[x++] = 0x09;        /* Language = English */
		
		b = 0;                  /* Not a part of the packet multiplex */
		pkt[x++] = (b & 0x00FF) >> 0;
		pkt[x++] = (b & 0xFF00) >> 8;
	}
	
	/* Update the CI command length */
	pkt[10] = x - pkt[10];
	
	/* Generate the DGS CRC */
	b = _crc16(&pkt[9], pkt[10] + 2);
	pkt[x++] = (b & 0x00FF) >> 0;
	pkt[x++] = (b & 0xFF00) >> 8;
	
	/* Update the DGH length */
	x -= 1;
	pkt[6] = _hamming[(x & 0xF0) >> 4];
	pkt[7] = _hamming[(x & 0x0F) >> 0];
	
	/* Test if the data is too large for a single packet */
	if(x > 45 - 2)
	{
		fprintf(stderr, "SI DG3 packet overflow (%d/43 bytes)\n", x);
	}
	
	/* Generate the overall packet CRC (excludes PT and CRC) */
	x = MAC_PAYLOAD_BYTES;
	b = _crc16(&pkt[1], x - 3);
	pkt[x - 2] = (b & 0x00FF) >> 0;
	pkt[x - 1] = (b & 0xFF00) >> 8;
}

static void _create_audio_si_packet(mac_t *s, uint8_t *pkt)
{
	uint16_t b;
	int x;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES);
	
	pkt[0] = 0x00;          /* PT == BI1 */
	pkt[1] = _hamming[0];   /* S1 Number of packets MSB */
	pkt[2] = _hamming[1];   /* S2 Number of packets LSB */
	pkt[3] = _hamming[0];   /* F1 Number of bytes in last packet MSB */
	pkt[4] = _hamming[12];  /* F2 Number of bytes in last packet LSB */
	
	pkt[5] = _hamming[1];   /* CI */
	pkt[6] = _hamming[10];  /* LI Length (10 bytes) */
	
	b  = 0 << 15; /* State (0: Signal Present, 1: interrupted) */
	b |= 0 << 13; /* CIB (0: music/speech ON, 1: cross-fade sound ON, 2+3 undefined) */
	b |= 0 << 12; /* Timing (0: Continuous, 1: intermittent) */
	b |= 1 << 11; /* ID of sound coding blocks (0: BC2, 1: BC1) */
	b |= 0 << 10; /* News flash (0: no, 1: yes) */
	b |= 0 <<  9; /* SDFSCR flag (0: store, 1: don't store) */
	
	b |= 0 <<  7; /* Level of error protection (0: first level, 1: second level) */
	b |= 1 <<  6; /* Coding law (0: linear, 1: companded) */
	b |= ((s->vsam & MAC_VSAM_CONTROLLED_ACCESS ? 1 : 0) & s->scramble_audio) << 5; /* Controlled access (0: no, 1: yes) */
	b |= s->scramble_audio << 4; /* Scrambling (0: no, 1: yes) */
	b |= 0 <<  3; /* Automatic mixing (0: mixing not intended, 1: mixing intended) */
	b |= 4 <<  0; /* Audio config (0: 15 kHz mono, 2: 7 kHz mono, 4: 15 kHz stereo) */
	b |= _parity(b) << 8; /* Parity bit */
	
	for(x = 0; x < 5; x++)
	{
		pkt[7 + x * 2] = (b & 0xFF00) >> 8;
		pkt[8 + x * 2] = (b & 0x00FF) >> 0;
	}
}

static int _calculate_audio_address(int channels, int quality, int protection, int mode, int index)
{
	int addr;
	
	/* Audio channel address calculation:
	 * 
	 * 001 ? ? ? ? ???_: Index (0-7)
	 *      \ \ \ \____: 0: 10-bit-NICAM, 1: Linear
	 *       \ \ \_____: 0: First level, 1: Second level protection
	 *        \ \______: 0: 7 kHz, 1: 15 kHz
	 *         \_______: 0: Mono, 1: Stereo
	 * 
	 * 224 = 001 1 1 0 0 000 = Stereo, 15 kHz, First level, Companded
	 * 129 = 001 0 0 0 0 001 = Mono, 7 kHz, First level, Companded
	 * 170 = 001 0 1 0 1 010 = Mono, 15 kHz, First level, Linear
	*/
	
	addr  = 1 << 7;
	addr |= (channels & 1) << 6;
	addr |= (quality & 1) << 5;
	addr |= (protection & 1) << 4;
	addr |= (mode & 1) << 3;
	addr |= index & 7;
	
	return(addr);
}

int mac_init(vid_t *s)
{
	mac_t *mac = &s->mac;
	int i, x;
	
	s->olines += 2; /* Increase the output buffer to three lines */
	s->audio = 1; /* MAC always has audio */
	
	memset(mac, 0, sizeof(mac_t));
	
	mac->vsam = MAC_VSAM_FREE_ACCESS;
	
	/* Initalise Eurocrypt, if required */
	if(s->conf.eurocrypt)
	{
		mac->vsam = MAC_VSAM_CONTROLLED_ACCESS;
		mac->eurocrypt = 1;
		i = eurocrypt_init(s, s->conf.eurocrypt);
		
		if(i != VID_OK)
		{
			return(i);
		}
	}
	
	/* Configure scrambling */
	switch(s->conf.scramble_video)
	{
	default: mac->vsam |= MAC_VSAM_UNSCRAMBLED; break;
	case 1:  mac->vsam |= MAC_VSAM_SINGLE_CUT; break;
	case 2:  mac->vsam |= MAC_VSAM_DOUBLE_CUT; break;
	}
	
	mac->scramble_audio = s->conf.scramble_audio;
	
	if(s->conf.mac_mode == MAC_MODE_D)
	{
		/* BSB receivers are ignoring the SI packets,
		 * and expect audio at packet address 128. */
		mac->audio_channel = 128; /* Stereo NICAM 32kHz, level 1 protection */
	}
	else
	{
		mac->audio_channel = _calculate_audio_address(
			MAC_STEREO,
			MAC_HIGH_QUALITY,
			MAC_FIRST_LEVEL_PROTECTION,
			MAC_COMPANDED,
		0);
	}
	
	mac->teletext = (s->conf.teletext ? 1 : 0);
	
	_update_udt(s->mac.udt, time(NULL));
	
	mac->rdf = 0;
	
	/* Generate the per-line PRBS seeds */
	mac->prbs[0] = _PRBS_POLY;
	
	for(i = 1; i < MAC_LINES; i++)
	{
		mac->prbs[i] = mac->prbs[i - 1];
		
		for(x = 0; x < (s->conf.mac_mode == MAC_MODE_D ? 1296 : 648); x++)
		{
			_prbs(&mac->prbs[i]);
		}
	}
	
	/* Init NICAM encoder */
	nicam_encode_init(&mac->nicam, NICAM_MODE_STEREO, 0);
	
	mac->subframes[0].pkt_bits = MAC_PACKET_BITS;
	mac->subframes[1].pkt_bits = MAC_PACKET_BITS;
	
	mac->polarity = -1;
	mac->lut = _duobinary_lut(s->conf.mac_mode, s->width, (s->white_level - s->black_level) * 0.4);
	
	/* Set the video properties */
	s->active_width &= ~1;	/* Ensure the active width is an even number */
	mac->chrominance_width = s->active_width / 2;	
	mac->chrominance_left  = round(s->sample_rate * (233.5 / MAC_CLOCK_RATE));
	mac->white_ref_left    = round(s->sample_rate * (371.0 / MAC_CLOCK_RATE));
	mac->black_ref_left    = round(s->sample_rate * (533.0 / MAC_CLOCK_RATE));
	mac->black_ref_right   = round(s->sample_rate * (695.0 / MAC_CLOCK_RATE));
	
	/* Setup PRBS */
	mac->cw = MAC_PRBS_CW_FA;
	
	/* Quick and dirty sample rate conversion array */
	for(x = 0; x < MAC_WIDTH; x++)
	{
		mac->video_scale[x] = round((double) x * s->width / MAC_WIDTH);
	}
	
	return(VID_OK);
}

void mac_free(vid_t *s)
{
	mac_t *mac = &s->mac;
	
        free(mac->lut);
}

int mac_write_packet(vid_t *s, int subframe, int address, int continuity, const uint8_t *data, int scramble)
{
	mac_subframe_t *sf = &s->mac.subframes[subframe];
	
	if(sf->queue.len == MAC_QUEUE_LEN)
	{
		/* The packet queue is full */
		return(-1);
	}
	
	sf->queue.pkts[sf->queue.p].address = address;
	sf->queue.pkts[sf->queue.p].continuity = continuity;
	memcpy(sf->queue.pkts[sf->queue.p].pkt, data, MAC_PAYLOAD_BYTES);
	sf->queue.pkts[sf->queue.p].scramble = scramble;
	
	if(++sf->queue.p == MAC_QUEUE_LEN)
	{
		sf->queue.p = 0;
	}
	
	sf->queue.len++;
	
	return(0);
}

int mac_write_audio(vid_t *s, const int16_t *audio)
{
	uint8_t data[MAC_PAYLOAD_BYTES];
	
	if(s->mac.subframes[0].audio_continuity % 80 == 0)
	{
		/* Write out an SI Sound Interpretation
		 * packet at least once per two frames */
		_create_audio_si_packet(&s->mac, data);
		mac_write_packet(s, 0, s->mac.audio_channel, s->mac.subframes[0].audio_continuity - 2, data, 0);
	}
	
	/* Encode and write the audio packet */
	nicam_encode_mac_packet(&s->mac.nicam, data, audio);
	mac_write_packet(s, 0, s->mac.audio_channel, s->mac.subframes[0].audio_continuity++, data, s->mac.scramble_audio);
	
	return(0);
}

static uint8_t _hsync_word(int frame, int line)
{
	int hsync = (frame + line) & 1;
	
	if(line == 623 || line == 624)
	{
		hsync ^= 1;
	}
	
	return(hsync ? MAC_LSW : ~MAC_LSW);
}

static int _line(vid_t *s, uint8_t *data, int x)
{
	uint16_t poly = s->mac.prbs[s->line - 1];
	uint64_t sr5 = 0;
	int i, c;
	
	/* A regular line, contains a short data burst of 105/205 bits for D2/D */
	
	for(c = 0; c < (s->conf.mac_mode == MAC_MODE_D2 ? 1 : 2); c++)
	{
		mac_subframe_t *sf = &s->mac.subframes[c];
		
		for(i = 0; i < 99; i++)
		{
			if(sf->pkt_bits == MAC_PACKET_BITS)
			{
				_mac_packet_queue_item_t pkt;
				
				if(s->line == 623)
				{
					/* Line 623 contains only the last 4 bits
					 * of packet 82. The remainder is empty */
					break;
				}
				
				/* Fetch the next packet */
				_read_packet(&s->mac, &pkt, c);
				
				/* Optionally encrypt packet */
				if(c == 0)
				{
					/* Generate I for PRBS3 - only do this on the first subframe */
					sr5 = _prbs1_update(&s->mac);
				}
				
				if(pkt.scramble)
				{
					_scramble_packet(pkt.pkt, sr5);
				}
				
				_encode_packet(sf->pkt, pkt.address, pkt.continuity, pkt.pkt);
				sf->pkt_bits = 0;
			}
			
			/* Feed in the bits, LSB first */
			x = _bits(data, x, (sf->pkt[sf->pkt_bits >> 3] & 1) ^ _prbs(&poly), 1);
			sf->pkt[sf->pkt_bits >> 3] >>= 1;
			sf->pkt_bits++;
		}
		
		/* For line 623, fill out remainder of the line with zeros */
		for(; i < 99; i++)
		{
			x = _bits(data, x, _prbs(&poly), 1);
		}
	}
	
	if(s->conf.mac_mode == MAC_MODE_D)
	{
		/* D-MAC always ends the packet burst with a spare bit */
		x = _rbits(data, x, 1, 1);
	}
	
	return(x);
}

static int _line_624(vid_t *s, uint8_t *data, int x)
{
	if(s->conf.mac_mode == MAC_MODE_D2)
	{
		/* D2-MAC line 624 contains 67 spare bits (transmitted MSB first) */
		x = _rbits(data, x, 0xAAAAAAAAAAAAAAAAUL, 64);
		x = _rbits(data, x, 0x5, 3);
	}
	else
	{
		/* D-MAC line 624 contains 166 spare bits (transmitted MSB first) */
		x = _rbits(data, x, 0xAAAAAAAAAAAAAAAAUL, 64);
		x = _rbits(data, x, 0xAAAAAAAAAAAAAAAAUL, 64);
		x = _rbits(data, x, 0x2AAAAAAAAAUL, 38);
	}
	
	/* 32-bit clamp marker (transmitted MSB first) */
	x = _rbits(data, x, MAC_CLAMP, 32);
	
	return(x);
}

static int _line_625(vid_t *s, uint8_t *data, int x)
{
	uint16_t poly = s->mac.prbs[s->line - 1];
	uint16_t b;
	uint8_t df[16];
	uint8_t il[69];
	_rdf_t *rdf;
	int dx, ix;
	int i;
	
	/* The clock run-in and frame sync word (transmitted MSB first) */
	x = _rbits(data, x, s->frame & 1 ? MAC_CRI : ~MAC_CRI, 32);
	x = _rbits(data, x, s->frame & 1 ? MAC_FSW : ~MAC_FSW, 64);
	
	/* UDT (transmitted MSB first) */
	ix = _rbits(il, 0, s->mac.udt[s->frame % 25], 5);
	
	/* SDF */
	dx = _bits(df,  0, 0x00B5, 16);        /* CHID Satellite channel identification */
	dx = _bits(df, dx, 0x00, 8);           /* SDFSCR Services configuration reference */
	
	/* MVSCG Multiplex and video scrambling control group */
	/* VSAM Vision scrambling and access mode (3 bits) */
	//b  = 0 << 7;    /* Access type: 0: Free access, 1: Controlled access */
	//b |= 0 << 6;    /* Scramble type: 0: Double-cut rotation, 1: single-cut line rotation */
	//b |= 1 << 5;    /* Scrambled: 0: Scrambled, 1: Unscrambled */
	b  = s->mac.vsam << 5;
	b |= 1 << 4;      /* Reserved, always 1 */
	//b |= 1 << 3;    /* Aspect ratio: 0: 16:9, 1: 4:3 */
	b |= (s->ratio <= (14.0 / 9.0) ? 1 : 0) << 3;
	b |= 1 << 2;      /* For satellite broadcast, this bit has no significance */
	b |= 1 << 1;      /* Sound/data multiplex format: 0: no or incompatible sound, 1: compatible sound */
	b |= 1 << 0;      /* Video configuration: 0: no or incompatible video, 1: compatible video */
	dx = _bits(df, dx, b, 8);
	
	dx = _bits(df, dx, (s->frame >> 8) & 0xFFFFF, 20); /* CAFCNT Conditional access frame count */
	dx = _bits(df, dx, 1, 1);                          /* Rp Repacement */
	dx = _bits(df, dx, 1, 1);                          /* Fp Fingerprint */
	dx = _bits(df, dx, 3, 2);                          /* Unallocated, both bits set to 1 */
	dx = _bits(df, dx, 1, 1);                          /* SIFT Service identification channel format */
	_bch_encode(df, 71, 57);
	
	ix = _bits_buf(il, ix, df, 71);
	
	/* RDF */
	rdf = (s->conf.mac_mode == MAC_MODE_D2 ? _rdf_d2 : _rdf_d);
	dx = _bits(df,  0, s->frame & 0xFF, 8);			/* FCNT (8 bits) */
	dx = _bits(df, dx, 0, 1);				/* UDF (1 bit) */
	dx = _bits(df, dx, rdf[s->mac.rdf].tdmcid, 8);		/* TDMCID (8 bits) */
	dx = _bits(df, dx, rdf[s->mac.rdf].fln1, 10);		/* FLN1 (10 bits) */
	dx = _bits(df, dx, rdf[s->mac.rdf].lln1, 10);		/* LLN1 (10 bits) */
	dx = _bits(df, dx, rdf[s->mac.rdf].fln2, 10);		/* FLN2 (10 bits) */
	dx = _bits(df, dx, rdf[s->mac.rdf].lln2, 10);		/* LLN2 (10 bits) */
	dx = _bits(df, dx, rdf[s->mac.rdf].fcp, 11);		/* FCP (11 bits) */
	dx = _bits(df, dx, rdf[s->mac.rdf].lcp, 11);		/* LCP (11 bits) */
	dx = _bits(df, dx, rdf[s->mac.rdf].links ^= 1, 1);	/* LINKS (1 bit) */
	_bch_encode(df, 94, 80);
	
	s->mac.rdf++;
	if(rdf[s->mac.rdf].tdmcid == 0x00)
	{
		s->mac.rdf = 0;
	}
	
	for(dx = 0; dx < 5; dx++)
	{
		ix = _bits_buf(il, ix, df, 94);
	}
	
	if(s->conf.mac_mode == MAC_MODE_D2)
	{
		x = _bits_buf(data, x, il, ix);
	}
	else
	{
		/* Skip the poly bits for the CRI and FSW */
		for(i = 0; i < 96; i++)
		{
			_prbs(&poly);
		}
		
		x = _bits_buf_il(data, x, il, ix, &poly);
		
		/* The remainder of line 625 is filled with PRBS output */
		while(x < MAC_WIDTH)
		{
			x = _bits(data, x, _prbs(&poly), 1);
		}
	}
	
	return(x);
}

static int _vbi_teletext(vid_t *s, uint8_t *data)
{
	uint16_t poly = s->mac.prbs[s->line - 1];
	uint8_t vbi[45];
	int x, i;
	
	if(!((s->line >= 1 && s->line <= 22) ||
	     (s->line >= 313 && s->line <= 334)))
	{
		/* Not a teletext line */
		return(-1);
	}
	
	/* Fetch the next teletext packet */
	i = tt_next_packet(&s->tt, vbi);
	
	if(i != TT_OK)
	{
		/* No packet ready */
		return(-1);
	}
	
	x = s->conf.mac_mode == MAC_MODE_D2 ? 116 : 230;
	
	for(i = 0; i < 360; i++)
	{
		x = _bits(data, x, vbi[i >> 3] >> (i & 7), 1);
		
		/* In D-MAC the teletext bits are interleaved with PRBS */
		if(s->conf.mac_mode == MAC_MODE_D)
		{
			x = _bits(data, x, _prbs(&poly), 1);
		}
	}
	
	/* The remainder of the line in D-MAC is filled with PRBS output */
	if(s->conf.mac_mode == MAC_MODE_D)
	{
		for(i = 0; i < 172; i++)
		{
			x = _bits(data, x, _prbs(&poly), 1);
			x = _bits(data, x, _prbs(&poly), 1);
		}
	}
	
	return(0);
}

static void _rotate(vid_t *s, int x1, int x2, int xc)
{
	int x;
	
	xc = s->mac.video_scale[xc - 2];
	
	for(x = s->mac.video_scale[x1 - 2]; x <= s->mac.video_scale[x2 + 2]; x++)
	{
		s->output[x * 2 + 1] = s->output[xc++ * 2];
		if(xc > s->mac.video_scale[x2]) xc = s->mac.video_scale[x1];
	}
	
	for(x = s->mac.video_scale[x1 - 2]; x <= s->mac.video_scale[x2 + 2]; x++)
	{
		s->output[x * 2] = s->output[x * 2 + 1];
	}
}

void mac_next_line(vid_t *s)
{
	uint8_t data[MAC_LINE_BYTES];
	int x, y;
	
	/* Blank the +1 line */
	for(x = 0; x < s->width; x++)
	{
		s->output[x * 2] = s->blanking_level;
	}
	
	/* Move to the 0 line */
	vid_adj_delay(s, 1);
	
	if(s->line == 1 && s->mac.eurocrypt)
	{
		eurocrypt_next_frame(s);
	}
	
	if(s->line == 1)
	{
		uint8_t pkt[MAC_PACKET_BYTES];
		
		/* Reset PRBS for packet scrambling */
		_prbs1_reset(&s->mac, s->frame - 1);
		
		/* Update the aspect ratio flag */
		s->mac.ratio = (s->ratio <= (14.0 / 9.0) ? 0 : 1);
		
		/* Push a service information packet at the start of each new
		 * frame. Alternates between DG0 and DG3 each frame. DG0 is
		 * added to both subframes for D-MAC */
		switch(s->frame & 1)
		{
		case 0: /* Write DG0 to 1st and 2nd subframes */
			
			_create_si_dg0_packet(&s->mac, pkt);
			
			mac_write_packet(s, 0, 0x000, 0, pkt, 0);
			
			if(s->conf.mac_mode == MAC_MODE_D)
			{
				mac_write_packet(s, 1, 0x000, 0, pkt, 0);
			}
			
			break;
		
		case 1: /* Write DG3 to 1st subframe */
			
			_create_si_dg3_packet(&s->mac, pkt);
			mac_write_packet(s, 0, 0x000, 0, pkt, 0);
			
			break;
		}
		
		/* Update the UDT date and time every 25 frames */
		if(s->frame % 25 == 0)
		{
			_update_udt(s->mac.udt, time(NULL));
		}
	}
	
	/* Clear the data buffer. This is where the duobinary data is packed */
	memset(data, 0, MAC_LINE_BYTES);
	
	x = 0;
	
	if(s->conf.mac_mode == MAC_MODE_D)
	{
		/* D-MAC always begins with a single run-in bit */
		x = _rbits(data, x, 1, 1);
	}
	
	/* Apply the line sync word (transmitted MSB first) */
	x = _rbits(data, x, _hsync_word(s->frame, s->line), 6);
	
	/* Apply the remainder of the line */
	if(s->line == 625)
	{
		x = _line_625(s, data, x);
	}
	else if(s->line == 624)
	{
		x = _line_624(s, data, x);
	}
	else
	{
		x = _line(s, data, x);
	}
	
	/* Generate the teletext data, if enabled */
	if(s->conf.teletext)
	{
		_vbi_teletext(s, data);
	}
	
	/* Render the duobinary into the line */
	_render_duobinary(s, data, (s->conf.mac_mode == MAC_MODE_D2 ? 648 : 1296));
	
	/* Flatten the clamping areas */
	/*if(s->line <= 624)
	{
		for(x = s->mac.video_scale[207]; x < s->mac.video_scale[207 + 20]; x++)
		{
			s->output[x * 2] = s->blanking_level;
		}
	}*/
	
	/* Lines 23 and 335 have a black luminance reference area */
	if(s->line == 23 || s->line == 335)
	{
		for(x = s->active_left; x < s->active_left + s->active_width; x++)
		{
			s->output[x * 2] = s->black_level;
		}
	}
	
	/* Line 624 has grey, black and white reference areas */
	if(s->line == 624)
	{
		/* White */
		for(x = s->mac.white_ref_left; x < s->mac.black_ref_left; x++)
		{
			s->output[x * 2] = s->white_level;
		}
		
		/* Black */
		for(; x < s->mac.black_ref_right; x++)
		{
			s->output[x * 2] = s->black_level;
		}
	}
	
	/* Render the luminance */
	y = -1;
	
	if(s->line >= 24 && s->line <= 310)
	{
		/* Top field */
		y = (s->line - 24) * 2 + 2;
	}
	else if(s->line >= 336 && s->line <= 622)
	{
		/* Bottom field */
		y = (s->line - 336) * 2 + 1;
	}
	
	if(y >= 0)
	{
		uint32_t *px = (s->framebuffer != NULL ? &s->framebuffer[y * s->active_width] : NULL);
		
		for(x = s->active_left; x < s->active_left + s->active_width; x++)
		{
			uint32_t rgb = (px != NULL ? *(px++) & 0xFFFFFF : 0x000000);
			s->output[x * 2] = s->y_level_lookup[rgb];
		}
	}
	
	/* Render the chrominance (one line ahead of the luminance) */
	vid_adj_delay(s, 1);
	
	if(y >= 0)
	{
		uint32_t *px = (s->framebuffer != NULL ? &s->framebuffer[y * s->active_width] : NULL);
		
		for(x = s->mac.chrominance_left; x < s->mac.chrominance_left + s->mac.chrominance_width; x++)
		{
			uint32_t rgb = (px != NULL ? *(px++) & 0xFFFFFF : 0x000000);
			s->output[x * 2] += (s->line & 1 ? s->q_level_lookup[rgb] : s->i_level_lookup[rgb]);
			if(px != NULL) px++;
		}
	}
	
	/* Scramble the line if enabled */
	if((s->mac.vsam & 1) == 0)
	{
		uint16_t prbs;
		
		/* Reset CA PRBS2 at the beginning of each frame */
		if(s->line == 1)
		{
			_prbs2_reset(&s->mac, s->frame - 1);
		}
		
		/* Fetch the next CA PRBS2 code */
		prbs = _prbs2_update(&s->mac);
		
		if(y >= 0)
		{
			if((s->mac.vsam & 2) == 0)
			{
				/* Double Cut rotation */
				_rotate(s, 229,  580, 282 + ((prbs & 0xFF00) >> 8)); /* Colour-diff */
				_rotate(s, 586, 1285, 682 + ((prbs & 0x00FF) << 1)); /* Luminance */
			}
			else
			{
				/* Single Cut rotation */
				_rotate(s, 230, 1285, 282 + ((prbs & 0xFF00) >> 8));
			}
		}
	}
}

