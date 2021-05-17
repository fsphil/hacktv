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
#include "graphics.h"

/* AV test pattern source */
typedef struct {
	int vid_width;
	int vid_height;
	uint32_t *video;
	int16_t *audio;
	size_t audio_samples;
	int img_width;
	int img_height;
	image_t image;
	av_font_t *font[10];
} av_test_t;

static uint32_t *_av_test_read_video(void *private, float *ratio)
{
	/* Get current time */
	char timestr[9];
	time_t secs = time(0);
	struct tm *local = localtime(&secs);
	sprintf(timestr, "%02d:%02d:%02d", local->tm_hour, local->tm_min, local->tm_sec);
	
	av_test_t *av = private;
	
	/* Print clock */
	if(av->font[0])
	{
		print_generic_text(	av->font[0],
							av->video,
							timestr,
							av->font[0]->x_loc, av->font[0]->y_loc, 0, 1, 0, 1);
	}
						
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

int av_test_open(vid_t *s, char *test_screen)
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
	av->vid_width = s->active_width;
	av->vid_height = s->conf.active_lines;
	av->video = malloc(vid_get_framebuffer_length(s));
	if(!av->video)
	{
		free(av);
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	/* Colour bars - for non-625 line modes */
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
	
	if(test_screen == NULL) test_screen = "pm5544";
	
	float img_ratio = (strcmp(test_screen, "pm5644") == 0) ? 16.0 / 9.0 : 4.0 / 3.0;
	
	/* Initialise default fonts */
	
	/* Clock */
	font_init(s, 56, img_ratio);
	av->font[0] = s->av_font;
	av->font[0]->x_loc = 50;
	av->font[0]->y_loc = 50;
	
	/* HACKTV text*/
	font_init(s, 72, img_ratio);
	av->font[1] = s->av_font;
	av->font[1]->x_loc = 50;
	av->font[1]->y_loc = 25;
	
	/* Overlay test screen */
	if(av->vid_height == 576 && strcmp(test_screen, "colourbars") != 0)
	{
		if(load_png(&av->image, av->vid_width, av->vid_height, test_screen, 1.0, img_ratio, IMG_TEST) == HACKTV_OK)
		{	
			overlay_image(av->video, &av->image, av->vid_width, av->vid_height, IMG_POS_FULL);
			
			if(strcmp(test_screen, "pm5544") == 0)
			{
				av->font[0]->y_loc = 82.3;
			}
			else if(strcmp(test_screen, "pm5644") == 0)
			{
				av->font[0]->y_loc = 82;
			}
			else if(strcmp(test_screen, "fubk") == 0)
			{
				/* Reinit font with new size */
				font_init(s, 44, img_ratio);
				av->font[0] = s->av_font;
				av->font[0]->x_loc = 52;
				av->font[0]->y_loc = 55.5;
			}
			else if(strcmp(test_screen, "ueitm") == 0)
			{
				/* Don't display clock */
				av->font[0] = NULL;
			}
			
		}
		else
		{
			print_generic_text(	av->font[1], av->video, "HACKTV", av->font[1]->x_loc, av->font[1]->y_loc, 0, 1, 0, 1);
		}
	}
	else
	{
		print_generic_text(	av->font[1], av->video, "HACKTV", av->font[1]->x_loc, av->font[1]->y_loc, 0, 1, 0, 1);
	}
	
	/* Print logo, if enabled */
	if(s->conf.logo)
	{
		if(load_png(&s->vid_logo, s->active_width, s->conf.active_lines, s->conf.logo, 0.75, 4.0/3.0, IMG_LOGO) != HACKTV_OK)
		{
			s->conf.logo = NULL;
		}
		else
		{
			overlay_image(av->video, &s->vid_logo, av->vid_width, av->vid_height, IMG_POS_TR);
		}
	}
	
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

