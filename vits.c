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
 *
 * 525 line modes are not supported yet, but should be simple.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "vits.h"

static const double _bursts[6] = {
	0.5e6,
	1.0e6,
	2.0e6,
	4.0e6,
	4.8e6,
	5.8e6,
};

static double _win(double t, double left, double width, double rise, double amplitude)
{
	double r, a;
	
	width -= rise;
	t -= left - rise / 2;
	
	if(t <= 0)
	{
		r = 0.0;
	}
	else if(t < rise)
	{
		a = t / rise * M_PI / 2;
		r = pow(sin(a), 2);
	}
	else if(t < rise + width)
	{
		r = 1.0;
	}
	else if(t < rise + width + rise)
	{
		a = (t - width) / rise * M_PI / 2;
		r = pow(sin(a), 2);
	}
	else
	{
		r = 0.0;
	}
	
	return(r * amplitude);
}

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

int vits_init(vits_t *s, unsigned int sample_rate, int width, int lines, int16_t level)
{
	int i, x, b;
	double r, c, t;
	double ts, h;
	double bs[6];
	
	memset(s, 0, sizeof(vits_t));
	
	switch(lines)
	{
	case 625: ts = 1.0 / 25 / 625; break;
	case 525: ts = 1001.0 / 30000 / 525; break;
	default: return(-1);
	}
	
	h = ts / 32;
	ts = ts / width;
	
	for(b = 0; b < 6; b++)
	{
		bs[b] = 2.0 * M_PI * _bursts[b] / sample_rate;
	}
	
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
				r += _win(t, 6 * h, 5 * h, 200e-9, 0.70);
				r += _pulse(t, 13 * h, 200e-9, 0.70);
				r += _pulse(t, 16 * h, 2000e-9, 0.70 / 2);
				c += _pulse(t, 16 * h, 2000e-9, 0.70 / 2);
				r += _win(t, 20 * h, 2 * h, 200e-9, 0.14);
				r += _win(t, 22 * h, 2 * h, 200e-9, 0.28);
				r += _win(t, 24 * h, 2 * h, 200e-9, 0.42);
				r += _win(t, 26 * h, 2 * h, 200e-9, 0.56);
				r += _win(t, 28 * h, 3 * h, 200e-9, 0.70);
				break;
			
			case 1: /* Line 18 */
				r += _win(t, 6 * h, 25 * h, 200e-9,  0.35);
				r += _win(t, 6 * h,  2 * h, 200e-9,  0.21);
				r += _win(t, 8 * h,  2 * h, 200e-9, -0.21);
				
				for(b = 0; b < 6; b++)
				{
					r += _win(t, (12 + 3 * b) * h, 3 * h, 200e-9, 0.21)
					   * sin((t - (12 + 3 * b) * h) + bs[b] * x);
				}
				
				break;
			
			case 2: /* Line 330 */
				r += _win(t, 6 * h, 5 * h, 200e-9, 0.70);
				r += _pulse(t, 13 * h, 200e-9, 0.70);
				c += _win(t, 15 * h, 15 * h, 1e-6, 0.28 / 2);
				r += _win(t, 20 * h, 2 * h, 200e-9, 0.14);
				r += _win(t, 22 * h, 2 * h, 200e-9, 0.28);
				r += _win(t, 24 * h, 2 * h, 200e-9, 0.42);
				r += _win(t, 26 * h, 2 * h, 200e-9, 0.56);
				r += _win(t, 28 * h, 3 * h, 200e-9, 0.70);
				break;
			
			case 3: /* Line 331 */
				r += _win(t, 6 * h, 25 * h, 200e-9, 0.35);
				c += _win(t, 7 * h, 7 * h, 1e-6, 0.70 / 2);
				c += _win(t, 17 * h, 13 * h, 1e-6, 0.42 / 2);
				break;
			}
			
			s->line[i][x * 2 + 0] = lround(r / 0.7 * level);
			s->line[i][x * 2 + 1] = lround(c / 0.7 * level);
		}
	}
	
	return(0);
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

int vits_render(vits_t *s, int16_t *buffer, int line, const int16_t *lut_i, const int16_t *lut_q)
{
	int i, x;
	
	if(line == 17 || line == 18)
	{
		i = line - 17;
	}
	else if(line == 330 || line == 331)
	{
		i = line - 330 + 2;
	}
	else
	{
		return(0);
	}
	
	for(x = 0; x < s->width; x++)
	{
		buffer[x * 2] += s->line[i][x * 2 + 0];
		buffer[x * 2] += (lut_i[x] * s->line[i][x * 2 + 1]) >> 15;
	}
	
	return(1);
}

