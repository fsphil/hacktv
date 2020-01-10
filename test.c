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

#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include "hacktv.h"
#include  "ascii.h"

/* A small 2-bit hacktv logo */
#define CHAR_WIDTH  8
#define CHAR_HEIGHT 9
#define CHARS 40
#define LOGO_SCALE  5

/* AV test pattern source */
typedef struct {
	int width;
	int height;
	int sample_rate;
	uint32_t *video;
	int16_t *audio;
	size_t audio_samples;
	int lwidth;
	int lheight;
	uint32_t *logo;
} av_test_t;

static char _logo_text[] = "HACKTV";

static uint32_t *_overlay_logo(void *private, char *logotext, int pos)
{
	av_test_t *av = private;
	
	int l, x, y, z, charindex = 0;
	int logotextlength = strlen(logotext);
	uint32_t c;
	
	for (z=0 ; z < logotextlength; z++)
	{
		/* Find char index within ASCII table */
		for(l=0;l<CHARS;l++)  if(toupper(logotext[z]) == chars[l]) charindex = l;
		
		for (x=0; x < CHAR_WIDTH * LOGO_SCALE; x++) 
		{
			for(y=0; y < CHAR_HEIGHT * LOGO_SCALE; y++)
			{
				c = (ascii[(y / LOGO_SCALE * CHAR_WIDTH + x / LOGO_SCALE) + (CHAR_WIDTH * CHAR_HEIGHT * charindex)  ] ==  ' ' ? 0x000000 : 0xFFFFFF ) ;
				av->video[(av->height / pos + y) * av->width + ((av->width - CHAR_WIDTH * (logotextlength - z * 2) * LOGO_SCALE) / 2 ) + x] = c;
			}
		}
	}
	return(av->video);
}

int _read_bmp(void *private)
{
	av_test_t *av = private;
	
	int row_padded, i, k;
	unsigned char *data;
	unsigned char info[54];
	
	FILE* f = fopen("hacktv.bmp", "rb");	
	if(f != NULL) 
	{
		fread(info, sizeof(unsigned char), 54, f); 
		av->lwidth = *(int*)&info[18];
		av->lheight = *(int*)&info[22];	
		
		row_padded = (av->lwidth * 3 + 3) & (~3);
		data = malloc(row_padded * sizeof(int));
		av->logo = malloc(av->lwidth * av->lheight * sizeof(uint32_t));	
		
		for(i = k = 0; i < av->lheight; i++)
		{
			fread(data, sizeof(unsigned char), row_padded, f);
			for(int j = 0; j < av->lwidth * 3; j += 3, k++)
			{
				av->logo[k] = data[j + 2] << 16 | data[j + 1] << 8 | data[j] << 0;
			}
		}
	
		fclose(f);
	}
	else
	{
		/* Overlay the logo */
		_overlay_logo(av,_logo_text, 7);
	}
}

static uint32_t *_overlay_bitmap(void *private, int pos)
{
	av_test_t *av = private;
	int x, y;
	uint32_t c;
	
	_read_bmp(av);
		
	for (x=0; x < av->lwidth; x++) 
	{
		for(y=0; y < av->lheight; y++)
		{
			c = av->logo[x + ((av->lheight - y - 1) * av->lwidth)];
			av->video[(av->height / pos + y) * av->width + ((av->width - av->lwidth) / 2) + x] = c;
		}
	}
	return(av->video);
}

static uint32_t *_av_test_read_video(void *private, float *ratio)
{
	/* Get time */
	char timestr[9];	
	time_t secs = time(0);
	struct tm *local = localtime(&secs);
	sprintf(timestr, "%02d:%02d:%02d", local->tm_hour, local->tm_min, local->tm_sec);
	
	av_test_t *av = private;
	_overlay_logo(av,timestr, 3);
	if(ratio) *ratio = 4.0 / 3.0;
	return(av->video);
}

static int16_t *_av_test_read_audio(void *private, size_t *samples)
{
	av_test_t *av = private;
	*samples = av->audio_samples;
	return(av->audio);
}

static int _av_test_close(void *private)
{
	av_test_t *av = private;
	if(av->video) free(av->video);
	if(av->audio) free(av->audio);
	free(av);
	return(HACKTV_OK);
}

int av_test_open(vid_t *s)
{
	uint32_t const bars[8] = {
		0x000000,
		0x0000BF,
		0xBF0000,
		0xBF00BF,
		0x00BF00,
		0x00BFBF,
		0xBFBF00,
		0xFFFFFF,
	};
	av_test_t *av;
	int c, x, y;
	double d;
	int16_t l;
	
	av = calloc(1, sizeof(av_test_t));
	if(!av)
	{
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	/* Generate a basic test pattern */
	av->width = s->active_width;
	av->height = s->conf.active_lines;
	av->video = malloc(vid_get_framebuffer_length(s));
	av->sample_rate = s->sample_rate;
	if(!av->video)
	{
		free(av);
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	for(y = 0; y < s->conf.active_lines; y++)
	{
		for(x = 0; x < s->active_width; x++)
		{
			if(y < s->conf.active_lines - 140)
			{
				/* 75% colour bars */
				c = 7 - x * 8 / s->active_width;
				c = bars[c];
			}
			else if(y < s->conf.active_lines - 120)
			{
				/* 75% red */
				c = 0xBF0000;
			}
			else if(y < s->conf.active_lines - 100)
			{
				/* Gradient black to white */
				c = x * 0xFF / (s->active_width - 1);
				c = c << 16 | c << 8 | c;
			}
			else
			{
				/* 8 level grey bars */
				c = x * 0xFF / (s->active_width - 1);
				c &= 0xE0;
				c = c | (c >> 3) | (c >> 6);
				c = c << 16 | c << 8 | c;
			}
			
			av->video[y * s->active_width + x] = c;
		}
	}
	
	/* Overlay the logo */
	_overlay_bitmap(av, 7);	
	
	/* Generate the 1khz test tones (BBC 1 style) */
	d = 1000.0 * 2 * M_PI / HACKTV_AUDIO_SAMPLE_RATE;
	y = HACKTV_AUDIO_SAMPLE_RATE * 64 / 100; /* 640ms */
	av->audio_samples = y * 10; /* 6.4 seconds */
	av->audio = malloc(av->audio_samples * 2 * sizeof(int16_t));
	if(!av->audio)
	{
		free(av);
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	for(x = 0; x < av->audio_samples; x++)
	{
		l = sin(x * d) * INT16_MAX * 0.1;
		
		if(x < y)
		{
			/* 0 - 640ms, interrupt left channel */
			av->audio[x * 2 + 0] = 0;
			av->audio[x * 2 + 1] = l;
		}
		else if(x >= y * 2 && x < y * 3)
		{
			/* 1280ms - 1920ms, interrupt right channel */
			av->audio[x * 2 + 0] = l;
			av->audio[x * 2 + 1] = 0;
		}
		else if(x >= y * 4 && x < y * 5)
		{
			/* 2560ms - 3200ms, interrupt right channel again */
			av->audio[x * 2 + 0] = l;
			av->audio[x * 2 + 1] = 0;
		}
		else
		{
			/* Use both channels for all other times */
			av->audio[x * 2 + 0] = l; /* Left */
			av->audio[x * 2 + 1] = l; /* Right */
		}
	}
	
	/* Register the callback functions */
	s->av_private = av;
	s->av_read_video = _av_test_read_video;
	s->av_read_audio = _av_test_read_audio;
	s->av_close = _av_test_close;
	
	return(HACKTV_OK);
}

