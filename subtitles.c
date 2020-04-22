/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2020 Philip Heron <phil@sanslogic.co.uk>                    */
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "subtitles.h"
#include "hacktv.h"
#include "bitmap.h"

/* Convert hh:mm:ss,mmm to milliseconds */
unsigned int get_ms(char *fmt)
{
	unsigned int ret = 0;
	int i;
	const int m[8] = {
		60 * 60 * 10, 60 * 60, 0,
		60 * 10, 60, 0, 10, 1
	};
	
	for(i = 0; i < 8; i++)
	{
		ret += (fmt[i] - 48) * m[i];
	}
	
	ret *= 1000;
	i = atoi(fmt + 9);
	ret += i;
	
	return(ret);
}

/* Strip simple HTML tags */
void strip_html(char *in)
{
	int i, j, k;
	
	for(i = j = k = 0; i < strlen(in); i++)
	{
		if(in[i] == '<' || in[i] == '{') 
		{
			j = 1;
		} 
		else if(in[i] == '>' || in[i] == '}') 
		{
			j = 0;
		}
		else if(!j)
		{
			/* Convert \N to '\n' newline char */
			if(in[i] == '\\' && in[i + 1] == 'N')
			{
				in[k++] = '\n';
				i++;
			}
			else
			{
				in[k++] = in[i];
			}
		}
	}
	
	/* Terminate string */
	in[k] = '\0';
}

char *get_subtitle_string(char *fmt)
{
	int c, i, l, s;
	c = 0;
	s = 0;
	
	l = strlen(fmt);
	static char txt[256];
	
	for(int a = 0; a < l; a++) txt[a] = '\0';
	
	for(i = 0; i < l - 3; i++)
	{
		if(fmt[i] == ',') c++;
		
		if(c > 8)
		{
			txt[s] = fmt[i + 1];
			s++;
		}
	}
	
	return txt;
}

char *parse_time(char *fmt, int ti)
{
	int c, i, l, s, t;
	c = 0;
	s = 1;

	l = strlen(fmt);
	static char time_txt[] = "00:00:00.000";
			
	t = 0;
	for(i = 0; i < l; i++)
	{
		if(fmt[i] == ',')
		{
			c++;
			t = i;
		}
			
		if(c == ti && t != i)
		{
			time_txt[s] = fmt[i];
			s++;
		}
	}
	
	return time_txt;
}

void load_text_subtitle(av_subs_t *subs, int32_t start_time, int32_t duration, char *fmt)
{
	int sindex;
	
	sindex = subs[0].number_of_subs;
	
	char *s = get_subtitle_string(fmt);
	
	/* Load subs struct with data */
	subs[sindex].index = sindex;
	subs[sindex].start_time = start_time;
	subs[sindex].end_time = start_time + duration;
	
	/* Strip HTML and convert \N to \n */
	strip_html(s);
	
	/* Copy subtitle text into subs struct */
	memcpy(subs[sindex].text, s, 256);
	
	/* Update number of subtitles */
	subs[0].number_of_subs++;
	
	subs[0].type = SUB_TEXT;
}


void load_bitmap_subtitle(av_subs_t *subs, int w, int h, int32_t start_time, int32_t duration, uint32_t *bitmap, int vid_width, int vid_height)
{
	
	int sindex;
	
	sindex = subs[0].number_of_subs;
		
	/* Load subs struct with data */
	subs[sindex].index = sindex;
	subs[sindex].start_time = start_time;
	subs[sindex].end_time = start_time + duration;
	
	subs[sindex].bitmap_height = h;
	int new_width = (float) (vid_width / (float) vid_height) / (16.0/9.0) * w;
	subs[sindex].bitmap_width = new_width;
		
	/* Resize bitmap subtitle and load into subs struct */
	subs[sindex].bitmap = malloc(new_width * h * sizeof(uint32_t));
	resize_bitmap(bitmap, subs[sindex].bitmap, w, h, new_width, h);
	
	/* Update number of subtitles */
	subs[0].number_of_subs++;
	
	/* Set subtitle type */
	subs[0].type = SUB_BITMAP;	
}

int subs_init_ffmpeg(vid_t *s)
{
	av_subs_t *subs;

	/* Give subs typedef some memory - 512Kb enough?! */
	subs = calloc(524288 * sizeof(char), sizeof(av_subs_t));
	
	memset(subs, 0, sizeof(av_subs_t));
	
	subs[0].pos = 0;
	subs[0].number_of_subs = 0;
	
	/* Initialise fonts here   */
	/*         size,    ratio  */
	if(font_init(s, 38, 16.0 / 9.0) !=0)
	{
		return(HACKTV_ERROR);
	};
	
	/* Callback */
	s->av_sub = subs;
	
	return(0);
}

int subs_init_file(char *filename, vid_t *s)
{
	int bufc, c, char_count, n;
	int sindex = 0;
	struct stat fs;
	
	av_subs_t *subs;
		
	/* Hopefully enough chars in arrays */
	char start_time[20], end_time[20], strbuf[256];

	/* Get file size */
	stat(filename, &fs);

	/* Give subs struct some memory */
	subs = calloc(fs.st_size * sizeof(char), sizeof(av_subs_t));
	
	memset(subs, 0, sizeof(av_subs_t));
	
	/* Initialise fonts here */
	/*         size, ratio */
	if(font_init(s, 38, 16.0 / 9.0) < 0)
	{
		return(HACKTV_ERROR);
	};
	
	if(!subs)
	{
		return(HACKTV_OUT_OF_MEMORY);
	}

	FILE *fp;
	fp = fopen(filename,"r");

	/* Rubbish hack to not break on first line for files with BOM */
	int start_file = 1;
	while(1)
	{
		if(fscanf(fp,"%d\n",&n) !=1 && start_file != 1) break;
		start_file = 0;

		/* Get start/end values */
		fscanf(fp,"%s --> %s\n",start_time,end_time);
		bufc=0; 
		char_count=0;

		/* Loop if not EOF */
		while(!feof(fp))
		{
			c = fgetc(fp);

			/* Check for end of section */
			if((c == '\r' || c == '\n') && char_count == 0 )
			{
				/* Line termination and break out of loop */
				strbuf[bufc-1]='\0';
				break;
			}

			/* Check for line break */
			if(char_count == 0)
			{
				/* Line termination */
				strbuf[bufc]='\0';
			}

			/* Add character to string buffer */
			strbuf[bufc++] = c;
				
			/* New line? */
			if(c == '\n')
			{
				/* Reset char count for that line */
				char_count = 0;
			}
			else
			{
				/* Track char counts */
				char_count++;
			} 
		}
		
		/* Load subs struct with data */
		subs[sindex].index = sindex;
		subs[sindex].start_time = get_ms(start_time);
		subs[sindex].end_time = get_ms(end_time);
		strip_html(strbuf);
		memcpy(subs[sindex].text, strbuf, 256);
		sindex++;
	}
	
	/* Close file */
	fclose(fp);
		
	subs[0].pos = 0;
	subs[0].number_of_subs = sindex;
	subs[0].type = SUB_TEXT;
	
	/* Callback */
	s->av_sub = subs;
	
	return(0);
}

char *get_text_subtitle(av_subs_t *subs, int32_t ts)
{
	char *fmt;
	int x;
	
	fmt = "";
	for(x = subs[0].pos; x < subs[0].number_of_subs; x++)
	{
		if(ts >= subs[x].start_time && ts <= subs[x].end_time)
		{
			fmt = subs[x].text;
			subs[0].pos = x;
			break;
		}
	}
	
	return fmt;
}

uint32_t *get_bitmap_subtitle(av_subs_t *subs, int32_t ts, int *w, int *h)
{
	uint32_t *fmt;
	int x;
	
	*w = 0;
	
	fmt = (uint32_t*) "";
	for(x = subs[0].pos; x < subs[0].number_of_subs; x++)
	{
		if(ts >= subs[x].start_time && ts <= subs[x].end_time)
		{
			fmt = subs[x].bitmap;
			subs[0].pos = x;
			*w = subs[x].bitmap_width;
			*h = subs[x].bitmap_height;
			break;
		}
	}
	return fmt;
}

int get_subtitle_type(av_subs_t *subs)
{
	return subs[0].type;
}
