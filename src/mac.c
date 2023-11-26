/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2022 Philip Heron <phil@sanslogic.co.uk>                    */
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
#include "mac.h"

/* MAC sync codes */
#define MAC_CLAMP 0xEAF3927FUL
#define MAC_LSW   0x0B
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

/* Pre-calculated J.17 pre-emphasis filter taps, 32kHz no low pass (high quality) */
static const double _j17_hq_taps[] = {
	-4.0638183114837725e-05, -6.0025586949698855e-05,
	-5.564043110245245e-05, -7.947984626961884e-05, -7.647905950877803e-05,
	-0.00010634219837159823, -0.00010573530630851049,
	-0.0001439262656801037, -0.0001473418005545947, -0.00019733025737867933,
	-0.00020744600452448253, -0.00027462470991584573,
	-0.0002959626579938912, -0.00038903864897297304,
	-0.00042951430210921436, -0.0005632229082537514, -0.0006373725671138244,
	-0.0008381833027058537, -0.0009744643521200177, -0.0012937307193290994,
	-0.001552751532064818, -0.0021004781086707324, -0.002625860940471562,
	-0.003668275121553212, -0.004845513644185225, -0.007121819711885047,
	-0.010127737762890622, -0.01599147906480894, -0.024853521327126284,
	-0.04278009416709207, -0.07239348501988721, -0.13738444727018054,
	0.7809561590505447, -0.1373844472702732, -0.07239348501983206,
	-0.04278009416712298, -0.02485352132710135, -0.01599147906482651,
	-0.010127737762873385, -0.007121819711900977, -0.0048455136441711835,
	-0.0036682751215641306, -0.0026258609404633286, -0.0021004781086770207,
	-0.0015527515320562413, -0.0012937307193406264, -0.0009744643521089602,
	-0.0008381833027124772, -0.0006373725671163757, -0.000563222908240734,
	-0.00042951430212660697, -0.00038903864895887066,
	-0.0002959626579998339, -0.0002746247099202026, -0.00020744600451413447,
	-0.00019733025738687381, -0.00014734180055051493,
	-0.0001439262656806838, -0.0001057353063093944, -0.00010634219837524443,
	-7.647905950078356e-05, -7.947984627567915e-05, -5.564043109916536e-05,
	-6.0025586949491874e-05, -4.063818311961655e-05
};

/* Pre-calculated J.17 pre-emphasis filter taps, 32kHz with 8kHz low pass (medium quality) */
static const double _j17_mq_taps[] = {
-0.0023983764740491817, 0.0005372369031780615, 0.0029088459406962433,
	-0.0006808310346680438, -0.0038400522316643093, 0.0003905164235471325,
	0.004598081791062745, -0.0003030199522471644, -0.005996593890780044,
	-0.0006099393949867584, 0.006900939227982249, 0.0011126454561315397,
	-0.008845190642068847, -0.0031512342941305426, 0.009594941549957162,
	0.0042182070235715335, -0.012261809244109948, -0.008289287373445343,
	0.012146295526980532, 0.009903710389951048, -0.016223088515771884,
	-0.018241246604564477, 0.013268559591784575, 0.019670208896615235,
	-0.02196471821546391, -0.040897915745289205, 0.007376815392497354,
	0.03632945551391454, -0.04261000237464269, -0.13505868867468723,
	-0.05987965058731034, 0.164415182169897, 0.29045946882740054,
	0.16441518216987408, -0.059879650587328914, -0.13505868867468418,
	-0.04261000237463042, 0.03632945551391707, 0.007376815392491072,
	-0.04089791574529129, -0.02196471821545942, 0.019670208896617532,
	0.013268559591781671, -0.01824124660456619, -0.016223088515769372,
	0.009903710389952778, 0.01214629552697836, -0.00828928737344731,
	-0.01226180924410831, 0.004218207023573651, 0.009594941549956208,
	-0.003151234294132517, -0.008845190642068512, 0.0011126454561330129,
	0.006900939227982167, -0.0006099393949875143, -0.005996593890779387,
	-0.00030301995224640626, 0.004598081791061394, 0.0003905164235454991,
	-0.0038400522316629185, -0.0006808310346651593, 0.002908845940695833,
	0.0005372369031742542, -0.0023983764740508006
};

/* MAC audio scaling factors */
typedef struct {
	int factor;
	int shift;
	int coding_range;
	int protection_range;
} _scale_factor_t;

static const _scale_factor_t _scale_factors[8] = {
	{ 0, 2, 5, 7 }, /* 0b000 */
	{ 1, 2, 5, 7 }, /* 0b001 */
	{ 2, 2, 5, 6 }, /* 0b010 */
	{ 4, 2, 5, 5 }, /* 0b100 */
	{ 3, 3, 4, 4 }, /* 0b011 */
	{ 5, 4, 3, 3 }, /* 0b101 */
	{ 6, 5, 2, 2 }, /* 0b110 */
	{ 7, 6, 1, 1 }, /* 0b111 */
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
	offset = (double) width / 1296 * (mode == MAC_MODE_D2 ? -3 : -1);
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

static void _render_duobinary(vid_t *s, vid_line_t **lines, uint8_t *data, int nbits)
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
		
		l = 1;
		xo = *taps;
		
		if(xo < 0)
		{
			l = 0;
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
			
			t = lines[l]->output[xo * 2] + (symbol == 1 ? taps[x] : -taps[x]);
			
			/* Don't let the duobinary signal clip */
			if(t < INT16_MIN) t = INT16_MIN;
			else if(t > INT16_MAX) t = INT16_MAX;
			
			lines[l]->output[xo * 2] = t;
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
	b |= s->audio.address;		/* Packet address of the main TV sound */
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
		/* PG */
		pkt[x++] = 0x80;
		pkt[x++] = 0x0D;
		
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
	
	b = 0x0400 | s->audio.address;  /* 0 0 0 0 01 audio_channel */
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
	
	s->audio = 1; /* MAC always has audio */
	
	memset(mac, 0, sizeof(mac_t));
	
	mac->vsam = MAC_VSAM_FREE_ACCESS;
	
	/* Calculate the threshold pixel aspect ratio for auto aspect ratio */
	mac->ratio_threshold = rational_div(
		(rational_t) { 14, 9 },
		(rational_t) { s->active_width, s->conf.active_lines }
	);
	
	/* Initialise Eurocrypt, if required */
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
	
	/* Initialise main TV audio */
	mac_audioenc_init(&mac->audio,
		s->conf.mac_audio_quality,
		s->conf.mac_audio_stereo,
		s->conf.mac_audio_protection,
		s->conf.mac_audio_companded,
		s->conf.scramble_audio,
		(s->mac.vsam & MAC_VSAM_CONTROLLED_ACCESS ? 1 : 0)
	);
	
	if(s->conf.mac_mode == MAC_MODE_D)
	{
		/* BSB receivers ignore the SI packets and
		 * always expect audio at packet address 128. */
		mac->audio.address = 128; /* Stereo NICAM 32kHz, level 1 protection */
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
	
	mac->subframes[0].pkt_bits = MAC_PACKET_BITS;
	mac->subframes[1].pkt_bits = MAC_PACKET_BITS;
	
	mac->polarity = -1;
	mac->lut = _duobinary_lut(s->conf.mac_mode, s->width, (s->white_level - s->black_level) * 0.4);
	
	/* Set the video properties */
	s->active_width &= ~1;	/* Ensure the active width is an even number */
	mac->chrominance_width = s->active_width / 2;
	mac->chrominance_left  = round(s->pixel_rate * (233.5 / MAC_CLOCK_RATE));
	mac->white_ref_left    = round(s->pixel_rate * (371.0 / MAC_CLOCK_RATE));
	mac->black_ref_left    = round(s->pixel_rate * (533.0 / MAC_CLOCK_RATE));
	mac->black_ref_right   = round(s->pixel_rate * (695.0 / MAC_CLOCK_RATE));
	
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
	mac_audioenc_free(&mac->audio);
}

static const _scale_factor_t *_scale_factor(const int16_t *pcm, int len, int step)
{
	int i, b;
	int16_t s;
	
	/* Calculate the optimal scale factor for this audio block */
	b = 1;
	
	/* Test each sample if it requires a larger range */
	for(i = 0; b < 7 && i < len; i++)
	{
		/* Negative values use the same scales */
		s = (*pcm < 0) ? ~*pcm : *pcm;
		
		/* Test if the scale factor needs to be increased */
		while(b < 7 && s >> (b + 8))
		{
			b++;
		}
		
		pcm += step;
	}
	
	return(&_scale_factors[b]);
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

int mac_write_audio(vid_t *s, mac_audioenc_t *enc, int subframe, const int16_t *audio, int len)
{
	const uint8_t *pkt;
	
	if(enc->si_timer <= 0)
	{
		/* Write out a Sound Interpretation (SI) packet */
		mac_write_packet(s, subframe, enc->address, enc->continuity - 2, enc->si_pkt, 0);
		
		/* Set the timer for the next SI packet in about 1/3 of a second */
		enc->si_timer = (enc->high_quality ? 32000 : 16000) / 3;
	}
	
	mac_audioenc_write(enc, audio, len);
	
	while((pkt = mac_audioenc_read(enc)) != NULL)
	{
		mac_write_packet(s, subframe, enc->address, enc->continuity++, pkt, enc->scramble);
	}
	
	return(0);
}

static void _audioenc_si_packet(mac_audioenc_t *enc, uint8_t *pkt)
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
	
	b |= (enc->protection ? 1 : 0) << 7; /* Level of error protection (0: first level, 1: second level) */
	b |= (enc->linear ? 0 : 1) << 6; /* Coding law (0: linear, 1: companded) */
	b |= enc->conditional << 5; /* Controlled access (0: no, 1: yes) */
	b |= enc->scramble << 4; /* Scrambling (0: no, 1: yes) */
	b |= 0 << 3; /* Automatic mixing (0: mixing not intended, 1: mixing intended) */
	b |= (enc->stereo ? 1 : 0) << 2; /* Channels (0: Mono, 1: Stereo) */
	b |= (enc->high_quality ? 0 : 1) << 1; /* Sample rate (0: 32kHz, 1: 16kHz) */
	b |= 0 << 0; /* Reserved */
	b |= _parity(b) << 8; /* Parity bit */
	
	for(x = 0; x < 5; x++)
	{
		pkt[7 + x * 2] = (b & 0xFF00) >> 8;
		pkt[8 + x * 2] = (b & 0x00FF) >> 0;
	}
}

int mac_audioenc_init(mac_audioenc_t *enc, int high_quality, int stereo, int protection, int linear, int scramble, int conditional)
{
	int i;
	
	memset(enc, 0, sizeof(mac_audioenc_t));
	
	/* Channel configuration */
	enc->high_quality = high_quality ? 1 : 0;
	enc->stereo = stereo ? 1 : 0;
	enc->linear = linear ? 1 : 0;
	enc->protection = protection ? 1 : 0;
	enc->scramble = scramble ? 1 : 0;
	enc->conditional = (conditional ? 1 : 0) & enc->scramble;
	
	/* Packet details */
	enc->address = _calculate_audio_address(
		enc->stereo,
		enc->high_quality,
		enc->protection,
		enc->linear,
		0);
	enc->continuity = 0;
	
	/* Initalise J.17 FIR filters */
	if(enc->high_quality)
	{
		fir_int16_init(&enc->channel[0].fir, _j17_hq_taps, sizeof(_j17_hq_taps) / sizeof(double), 1, 1, 0);
		fir_int16_init(&enc->channel[1].fir, _j17_hq_taps, sizeof(_j17_hq_taps) / sizeof(double), 1, 1, 0);
	}
	else
	{
		/* Medium quality applies an 8 kHz low pass filter, for x2 decimation (32kHz > 16kHz) */
		fir_int16_init(&enc->channel[0].fir, _j17_mq_taps, sizeof(_j17_mq_taps) / sizeof(double), 1, 2, 0);
		fir_int16_init(&enc->channel[1].fir, _j17_mq_taps, sizeof(_j17_mq_taps) / sizeof(double), 1, 2, 0);
	}
	
	/* Linear + L2 protection mode has 36 samples per coding block,
	 * all others are 64 */
	enc->samples_per_block = enc->linear && enc->protection ? 36 : 64;
	
	/* Medium Quality requires twice the number of source samples
	 * due to the 2x decimation in the FIR filter stage */
	enc->src_samples_per_block = enc->samples_per_block * (enc->high_quality ? 1 : 2);
	
	/* Per channel encoder settings */
	for(i = 0; i < 2; i++)
	{
		enc->channel[i].len = enc->samples_per_block / 2;
		enc->channel[i].offset = (i ? (enc->stereo ? 1 : enc->channel[i].len) : 0);
		enc->channel[i].src_len = enc->channel[i].len * (enc->high_quality ? 1 : 2);
		enc->channel[i].src_offset = (i ? (enc->stereo ? 1 : enc->channel[i].src_len) : 0);
		enc->channel[i].sf_len = (enc->linear && enc->protection ? 18 : 27);
		enc->channel[i].sf_offset = (i ? (enc->stereo ? 1 : enc->channel[i].sf_len) : 0);
	}
	
	/* Bits per coded sample */
	enc->bits_per_sample  = enc->linear ? 14 : 10;
	enc->bits_per_sample += enc->protection ? 5 : 1;
	
	/* Sound coding block length (bytes) */
	enc->block_len = enc->linear ^ enc->protection ? 120 : 90;
	enc->x = enc->block_len;
	
	/* PT Packet Type */
	enc->pkt[0] = 0xC7; /* Sound Coding Block (BC1) */
	
	enc->j17x = 0;
	enc->pktx = 1;
	
	/* Initalise SI packet */
	_audioenc_si_packet(enc, enc->si_pkt);
	enc->si_timer = 0;
	
	return(0);
}

int mac_audioenc_free(mac_audioenc_t *enc)
{
	fir_int16_free(&enc->channel[0].fir);
	fir_int16_free(&enc->channel[1].fir);
	return(0);
}

static uint8_t _l2_hamming(uint16_t b)
{
	uint8_t p;
	
	p  = (((b >> 0) ^ (b >> 3) ^ (b >> 4) ^ (b >> 6) ^ (b >> 7) ^ (b >> 8) ^ (b >> 10)) & 1) << 0;
	p |= (((b >> 0) ^ (b >> 1) ^ (b >> 3) ^ (b >> 5) ^ (b >> 6) ^ (b >> 8) ^ (b >>  9)) & 1) << 1;
	p |= (((b >> 0) ^ (b >> 1) ^ (b >> 2) ^ (b >> 4) ^ (b >> 6) ^ (b >> 7) ^ (b >>  9)) & 1) << 2;
	p |= (((b >> 1) ^ (b >> 2) ^ (b >> 4) ^ (b >> 5) ^ (b >> 6) ^ (b >> 8) ^ (b >> 10)) & 1) << 3;
	p |= (((b >> 2) ^ (b >> 3) ^ (b >> 5) ^ (b >> 6) ^ (b >> 7) ^ (b >> 9) ^ (b >> 10)) & 1) << 4;
	
	return(p);
}

const uint8_t *mac_audioenc_read(mac_audioenc_t *enc)
{
	const _scale_factor_t *sf;
	uint32_t s[64];
	int bx = 0;
	int step;
	int i, a, b;
	uint32_t sfc = 0;
	
	/* Copy excess data from previous run into start of packet */
	for(; enc->pktx < MAC_PAYLOAD_BYTES && enc->x < enc->block_len; enc->pktx++, enc->x++)
	{
		enc->pkt[enc->pktx] = enc->block[enc->x];
	}
	
	/* Return the packet if it's full */
	if(enc->pktx == MAC_PAYLOAD_BYTES)
	{
		enc->pktx = 1;
		return(enc->pkt);
	}
	
	/* Copy the audio into the filter buffer */
	if(enc->stereo)
	{
		for(; enc->j17x < enc->src_samples_per_block && enc->audio_len > 0; enc->j17x++, enc->audio_len--)
		{
			enc->j17[enc->j17x] = *(enc->audio++);
		}
	}
	else
	{
		for(; enc->j17x < enc->src_samples_per_block && enc->audio_len > 0; enc->j17x++, enc->audio_len -= 2)
		{
			/* Downmix stereo input to mono */
			enc->j17[enc->j17x] = (enc->audio[0] + enc->audio[1]) / 2;
			enc->audio += 2;
		}
	}
	
	if(enc->j17x != enc->src_samples_per_block)
	{
		/* Didn't get enough audio, request more */
		return(NULL);
	}
	
	enc->j17x = 0;
	
	/* The step between each sample for this block */
	step = enc->stereo ? 2 : 1;
	
	/* Process each block or channel */
	for(b = 0; b < 2; b++)
	{
		/* Apply J.17 pre-emphasis filter */
		fir_int16_process(
			&enc->channel[enc->stereo ? b : 0].fir,
			enc->j17 + enc->channel[b].offset,
			enc->j17 + enc->channel[b].src_offset,
			enc->channel[b].src_len,
			step
		);
		
		/* Calculate scale factors */
		sf = _scale_factor(
			enc->j17 + enc->channel[b].offset,
			enc->channel[b].len,
			step
		);
		sfc = (sfc << 9) | (sf->factor << 6) | (sf->factor << 3) | sf->factor;
		
		/* Encode the samples */
		a = enc->channel[b].offset;
		
		if(enc->linear)
		{
			/* Linear */
			for(i = 0; i < enc->channel[b].len; i++, a += step)
			{
				/* Shift 16-bit sample to 14-bit */
				s[a] = (enc->j17[a] >> 2) & 0x3FFF;
			}
		}
		else
		{
			/* Companded */
			for(i = 0; i < enc->channel[b].len; i++, a += step)
			{
				/* Shift 16-bit sample to 10-bit companded */
				s[a] = (enc->j17[a] >> sf->shift) & 0x3FF;
			}
		}
		
		/* Apply protection */
		a = enc->channel[b].offset;
		
		if(enc->protection)
		{
			/* Second level */
			for(i = 0; i < enc->channel[b].len; i++, a += step)
			{
				s[a] |= _l2_hamming(enc->linear ? s[a] >> 3 : (s[a] << 1) & 0x7E0) << (enc->bits_per_sample - 5);
			}
		}
		else
		{
			/* First level */
			for(i = 0; i < enc->channel[b].len; i++, a += step)
			{
				s[a] |= _parity(s[a] >> (enc->linear ? 3 : 4)) << (enc->bits_per_sample - 1);
			}
		}
		
		/* Apply scale factor code */
		a = enc->channel[b].sf_offset;
		for(i = 0; i < enc->channel[b].sf_len; i++, a += step)
		{
			s[a] ^= ((sf->factor >> (2 - (i % 3))) & 1) << (enc->bits_per_sample - 1);
		}
	}
	
	/* L1 companded blocks start with two reserved bytes */
	if(!enc->linear && !enc->protection) bx = _bits(enc->block, bx, 0, 16);
	
	/* L2 linear blocks have a 36-bit header */
	if(enc->linear && enc->protection)
	{
		bx = _bits(enc->block, bx, 0, 8); /* Reserved */
		bx = _bits(enc->block, bx, 0, 10);
		bx = _rbits(enc->block, bx, sfc, 18);
	}
	
	/* Pack the samples into the sound coding block */
	for(i = 0; i < enc->samples_per_block; i++)
	{
		bx = _bits(enc->block, bx, s[i], enc->bits_per_sample);
	}
	
	enc->x = 0;
	enc->si_timer -= enc->stereo ? enc->samples_per_block : enc->samples_per_block / 2;
	
	for(; enc->pktx < MAC_PAYLOAD_BYTES && enc->x < enc->block_len; enc->pktx++, enc->x++)
	{
		enc->pkt[enc->pktx] = enc->block[enc->x];
	}
	
	enc->pktx = 1;
	
	return(enc->pkt);
}

int mac_audioenc_write(mac_audioenc_t *enc, const int16_t *audio, size_t len)
{
	enc->audio = audio;
	enc->audio_len = len;
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

static int _line(vid_t *s, int frame, int line, uint8_t *data, int x)
{
	uint16_t poly = s->mac.prbs[line - 1];
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
				
				if(line == 623)
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

static int _line_624(vid_t *s, int frame, int line, uint8_t *data, int x)
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

static int _line_625(vid_t *s, int frame, int line, uint8_t *data, int x)
{
	uint16_t poly = s->mac.prbs[line - 1];
	uint16_t b;
	uint8_t df[16];
	uint8_t il[69];
	_rdf_t *rdf;
	int dx, ix;
	int i;
	
	/* The clock run-in and frame sync word (transmitted MSB first) */
	x = _rbits(data, x, frame & 1 ? MAC_CRI : ~MAC_CRI, 32);
	x = _rbits(data, x, frame & 1 ? MAC_FSW : ~MAC_FSW, 64);
	
	/* UDT (transmitted MSB first) */
	ix = _rbits(il, 0, s->mac.udt[frame % 25], 5);
	
	/* SDF */
	dx = _bits(df,  0, s->conf.chid, 16);  /* CHID Satellite channel identification */
	dx = _bits(df, dx, 0x00, 8);           /* SDFSCR Services configuration reference */
	
	/* MVSCG Multiplex and video scrambling control group */
	/* VSAM Vision scrambling and access mode (3 bits) */
	//b  = 0 << 7;    /* Access type: 0: Free access, 1: Controlled access */
	//b |= 0 << 6;    /* Scramble type: 0: Double-cut rotation, 1: single-cut line rotation */
	//b |= 1 << 5;    /* Scrambled: 0: Scrambled, 1: Unscrambled */
	b  = s->mac.vsam << 5;
	b |= 1 << 4;      /* Reserved, always 1 */
	//b |= 1 << 3;    /* Aspect ratio: 0: 16:9, 1: 4:3 */
	b |= (rational_cmp(s->vframe.pixel_aspect_ratio, s->mac.ratio_threshold) <= 0 ? 1 : 0) << 3;
	b |= 1 << 2;      /* For satellite broadcast, this bit has no significance */
	b |= 1 << 1;      /* Sound/data multiplex format: 0: no or incompatible sound, 1: compatible sound */
	b |= 1 << 0;      /* Video configuration: 0: no or incompatible video, 1: compatible video */
	dx = _bits(df, dx, b, 8);
	
	dx = _bits(df, dx, (frame >> 8) & 0xFFFFF, 20);    /* CAFCNT Conditional access frame count */
	dx = _bits(df, dx, 1, 1);                          /* Rp Replacement */
	dx = _bits(df, dx, 1, 1);                          /* Fp Fingerprint */
	dx = _bits(df, dx, 3, 2);                          /* Unallocated, both bits set to 1 */
	dx = _bits(df, dx, 0, 1);                          /* SIFT Service identification channel format */
	_bch_encode(df, 71, 57);
	
	ix = _bits_buf(il, ix, df, 71);
	
	/* RDF */
	rdf = (s->conf.mac_mode == MAC_MODE_D2 ? _rdf_d2 : _rdf_d);
	dx = _bits(df,  0, frame & 0xFF, 8);			/* FCNT (8 bits) */
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

static int _vbi_teletext(vid_t *s, uint8_t *data, int frame, int line)
{
	uint16_t poly = s->mac.prbs[line - 1];
	uint8_t vbi[45];
	int x, i;
	
	if(!((line >=   1 && line <=  22) ||
	     (line >= 313 && line <= 334)))
	{
		/* Not a teletext line */
		return(-1);
	}
	
	/* Fetch the next teletext packet */
	i = tt_next_packet(&s->tt, vbi, frame, line);
	
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

static void _rotate(vid_t *s, int16_t *output, int x1, int x2, int xc)
{
	int x;
	
	xc = s->mac.video_scale[xc - 2];
	
	for(x = s->mac.video_scale[x1 - 2]; x <= s->mac.video_scale[x2 + 2]; x++)
	{
		output[x * 2 + 1] = output[xc++ * 2];
		if(xc > s->mac.video_scale[x2]) xc = s->mac.video_scale[x1];
	}
	
	for(x = s->mac.video_scale[x1 - 2]; x <= s->mac.video_scale[x2 + 2]; x++)
	{
		output[x * 2] = output[x * 2 + 1];
	}
}

int mac_next_line(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	uint8_t data[MAC_LINE_BYTES];
	vid_line_t *l = lines[1];
	int x, y;
	
	l->width    = s->width;
	l->frame    = s->bframe;
	l->line     = s->bline;
	l->vbialloc = 0;
	
	/* Blank the +1 line */
	for(x = 0; x < s->width; x++)
	{
		lines[2]->output[x * 2] = s->blanking_level;
	}
	
	if(l->line == 1 && s->mac.eurocrypt)
	{
		eurocrypt_next_frame(s, l->frame);
	}
	
	if(l->line == 1)
	{
		uint8_t pkt[MAC_PACKET_BYTES];
		
		/* Reset PRBS for packet scrambling */
		_prbs1_reset(&s->mac, l->frame - 1);
		
		/* Update the aspect ratio flag */
		s->mac.ratio = (rational_cmp(s->vframe.pixel_aspect_ratio, s->mac.ratio_threshold) <= 0 ? 0 : 1);
		
		/* Push the DG0 and DG3 SI packets every four frames.
		 * DG0 is sent on both subframes for D-MAC. */
		switch(l->frame & 3)
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
		if(l->frame % 25 == 0)
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
	x = _rbits(data, x, _hsync_word(l->frame, l->line), 6);
	
	/* Apply the remainder of the line */
	if(l->line == 625)
	{
		x = _line_625(s, l->frame, l->line, data, x);
	}
	else if(l->line == 624)
	{
		x = _line_624(s, l->frame, l->line, data, x);
	}
	else
	{
		x = _line(s, l->frame, l->line, data, x);
	}
	
	/* Generate the teletext data, if enabled */
	if(s->conf.teletext)
	{
		_vbi_teletext(s, data, l->frame, l->line);
	}
	
	/* Render the duobinary into the line */
	_render_duobinary(s, lines, data, (s->conf.mac_mode == MAC_MODE_D2 ? 648 : 1296));
	
	/* Flatten the clamping areas */
	/*if(l->line <= 624)
	{
		for(x = s->mac.video_scale[207]; x < s->mac.video_scale[207 + 20]; x++)
		{
			l->output[x * 2] = s->blanking_level;
		}
	}*/
	
	/* Lines 23 and 335 have a black luminance reference area */
	if(l->line == 23 || l->line == 335)
	{
		for(x = s->active_left; x < s->active_left + s->active_width; x++)
		{
			l->output[x * 2] = s->black_level;
		}
	}
	
	/* Line 624 has grey, black and white reference areas */
	if(l->line == 624)
	{
		/* White */
		for(x = s->mac.white_ref_left; x < s->mac.black_ref_left; x++)
		{
			l->output[x * 2] = s->white_level;
		}
		
		/* Black */
		for(; x < s->mac.black_ref_right; x++)
		{
			l->output[x * 2] = s->black_level;
		}
	}
	
	/* Render the luminance */
	y = -1;
	
	if(l->line >= 24 && l->line <= 310)
	{
		/* Top field */
		y = (l->line - 24) * 2 + 2;
	}
	else if(l->line >= 336 && l->line <= 622)
	{
		/* Bottom field */
		y = (l->line - 336) * 2 + 1;
	}
	
	/* Shift the lines by one if the source
	 * video has the bottom field first */
	if(s->vframe.interlaced == 2) y += 1;
	
	if(y < 0 || y >= s->conf.active_lines) y = -1;
	
	/* Render the luminance */
	if(y >= 0)
	{
		uint32_t rgb = 0x000000;
		uint32_t *px = &rgb;
		int stride = 0;
		
		if(s->vframe.framebuffer != NULL)
		{
			px = &s->vframe.framebuffer[y * s->vframe.line_stride];
			stride = s->vframe.pixel_stride;
		}
		
		for(x = s->active_left; x < s->active_left + s->active_width; x++, px += stride)
		{
			l->output[x * 2] = s->yiq_level_lookup[*px & 0xFFFFFF].y;
		}
	}
	
	/* Render the chrominance (one line ahead of the luminance) */
	l = lines[0];
	
	if(y >= 0)
	{
		uint32_t rgb = 0x000000;
		uint32_t *px = &rgb;
		int stride = 0;
		
		if(s->vframe.framebuffer != NULL)
		{
			px = &s->vframe.framebuffer[y * s->vframe.line_stride];
			stride = s->vframe.pixel_stride * 2;
		}
		
		for(x = s->mac.chrominance_left; x < s->mac.chrominance_left + s->mac.chrominance_width; x++, px += stride)
		{
			l->output[x * 2] += (l->line & 1 ? s->yiq_level_lookup[*px & 0xFFFFFF].i : s->yiq_level_lookup[*px & 0xFFFFFF].q);
		}
	}
	
	/* Scramble the line if enabled */
	if((s->mac.vsam & 1) == 0)
	{
		uint16_t prbs;
		
		/* Reset CA PRBS2 at the beginning of each frame */
		if(l->line == 1)
		{
			_prbs2_reset(&s->mac, l->frame - 1);
		}
		
		/* Fetch the next CA PRBS2 code */
		prbs = _prbs2_update(&s->mac);
		
		if(y >= 0)
		{
			if((s->mac.vsam & 2) == 0)
			{
				/* Double Cut rotation */
				_rotate(s, l->output, 229,  580, 282 + ((prbs & 0xFF00) >> 8)); /* Colour-diff */
				_rotate(s, l->output, 586, 1285, 682 + ((prbs & 0x00FF) << 1)); /* Luminance */
			}
			else
			{
				/* Single Cut rotation */
				_rotate(s, l->output, 230, 1285, 282 + ((prbs & 0xFF00) >> 8));
			}
		}
	}
	
	/* Clear the Q channel */
	for(x = 0; x < s->width; x++)
	{
		l->output[x * 2 + 1] = 0;
	}
	
	return(1);
}

