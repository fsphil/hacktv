/* Nagravision Syster encoder for hacktv                                 */
/*=======================================================================*/
/* Copyright 2018 Alex L. James                                          */
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

/* -=== Nagravision Syster encoder ===-
 * 
 * These functions implement the image scrambler for Nagravision Syster.
 * This system uses line shuffling to obscure the image.
 * 
 * This does not yet produce any VBI line data so is not compatible
 * with real hardware.
 *
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "video.h"

/* The standard syster substitution table */

const static uint8_t _key_table[256] = {
	10, 11, 12, 13, 16, 17, 18, 19, 13, 14, 15, 16,  0,  1,  2,  3,
	21, 22, 23, 24, 18, 19, 20, 21, 23, 24, 25, 26, 26, 27, 28, 29,
	19, 20, 21, 22, 11, 12, 13, 14, 28, 29, 30, 31,  4,  5,  6,  7,
	22, 23, 24, 25,  5,  6,  7,  8, 31,  0,  1,  2, 27, 28, 29, 30,
	 3,  4,  5,  6,  8,  9, 10, 11, 14, 15, 16, 17, 25, 26, 27, 28,
	15, 16, 17, 18,  7,  8,  9, 10, 17, 18, 19, 20, 29, 30, 31,  0,
	24, 25, 26, 27, 20, 21, 22, 23,  1,  2,  3,  4,  6,  7,  8,  9,
	12, 13, 14, 15,  9, 10, 11, 12,  2,  3,  4,  5, 30, 31,  0,  1,
	24, 25, 26, 27,  2,  3,  4,  5, 31,  0,  1,  2,  7,  8,  9, 10,
	13, 14, 15, 16, 26, 27, 28, 29, 14, 15, 16, 17, 18, 19, 20, 21,
	22, 23, 24, 25,  5,  6,  7,  8, 19, 20, 21, 22, 12, 13, 14, 15,
	17, 18, 19, 20, 27, 28, 29, 30, 10, 11, 12, 13, 11, 12, 13, 14,
	 6,  7,  8,  9,  1,  2,  3,  4,  0,  1,  2,  3,  4,  5,  6,  7,
	 3,  4,  5,  6,  8,  9, 10, 11, 15, 16, 17, 18, 23, 24, 25, 26,
	29, 30, 31,  0, 25, 26, 27, 28,  9, 10, 11, 12, 21, 22, 23, 24,
	20, 21, 22, 23, 30, 31,  0,  1, 16, 17, 18, 19, 28, 29, 30, 31
};

static void _update_field_order(ng_t *s)
{
	int i, j;
	int b[32];
	
	/* This function generates the scrambled line order for the
	 * next field based on _key_table, s->s and s->r parameters.
	 *
	 * Based on work by Markus G. Kuhn from his publication
	 * 'Analysis of the Nagravision Video Scrambling Method', 1998-07-09
	*/
	
	for(i = 0; i < 32; i++)
	{
		b[i] = -32 + i;
	}
	
	for(i = 0; i < 287; i++)
	{
		j = i <= 254 ? _key_table[(s->r + (2 * s->s + 1) * i) & 0xFF] : i - 255;
		b[j] = s->order[b[j] + 32] = i;
	}
}

int ng_init(ng_t *s, vid_t *vid)
{
	int i;
	
	memset(s, 0, sizeof(ng_t));
	
	s->vid = vid;
	
	/* Initial seeds. Updated every field. */
	s->s = 0;
	s->r = 0;
	_update_field_order(s);
	
	/* Allocate memory for the delay */
	s->vid->delay += NG_DELAY_LINES;
	s->delay = calloc(2 * vid->width * NG_DELAY_LINES, sizeof(int16_t));
	if(!s->delay)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	/* Setup the delay line pointers */
	for(i = 0; i < NG_DELAY_LINES; i++)
	{
		s->delay_line[i] = &s->delay[2 * vid->width * i];
	}
	
	return(VID_OK);
}

void ng_free(ng_t *s)
{
	free(s->delay);
}

void ng_render_line(ng_t *s)
{
	int j = 0;
	int x, f, i;
	int line;
	int16_t *dline;
	
	/* Calculate which line is about to be transmitted due to the delay */
	line = s->vid->line - NG_DELAY_LINES;
	if(line < 0) line += s->vid->conf.lines;
	
	/* Calculate the field and field line */
	f = (line < NG_FIELD_2_START ? 1 : 2);
	i = line - (f == 1 ? NG_FIELD_1_START : NG_FIELD_2_START);
	
	if(i >= 0 && i < NG_LINES_PER_FIELD)
	{
		/* Adjust for the decoder's 32 line delay */
		i += 32;
		if(i >= NG_LINES_PER_FIELD)
		{
			i -= NG_LINES_PER_FIELD;
			f = (f == 1 ? 2 : 1);
		}
		
		/* Reinitialise the seeds if this is a new field */
		if(i == 0)
		{
			s->s = rand() & 0x7F;
			s->r = rand() & 0xFF;
			_update_field_order(s);
		}
		
		/* Calculate which line in the delay buffer to copy image data from */
		j = (f == 1 ? NG_FIELD_1_START : NG_FIELD_2_START) + s->order[i];
		if(j < line) j += s->vid->conf.lines;
		j -= line;
		
		if(j < 0 || j >= NG_DELAY_LINES)
		{
			/* We should never get to this point */
			fprintf(stderr, "*** Nagravision Syster scrambler is trying to read an invalid line ***\n");
			j = 0;
		}
	}
	
	/* Swap the active line with the oldest line in the delay buffer,
	 * with active video offset in j if necessary. */
	for(x = 0; x < s->vid->width * 2; x += 2)
	{
		int16_t t = s->vid->output[x];
		s->vid->output[x] = s->delay_line[x >= s->vid->active_left * 2 ? j : 0][x];
		s->delay_line[0][x] = t;
	}
	
	/* Advance the delay buffer */
	dline = s->delay_line[0];
	for(x = 0; x < NG_DELAY_LINES - 1; x++)
	{
		s->delay_line[x] = s->delay_line[x + 1];
	}
	s->delay_line[x] = dline;
}

