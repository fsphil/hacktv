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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fir.h"



/* Some of the filter design functions contained within here where taken
   from or are based on those within gnuradio's gr-filter/lib/firdes.cc */



static double i_zero(double x)
{
	double sum, u, halfx, temp;
	int n;
	
	sum = u = n = 1;
	halfx = x / 2.0;
	
	do
	{
		temp = halfx / (double) n;
		n += 1;
		temp *= temp;
		u *= temp;
		sum += u;
	}
	while(u >= 1e-21 * sum);
	
	return(sum);
}

static void kaiser(double *taps, size_t ntaps, double beta)
{
	double i_beta = 1.0 / i_zero(beta);
	double inm1 = 1.0 / ((double) (ntaps - 1));
	double temp;
	int i;
	
	taps[0] = i_beta;
	
	for(i = 1; i < ntaps - 1; i++)
	{
		temp = 2 * i * inm1 - 1;
		taps[i] = i_zero(beta * sqrt(1.0 - temp * temp)) * i_beta;
	}
	
	taps[ntaps - 1] = i_beta;
}

void fir_low_pass(double *taps, size_t ntaps, double sample_rate, double cutoff, double width, double gain)
{
	int n;
	int M = (ntaps - 1) / 2;
	double fmax;
	double fwT0 = 2 * M_PI * cutoff / sample_rate;
	
	/* Create the window */
	kaiser(taps, ntaps, 7.0);
	
	/* Generate the filter taps */
	for(n = -M; n <= M; n++)
	{
		if(n == 0)
		{
			taps[n + M] *= fwT0 / M_PI;
		}
		else
		{
			taps[n + M] *= sin(n * fwT0) / (n * M_PI);
		}
	}
	
	/* find the factor to normalize the gain, fmax.
	 * For low-pass, gain @ zero freq = 1.0 */
	
	fmax = taps[0 + M];
	
	for(n = 1; n <= M; n++)
	{
		fmax += 2 * taps[n + M];
	}
	
	/* Normalise */
	gain /= fmax;
	
	for(n = 0; n < ntaps; n++)
	{
		 taps[n] *= gain;
	}
}

void fir_complex_band_pass(double *taps, size_t ntaps, double sample_rate, double low_cutoff, double high_cutoff, double width, double gain)
{
	double *lptaps;
	double freq =  M_PI * (high_cutoff + low_cutoff) / sample_rate;
	double phase;
	int i;
	
	lptaps = &taps[ntaps];
	
	fir_low_pass(lptaps, ntaps, sample_rate, (high_cutoff - low_cutoff) / 2, width, gain);
	
	if(ntaps & 1)
	{
		phase = -freq * (ntaps >> 1);
	}
	else
	{
		phase = -freq / 2.0 * ((1 + 2 * ntaps) >> 1);
	}
	
	for(i = 0; i < ntaps; i++, phase += freq)
	{
		taps[i * 2 + 0] = lptaps[i] * cos(phase);
		taps[i * 2 + 1] = lptaps[i] * sin(phase);
	}
}



/* int16_t */



void fir_int16_low_pass(int16_t *taps, size_t ntaps, double sample_rate, double cutoff, double width, double gain)
{
	double *dtaps;
	int i;
	int a;
	
	dtaps = calloc(ntaps, sizeof(double));
	
	fir_low_pass(dtaps, ntaps, sample_rate, cutoff, width, gain);
	
	for(a = i = 0; i < ntaps; i++)
	{
		taps[i] = round(dtaps[i] * 32767.0);
		a += taps[i];
	}
	
	free(dtaps);
}

int fir_int16_init(fir_int16_t *s, const int16_t *taps, unsigned int ntaps)
{
	int i;
	
	s->type  = 1;
	s->ntaps = ntaps;
	s->itaps = malloc(s->ntaps * sizeof(int16_t));
	s->qtaps = NULL;
	
	for(i = 0; i < ntaps; i++)
	{
		s->itaps[i] = taps[i];
	}
	
	s->win = calloc(s->ntaps * 2, sizeof(int16_t));
	
	return(0);
}

size_t fir_int16_process(fir_int16_t *s, int16_t *signal, size_t samples)
{
	int a;
	int x;
	int y;
	int p;
	
	if(s->type == 0) return(0);
	else if(s->type == 2) return(fir_int16_complex_process(s, signal, samples));
	
	for(x = 0; x < samples; x++)
	{
		/* Append the next input sample to the round buffer */
		s->win[s->owin] = s->win[s->owin + s->ntaps] = *signal;
		if(++s->owin == s->ntaps) s->owin = 0;
		
		/* Calculate the next output sample */
		a = 0;
		p = s->owin;
		
		for(y = 0; y < s->ntaps; y++, p++)
		{
			a += s->win[p] * s->itaps[y];
		}
		
		*signal = a >> 15;
		signal += 2;
	}
	
	return(samples);
}


void fir_int16_free(fir_int16_t *s)
{
	free(s->win);
	free(s->itaps);
	free(s->qtaps);
	memset(s, 0, sizeof(fir_int16_t));
}



/* complex int16_t */



void fir_int16_complex_band_pass(int16_t *taps, size_t ntaps, double sample_rate, double low_cutoff, double high_cutoff, double width, double gain)
{
	double *dtaps;
	int i;
	int a;
	
	dtaps = calloc(ntaps, sizeof(double) * 2);
	
	fir_complex_band_pass(dtaps, ntaps, sample_rate, low_cutoff, high_cutoff, width, gain);
	
	for(a = i = 0; i < ntaps * 2; i++)
	{
		taps[i] = round(dtaps[i] * 32767.0);
		a += taps[i];
	}
	
	free(dtaps);
}

int fir_int16_complex_init(fir_int16_t *s, const int16_t *taps, unsigned int ntaps)
{
	int i;
	
	s->type  = 2;
	s->ntaps = ntaps;
	s->itaps = malloc(s->ntaps * sizeof(int16_t) * 2);
	s->qtaps = malloc(s->ntaps * sizeof(int16_t) * 2);
	
	/* Copy the taps in the order and format they are to be used */
	for(i = 0; i < ntaps; i++)
	{
		s->itaps[i * 2 + 0] =  taps[i * 2 + 0];
		s->itaps[i * 2 + 1] = -taps[i * 2 + 1];
		s->qtaps[i * 2 + 0] =  taps[i * 2 + 1];
		s->qtaps[i * 2 + 1] =  taps[i * 2 + 0];
	}
	
	s->win = calloc(s->ntaps * 2, sizeof(int16_t) * 2);
	
	return(0);
}

size_t fir_int16_complex_process(fir_int16_t *s, int16_t *signal, size_t samples)
{
	int32_t ai, aq;
	int x;
	int y;
	int p;
	
	for(x = 0; x < samples; x++)
	{
		/* Append the next input sample to the sliding window */
		s->win[s->owin * 2 + 0] = s->win[(s->owin + s->ntaps) * 2 + 0] = signal[0];
		s->win[s->owin * 2 + 1] = s->win[(s->owin + s->ntaps) * 2 + 1] = signal[1];
		if(++s->owin == s->ntaps) s->owin = 0;
		
		/* Calculate the next output sample */
		ai = aq = 0;
		
		for(p = s->owin * 2, y = 0; y < s->ntaps * 2; y++, p++)
		{
			ai += s->win[p] * s->itaps[y];
			aq += s->win[p] * s->qtaps[y];
		}
		
		signal[0] = aq >> 15;
		signal[1] = ai >> 15;
		signal += 2;
	}
	
	return(samples);
}

