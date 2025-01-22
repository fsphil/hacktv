/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2019 Philip Heron <phil@sanslogic.co.uk>                    */
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <osmo-fl2k.h>
#include <pthread.h>
#include "rf.h"
#include "fifo.h"
#include "fir.h"
#include "spdif.h"

#define BUFFERS 4

typedef struct {
	
	fl2k_dev_t *d;
	unsigned int sample_rate;
	int abort;
	
	fifo_t buffer[3];
	fifo_reader_t reader[3];
	int phase;
	
	int baseband;
	int audio_mode;
	
	/* Analogue audio */
	int interp;
	uint16_t audio[2];
	uint16_t err[2];
	
	/* SPDIF audio */
	int16_t pcm[SPDIF_BLOCK_SAMPLES];
	int pcm_len;
	fir_int16_t spdif_resampler;
	
} fl2k_t;

static void _callback(fl2k_data_info_t *data_info)
{
	fl2k_t *rf = data_info->ctx;
	void *channels[3] = {
		&data_info->r_buf,
		&data_info->g_buf,
		&data_info->b_buf
	};
	int i;
	
	if(data_info->device_error)
	{
		rf->abort = 1;
		return;
	}
	
	for(; rf->phase < 3; rf->phase++)
	{
		i = fifo_read(&rf->reader[rf->phase], channels[rf->phase], FL2K_BUF_LEN, 0);
		if(i == 0)
		{
			if(rf->reader[rf->phase].prefill == NULL) fprintf(stderr, "U");
			break;
		}
	}
	
	if(rf->phase == 3) rf->phase = 0;
	
	data_info->sampletype_signed = 0;
}

static int _rf_write(void *private, const int16_t *iq_data, size_t samples)
{
	fl2k_t *rf = private;
	uint8_t *buf[2];
	int i, r;
	
	if(rf->abort)
	{
		return(RF_ERROR);
	}
	
	r = 0;
	
	while(samples > 0)
	{
		r = fifo_write_ptr(&rf->buffer[0], (void **) &buf[0], 1);
		if(r < 0) break;
		
		if(!rf->baseband)
		{
			i = fifo_write_ptr(&rf->buffer[1], (void **) &buf[1], 1);
			if(i < r) r = i;
			if(r < 0) break;
		}
		
		for(i = 0; i < r && i < samples; i++)
		{
			buf[0][i] = (iq_data[i * 2] - INT16_MIN) >> 8;
		}
		
		fifo_write(&rf->buffer[0], i);
		
		if(!rf->baseband)
		{
			for(i = 0; i < r && i < samples; i++)
			{
				buf[1][i] = (iq_data[i * 2 + 1] - INT16_MIN) >> 8;
			}
			
			fifo_write(&rf->buffer[1], i);
		}
		
		iq_data += i * 2;
		samples -= i;
	}
	
	return(r >= 0 ? RF_OK : RF_ERROR);
}

static int _rf_write_audio(void *private, const int16_t *audio, size_t samples)
{
	fl2k_t *rf = private;
	uint8_t *buf[2];
	int i, r, c;
	
	if(audio == NULL) return(RF_OK);
	
	r = 0;
	samples /= 2;
	
	while(samples > 0)
	{
		r = fifo_write_ptr(&rf->buffer[1], (void **) &buf[0], 1);
		if(r < 0) break;
		
		i = fifo_write_ptr(&rf->buffer[2], (void **) &buf[1], 1);
		if(i < 0) break;
		
		if(i < r) r = i;
		
		for(i = 0; i < r && samples > 0; i++)
		{
			rf->interp += 32000; /* TODO: Don't hardwire this */
			if(rf->interp >= rf->sample_rate)
			{
				rf->interp -= rf->sample_rate;
				rf->audio[0] = audio[0] - INT16_MIN;
				rf->audio[1] = audio[1] - INT16_MIN;
				samples--;
				audio += 2;
			}
			
			for(c = 0; c < 2; c++)
			{
				/* Apply a delta-sigma modulation / dither using the lost
				 * lower 8-bits of the 16-bit audio samples. Use a low pass
				 * filter on the output to reconstruct the full signal */
				
				buf[c][i] = (rf->audio[c] & 0xFE00) >> 8;
				rf->err[c] += rf->audio[c] & 0x1FF;
				if(rf->err[c] >= 0x1FF)
				{
					buf[c][i]++;
					rf->err[c] -= 0x1FF;
				}
			}
		}
		
		fifo_write(&rf->buffer[1], i);
		fifo_write(&rf->buffer[2], i);
	}
	
	return(r >= 0 ? RF_OK : RF_ERROR);
}

static int _rf_write_audio_spdif(void *private, const int16_t *audio, size_t samples)
{
	fl2k_t *rf = private;
	uint8_t *buf;
	uint8_t block[SPDIF_BLOCK_BYTES];
	int16_t block_s[SPDIF_BLOCK_BITS * 5];
	int r, i;
	int16_t s;
	
	if(audio == NULL) return(RF_OK);
	
	r = 0;
	
	while(samples > 0)
	{
		/* Copy audio PCM samples */
		r = SPDIF_BLOCK_SAMPLES - rf->pcm_len;
		if(r > samples) r = samples;
		
		memcpy(&rf->pcm[rf->pcm_len], audio, r * sizeof(int16_t));
		audio += r;
		rf->pcm_len += r;
		samples -= r;
		
		/* Incomplete PCM block, return for more */
		if(rf->pcm_len < SPDIF_BLOCK_SAMPLES) return(RF_OK);
		
		/* Encode the SPDIF block (32000 kHz, 4096000 bit/s) */
		spdif_block(block, rf->pcm);
		rf->pcm_len = 0;
		
		for(r = 0; r < SPDIF_BLOCK_BITS * 5; r++)
		{
			i = r / 5;
			block_s[r] = (block[i >> 3] >> (7 - (i & 7))) & 1 ? 23405 : -23405;
		}
		
		fir_int16_feed(&rf->spdif_resampler, block_s, r, 1);
		
		/* Feed the output of the resampler into the FIFO */
		do
		{
			r = fifo_write_ptr(&rf->buffer[2], (void **) &buf, 1);
			if(r < 0) break;
			
			r = fir_int16_process(&rf->spdif_resampler, &s, 1, 1);
			if(r <= 0) break;
			
			buf[0] = (s - INT16_MIN) >> 8;
			
			fifo_write(&rf->buffer[2], 1);
		}
		while(r > 0);
	}
	
	return(r >= 0 ? RF_OK : RF_ERROR);
}

static int _rf_close(void *private)
{
	fl2k_t *rf = private;
	int i;
	
	rf->abort = 1;
	
	fl2k_stop_tx(rf->d);
	fl2k_close(rf->d);
	
	for(i = 0; i < 3; i++)
	{
		fifo_reader_close(&rf->reader[i]);
	}
	
	for(i = 0; i < 3; i++)
	{
		fifo_free(&rf->buffer[i]);
	}
	
	if(rf->audio_mode == FL2K_AUDIO_SPDIF)
	{
		fir_int16_free(&rf->spdif_resampler);
	}
	
	free(rf);
	
	return(RF_OK);
}

int rf_fl2k_open(rf_t *s, const char *device, unsigned int sample_rate, int baseband, int audio_mode)
{
	fl2k_t *rf;
	int r;
	
	rf = calloc(1, sizeof(fl2k_t));
	if(!rf)
	{
		return(RF_OUT_OF_MEMORY);
	}
	
	rf->abort = 0;
	rf->sample_rate = sample_rate;
	rf->baseband = baseband ? 1 : 0;
	rf->audio_mode = audio_mode;
	
	r = device ? atoi(device) : 0;
	
	fl2k_open(&rf->d, r);
	if(rf->d == NULL)
	{
		fprintf(stderr, "fl2k_open() failed to open device #%d.\n", r);
		_rf_close(rf);
		return(RF_ERROR);
	}
	
	/* Red channel is composite video / in-phase complex component */
	fifo_init(&rf->buffer[0], BUFFERS, FL2K_BUF_LEN);
	fifo_reader_init(&rf->reader[0], &rf->buffer[0], -1);
	
	if(!rf->baseband)
	{
		/* Green channel is chrominance / quadrature complex component */
		fifo_init(&rf->buffer[1], BUFFERS, FL2K_BUF_LEN);
		fifo_reader_init(&rf->reader[1], &rf->buffer[1], 0);
	}
	
	if(audio_mode == FL2K_AUDIO_STEREO)
	{
		if(!rf->baseband)
		{
			fprintf(stderr, "fl2k: Stereo audio is not available with S-Video or complex modes\n");
			_rf_close(rf);
			return(RF_ERROR);
		}
		
		rf->interp = 0;
		
		/* Green channel is left audio */
		fifo_init(&rf->buffer[1], BUFFERS, FL2K_BUF_LEN);
		fifo_reader_init(&rf->reader[1], &rf->buffer[1], 0);
		
		/* Blue channel is right audio */
		fifo_init(&rf->buffer[2], BUFFERS, FL2K_BUF_LEN);
		fifo_reader_init(&rf->reader[2], &rf->buffer[2], 0);
		
		/* Register the callback */
		s->write_audio = _rf_write_audio;
	}
	else if(audio_mode == FL2K_AUDIO_SPDIF)
	{
		/* SPDIF init */
		rf->pcm_len = 0;
		r = fir_int16_resampler_init(
			&rf->spdif_resampler,
			(r64_t) { rf->sample_rate, 1 },
			(r64_t) { spdif_bitrate(32000) * 5, 1 }
		);
		
		/* Blue channel is S/PDIF digital audio */
		fifo_init(&rf->buffer[2], BUFFERS, FL2K_BUF_LEN);
		fifo_reader_init(&rf->reader[2], &rf->buffer[2], 0);
		
		/* Register the callback */
		s->write_audio = _rf_write_audio_spdif;
	}
	
	rf->phase = 0;
	
	r = fl2k_start_tx(rf->d, _callback, rf, 0);
	if(r < 0)
	{
		fprintf(stderr, "fl2k_start_tx() failed: %d\n", r);
		_rf_close(rf);
		return(RF_ERROR);
	}
	
	r = fl2k_set_sample_rate(rf->d, rf->sample_rate);
	if(r < 0)
	{
		fprintf(stderr, "fl2k_set_sample_rate() failed: %d\n", r);
		_rf_close(rf);
		return(RF_ERROR);
	}
	
	/* Read back the actual frequency */
	r = fl2k_get_sample_rate(rf->d);
	if(r != rf->sample_rate)
	{
		//fprintf(stderr, "fl2k sample rate changed from %d > %d\n", s->vid.sample_rate, r);
		//_rf_close(rf);
		//return(RF_ERROR);
	}
	
	/* Register the callback functions */
	s->ctx = rf;
	s->write = _rf_write;
	s->close = _rf_close;
	
	return(RF_OK);
}

