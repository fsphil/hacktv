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
#include <osmo-fl2k.h>
#include <pthread.h>
#include "rf.h"

#define BUFFERS 4

typedef struct {
	
	fl2k_dev_t *d;
	int abort;
	
	uint8_t buffer_r[BUFFERS][FL2K_BUF_LEN];
	uint8_t buffer_g[BUFFERS][FL2K_BUF_LEN];
	pthread_mutex_t mutex[BUFFERS];
	int len;
	int in;
	int out;
	
} fl2k_t;

static void _callback(fl2k_data_info_t *data_info)
{
	fl2k_t *rf = data_info->ctx;
	int i;
	
	if(data_info->device_error)
	{
		rf->abort = 1;
		return;
	}
	
	/* Try to get a lock on the next output buffer */
	i = (rf->out + 1) % BUFFERS;
	
	if(pthread_mutex_trylock(&rf->mutex[i]) != 0)
	{
		/* No luck, the writer must have it */
		fprintf(stderr, "U");
		return;
	}
	
	/* Got a lock on the next buffer, clear and release the previous */
	pthread_mutex_unlock(&rf->mutex[rf->out]);
	
	rf->out = i;
	
	data_info->sampletype_signed = 0;
	data_info->r_buf = (char *) rf->buffer_r[rf->out];
	data_info->g_buf = (char *) rf->buffer_g[rf->out];
	data_info->b_buf = NULL;
}

static int _rf_write(void *private, int16_t *iq_data, size_t samples)
{
	fl2k_t *rf = private;
	int i;
	
	if(rf->abort)
	{
		return(RF_ERROR);
	}
	
	while(samples > 0)
	{
		for(; rf->len < FL2K_BUF_LEN && samples > 0; rf->len++, samples--)
		{
			rf->buffer_r[rf->in][rf->len] = 128 + (*(iq_data++) / 256);
			rf->buffer_g[rf->in][rf->len] = 128 + (*(iq_data++) / 256);
		}
		
		if(rf->len == FL2K_BUF_LEN)
		{
			/* This buffer is full. Move on to the next. */
			i = (rf->in + 1) % BUFFERS;
			pthread_mutex_lock(&rf->mutex[i]);
			pthread_mutex_unlock(&rf->mutex[rf->in]);
			rf->in = i;
			rf->len = 0;
		}
	}
	
	return(RF_OK);
}

static int _rf_close(void *private)
{
	fl2k_t *rf = private;
	int r;
	
	rf->abort = 1;
	
	fl2k_stop_tx(rf->d);
	fl2k_close(rf->d);
	
	for(r = 0; r < BUFFERS; r++)
	{
		pthread_mutex_destroy(&rf->mutex[r]);
	}
	
	free(rf);
	
	return(RF_OK);
}

int rf_fl2k_open(rf_t *s, const char *device, unsigned int sample_rate)
{
	fl2k_t *rf;
	int r;
	
	rf = calloc(1, sizeof(fl2k_t));
	if(!rf)
	{
		return(RF_OUT_OF_MEMORY);
	}
	
	rf->abort = 0;
	
	r = device ? atoi(device) : 0;
	
	fl2k_open(&rf->d, r);
	if(rf->d == NULL)
	{
		fprintf(stderr, "fl2k_open() failed to open device #%d.\n", r);
		_rf_close(rf);
		return(RF_ERROR);
	}
	
	for(r = 0; r < BUFFERS; r++)
	{
		pthread_mutex_init(&rf->mutex[r], NULL);
	}
	
	/* Lock the initial buffer for the provider */
	rf->in = 0;
	pthread_mutex_lock(&rf->mutex[rf->in]);
	
	/* Lock the last empty buffer for the consumer */
	rf->out = BUFFERS - 1;
	pthread_mutex_lock(&rf->mutex[rf->out]);
	
	rf->len = 0;
	
	r = fl2k_start_tx(rf->d, _callback, rf, 0);
	if(r < 0)
	{
		fprintf(stderr, "fl2k_start_tx() failed: %d\n", r);
		_rf_close(rf);
		return(RF_ERROR);
	}
	
	r = fl2k_set_sample_rate(rf->d, sample_rate);
	if(r < 0)
	{
		fprintf(stderr, "fl2k_set_sample_rate() failed: %d\n", r);
		_rf_close(rf);
		return(RF_ERROR);
	}
	
	/* Read back the actual frequency */
	r = fl2k_get_sample_rate(rf->d);
	if(r != sample_rate)
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

