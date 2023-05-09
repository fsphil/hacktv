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
#include "common.h"



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
	int n, M;
	double fmax, fwT0;
	
	/* Ensure an odd number of taps */
	if((ntaps & 1) == 0)
	{
		ntaps -= 1;
		taps[ntaps] = 0;
	}
	
	/* Create the window */
	kaiser(taps, ntaps, 7.0);
	
	/* Generate the filter taps */
	M = (ntaps - 1) / 2;
	fwT0 = 2.0 * M_PI * cutoff / sample_rate;
	
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

void fir_band_reject(double *taps, size_t ntaps, double sample_rate, double low_cutoff, double high_cutoff, double width, double gain)
{
	int n, M;
	double fmax, fwT0, fwT1;
	
	/* Ensure an odd number of taps */
	if((ntaps & 1) == 0)
	{
		ntaps -= 1;
		taps[ntaps] = 0;
	}
	
	/* Create the window */
	kaiser(taps, ntaps, 7.0);
	
	/* Generate the filter taps */
	M = (ntaps - 1) / 2;
	fwT0 = 2.0 * M_PI * low_cutoff / sample_rate;
	fwT1 = 2.0 * M_PI * high_cutoff / sample_rate;
	
	for(n = -M; n <= M; n++)
	{
		if(n == 0)
		{
			taps[n + M] *= 1.0 + (fwT0 - fwT1) / M_PI;
		}
		else
		{
			taps[n + M] *= (sin(n * fwT0) - sin(n * fwT1)) / (n * M_PI);
		}
	}
	
	/* find the factor to normalize the gain, fmax.
	 * For band-reject, gain @ zero freq = 1.0 */
	
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
	double freq = M_PI * (high_cutoff + low_cutoff) / sample_rate;
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



int fir_int16_init(fir_int16_t *s, const double *taps, unsigned int ntaps, int interpolation, int decimation, int delay)
{
	int i, j;
	
	s->type = 1;
	
	s->interpolation = interpolation;
	s->decimation = decimation;
	
	/* Round number of taps up to a multiple of the interpolation factor */
	s->ntaps = ntaps + (ntaps % interpolation ? interpolation - (ntaps % interpolation) : 0);
	s->ataps = s->ntaps / interpolation;
	
	s->itaps = calloc(s->ntaps, sizeof(int16_t));
	s->qtaps = NULL;
	
	/* Copy taps into the order they will be applied */
	j = s->ntaps - s->ataps;
	for(i = ntaps - 1; i >= 0; i--)
	{
		s->itaps[j] = lround(taps[i] * 32767.0);
		j -= s->ataps;
		if(j < 0) j += s->ntaps + 1;
	}
	
	s->lwin = s->ataps + delay;
	s->win = calloc(s->ataps * 2 + delay, sizeof(int16_t));
	s->owin = 0;
	s->d = 0;
	
	return(0);
}

size_t fir_int16_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples, int step)
{
	int a;
	int x, y;
	const int16_t *win, *taps;
	
	if(s->type == 0) return(0);
	else if(s->type == 2) return(fir_int16_complex_process(s, out, in, samples));
	else if(s->type == 3) return(fir_int16_scomplex_process(s, out, in, samples));
	
	for(x = 0; samples; samples--)
	{
		/* Append the next input sample to the round buffer */
		s->win[s->owin] = *in;
		if(s->owin < s->ataps) s->win[s->owin + s->lwin] = *in;
		if(++s->owin == s->lwin) s->owin = 0;
		
		for(; s->d < s->interpolation; s->d += s->decimation)
		{
			win = &s->win[s->owin];
			taps = &s->itaps[s->d * s->ataps];
			
			/* Calculate the next output sample */
			for(a = y = 0; y < s->ataps; y++)
			{
				a += *(win++) * *(taps++);
			}
			
			a >>= 15;
			*out = a < INT16_MIN ? INT16_MIN : (a > INT16_MAX ? INT16_MAX : a);
			out += step;
			x++;
		}
		s->d -= s->interpolation;
		
		in += step;
	}
	
	return(x);
}

size_t fir_int16_process_block(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples, int step)
{
	int x;
	
	/* Pre-fill buffer */
	memset(s->win, 0, (s->lwin + s->ataps) * sizeof(int16_t));
	s->owin = 0;
	
	for(s->owin = 0; s->owin < s->ataps / 2; s->owin++, in += 2)
	{
		s->win[s->owin] = *in;
		if(s->owin < s->ataps) s->win[s->owin + s->lwin] = *in;
	}
	
	x = fir_int16_process(s, out, in, samples, step);
	
	return(x);
}

void fir_int16_free(fir_int16_t *s)
{
	free(s->win);
	free(s->itaps);
	free(s->qtaps);
	memset(s, 0, sizeof(fir_int16_t));
}

/* Initialise int16 FIR filter rational resampler */
int fir_int16_resampler_init(fir_int16_t *s, int interpolation, int decimation)
{
	int ntaps;
	double *taps;
	int d;
	
	/* Simplify ratio */
	d = gcd(interpolation, decimation);
	interpolation /= d;
	decimation /= d;
	
	/* Generate the filter taps */
	ntaps = 21 * interpolation;
	ntaps += (ntaps % interpolation ? interpolation - (ntaps % interpolation) : 0);
	if((ntaps & 1) == 0) ntaps--;
	
	taps = calloc(ntaps, sizeof(double));
	if(!taps)
	{
		return(-1);
	}
	
	if(interpolation > decimation)
	{
		fir_low_pass(taps, ntaps, interpolation, 0.45, 0.1, interpolation);
	}
	else
	{
		fir_low_pass(taps, ntaps, interpolation, 0.45 * interpolation / decimation, 0.1 * interpolation / decimation, interpolation);
	}
	
	/* Create the FIR filter */
	d = fir_int16_init(s, taps, ntaps, interpolation, decimation, 0);
	free(taps);
	
	return(d);
}



/* complex int16_t */



int fir_int16_complex_init(fir_int16_t *s, const double *taps, unsigned int ntaps, int interpolation, int decimation, int delay)
{
	int i, j;
	
	s->type = 2;
	
	s->interpolation = interpolation;
	s->decimation = decimation;
	
	/* Round number of taps up to a multiple of the interpolation factor */
	s->ntaps = ntaps + (ntaps % interpolation ? interpolation - (ntaps % interpolation) : 0);
	s->ataps = s->ntaps / interpolation;
	
	s->itaps = calloc(s->ntaps, sizeof(int16_t) * 2);
	s->qtaps = calloc(s->ntaps, sizeof(int16_t) * 2);
	
	/* Copy the taps in the order and format they are to be used */
	j = s->ntaps - s->ataps;
	for(i = ntaps - 1; i >= 0; i--)
	{
		s->itaps[j * 2 + 0] = lround( taps[i * 2 + 0] * 32767.0);
		s->itaps[j * 2 + 1] = lround(-taps[i * 2 + 1] * 32767.0);
		s->qtaps[j * 2 + 0] = lround( taps[i * 2 + 1] * 32767.0);
		s->qtaps[j * 2 + 1] = lround( taps[i * 2 + 0] * 32767.0);
		j -= s->ataps;
		if(j < 0) j += s->ntaps + 1;
	}
	
	s->lwin = s->ataps + delay;
	s->win = calloc(s->ataps * 2 + delay, sizeof(int16_t) * 2);
	s->owin = 0;
	s->d = 0;
	
	return(0);
}

size_t fir_int16_complex_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples)
{
	int32_t ai, aq;
	int x, y;
	const int16_t *win, *itaps, *qtaps;
	
	for(x = 0; samples; samples--)
	{
		/* Append the next input sample to the round buffer */
		s->win[s->owin * 2 + 0] = in[0];
		s->win[s->owin * 2 + 1] = in[1];
		if(s->owin < s->ataps)
		{
			s->win[(s->owin + s->lwin) * 2 + 0] = in[0];
			s->win[(s->owin + s->lwin) * 2 + 1] = in[1];
		}
		if(++s->owin == s->lwin) s->owin = 0;
		
		for(; s->d < s->interpolation; s->d += s->decimation)
		{
			win = &s->win[s->owin * 2];
			itaps = &s->itaps[s->d * s->ataps];
			qtaps = &s->qtaps[s->d * s->ataps];
			
			/* Calculate the next output sample */
			for(ai = aq = y = 0; y < s->ataps; y++)
			{
				ai += *(win++) * *(itaps++);
				aq += *(win++) * *(qtaps++);
			}
			
			ai >>= 15;
			aq >>= 15;
			out[0] = ai < INT16_MIN ? INT16_MIN : (ai > INT16_MAX ? INT16_MAX : ai);
			out[1] = aq < INT16_MIN ? INT16_MIN : (aq > INT16_MAX ? INT16_MAX : aq);
			out += 2;
			x++;
		}
		s->d -= s->interpolation;
		
		in += 2;
	}
	
	return(x);
}

int fir_int16_scomplex_init(fir_int16_t *s, const double *taps, unsigned int ntaps, int interpolation, int decimation, int delay)
{
	int i, j;
	
	s->type = 3;
	
	s->interpolation = interpolation;
	s->decimation = decimation;
	
	/* Round number of taps up to a multiple of the interpolation factor */
	s->ntaps = ntaps + (ntaps % interpolation ? interpolation - (ntaps % interpolation) : 0);
	s->ataps = s->ntaps / interpolation;
	
	s->itaps = calloc(s->ntaps, sizeof(int16_t));
	s->qtaps = calloc(s->ntaps, sizeof(int16_t));
	
	/* Copy the taps in the order and format they are to be used */
	j = s->ntaps - s->ataps;
	for(i = ntaps - 1; i >= 0; i--)
	{
		s->itaps[j] = lround(taps[i * 2 + 0] * 32767.0);
		s->qtaps[j] = lround(taps[i * 2 + 1] * 32767.0);
		j -= s->ataps;
		if(j < 0) j += s->ntaps + 1;
	}
	
	s->lwin = s->ataps + delay;
	s->win = calloc(s->ataps * 2 + delay, sizeof(int16_t));
	s->owin = 0;
	s->d = 0;
	
	return(0);
}

size_t fir_int16_scomplex_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples)
{
	int32_t ai, aq;
	int x, y;
	const int16_t *win, *itaps, *qtaps;
	
	for(x = 0; samples; samples--)
	{
		/* Append the next input sample to the round buffer */
		s->win[s->owin] = *in;
		if(s->owin < s->ataps) s->win[s->owin + s->lwin] = *in;
		if(++s->owin == s->lwin) s->owin = 0;
		
		for(; s->d < s->interpolation; s->d += s->decimation)
		{
			win = &s->win[s->owin];
			itaps = &s->itaps[s->d * s->ataps];
			qtaps = &s->qtaps[s->d * s->ataps];
			
			/* Calculate the next output sample */
			for(ai = aq = y = 0; y < s->ataps; y++)
			{
				ai += *win     * *(itaps++);
				aq += *(win++) * *(qtaps++);
			}
			
			ai >>= 15;
			aq >>= 15;
			out[0] = ai < INT16_MIN ? INT16_MIN : (ai > INT16_MAX ? INT16_MAX : ai);
			out[1] = aq < INT16_MIN ? INT16_MIN : (aq > INT16_MAX ? INT16_MAX : aq);
			out += 2;
			x++;
		}
		s->d -= s->interpolation;
		
		in += 2;
	}
	
	return(x);
}



/* int32_t */



int fir_int32_init(fir_int32_t *s, const double *taps, unsigned int ntaps, int interpolation, int decimation, int delay)
{
	int i, j;
	
	s->type = 1;
	
	s->interpolation = interpolation;
	s->decimation = decimation;
	
	/* Round number of taps up to a multiple of the interpolation factor */
	s->ntaps = ntaps + (ntaps % interpolation ? interpolation - (ntaps % interpolation) : 0);
	s->ataps = s->ntaps / interpolation;
	
	s->itaps = malloc(s->ntaps * sizeof(int32_t));
	s->qtaps = NULL;
	
	j = s->ntaps - s->ataps;
	for(i = ntaps - 1; i >= 0; i--)
	{
		s->itaps[j] = lround(taps[i] * 32767.0);
		j -= s->ataps;
		if(j < 0) j += s->ntaps + 1;
	}
	
	s->lwin = s->ataps + delay;
	s->win = calloc(s->ataps * 2 + delay, sizeof(int32_t));
	s->owin = 0;
	s->d = 0;
	
	return(0);
}

size_t fir_int32_process(fir_int32_t *s, int32_t *out, const int32_t *in, size_t samples)
{
	int64_t a;
	int x, y;
	const int32_t *win, *taps;
	
	if(s->type == 0) return(0);
	//else if(s->type == 2) return(fir_int32_complex_process(s, out, in, samples));
	//else if(s->type == 3) return(fir_int32_scomplex_process(s, out, in, samples));
	
	for(x = 0; samples; samples--)
	{
		/* Append the next input sample to the round buffer */
		s->win[s->owin] = *in;
		if(s->owin < s->ataps) s->win[s->owin + s->lwin] = *in;
		if(++s->owin == s->lwin) s->owin = 0;
		
		for(; s->d < s->interpolation; s->d += s->decimation)
		{
			win = &s->win[s->owin];
			taps = &s->itaps[s->d * s->ataps];
			
			/* Calculate the next output sample */
			for(a = y = 0; y < s->ataps; y++)
			{
				a += (int64_t) *(win++) * (int64_t) *(taps++);
			}
			
			a >>= 15;
			*out = a < INT32_MIN ? INT32_MIN : (a > INT32_MAX ? INT32_MAX : a);
			out += 2;
			x++;
		}
		s->d -= s->interpolation;
		
		in += 2;
	}
	
	return(x);
}

void fir_int32_free(fir_int32_t *s)
{
	free(s->win);
	free(s->itaps);
	free(s->qtaps);
	memset(s, 0, sizeof(fir_int32_t));
}



/* IIR filter */



int iir_int16_init(iir_int16_t *s, const double *a, const double *b)
{
	s->a[0] = a[0];
	s->a[1] = a[1];
	s->b[0] = b[0];
	s->b[1] = b[1];
	s->ix = 0;
	s->iy = 0;
	return(0);
}

size_t iir_int16_process(iir_int16_t *s, int16_t *out, const int16_t *in, size_t samples, size_t step)
{
	size_t i;
	
	for(i = 0; i < samples; i++)
	{
		s->iy = (double) *in * s->b[0] + s->ix * s->b[1] - s->iy * s->a[1];
		s->ix = (double) *in;
		*out = lround(s->iy < INT16_MIN ? INT16_MIN : (s->iy > INT16_MAX ? INT16_MAX : s->iy));
		in += step;
		out += step;
	}
	
	return(samples);
}

void iir_int16_free(iir_int16_t *s)
{
	memset(s, 0, sizeof(iir_int16_t));
}



/* Soft Limiter */



void limiter_free(limiter_t *s)
{
	fir_int32_free(&s->vfir);
	fir_int32_free(&s->ffir);
	free(s->shape);
	free(s->att);
	free(s->fix);
	free(s->var);
}

int limiter_init(limiter_t *s, int16_t level, int width, const double *vtaps, const double *ftaps, int ntaps)
{
	int i;
	
	memset(s, 0, sizeof(limiter_t));
	
	if(ntaps > 0)
	{
		if(vtaps)
		{
			i = fir_int32_init(&s->vfir, vtaps, ntaps, 1, 1, 0);
			if(i != 0)
			{
				limiter_free(s);
				return(-1);
			}
		}
		
		if(ftaps)
		{
			i = fir_int32_init(&s->ffir, ftaps, ntaps, 1, 1, 0);
			if(i != 0)
			{
				limiter_free(s);
				return(-1);
			}
		}
	}
	
	/* Generate the limiter response shape */
	s->width = width | 1;
	s->shape = malloc(sizeof(int16_t) * s->width);
	if(!s->shape)
	{
		limiter_free(s);
		return(-1);
	}
	
	for(i = 0; i < s->width; i++)
	{
		s->shape[i] = lround((1.0 - cos(2.0 * M_PI / (s->width + 1) * (i + 1))) * 0.5 * INT16_MAX);
	}
	
	/* Initial state */
	s->level = level;
	s->att = calloc(sizeof(int16_t), s->width);
	s->fix = calloc(sizeof(int32_t), s->width);
	s->var = calloc(sizeof(int32_t), s->width);
	if(!s->att || !s->fix || !s->var)
	{
		limiter_free(s);
		return(-1);
	}
	
	s->p = 0;
	s->h = s->width / 2;
	
	return(0);
}

void limiter_process(limiter_t *s, int16_t *out, const int16_t *vin, const int16_t *fin, int samples, int step)
{
	int i, j;
	int32_t a, b;
	
	for(i = 0; i < samples; i++)
	{
		s->var[s->p] = *vin;
		s->fix[s->p] = (fin ? *fin : 0);
		s->att[s->p] = 0;
		
		/* Apply input filters */
		if(s->vfir.type) fir_int32_process(&s->vfir, &s->var[s->p], &s->var[s->p], 1);
		if(s->ffir.type) fir_int32_process(&s->ffir, &s->fix[s->p], &s->fix[s->p], 1);
		
		/* Hard limit the fixed input */
		if(s->fix[s->p] < -s->level) s->fix[s->p] = -s->level;
		else if(s->fix[s->p] > s->level) s->fix[s->p] = s->level;
		
		/* The variable signal is the difference between vin and fin */
		s->var[s->p] -= s->fix[s->p];
		
		if(++s->p == s->width) s->p = 0;
		if(++s->h == s->width) s->h = 0;
		
		/* Soft limit the variable input */
		a = abs(s->var[s->h] + s->fix[s->h]);
		if(a > s->level)
		{
			a = INT16_MAX - (s->level + abs(s->var[s->h]) - a) * INT16_MAX / abs(s->var[s->h]);
			
			for(j = 0; j < s->width; j++)
			{
				b = (a * s->shape[j]) >> 15;
				if(b > s->att[s->p]) s->att[s->p] = b;
				if(++s->p == s->width) s->p = 0;
			}
		}
		
		a  = s->fix[s->p];
		a += ((int64_t) s->var[s->p] * (INT16_MAX - s->att[s->p])) >> 15;
		
		/* Hard limit to catch rounding errors */
		if(a < -s->level) a = -s->level;
		else if(a > s->level) a = s->level;
		
		*out = a;
		
		vin += step;
		out += step;
		if(fin) fin += step;
	}
}

