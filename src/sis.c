/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2024 Philip Heron <phil@sanslogic.co.uk>                    */
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

/* -=== SiS (Sound-in-Syncs) encoder ===- */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video.h"
#include "vbidata.h"

static double _raised_cosine(double x)
{
	if(x <= -1 || x >= 1) return(0);
	return((1.0 + cos(M_PI * x)) / 2);
}

static int __init_quits(vbidata_lut_t *lut, unsigned int nsymbols, unsigned int dwidth, int level, double bwidth, double offset)
{
	int l;
	int b, x;
	int levels[2];
	vbidata_lut_t lc;
	vbidata_lut_t *lptr = (lut ? lut : &lc);
	
	nsymbols *= 2;
	
	levels[0] = level / 2 / 0.75;
	levels[1] = level / 4 / 0.75;
	
	l = 0;
	
	for(b = 0; b < nsymbols; b++)
	{
		double t = -bwidth * (b / 2) - offset;
		
		lptr->offset = lptr->length = 0;
		
		for(x = 0; x < dwidth; x++)
		{
			double h = _raised_cosine((t + x) / bwidth) * levels[b & 1];
			vbidata_update(lptr, lut ? 1 : 0, x, round(h));
		}
		
		l += 2 + lptr->length;
		
		if(lut)
		{
			lptr = (vbidata_lut_t *) &lptr->value[lptr->length];
		}
	}
	
	/* End of LUT marker */
	if(lut)
	{
		lptr->length = -1;
	}
	
	l++;
	
	return(l * sizeof(int16_t));
}

vbidata_lut_t *_init_quits(unsigned int nsymbols, unsigned int dwidth, int level, double bwidth, double offset)
{
	int l;
	vbidata_lut_t *lut;
	
	/* Calculate the length of the lookup-table and allocate memory */
	l = __init_quits(NULL, nsymbols, dwidth, level, bwidth, offset);
	
	lut = malloc(l);
	if(!lut)
	{
		return(NULL);
	}
	
	/* Generate the lookup-table and return */
	__init_quits(lut, nsymbols, dwidth, level, bwidth, offset);
	
	return(lut);
}

int sis_init(sis_t *s, const char *sismode, vid_t *vid, uint8_t mode, uint8_t reserve)
{
	double left, rise, width;
	int i;
	
	memset(s, 0, sizeof(sis_t));
	
	if(strcmp(sismode, "dcsis") == 0)
	{
		/* Nothing yet */
	}
	else
	{
		fprintf(stderr, "Unrecognised SiS mode '%s'.\n", sismode);
		return(VID_ERROR);
	}
	
	/* Render the "quits" - the 4-level symbols */
	s->lut = _init_quits(
		25, vid->width,
		round((vid->white_level - vid->black_level)),
		(double) vid->width / 382,
		(double) vid->width / 382 * 3.32 /* Measured */
	);
	if(!s->lut)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	/* Render the blank window - timings measured from captures */
	left = 0.2e-6;
	rise = 80e-9;
	width = 4.56e-6;
	s->blank_left = floor(vid->pixel_rate * (left - rise / 2));
	s->blank_width = ceil(vid->pixel_rate * (width + rise));
	s->blank_win = malloc(s->blank_width * sizeof(int16_t));
	s->blank_level = vid->sync_level; /* Blank to the sync level */
	if(!s->blank_win)
	{
		free(s->lut);
		return(VID_OUT_OF_MEMORY);
	}
	
	for(i = s->blank_left; i < s->blank_left + s->blank_width; i++)
	{
		double t = 1.0 / vid->pixel_rate * i;
		s->blank_win[i - s->blank_left] = round(rc_window(t, left, width, rise) * INT16_MAX);
	}
	
	/* Init the NICAM encoder */
	nicam_encode_init(&s->nicam, mode, reserve);
	
	return(VID_OK);
}

void sis_free(sis_t *s)
{
	if(!s) return;
	
	free(s->blank_win);
	free(s->lut);
	
	memset(s, 0, sizeof(sis_t));
}

int sis_render(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	sis_t *sis = arg;
	vid_line_t *l = lines[0];
	uint8_t gc[2][4] = { { 3, 0, 2, 1 }, { 0, 3, 1, 2 } };
	uint8_t vbi[7];
	int x, nb;
	
	/* Rate limit by varying the length of the data burst (nb) */
	nb = 50;
	if((sis->re += 44) >= 125)
	{
		nb -= 4;
		sis->re -= 125;
	}
	
	memset(vbi, 0, 7);
	vbi[0] = 0xC0;
	
	for(x = 2; x < nb; x += 2, sis->frame_bit += 2)
	{
		uint8_t sym;
		
		if(sis->frame_bit >= NICAM_FRAME_BITS)
		{
			/* Encode the next frame */
			nicam_encode_frame(&sis->nicam, sis->frame, sis->audio);
			sis->frame_bit = 0;
		}
		
		/* Read the next NICAM bit pair */
		sym = (sis->frame[sis->frame_bit >> 3] >> (6 - (sis->frame_bit & 0x07))) & 0x03;
		
		/* Apply grey coding */
		sym = gc[x & 4 ? 1 : 0][sym];
		
		/* Push it into the data burst */
		vbi[x >> 3] |= sym << (6 - (x & 0x07));
	}
	
	/* Blank the data area */
	for(x = sis->blank_left; x < sis->blank_left + sis->blank_width; x++)
	{
		l->output[x * 2] = (l->output[x * 2] * (INT16_MAX - sis->blank_win[x - sis->blank_left])
		                 + sis->blank_level * sis->blank_win[x - sis->blank_left]) >> 15;
	}
	
	vbidata_render(sis->lut, vbi, 50 - nb, nb, VBIDATA_MSB_FIRST, l);
	
	return(1);
}

int sis_write_audio(sis_t *s, const int16_t *audio)
{
	memcpy(s->audio, audio, sizeof(int16_t) * NICAM_AUDIO_LEN * 2);
	return(VID_OK);
}

