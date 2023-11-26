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
#include "hacktv.h"

/* A small 2-bit hacktv logo */
#define LOGO_WIDTH  48
#define LOGO_HEIGHT 9
#define LOGO_SCALE  4
static const char *_logo =
	"                                                "
	" ##  ##    ##     ####   ##  ##  ######  ##  ## "
	" ##  ##   ####   ##  ##  ## ##     ##    ##  ## "
	" ##  ##  ##  ##  ##      ####      ##    ##  ## "
	" ######  ######  ##      ###       ##    ##  ## "
	" ##  ##  ##  ##  ##      ####      ##    ##  ## "
	" ##  ##  ##  ##  ##  ##  ## ##     ##     ####  "
	" ##  ##  ##  ##   ####   ##  ##    ##      ##   "
	"                                                ";

/* AV test pattern source */
typedef struct {
	int width;
	int height;
	uint32_t *video;
	int16_t *audio;
	size_t audio_samples;
} av_test_t;

static int _test_read_video(void *ctx, av_frame_t *frame)
{
	av_test_t *s = ctx;
	av_frame_init(frame, s->width, s->height, s->video, 1, s->width);
	av_set_display_aspect_ratio(frame, (rational_t) { 4, 3 });
	return(AV_OK);
}

static int16_t *_test_read_audio(void *ctx, size_t *samples)
{
	av_test_t *s = ctx;
	*samples = s->audio_samples;
	return(s->audio);
}

static int _test_close(void *ctx)
{
	av_test_t *s = ctx;
	if(s->video) free(s->video);
	if(s->audio) free(s->audio);
	free(s);
	return(HACKTV_OK);
}

int av_test_open(av_t *av)
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
	av_test_t *s;
	int c, x, y;
	double d;
	int16_t l;
	
	s = calloc(1, sizeof(av_test_t));
	if(!s)
	{
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	/* Generate a basic test pattern */
	s->width = av->width;
	s->height = av->height;
	s->video = malloc(av->width * av->height * sizeof(uint32_t));
	if(!s->video)
	{
		free(s);
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	for(y = 0; y < s->height; y++)
	{
		for(x = 0; x < s->width; x++)
		{
			if(y < s->height - 140)
			{
				/* 75% colour bars */
				c = 7 - x * 8 / s->width;
				c = bars[c];
			}
			else if(y < s->height - 120)
			{
				/* 75% red */
				c = 0xBF0000;
			}
			else if(y < s->height - 100)
			{
				/* Gradient black to white */
				c = x * 0xFF / (s->width - 1);
				c = c << 16 | c << 8 | c;
			}
			else
			{
				/* 8 level grey bars */
				c = x * 0xFF / (s->width - 1);
				c &= 0xE0;
				c = c | (c >> 3) | (c >> 6);
				c = c << 16 | c << 8 | c;
			}
			
			s->video[y * s->width + x] = c;
		}
	}
	
	/* Overlay the logo */
	if(s->width >= LOGO_WIDTH * LOGO_SCALE &&
	   s->height >= LOGO_HEIGHT * LOGO_SCALE)
	{
		x = s->width / 2;
		y = s->height / 10;
		
		for(x = 0; x < LOGO_WIDTH * LOGO_SCALE; x++)
		{
			for(y = 0; y < LOGO_HEIGHT * LOGO_SCALE; y++)
			{
				c = _logo[y / LOGO_SCALE * LOGO_WIDTH + x / LOGO_SCALE] == ' ' ? 0x000000 : 0xFFFFFF;
				
				s->video[(s->height / 10 + y) * s->width + ((s->width - LOGO_WIDTH * LOGO_SCALE) / 2) + x] = c;
			}
		}
	}
	
	/* Generate the 1khz test tones (BBC 1 style) */
	d = 1000.0 * 2 * M_PI * av->sample_rate.den / av->sample_rate.num;
	y = av->sample_rate.num / av->sample_rate.den * 64 / 100; /* 640ms */
	s->audio_samples = y * 10; /* 6.4 seconds */
	s->audio = malloc(s->audio_samples * 2 * sizeof(int16_t));
	if(!s->audio)
	{
		free(s->video);
		free(s);
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	for(x = 0; x < s->audio_samples; x++)
	{
		l = sin(x * d) * INT16_MAX * 0.1;
		
		if(x < y)
		{
			/* 0 - 640ms, interrupt left channel */
			s->audio[x * 2 + 0] = 0;
			s->audio[x * 2 + 1] = l;
		}
		else if(x >= y * 2 && x < y * 3)
		{
			/* 1280ms - 1920ms, interrupt right channel */
			s->audio[x * 2 + 0] = l;
			s->audio[x * 2 + 1] = 0;
		}
		else if(x >= y * 4 && x < y * 5)
		{
			/* 2560ms - 3200ms, interrupt right channel again */
			s->audio[x * 2 + 0] = l;
			s->audio[x * 2 + 1] = 0;
		}
		else
		{
			/* Use both channels for all other times */
			s->audio[x * 2 + 0] = l; /* Left */
			s->audio[x * 2 + 1] = l; /* Right */
		}
	}
	
	/* Register the callback functions */
	av->av_source_ctx = s;
	av->read_video = _test_read_video;
	av->read_audio = _test_read_audio;
	av->close = _test_close;
	
	return(HACKTV_OK);
}

