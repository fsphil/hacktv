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

#ifndef _TELETEXT_H
#define _TELETEXT_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "video.h"

#define TT_OK            0
#define TT_ERROR         1
#define TT_NO_PACKET     2
#define TT_OUT_OF_MEMORY 3

typedef struct _tt_page_t {
	
	/* The page number, 0x100 - 0x8FF */
	uint16_t page;
	
	/* Subpage number: 0x00 - 0xFF */
	uint8_t subpage;
	
	/* Subcode: 0x0000 - 0x3F7F */
	uint16_t subcode;
	
	/* Page status 0x0000 - 0xFFFF */
	uint16_t page_status;
	
	/* Cycle mode / time */
	int cycle_mode;  /* 0 = Seconds, 1 = Cycles */
	int cycle_time;  /* Seconds / cycles until next subpage */
	int cycle_count; /* Cycle counter */
	
	/* Fastext links */
	int links[6];
	
	/* Flag to transmit the erasure code, only done on a new subpage */
	int erase;       /* 0 = Don't erase, 1 = Erase */
	
	/* The number of packets that make up the page,
	 * not including the header */
	int packets;
	
	/* The number of packets that can be transmitted
	 * within 20ms of the header packet */
	int nodelay_packets;
	
	/* Pointer to the packets that make up this
	 * page. Each packet is 45 bytes long and
	 * represents the full VBI line. */
	uint8_t *data;
	
	/* Flag to signal the arrival of an updated page
	 * to avoid reading a non-existent row. */
	int update;
	
	/* A pointer to the first subpage */
	struct _tt_page_t *subpages;
	
	/* A pointer to the next subpage */
	struct _tt_page_t *next_subpage;
	
	/* A pointer to the next page */
	struct _tt_page_t *next;
	
} tt_page_t;

typedef struct {
	
	/* The magazine number, 1-8 */
	int magazine;
	
	/* Set to 1 if the next magazine packet has to
	 * to be a header filler packet */
	int filler;
	
	/* A pointer to the first page */
	tt_page_t *pages;
	
	/* A pointer to the currently active page */
	tt_page_t *page;
	
	/* The currently active row */
	int row;
	
	/* Timecode to resume sending display packets */
	int delay;
	
} tt_magazine_t;

typedef struct {
	
	/* The current timestamp to use for the clock */
	time_t timestamp;
	
	/* The number of ticks that represent 20ms. This is
	 * used to enforce a minimum time between header
	 * packets and displayable packets of the same page.
	 * Sometimes referred to as the 20ms rule, page
	 * erasure interval or page clearing interval. */
	unsigned int header_delay;
	
	/* The number of clock ticks that represent 1s.
	 * This is used to determine when to send the next
	 * 8/30 packet, containing the updated time */
	unsigned int second_delay;
	
	/* The currently active magazine */
	unsigned int magazine;
	
	/* The available magazines */
	tt_magazine_t magazines[8];
	
} tt_service_t;

typedef struct {
	vid_t *vid;
	int16_t *lut;
	FILE *raw;
	tt_service_t service;
	unsigned int timecode;
} tt_t;

extern int tt_init(tt_t *s, vid_t *vid, char *path);
extern void tt_free(tt_t *s);
extern int tt_next_packet(tt_t *s, uint8_t vbi[45], int frame, int line);
extern int tt_render_line(vid_t *s, void *arg, int nlines, vid_line_t **lines);
extern int update_teletext_subtitle(char *fmt, tt_service_t *s);

#endif

