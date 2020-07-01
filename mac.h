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

#ifndef _MAC_H
#define _MAC_H

#define MAC_CLOCK_RATE 20250000
#define MAC_WIDTH 1296
#define MAC_LINES 625

/* The two data modes */
#define MAC_MODE_D  0
#define MAC_MODE_D2 1

/* MAC VSAM modes */
#define MAC_VSAM_DOUBLE_CUT        (0 << 0)
#define MAC_VSAM_UNSCRAMBLED       (1 << 0)
#define MAC_VSAM_SINGLE_CUT        (2 << 0)

#define MAC_VSAM_FREE_ACCESS       (0 << 2)
#define MAC_VSAM_CONTROLLED_ACCESS (1 << 2)

#define MAC_VSAM_FREE_ACCESS_DOUBLE_CUT       0 /* 000: free access, double-cut component rotation scrambling */
#define MAC_VSAM_FREE_ACCESS_UNSCRAMBLED      1 /* 001: free access, unscrambled */
#define MAC_VSAM_FREE_ACCESS_SINGLE_CUT       2 /* 010: free access, single-cut line rotation scrambling */
#define MAC_VSAM_CONTROLLED_ACCESS_DOUBLE_CUT 4 /* 100: controlled access, double-cut component rotation scrambling */
#define MAC_VSAM_CONTROLLED_ACCESS_SINGLE_CUT 6 /* 110: controlled access, single-cut line rotation scrambling */

/* Video aspect ratios */
#define MAC_RATIO_4_3  0
#define MAC_RATIO_16_9 1

/* Number of bits and bytes in a packet, bytes rounded up */
#define MAC_PACKET_BITS   751
#define MAC_PACKET_BYTES  94

/* Number of bits and bytes in a packet payload */
#define MAC_PAYLOAD_BITS  728
#define MAC_PAYLOAD_BYTES 91

/* Number of packets in the transmit queue */
#define MAC_QUEUE_LEN 12

/* Maximum number of bytes per line (for D-MAC, D2 is half) */
#define MAC_LINE_BYTES (MAC_WIDTH / 8)

/* Audio defines */
#define MAC_MEDIUM_QUALITY 0
#define MAC_HIGH_QUALITY   1

#define MAC_MONO   0
#define MAC_STEREO 1

#define MAC_COMPANDED 0
#define MAC_LINEAR    1

#define MAC_FIRST_LEVEL_PROTECTION  0
#define MAC_SECOND_LEVEL_PROTECTION 1

/* CA PRBS defines */
#define MAC_PRBS_CW_FA    (((uint64_t) 1 << 60) - 1)
#define MAC_PRBS_CW_MASK  (((uint64_t) 1 << 60) - 1)
#define MAC_PRBS_SR1_MASK (((uint32_t) 1 << 31) - 1)
#define MAC_PRBS_SR2_MASK (((uint32_t) 1 << 29) - 1)
#define MAC_PRBS_SR3_MASK (((uint32_t) 1 << 31) - 1)
#define MAC_PRBS_SR4_MASK (((uint32_t) 1 << 29) - 1)
#define MAC_PRBS_SR5_MASK (((uint32_t) 1 << 61) - 1)

#include "eurocrypt.h"

typedef struct {
	uint8_t pkt[MAC_PAYLOAD_BYTES];
	int address;
	int continuity;
	int scramble; /* 0 = Don't scramble, 1 = Scramble */
} _mac_packet_queue_item_t;

typedef struct {
	
	/* The packet queue */
	_mac_packet_queue_item_t pkts[MAC_QUEUE_LEN];	/* Copy of the packets */
	int len;					/* Number of packets in the queue */
	int p;						/* Index of the next free slot */
	
} mac_packet_queue_t;

typedef struct {
	
	mac_packet_queue_t queue;			/* Packet queue for this subframe */
	uint8_t pkt[MAC_PACKET_BYTES];			/* The current packet */
	int pkt_bits;					/* Bits sent of the current packet */
	
	/* Channel continuity counters */
	int service_continuity;				/* Channel 0    -- Service information */
	int audio_continuity;				/* Channel 224  -- Audio packets */
	int dummy_continuity;				/* Channel 1023 -- Dummy packets */
	
} mac_subframe_t;

typedef struct {
	int emode;						/* Eurocrypt M or S2 mode */
	unsigned char key[7];			/* Decryption key */
	unsigned char data[42];			/* General ECM data */
	unsigned char decevencw[8];		/* Decrypted even control word */
	unsigned char decoddcw[8];		/* Decrypted odd control word */
} ec_t ;

typedef struct {
	
	uint8_t vsam; /* VSAM Vision scrambling and access mode */
	uint8_t ratio; /* 0: 4:3, 1: 16:9 */
	uint16_t audio_channel;
	int scramble_audio;
	
	/* 1 = Teletext enabled */
	int teletext;
	
	/* UDT (Unified Date and Time) sequence */
	uint8_t udt[25];
	
	/* RDF sequence index */
	int rdf;
	
	/* The data subframes */
	mac_subframe_t subframes[2];
	
	/* PRBS seed, per-line */
	uint16_t prbs[MAC_LINES];
	
	/* Duobinary state */
	int polarity;
	int16_t *lut;
	int width;
	
	/* NICAM encoder */
	nicam_enc_t nicam;
	
	/* Video properties */
	int chrominance_width;
	int chrominance_left;
	int white_ref_left;
	int black_ref_left;
	int black_ref_right;
	
	/* PRBS generators */
	uint64_t cw;
	uint64_t sr1;
	uint64_t sr2;
	uint64_t sr3;
	uint64_t sr4;
	int video_scale[MAC_WIDTH];
	
	/* Eurocrypt state */
	int eurocrypt;
	eurocrypt_t ec;
	
} mac_t;

extern void mac_golay_encode(uint8_t *data, int blocks);

extern int mac_init(vid_t *s);
extern void mac_free(vid_t *s);

extern int mac_write_packet(vid_t *s, int subframe, int address, int continuity, const uint8_t *data, int scramble);
extern int mac_write_audio(vid_t *s, const int16_t *audio);

extern void mac_next_line(vid_t *s);

#endif

