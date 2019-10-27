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

typedef struct {
	
	unsigned int ntaps;
	const int16_t *taps;
	
	unsigned int interpolation;
	
	unsigned int decimation;
	unsigned int ds;
	
	unsigned int lwin;
	unsigned int owin;
	int16_t *win;
	
} fir_int16_t;

extern void fir_low_pass(double *taps, size_t ntaps, double sample_rate, double cutoff, double width, double gain);
extern void fir_complex_band_pass(double *taps, size_t ntaps, double sample_rate, double low_cutoff, double high_cutoff, double width, double gain);

extern void fir_int16_low_pass(int16_t *taps, size_t ntaps, double sample_rate, double cutoff, double width, double gain);
extern void fir_int16_complex_band_pass(int16_t *taps, size_t ntaps, double sample_rate, double low_cutoff, double high_cutoff, double width, double gain);

extern int fir_int16_init(fir_int16_t *s, const int16_t *taps, unsigned int ntaps, unsigned int interpolation, unsigned int decimation);
extern size_t fir_int16_process(fir_int16_t *s, int16_t *output, size_t ostep, const int16_t *input, size_t samples, size_t istep);
extern size_t fir_int16_process_simple(fir_int16_t *s, int16_t *signal, size_t samples);
extern void fir_int16_free(fir_int16_t *s);

extern int fir_int16_complex_init(fir_int16_t *s, const int16_t *taps, unsigned int ntaps, unsigned int interpolation, unsigned int decimation);
extern size_t fir_int16_complex_process(fir_int16_t *s, int16_t *output, size_t ostep, const int16_t *input, size_t samples, size_t istep);
extern void fir_int16_complex_free(fir_int16_t *s);

