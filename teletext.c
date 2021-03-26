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

/* -=== Teletext encoder ===-
 * 
 * Encodes a teletext stream, inserting packets into the VBI area of the
 * video signal. Teletext pages in the TTI file format are supported.
 * 
 * This version only works with 625-line PAL modes. There are NTSC and
 * SECAM variations of Teletext but those are not currently supported.
 * 
 * TODO: The clock on the header line will not be accurate when not
 * transmitting live. This could be fixed by calculating the time based
 * on the timecode, but it will need some flag to indicate if the output
 * is live or not.
 * 
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include "video.h"
#include "vbidata.h"

static const uint8_t _parity[0x80] = {
	0x80, 0x01, 0x02, 0x83, 0x04, 0x85, 0x86, 0x07,
	0x08, 0x89, 0x8A, 0x0B, 0x8C, 0x0D, 0x0E, 0x8F,
	0x10, 0x91, 0x92, 0x13, 0x94, 0x15, 0x16, 0x97,
	0x98, 0x19, 0x1A, 0x9B, 0x1C, 0x9D, 0x9E, 0x1F,
	0x20, 0xA1, 0xA2, 0x23, 0xA4, 0x25, 0x26, 0xA7,
	0xA8, 0x29, 0x2A, 0xAB, 0x2C, 0xAD, 0xAE, 0x2F,
	0xB0, 0x31, 0x32, 0xB3, 0x34, 0xB5, 0xB6, 0x37,
	0x38, 0xB9, 0xBA, 0x3B, 0xBC, 0x3D, 0x3E, 0xBF,
	0x40, 0xC1, 0xC2, 0x43, 0xC4, 0x45, 0x46, 0xC7,
	0xC8, 0x49, 0x4A, 0xCB, 0x4C, 0xCD, 0xCE, 0x4F,
	0xD0, 0x51, 0x52, 0xD3, 0x54, 0xD5, 0xD6, 0x57,
	0x58, 0xD9, 0xDA, 0x5B, 0xDC, 0x5D, 0x5E, 0xDF,
	0xE0, 0x61, 0x62, 0xE3, 0x64, 0xE5, 0xE6, 0x67,
	0x68, 0xE9, 0xEA, 0x6B, 0xEC, 0x6D, 0x6E, 0xEF,
	0x70, 0xF1, 0xF2, 0x73, 0xF4, 0x75, 0x76, 0xF7,
	0xF8, 0x79, 0x7A, 0xFB, 0x7C, 0xFD, 0xFE, 0x7F
};

static const uint8_t _hamming84[0x10] = {
	0x15, 0x02, 0x49, 0x5E, 0x64, 0x73, 0x38, 0x2F,
	0xD0, 0xC7, 0x8C, 0x9B, 0xA1, 0xB6, 0xFD, 0xEA,
};



static uint8_t _unhamming84(uint8_t b)
{
	int i;
	
	/* This won't handle bit errors, it's only for internal use */
	
	for(i = 0; i < 16; i++)
	{
		if(_hamming84[i] == b)
		{
			return(i);
		}
	}
	
	return(0);
}

static uint16_t _crc(uint16_t crc, const uint8_t *data, size_t length)
{
	uint16_t i, bit;
	uint8_t b;
	
	while(length--)
	{
		b = *(data++);
		
		/* As per ETS 300 706 9.6.1 */
		for(i = 0; i < 8; i++, b <<= 1)
		{
			bit = ((crc >> 15) ^ (crc >> 11) ^ (crc >> 8) ^ (crc >> 6) ^ (b >> 7)) & 1;
			crc = (crc << 1) | bit;
		}
	}
	
	return(crc);
}

static int _line_packet_number(const uint8_t line[45])
{
	return(
		(_unhamming84(line[4]) << 1) |
		(_unhamming84(line[3]) >> 3)
	);
}

/*static void _dump_line(uint8_t line[45])
{
	int magazine;
	int packet_number;
	int i;
	char c;
	
	magazine = _unhamming84(line[3]) & 7;
	if(magazine == 0) magazine = 8;
	
	packet_number = _line_packet_number(line);
	
	printf("%d/%d '", magazine, packet_number);
	
	for(i = 0; i < (packet_number == 0 ? 32 : 40); i++)
	{
		c = line[(packet_number == 0 ? 13 : 5) + i] & 0x7F;
		printf("%c", isprint(c) ? c : '_');
	}
	
	printf("'\n");
}*/

static void *_paritycpy(void *dest, const void *src, size_t n, uint8_t pad)
{
	uint8_t *d = dest;
	const uint8_t *s = src;
	
	/* Copy the bytes, applying parity bits */
	for(; n && *s; n--)
	{
		*(d++) = _parity[*(s++) & 0x7F];
	}
	
	/* Fill the remainder of the line with spaces */
	while(n--)
	{
		*(d++) = _parity[pad & 0x7F];
	}
	
	return(dest);
}

static int _mjd(int year, int month, int day)
{
	double mjd;
	
	/* Calculate Modified Julian Date */
	
	mjd = 367.0 * year - (int) (7.0 * (year + (int) ((month + 9.0) / 12.0)) / 4.0)
	    + (int) (275.0 * month / 9.0) + day - 678987.0;
	
	return((int) mjd);
}

#ifdef __MINGW32__
static struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
	struct tm *tm = gmtime(timep);
	
	if(!tm)
	{
		return(NULL);
	}
	
	memcpy(result, tm, sizeof(struct tm));
	
	return(result);
}
#endif

static void _packet830(uint8_t line[45], time_t timestamp)
{
	int magazine = 8;
	int packet_number = 30;
	int initial_page = 0x100;
	int initial_subcode = 0x3F7F;
	struct tm tm;
	int mjd;
	
	/* Synchronization sequence (Clock run-in and framing code) */
	line[0] = 0x55;
	line[1] = 0x55;
	line[2] = 0x27;
	
	/* Packet address */
	line[3] = _hamming84[((packet_number & 1) << 3) | (magazine & 7)];
	line[4] = _hamming84[(packet_number >> 1) & 15];
	
	/* Designation code */
	line[5] = _hamming84[0]; /* 0 = Multiplexed, 1 = Non-multiplexed */
	
	/* Initial Page */
	line[6] = _hamming84[initial_page & 0x0F];
	line[7] = _hamming84[(initial_page >> 4) & 0x0F];
	line[8] = _hamming84[initial_subcode & 0x0F];
	line[9] = _hamming84[
		(((initial_page >> 8) & 0x01) << 3) |
		((initial_subcode >> 4) & 0x07)
	];
	line[10] = _hamming84[(initial_subcode >> 8) & 0x0F];
	line[11] = _hamming84[
		(((initial_page >> 9) & 0x03) << 2) |
		((initial_subcode >> 12) & 0x03)
	];
	
	/* Network Identification Code */
	/* This is not supported yet. Note: Bits are reversed */
	line[12] = 0x00;
	line[13] = 0x00;
	
	/* Time Offset Code (TODO) */
	line[14] = 0;
	
	/* Modified Julian Date */
	gmtime_r(&timestamp, &tm);
	mjd = _mjd(1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday);
	
	line[15] = mjd % 100000 / 10000 + 1;
	line[16] = (mjd % 10000 / 1000 + 1) << 4
	         | (mjd % 1000 / 100 + 1);
	line[17] = (mjd % 100 / 10 + 1) << 4
	         | (mjd % 10 + 1);
	
	/* UTC time */
	line[18] = ((tm.tm_hour / 10) + 1) << 4
	         | ((tm.tm_hour % 10) + 1);
	line[19] = ((tm.tm_min / 10) + 1) << 4
	         | ((tm.tm_min % 10) + 1);
	line[20] = ((tm.tm_sec / 10) + 1) << 4
	         | ((tm.tm_sec % 10) + 1);
	
	/* Reserved */
	line[21] = 0x00;
	line[22] = 0x00;
	line[23] = 0x00;
	line[24] = 0x00;
	
	/* Status Display */
	_paritycpy(&line[25], "hacktv", 20, ' ');
}

static void _header(uint8_t line[45], int magazine, int page, int subcode, int status, char *data)
{
	int packet_number = 0;
	int erase_page;
	int subtitle;
	int newsflash;
	int inhibit_display;
	int interrupted_sequence;
	int update_indicator;
	int suppress_header;
	int national_option_character_subset;
	int magazine_serial;
	
	erase_page                       = (status >> 14) & 1;
	newsflash                        = (status >> 0) & 1;
	subtitle                         = (status >> 1) & 1;
	suppress_header                  = (status >> 2) & 1;
	update_indicator                 = (status >> 3) & 1;
	interrupted_sequence             = (status >> 4) & 1;
	inhibit_display                  = (status >> 5) & 1;
	magazine_serial                  = 0; // (status >> 6) & 1; /* We only use parallel */
	national_option_character_subset = (status >> 7) & 7;
	
	/* Synchronization sequence (Clock run-in and framing code) */
	line[0] = 0x55;
	line[1] = 0x55;
	line[2] = 0x27;
	
	/* Packet address */
	line[3] = _hamming84[((packet_number & 1) << 3) | (magazine & 7)];
	line[4] = _hamming84[(packet_number >> 1) & 15];
	
	/* Page packet header (Y = 0) */
	line[5] = _hamming84[page & 0x0F];
	line[6] = _hamming84[(page >> 4) & 0x0F];
	line[7] = _hamming84[subcode & 0x0F];
	line[8] = _hamming84[
		(erase_page           ? 1 << 3 : 0) |
		((subcode >> 4) & 0x07)
	];
	line[9] = _hamming84[(subcode >> 8) & 0x0F];
	line[10] = _hamming84[
		(subtitle             ? 1 << 3 : 0) |
		(newsflash            ? 1 << 2 : 0) |
		((subcode >> 12) & 0x03)
	];
	line[11] = _hamming84[
		(inhibit_display      ? 1 << 3 : 0) |
		(interrupted_sequence ? 1 << 2 : 0) |
		(update_indicator     ? 1 << 1 : 0) |
		(suppress_header      ? 1 << 0 : 0)
	];
	line[12] = _hamming84[
		(national_option_character_subset << 1) |
		(magazine_serial      ? 1 << 0 : 0)
	];
	
	/* Copy the data, applying parity bits */
	_paritycpy(&line[13], data, 32, ' ');
}

static void _fastext_line(uint8_t line[45], int magazine, int links[6])
{
	int packet_number = 27;
	int page;
	int subcode;
	uint8_t *link;
	int i;
	
	/* Synchronization sequence (Clock run-in and framing code) */
	line[0] = 0x55;
	line[1] = 0x55;
	line[2] = 0x27;
	
	/* Packet address */
	line[3] = _hamming84[((packet_number & 1) << 3) | (magazine & 7)];
	line[4] = _hamming84[(packet_number >> 1) & 15];
	
	/* Designation code, always 0 */
	line[5] = _hamming84[0];
	
	for(i = 0; i < 6; i++)
	{
		link = &line[6 + (6 * i)];
		
		if(links[i] < 0x100)
		{
			page = 0x8FF;
			subcode = 0x3F7F;
		}
		else if(links[i] < 0x10000)
		{
			page = links[i];
			subcode = 0x3F7F;
		}
		else
		{
			page = links[i] >> 8;
			subcode = links[i] & 0xFF;
		}
		
		/* The magazine number is xor'ed with the page number */
		page ^= (magazine & 7) << 8;
		
		link[0] = _hamming84[page & 0x0F];
		link[1] = _hamming84[(page >> 4) & 0x0F];
		link[2] = _hamming84[subcode & 0x0F];
		link[3] = _hamming84[
			(((page >> 8) & 0x01) << 3) |
			((subcode >> 4) & 0x07)
		];
		link[4] = _hamming84[(subcode >> 8) & 0x0F];
		link[5] = _hamming84[
			(((page >> 9) & 0x03) << 2) |
			((subcode >> 12) & 0x03)
		];
	}
	
	/* Link Control Byte, always 0x0F */
	line[42] = _hamming84[0x0F];
	
	/* Page CRC padding. Real CRC is generated later. */
	line[43] = 0x12;
	line[44] = 0x34;
}

static void _line(uint8_t line[45], int magazine, int packet_number, const uint8_t *data)
{
	/* Synchronization sequence (Clock run-in and framing code) */
	line[0] = 0x55;
	line[1] = 0x55;
	line[2] = 0x27;
	
	/* Packet address */
	line[3] = _hamming84[((packet_number & 1) << 3) | (magazine & 7)];
	line[4] = _hamming84[(packet_number >> 1) & 15];
	
	/* Copy the data, applying parity bits */
	_paritycpy(&line[5], data, 40, ' ');
}

/*static void _dump_service_map(tt_service_t *s)
{
	tt_magazine_t *mag;
	tt_page_t *page;
	tt_page_t *subpage;
	int i;
	
	for(i = 1; i <= 8; i++)
	{
		printf("Magazine %d:\n", i);
		
		mag = &s->magazines[i & 7];
		
		page = mag->pages;
		while(page)
		{
			printf("+ Page %03X/%d\n", page->page, page->subpage);
			
			for(subpage = page->next_subpage; subpage != page; subpage = subpage->next_subpage)
			{
				printf("+++ Subpage %03X/%X", subpage->page, subpage->subpage);
				
				if(subpage->subpages != page->subpages)
				{
					printf(" ! subpage->subpages != page->subpages");
				}
				
				printf("\n");
			}
			
			page = page->next;
			if(page == mag->pages) break;
		}
	}
}*/

static char *_mk_header(char *s, uint16_t page, time_t timestamp)
{
	char temp[33];
	struct tm *tm;
	
	/* TODO: Make this customisable */
	
	tm = localtime(&timestamp);
	snprintf(temp, 33, "hacktv   %03X %%a %%d %%b\x03" "%%H:%%M/%%S", page);
	strftime(s, 33, temp, tm);
	
	return(s);
}

static void _update_page_crc(tt_page_t *page, const uint8_t header[45])
{
	const uint8_t *blank = (const uint8_t *) "                                        ";
	uint8_t *line;
	uint16_t crc;
	int l, i;
	
	/* Begin calculating the CRC from the header */
	crc = _crc(0x0000, &header[13], 24);
	
	/* Scan each line in order, using the blank line if not found */
	for(l = 1; l < 26; l++)
	{
		line = NULL;
		
		/* Try to find this line */
		for(i = 0; i < page->packets; i++)
		{
			if(_line_packet_number(&page->data[i * 45]) == l)
			{
				line = &page->data[i * 45 + 5];
				break;
			}
		}
		
		crc = _crc(crc, line != NULL ? line : blank, 40);
	}
	
	/* Scan for packet 27 and update CRC */
	for(line = page->data, i = 0; i < page->packets; line += 45, i++)
	{
		if(_line_packet_number(line) == 27)
		{
			line[43] = (crc >> 8) & 0xFF;
			line[44] = (crc >> 0) & 0xFF;
		}
	}
}

static int _next_magazine_packet(tt_service_t *s, tt_magazine_t *mag, uint8_t line[45], unsigned int timecode)
{
	char header[33];
	
	if(mag->filler)
	{
		/* Generate the filler header packet */
		_mk_header(header, 0x8FF, s->timestamp);
		_header(line, mag->magazine & 0x07, 0xFF, 0x3F7F, 0x8000, header);
		
		mag->filler = 0;
		
		return(TT_OK);
	}
	
	if(mag->pages == NULL)
	{
		return(TT_NO_PACKET);
	}
	
	if(mag->row == 0)
	{
		int status = mag->page->page_status;
		
		/* Set the erase flag if needed */
		status &= ~(1 << 14);
		status |= mag->page->erase << 14;
		mag->page->erase = 0;
		
		_mk_header(header, mag->page->page, s->timestamp);
		_header(line, mag->magazine & 0x07, mag->page->page & 0xFF, mag->page->subcode, status, header);
		
		/* Update the page CRC */
		_update_page_crc(mag->page, line);
		
		/* Set the delay time (20ms rule) */
		mag->delay = timecode + s->header_delay;
		mag->row++;
	}
	else
	{
		/* Return nothing if the next row is a display row,
		 * and we're still within the delay period */
		if(mag->row - 1 == mag->page->nodelay_packets && timecode < mag->delay)
		{
			return(TT_NO_PACKET);
		}
		
		/* Check if this page has been updated. Reset back to row 1. */
		if(mag->page->update)
		{
			mag->page->update = 0;
			mag->row = 1;
			return(TT_NO_PACKET);
		}
		
		/* Copy the packet */
		memcpy(line, &mag->page->data[(mag->row - 1) * 45], 45);
		
		mag->row++;
	}
	
	/* Test if this is the last row on this page */
	if(mag->row - 1 == mag->page->packets)
	{
		tt_page_t *npage = mag->page->next;
		
		/* Test if we need to advance the next page's subpage */
		if(npage->cycle_time && npage != npage->next_subpage)
		{
			int adv = 0;
			
			if(npage->cycle_mode == 0)
			{
				/* Timer mode */
				if(timecode >= npage->cycle_count)
				{
					npage->cycle_count = timecode + npage->cycle_time * s->second_delay;
					adv = 1;
				}
			}
			else
			{
				/* Cycle mode */
				npage->cycle_count++;
				
				if(npage->cycle_count == npage->cycle_time)
				{
					npage->cycle_count = 0;
					adv = 1;
				}
			}
			
			if(adv)
			{
				mag->page->next = npage->next_subpage;
				npage->next_subpage->next = npage->next;
				npage->next_subpage->cycle_count = npage->cycle_count;
				npage->next_subpage->erase = 1;
			}
		}
		
		/* Advance magazine to the next page */
		mag->page = mag->page->next;
		mag->row = 0;
		
		/* Special case for magazines with only one page,
		 * set the filler flag to correctly end the page */
		/* TODO: Am I correct here? Is this needed? What about subpages? */
		if(mag->pages->next == mag->pages)
		{
			mag->filler = 1;
		}
	}
	
	return(TT_OK);
}

static int _next_packet(tt_service_t *s, uint8_t line[45], unsigned int timecode)
{
	int i, r;
	time_t timestamp;
	
	/* Update the timestamp */
	timestamp = time(NULL);
	
	/* If the timestamp has changed, we need to insert an 8/30 packet */
	if(s->timestamp != timestamp)
	{
		s->timestamp = timestamp;
		
		_packet830(line, timestamp);
		
		return(TT_OK);
	}
	
	/* Test each magazine for the next available packet */
	for(i = 0; i < 8; i++)
	{
		r = _next_magazine_packet(s, &s->magazines[s->magazine++], line, timecode);
		
		s->magazine &= 7;
		
		if(r == TT_OK)
		{
			return(TT_OK);
		}
	}
	
	/* No magazine returned a packet, return nothing */
	
	return(TT_NO_PACKET);
}

static int _line_len(uint8_t line[40])
{
	int x;
	
	for(x = 0; x < 40; x++)
	{
		if(line[x] != ' ' && line[x] != '\0') break;
	}
	
	return(40 - x);
}

static int _page_mkpackets(tt_page_t *page, uint8_t lines[25][40])
{
	int i, j;
	
	/* Count the number of non-empty packets (+ 1 for fastext packet) */
	page->packets = 1;
	page->nodelay_packets = 0;
	
	for(i = 1; i < 25; i++)
	{
		if(_line_len(lines[i]) > 0)
		{
			page->packets++;
		}
	}
	
	/* (Re)allocate memory for the packets */
	page->data = realloc(page->data, page->packets * 45);
	
	/* The fastext packet is transmitted before the page content (Annex B.2) */
	_fastext_line(&page->data[0], (page->page >> 8) & 0x07, page->links);
	
	/* Generate the line packets */
	for(j = 1, i = 1; i < 25; i++)
	{
		if(_line_len(lines[i]) > 0)
		{
			_line(&page->data[j++ * 45], (page->page >> 8) & 0x07, i, lines[i]);
		}
	}
	
	return(TT_OK);
}

static void _add_page(tt_service_t *s, tt_page_t *new_page)
{
	tt_magazine_t *mag;
	tt_page_t *page;
	tt_page_t *subpage;
	
	/* Make sure erase flag is set for the new page */
	new_page->erase = 1;
	
	mag = &s->magazines[(new_page->page >> 8) & 0x07];
	
	if(mag->pages == NULL)
	{
		/* This is the first page added to the magazine */
		mag->pages = new_page;
		mag->page = new_page;
		
		new_page->next = new_page;
		new_page->subpages = new_page;
		new_page->next_subpage = new_page;
		
		return;
	}
	
	/* Scan the magazine for the page insertion point */
	for(page = mag->pages; page->next != mag->pages; page = page->next)
	{
		if(page->page <= new_page->page &&
		   page->next->page > new_page->page) break;
	}
	
	if(page->page != new_page->page)
	{
		/* This is a new page, to be appended */
		new_page->next = page->next;
		new_page->subpages = new_page;
		new_page->next_subpage = new_page;
		
		page->next = new_page;
		
		if(new_page->page < mag->pages->page)
		{
			mag->pages = new_page;
		}
	}
	else
	{
		/* This is an existing page */
		new_page->next = page->next;
		
		/* Scan for the sub-page insertion point */
		for(subpage = page->subpages; subpage->next_subpage != page->subpages; subpage = subpage->next_subpage)
		{
			if(subpage->subpage <= new_page->subpage &&
			   subpage->next_subpage->subpage > new_page->subpage) break;
		}
		
		if(subpage->subpage != new_page->subpage)
		{
			/* This is a new subpage, to be appended */
			new_page->next_subpage = subpage->next_subpage;
			subpage->next_subpage = new_page;
			
			if(new_page->subpage < page->subpages->subpage)
			{
				page->subpages = new_page;
			}
			
			new_page->subpages = page->subpages;
		}
		else
		{
			/* Set the update flag */
			new_page->update = 1;
			
			/* This is an existing subpage, replace it */
			
			/* Copy the subpage pointers */
			new_page->next_subpage = subpage->next_subpage;
			new_page->subpages = subpage->subpages;
			
			/* Free the old page packet data */
			free(subpage->data);
			
			/* Overwrite the old page data with the new one */
			memcpy(subpage, new_page, sizeof(tt_page_t));
			
			/* And finally free the new page data */
			free(new_page);
		}
	}
}

int update_teletext_subtitle(char *t, tt_service_t *s)
{
	tt_page_t *page;
	uint8_t lines[25][40];
	int c, cc, l, i, p;
	
	uint8_t tlines[25][80];
	
	/* Double height, 2x start box markers */
	const char header[3] = {0xD, 0xB, 0xB};
	
	/* 2x end box markers */
	const char footer[2] = {0xA, 0xA};
	
	/* Create new page/allocate memory */
	page = calloc(sizeof(tt_page_t), 1);
	if(!page)
	{
		perror("calloc");
		return(TT_OUT_OF_MEMORY);
	}
	
	/* Set up page data */
	page->data = NULL;
	page->page = 0x888;
	page->subpage = 0x7F;
	page->cycle_time = 8;
	page->cycle_mode = 0;
	page->page_status = 0xC016;
	page->subcode = 0x3F7F;
	
	/* Clear existing page data */
	for(c = 0; c < 25; c++)
	{
		memset(lines[c], ' ', 40);
		memset(tlines[c], 0, 80);
	}

	if(*t)
	{
		/* Break up text into several lines */
		for(c = 0, l = 0; *t; t++)
		{
			/* Skip undisplayable characters */
			if((uint8_t) *t > 0x7F)
			{
				continue;
			}
			
			/* TODO: tidy up and optimise... eventually */
			if(c > 36)
			{
				/* Break up this line - it's too long for teletext */
				int space = 1;
				for(cc = c/3; cc < c && space; cc++)
				{
					if(isspace(tlines[l][cc]))
					{
						/* Found space character - looks like a good place to split it */
						memcpy(tlines[l + 2], tlines[l] + cc + 1, c - cc);
						memset(tlines[l] + cc, 0, c - cc);
						space = 0;
					}
				}
				l += 2;
				c = c - cc;
			}
			
			if(*t == '\n')
			{
				/* New line character - skip and jump to next line */
				c  = 0;
				l += 2;
				continue;
			}
			
			tlines[l][c++] = ((*t & 0x7F) == '[' ? '(' : (*t & 0x7F) == ']' ? ')' : (*t & 0x7F));
		}
		
		c = 0;
		
		/* Display each line */
		for(i = 0; i <= l; i += 2)
		{
			/* Hack to centre subtitles on screen */
			p = 17 - (strlen(((char *) tlines[l - i]) + 1) / 2);
			p = p < 0 ? 0 : p;
			
			/* Start box */
			memcpy(lines[22 - i] + p, header, 3);
			
			/* Copy line */
			for(c = 0; tlines[l - i][c]; c++)
			{
				lines[22 - i][c + 3 + p] = tlines[l - i][c];
			}
			
			/* End box */
			memcpy(lines[22 - i] + c + 3 + p, footer, 2);
		}
	}
	
	_page_mkpackets(page, lines);
	_add_page(s, page);
	return 0;
}

static int _load_tti(tt_service_t *s, char *filename)
{
	char buf[200];
	size_t i, len;
	int c;
	FILE *f;
	unsigned int x;
	char *t;
	uint8_t lines[25][40];
	tt_page_t *page;
	int esc;
	
	f = fopen(filename, "rb");
	if(!f)
	{
		perror("fopen");
		return(TT_ERROR);
	}
	
	page = calloc(sizeof(tt_page_t), 1);
	if(!page)
	{
		perror("calloc");
		fclose(f);
		return(TT_OUT_OF_MEMORY);
	}
	
	len = 0;
	
	/* Test if this is a TTI file */
	len += fread(buf, sizeof(char), 3, f);
	
	/* Expect the file to begin with a two letter code followed by a comma */
	if(len != 3 ||
	   buf[0] < 'A' || buf[0] > 'Z' ||
	   buf[1] < 'A' || buf[1] > 'Z' ||
	   buf[2] != ',')
	{
		fprintf(stderr, "%s: Unrecognised file format. Skipping...\n", filename);
		free(page);
		fclose(f);
		return(TT_ERROR);
	}
	
	while(!feof(f))
	{
		/* Top-up input buffer */
		len += fread(buf + len, sizeof(char), 200 - len, f);
		
		while(len > 0)
		{
			/* Search for a newline */
			for(i = 0; i < len; i++)
			{
				if(buf[i] == '\r' || buf[i] == '\n' || buf[i] == '\0')
				{
					buf[i] = '\0';
					break;
				}
			}
			
			if(i == len)
			{
				/* No newline found */
				break;
			}
			
			if(buf[0] == '\0')
			{
				/* Blank line */
			}
			else if(strncmp("PN,", buf, 3) == 0)
			{
				/* PN - Page Number */
				/* PN,mppss (m = magazine, pp = page, ss = subpage) */
				
				if(page->page > 0)
				{
					tt_page_t *opage = page;
					
					/* Save current page */
					_page_mkpackets(page, lines);
					_add_page(s, page);
					
					/* Lazily copy the old page settings */
					page = malloc(sizeof(tt_page_t));
					if(!page)
					{
						perror("malloc");
						fclose(f);
						return(TT_OUT_OF_MEMORY);
					}
					
					memcpy(page, opage, sizeof(tt_page_t));
					
					/* Have to unset the packet pointer */
					page->data = NULL;
				}
				
				/* Clear existing page data */
				for(c = 0; c < 25; c++)
				{
					memset(lines[c], ' ', 40);
				}
				
				x = strtol(buf + 3, NULL, 16);
				
				if(x < 0x10000)
				{
					page->page = x;
					page->subpage = 0;
				}
				else
				{
					page->page = x >> 8;
					page->subpage = x & 0xFF;
				}
			}
			else if(strncmp("CT,", buf, 3) == 0)
			{
				/* CT - Cycle Time */
				/* CT,n,<t> (n = delay, t = C/T (Cycle/Timed) */
				
				page->cycle_time = strtol(buf + 3, &t, 10);
				page->cycle_mode = 0;
				
				if(t[0] == ',' && (t[1] == 'C' || t[1] == 'c'))
				{
					page->cycle_mode = 1;
				}
			}
			else if(strncmp("DE,", buf, 3) == 0)
			{
				/* DE - Description */
				/* DE,<text> */
			}
			else if(strncmp("PS,", buf, 3) == 0)
			{
				/* PS - Page Status */
				/* PS,ssss */
				
				x = strtol(buf + 3, NULL, 16);
				
				page->page_status = x;
			}
			else if(strncmp("SC,", buf, 3) == 0)
			{
				/* SC - Subcode */
				/* SC,ssss */
				
				x = strtol(buf + 3, NULL, 16);
				
				page->subcode = x;
			}
			else if(strncmp("OL,", buf, 3) == 0)
			{
				/* OL - Output Line */
				/* OL,nn,<line> */
				
				x = strtol(buf + 3, &t, 10);
				
				if(x > 0 && x < 25)
				{
					if(*t == ',')
					{
						t++;
					}
					
					for(esc = 0, c = 0; *t && c < 40; t++)
					{
						if(*t == 0x1B)
						{
							esc = 1;
							continue;
						}
						
						lines[x][c++] = (esc ? *t - 0x40 : *t) & 0x7F;
						
						esc = 0;
					}
				}
			}
			else if(strncmp("FL,", buf, 3) == 0)
			{
				/* FL - Fastext Link */
				/* FL,rrr,ggg,yyy,ccc,lll,iii */
				
				t = buf + 2;
				
				for(c = 0; *t == ',' && c < 6; c++)
				{
					page->links[c] = strtol(t + 1, &t, 16);
				}
			}
			else if(buf[2] != ',')
			{
				fprintf(stderr, "%s: Unrecognised line: '%s'\n", filename, buf);
			}
			
			len -= i + 1;
			memmove(buf, buf + i + 1, len);
		}
		
		if(len == 200)
		{
			fprintf(stderr, "%s: Line too long (>200 characters)\n", filename);
			len = 0;
		}
	}
	
	fclose(f);
	
	if(page->page > 0)
	{
		_page_mkpackets(page, lines);
		_add_page(s, page);
	}
	
	return(TT_OK);
}

static int _new_service(tt_service_t *s)
{
	int i;
	tt_magazine_t *mag;
	
	/* Create an empty service */
	s->timestamp = 0;
	s->second_delay = 25 * 625;
	s->header_delay = (20e-3 * s->second_delay) + 0.5;
	s->magazine = 1;
	
	for(i = 1; i <= 8; i++)
	{
		mag = &s->magazines[i & 0x07];
		
		mag->magazine = i;
		mag->filler = 0;
		mag->pages = NULL;
		mag->row = 0;
		mag->delay = 0;
	}
	
	return(TT_OK);
}

static void _free_service(tt_service_t *s)
{
	tt_magazine_t *mag;
	tt_page_t *npage;
	tt_page_t *nsubpage;
	int i;
	
	for(i = 0; i < 8; i++)
	{
		mag = &s->magazines[i];
		if(mag->pages == NULL) continue;
		
		mag->page = mag->pages->next->subpages;
		mag->pages->next = NULL;
		
		while(mag->page)
		{
			npage = mag->page->next == NULL ? NULL : mag->page->next->subpages;
			
			for(mag->page->next_subpage = mag->page->subpages->next_subpage;
			    mag->page->next_subpage != mag->page;
			    mag->page->next_subpage = nsubpage)
			{
				nsubpage = mag->page->next_subpage->next_subpage;
				
				free(mag->page->next_subpage->data);
				free(mag->page->next_subpage);
			}
			
			free(mag->page->data);
			free(mag->page);
			
			mag->page = npage;
		}
		
		mag->pages = NULL;
	}
}



int tt_init(tt_t *s, vid_t *vid, char *path)
{
	int16_t level;
	struct stat fs;
	
	memset(s, 0, sizeof(tt_t));
	
	/* Calculate the high level for teletext data, 66% of the white level */
	level = round((vid->white_level - vid->black_level) * 0.66);
	
	s->vid = vid;
	s->lut = vbidata_init(
		444, s->vid->width,
		level,
		VBIDATA_FILTER_RC, 0.7
	);
	
	if(!s->lut)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	/* Is the path to a raw teletext packet source? */
	if(strncmp(path, "raw:", 4) == 0)
	{
		if(strcmp(path + 4, "-") == 0)
		{
			s->raw = stdin;
		}
		else
		{
			s->raw = fopen(path + 4, "rb");
			
			if(!s->raw)
			{
				fprintf(stderr, "%s: ", path + 4);
				perror("fopen");
				tt_free(s);
				return(VID_ERROR);
			}
		}
		
		return(VID_OK);
	}
	
	_new_service(&s->service);
	
	if(strcmp(path,"subtitles") == 0)
	{
		update_teletext_subtitle("", &s->service);
	}
	else
	{
		/* Test if the path is a file or a directory */
		if(stat(path, &fs) != 0)
		{
			fprintf(stderr, "%s: ", path);
			perror("stat");
			tt_free(s);
			return(VID_ERROR);
		}
		
		if(fs.st_mode & S_IFDIR)
		{
			DIR *dir;
			struct dirent *ent;
			char filename[PATH_MAX];
			
			/* Path is a directory, scan all the files within */
			
			dir = opendir(path);
			
			if(!dir)
			{
				fprintf(stderr, "%s: ", path);
				perror("opendir");
				tt_free(s);
				return(VID_ERROR);
			}
			
			while((ent = readdir(dir)))
			{
				/* Skip hidden dot files */
				if(ent->d_name[0] == '.')
				{
					continue;
				}
				
				snprintf(filename, PATH_MAX, "%s/%s", path, ent->d_name);
				_load_tti(&s->service, filename);
			}
			
			closedir(dir);
		}
		else if(fs.st_mode & S_IFREG)
		{
			/* Path is a single file */
			_load_tti(&s->service, path);
		}
		else
		{
			fprintf(stderr, "%s: Not a file or directory\n", path);
		}
	}
	return(VID_OK);
}

void tt_free(tt_t *s)
{
	if(s == NULL) return;
	
	if(s->raw && s->raw != stdin)
	{
		fclose(s->raw);
	}
	else
	{
		_free_service(&s->service);
	}
	
	free(s->lut);
	
	memset(s, 0, sizeof(tt_t));
}

int tt_next_packet(tt_t *s, uint8_t vbi[45], int frame, int line)
{
	int r;
	
	/* Update the timecode */
	s->timecode  = (frame - 1) * s->vid->conf.lines;
	s->timecode += line - 1;
	
	/* Fetch the next line, or TT_NO_PACKET */
	if(s->raw)
	{
		if(feof(s->raw))
		{
			/* Return to the start of the file when we hit the end */
			fseek(s->raw, 0, SEEK_SET);
		}
		
		/* Synchronization sequence (Clock run-in and framing code) */
		vbi[0] = 0x55;
		vbi[1] = 0x55;
		vbi[2] = 0x27;
		
		r = fread(&vbi[3], 1, 42, s->raw);
		r = r == 42 ? TT_OK : TT_NO_PACKET;
	}
	else
	{
		r = _next_packet(&s->service, vbi, s->timecode);
	}
	
	return(r);
}

int tt_render_line(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	tt_t *tt = arg;
	vid_line_t *l = lines[0];
	uint8_t vbi[45];
	int r;
	
	/* Don't render teletext if this VBI line has already been allocated */
	if(l->vbialloc != 0) return(1);
	
	/* Use 16 lines per field for teletext */
	if((l->line >=   7 && l->line <=  22) ||
	   (l->line >= 320 && l->line <= 335))
	{
		r = tt_next_packet(tt, vbi, l->frame, l->line);
		
		if(r == TT_OK)
		{
			vbidata_render_nrz(tt->lut, vbi, -70, 360, VBIDATA_LSB_FIRST, l->output, 2);
		}
		
		l->vbialloc = 1;
	}
	
	return(1);
}

