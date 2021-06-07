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
	
	unsigned int ntaps;
	int16_t *itaps;
	int16_t *qtaps;
	
	unsigned int owin;
	int16_t *win;
	
} fir_int16_t;

typedef struct {
	
	int type;
	
	unsigned int ntaps;
	int32_t *itaps;
	int32_t *qtaps;
	
	unsigned int owin;
	int32_t *win;
	
} fir_int32_t;

extern void fir_low_pass(double *taps, size_t ntaps, double sample_rate, double cutoff, double width, double gain);
extern void fir_complex_band_pass(double *taps, size_t ntaps, double sample_rate, double low_cutoff, double high_cutoff, double width, double gain);

extern void fir_int16_low_pass(int16_t *taps, size_t ntaps, double sample_rate, double cutoff, double width, double gain);
extern void fir_int16_complex_band_pass(int16_t *taps, size_t ntaps, double sample_rate, double low_cutoff, double high_cutoff, double width, double gain);

extern int fir_int16_init(fir_int16_t *s, const int16_t *taps, unsigned int ntaps);
extern size_t fir_int16_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples);
extern void fir_int16_free(fir_int16_t *s);

extern int fir_int16_complex_init(fir_int16_t *s, const int16_t *taps, unsigned int ntaps);
extern size_t fir_int16_complex_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples);

extern int fir_int16_scomplex_init(fir_int16_t *s, const int16_t *taps, unsigned int ntaps);
extern size_t fir_int16_scomplex_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples);

extern int fir_int32_init(fir_int32_t *s, const int32_t *taps, unsigned int ntaps);
extern size_t fir_int32_process(fir_int32_t *s, int32_t *out, const int32_t *in, size_t samples);
extern void fir_int32_free(fir_int32_t *s);

extern void iir_fm_preemphasis(double *dtaps, const double *taps, size_t ntaps, double fs, double tau, double fh);

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
extern int limiter_init(limiter_t *s, int16_t level, int width, const int32_t *vtaps, const int32_t *ftaps, int ntaps);
extern void limiter_process(limiter_t *s, int16_t *out, const int16_t *vin, const int16_t *fin, int samples, int step);

#endif

