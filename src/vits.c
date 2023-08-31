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

/* -=== VITS test signal inserter ===- */

/* TODO: The phase of the chrominance signal is likely wrong.
 *
 * The rise and fall shape of the five-riser staircase is specified as
 * "shaped by a Thomson filter (or similar network) with a transfer function
 * modulus having its first zero at 4.43 MHz to restrict the amplitude of
 * components of the luminance signal in the vicinity of the colour
 * sub-carrier". This code uses the same shape as the other parts:
 * "derived from the shaping network of the sine-squared pulse".
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video.h"

static const double _bursts_625[6] = {
	0.5e6,
	1.0e6,
	2.0e6,
	4.0e6,
	4.8e6,
	5.8e6,
};

static const double _bursts_525[6] = {
	0.50e6,
	1.00e6,
	2.00e6,
	3.00e6,
	3.58e6,
	4.20e6,
};

static double _pulse(double t, double position, double width, double amplitude)
{
	double a;
	
	t -= position - width;
	
	if(t <= 0 || t >= width * 2)
	{
		return(0);
	}
	
	a = t / (width * 2) * M_PI;
	
	return(pow(sin(a), 2) * amplitude);
}

static int _init_625(vits_t *s, unsigned int sample_rate, int width, int level)
{
	int i, x, b;
	double r, c, t;
	double ts, h;
	double bs[6];
	
	/* Setup timing */
	ts = 1.0 / 25 / 625;
	h = ts / 32;
	ts = ts / width;
	
	for(b = 0; b < 6; b++)
	{
		bs[b] = 2.0 * M_PI * _bursts_625[b];
	}
	
	s->lines = 625;
	s->width = width;
	
	for(i = 0; i < 4; i++)
	{
		s->line[i] = malloc(sizeof(int16_t) * 2 * width);
		if(!s->line[i])
		{
			perror("malloc");
			vits_free(s);
			return(-1);
		}
		
		for(x = 0; x < width; x++)
		{
			t = ts * x;
			r = 0.0;
			c = 0.0;
			
			switch(i)
			{
			case 0: /* Line 17 */
				r += rc_window(t, 6 * h, 5 * h, 200e-9) * 0.70;
				r += _pulse(t, 13 * h, 200e-9, 0.70);
				r += _pulse(t, 16 * h, 2000e-9, 0.70 / 2);
				c += _pulse(t, 16 * h, 2000e-9, 0.70 / 2);
				r += rc_window(t, 20 * h, 2 * h, 200e-9) * 0.14;
				r += rc_window(t, 22 * h, 2 * h, 200e-9) * 0.28;
				r += rc_window(t, 24 * h, 2 * h, 200e-9) * 0.42;
				r += rc_window(t, 26 * h, 2 * h, 200e-9) * 0.56;
				r += rc_window(t, 28 * h, 3 * h, 200e-9) * 0.70;
				break;
			
			case 1: /* Line 18 */
				r += rc_window(t, 6 * h, 25 * h, 200e-9) *  0.35;
				r += rc_window(t, 6 * h,  2 * h, 200e-9) *  0.21;
				r += rc_window(t, 8 * h,  2 * h, 200e-9) * -0.21;
				
				for(b = 0; b < 6; b++)
				{
					r += rc_window(t, (12 + 3 * b) * h, 2 * h, 200e-9) * 0.21
					   * sin((t - (12 + 3 * b) * h) * bs[b]);
				}
				
				break;
			
			case 2: /* Line 330 */
				r += rc_window(t, 6 * h, 5 * h, 200e-9) * 0.70;
				r += _pulse(t, 13 * h, 200e-9, 0.70);
				c += rc_window(t, 15 * h, 15 * h, 1e-6) * 0.28 / 2;
				r += rc_window(t, 20 * h, 2 * h, 200e-9) * 0.14;
				r += rc_window(t, 22 * h, 2 * h, 200e-9) * 0.28;
				r += rc_window(t, 24 * h, 2 * h, 200e-9) * 0.42;
				r += rc_window(t, 26 * h, 2 * h, 200e-9) * 0.56;
				r += rc_window(t, 28 * h, 3 * h, 200e-9) * 0.70;
				break;
			
			case 3: /* Line 331 */
				r += rc_window(t, 6 * h, 25 * h, 200e-9) * 0.35;
				c += rc_window(t, 7 * h, 7 * h, 1e-6) * 0.70 / 2;
				c += rc_window(t, 17 * h, 13 * h, 1e-6) * 0.42 / 2;
				break;
			}
			
			s->line[i][x * 2 + 0] = lround(r / 0.7 * level);
			s->line[i][x * 2 + 1] = lround(c / 0.7 * level);
		}
	}
	
	return(0);
}

static int _init_525(vits_t *s, unsigned int sample_rate, int width, int level)
{
	int i, x, b;
	double r, c, t;
	double ts, h;
	double bs[6];
	
	/* Setup timing */
	ts = 1001.0 / 30000 / 525;
	h = ts / 128;
	ts = ts / width;
	
	for(b = 0; b < 6; b++)
	{
		bs[b] = 2.0 * M_PI * _bursts_525[b];
	}
	
	s->lines = 525;
	s->width = width;
	
	for(i = 0; i < 2; i++)
	{
		s->line[i] = malloc(sizeof(int16_t) * 2 * width);
		if(!s->line[i])
		{
			perror("malloc");
			vits_free(s);
			return(-1);
		}
		
		for(x = 0; x < width; x++)
		{
			t = ts * x;
			r = 0.0;
			c = 0.0;
			
			switch(i)
			{
			case 0: /* Line 17 */
				r += rc_window(t, 24 * h, 36 * h, 125e-9) * 100;
				r += _pulse(t, 68 * h, 250e-9, 100);
				r += _pulse(t, 75 * h, 1570e-9, 100 / 2);
				c += _pulse(t, 75 * h, 1570e-9, 100 / 2);
				r += rc_window(t,  92 * h,  6 * h, 250e-9) * 18;
				r += rc_window(t,  98 * h,  6 * h, 250e-9) * 36;
				r += rc_window(t, 104 * h,  6 * h, 250e-9) * 54;
				r += rc_window(t, 110 * h,  6 * h, 250e-9) * 72;
				r += rc_window(t, 116 * h,  8 * h, 250e-9) * 90;
				c += rc_window(t,  84 * h, 38 * h, 400e-9) * 40 / 2;
				break;
			
			case 1: /* Line 280 */
				r += rc_window(t, 24 * h, 8 * h, 125e-9) * 100;
				r += rc_window(t, 32 * h, 92 * h, 125e-9) * 50;
				
				r += rc_window(t, 36 * h, 12 * h, 250e-9) * 50 / 2
				   * sin((t - 36 * h) * bs[0]);
				
				for(b = 1; b < 6; b++)
				{
					r += rc_window(t, (40 + 8 * b) * h, 8 * h, 250e-9) * 50 / 2
					   * sin((t - (40 + 8 * b) * h) * bs[b]);
				}
				
				c += rc_window(t,  92 * h,  8 * h, 400e-9) * 20 / 2;
				c += rc_window(t, 100 * h,  8 * h, 400e-9) * 40 / 2;
				c += rc_window(t, 108 * h, 12 * h, 400e-9) * 80 / 2;
				
				break;
			}
			
			s->line[i][x * 2 + 0] = lround(r / 100 * level);
			s->line[i][x * 2 + 1] = lround(c / 100 * level);
		}
	}
	
	return(0);
}

int vits_init(vits_t *s, unsigned int sample_rate, int width, int lines, int pal, int level)
{
	memset(s, 0, sizeof(vits_t));
	
	if(lines == 625) return(_init_625(s, sample_rate, width, level));
	else if(lines == 525) return(_init_525(s, sample_rate, width, level));
	
	if(pal)
	{
		/* The insertion signals is locked at 60 ± 5° from the positive (B-Y) axis for PAL */
		double p = 135.0 * (M_PI / 180.0);
		s->cs_phase = (cint16_t) {
			round(sin(p) * INT16_MAX),
			round(cos(p) * INT16_MAX),
		};
	}
	else
	{
		/* For NTSC is is the same as the burst, 180° */
		s->cs_phase = (cint16_t) { 0, -INT16_MAX };
	}
	
	return(-1);
}

void vits_free(vits_t *s)
{
	int i;
	
	for(i = 0; i < 4; i++)
	{
		free(s->line[i]);
	}
	
	memset(s, 0, sizeof(vits_t));
}

int vits_render(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	vits_t *v = arg;
	int x, i = -1;
	vid_line_t *l = lines[0];
	
	if(v->lines == 625)
	{
		if(l->line == 17 || l->line == 18) i = l->line - 17;
		else if(l->line == 330 || l->line == 331) i = l->line - 330 + 2;
	}
	else if(v->lines == 525)
	{
		if(l->line == 17) i = l->line - 17;
		else if(l->line == 280) i = l->line - 280 + 1;
	}
	
	if(i < 0) return(0);
	if(!v->line[i]) return(0);
	
	for(x = 0; x < s->width; x++)
	{
		l->output[x * 2] += v->line[i][x * 2 + 0];
		if(l->lut)
		{
			l->output[x * 2] += (((v->cs_phase.i * l->lut[x].q +
				v->cs_phase.q * l->lut[x].i) >> 15) * v->line[i][x * 2 + 1]) >> 15;
		}
	}
	
	l->vbialloc = 1;
	
	return(1);
}

