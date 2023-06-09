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

#ifndef _FIR_H
#define _FIR_H

typedef struct {
	
	int type;
	
	int interpolation;
	int decimation;
	
	unsigned int ntaps;
	unsigned int ataps;
	int16_t *itaps;
	int16_t *qtaps;
	
	unsigned int owin;
	unsigned int lwin;
	int16_t *win;
	int d;
	
} fir_int16_t;

typedef struct {
	
	int type;
	
	int interpolation;
	int decimation;
	
	unsigned int ntaps;
	unsigned int ataps;
	int32_t *itaps;
	int32_t *qtaps;
	
	unsigned int owin;
	unsigned int lwin;
	int32_t *win;
	int d;
	
} fir_int32_t;

extern void fir_low_pass(double *taps, size_t ntaps, double sample_rate, double cutoff, double width, double gain);
extern void fir_band_reject(double *taps, size_t ntaps, double sample_rate, double low_cutoff, double high_cutoff, double width, double gain);
extern void fir_complex_band_pass(double *taps, size_t ntaps, double sample_rate, double low_cutoff, double high_cutoff, double width, double gain);

extern int fir_int16_init(fir_int16_t *s, const double *taps, unsigned int ntaps, int interpolation, int decimation, int delay);
extern size_t fir_int16_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples, int step);
extern size_t fir_int16_process_block(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples, int step);
extern void fir_int16_free(fir_int16_t *s);

extern int fir_int16_resampler_init(fir_int16_t *s, int interpolation, int decimation);

extern int fir_int16_complex_init(fir_int16_t *s, const double *taps, unsigned int ntaps, int interpolation, int decimation, int delay);
extern size_t fir_int16_complex_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples);

extern int fir_int16_scomplex_init(fir_int16_t *s, const double *taps, unsigned int ntaps, int interpolation, int decimation, int delay);
extern size_t fir_int16_scomplex_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples);

extern int fir_int32_init(fir_int32_t *s, const double *taps, unsigned int ntaps, int interpolation, int decimation, int delay);
extern size_t fir_int32_process(fir_int32_t *s, int32_t *out, const int32_t *in, size_t samples);
extern void fir_int32_free(fir_int32_t *s);

typedef struct {
	double a[2];
	double b[2];
	double ix;
	double iy;
} iir_int16_t;

extern int iir_int16_init(iir_int16_t *s, const double *a, const double *b);
extern size_t iir_int16_process(iir_int16_t *s, int16_t *out, const int16_t *in, size_t samples, size_t step);
extern void iir_int16_free(iir_int16_t *s);

typedef struct {
	
	/* Input fir filters */
	fir_int32_t vfir;
	fir_int32_t ffir;
	
	/* Limiter shape */
	int width;
	int16_t *shape;
	
	/* Limiter state */
	int16_t level;
	int32_t *fix;
	int32_t *var;
	int16_t *att;
	int p;
	int h;
	
} limiter_t;

extern void limiter_free(limiter_t *s);
extern int limiter_init(limiter_t *s, int16_t level, int width, const double *vtaps, const double *ftaps, int ntaps);
extern void limiter_process(limiter_t *s, int16_t *out, const int16_t *vin, const int16_t *fin, int samples, int step);

#endif

