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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libhackrf/hackrf.h>
#include <pthread.h>
#include <unistd.h>
#include "hacktv.h"

typedef enum {
	BUFFER_EMPTY,
	BUFFER_PREFILL,
	BUFFER_FILL,
	BUFFER_READY,
} buffer_status_t;

typedef struct {
	
	/* Buffers are locked while reading/writing */
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	buffer_status_t status;
	
	/* Pointer to the start of the buffer */
	int8_t *data;
	
	/* Offset to start of data */
	size_t start;
	
	/* Length of data ready */
	size_t length;
	
} buffer_t;

typedef struct {
	
	buffer_t *buffers;
	
	int length;
	int count;
	int prefill;
	int in;
	int out;
	
} buffers_t;

typedef struct {
	
	/* HackRF device */
	hackrf_device *d;
	
	/* Buffers */
	buffers_t buffers;
	
} hackrf_t;

static int _buffer_init(buffers_t *buffers, size_t count, size_t length)
{
	int i;
	
	buffers->count = count;
	buffers->length = length;
	buffers->buffers = calloc(count, sizeof(buffer_t));
	
	for(i = 0; i < count; i++)
	{
		pthread_mutex_init(&buffers->buffers[i].mutex, NULL);
		pthread_cond_init(&buffers->buffers[i].cond, NULL);
		buffers->buffers[i].data = malloc(length);
		buffers->buffers[i].start = buffers->length;
		buffers->buffers[i].length = buffers->length;
		buffers->buffers[i].status = BUFFER_EMPTY;
	}
	
	buffers->prefill = 1;
	buffers->in = 0;
	buffers->out = 0;
	
	return(0);
}

static int _buffer_free(buffers_t *buffers)
{
	int i;
	
	for(i = 0; i < buffers->count; i++)
	{
		free(buffers->buffers[i].data);
		pthread_cond_destroy(&buffers->buffers[i].cond);
		pthread_mutex_destroy(&buffers->buffers[i].mutex);
	}
	
	free(buffers->buffers);
	memset(buffers, 0, sizeof(buffers_t));
	
	return(0);
}

static int _buffer_read(buffers_t *buffers, int8_t *dst, size_t length)
{
	buffer_t *buf = &buffers->buffers[buffers->out];
	
	if(buf->start == buffers->length)
	{
		buffer_status_t r;
		
		/* Check if we can read this block */
		pthread_mutex_lock(&buf->mutex);
		r = buf->status;
		pthread_mutex_unlock(&buf->mutex);
		
		if(r != BUFFER_READY)
		{
			/* This buffer is not ready - display warning if not in prefill stage */
			if(r != BUFFER_PREFILL)
			{
				fprintf(stderr, "U");
			}
			
			return(0);
		}
		
		buf->start = 0;
	}
	
	if(length > buffers->length - buf->start)
	{
		length = buffers->length - buf->start;
	}
	
	memcpy(dst, buf->data + buf->start, length);
	buf->start += length;
	
	if(buf->start == buffers->length)
	{
		/* Flag the current block as avaliable for writing */
		pthread_mutex_lock(&buf->mutex);
		buf->status = BUFFER_EMPTY;
		pthread_mutex_unlock(&buf->mutex);
		pthread_cond_broadcast(&buf->cond);
		
		buffers->out = (buffers->out + 1) % buffers->count;
	}
	
	return(length);
}

static size_t _buffer_write_ptr(buffers_t *buffers, int8_t **src)
{
	buffer_t *buf = &buffers->buffers[buffers->in];
	
	if(buf->length == buffers->length)
	{
		pthread_mutex_lock(&buf->mutex);
		
		if(buf->status == BUFFER_PREFILL)
		{
			buffers->prefill = 0;
			buf->status = BUFFER_READY;
		}
		
		while(buf->status != BUFFER_EMPTY)
		{
			pthread_cond_wait(&buf->cond, &buf->mutex);
		}
		
		pthread_mutex_unlock(&buf->mutex);
		
		buf->length = 0;
	}
	
	*src = buf->data + buf->length;
	
	return(buffers->length - buf->length);
}

static int _buffer_write(buffers_t *buffers, size_t length)
{
	buffer_t *buf = &buffers->buffers[buffers->in];
	
	buf->length += length;
	
	if(buf->length == buffers->length)
	{
		pthread_mutex_lock(&buf->mutex);
		buf->status = (buffers->prefill ? BUFFER_PREFILL : BUFFER_READY);
		pthread_mutex_unlock(&buf->mutex);
		
		buffers->in = (buffers->in + 1) % buffers->count;
	}
	
	return(length);
}

static int _tx_callback(hackrf_transfer *transfer)
{
	hackrf_t *rf = transfer->tx_ctx;
	size_t l = transfer->valid_length;
	uint8_t *buf = transfer->buffer;
	int r;
	
	while(l)
	{
		r = _buffer_read(&rf->buffers, (int8_t *) buf, l);
		
		if(r == 0)
		{
			/* Buffer underrun, fill with zero */
			memset(buf, 0, l);
			l = 0;
		}
		else
		{
			l -= r;
			buf += r;
		}
	}
	
	return(0);
}

static int _rf_write(void *private, int16_t *iq_data, size_t samples)
{
	hackrf_t *rf = private;
	int8_t *iq8 = NULL;
	int i, r;
	
	samples *= 2;
	
	while(samples > 0)
	{
		r = _buffer_write_ptr(&rf->buffers, &iq8);
		
		for(i = 0; i < r && i < samples; i++)
		{
			iq8[i] = iq_data[i] >> 8;
		}
		
		_buffer_write(&rf->buffers, i);
		
		iq_data += i;
		samples -= i;
	}
	
	return(HACKTV_OK);
}

static int _rf_close(void *private)
{
	hackrf_t *rf = private;
	int r;
	
	r = hackrf_stop_tx(rf->d);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_stop_tx() failed: %s (%d)\n", hackrf_error_name(r), r);
		return(HACKTV_ERROR);
	}
	
	/* Wait until streaming has stopped */
	while(hackrf_is_streaming(rf->d) == HACKRF_TRUE)
	{
		usleep(100);
	}
	
	r = hackrf_close(rf->d);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_close() failed: %s (%d)\n", hackrf_error_name(r), r);
	}
	
	hackrf_exit();
	
	_buffer_free(&rf->buffers);
	free(rf);
	
	return(HACKTV_OK);
}

int rf_hackrf_open(hacktv_t *s, const char *serial, uint64_t frequency_hz, unsigned int txvga_gain, unsigned char amp_enable)
{
	hackrf_t *rf;
	int r;
	
	if(s->vid.conf.output_type != HACKTV_INT16_COMPLEX)
	{
		fprintf(stderr, "rf_hackrf_open(): Unsupported mode output type for this device.\n");
		return(HACKTV_ERROR);
	}
	
	rf = calloc(1, sizeof(hackrf_t));
	if(!rf)
	{
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	/* Prepare the HackRF for output */
	r = hackrf_init();
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_init() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(HACKTV_ERROR);
	}
	
	r = hackrf_open_by_serial(serial, &rf->d);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_open() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(HACKTV_ERROR);
	}
	
	r = hackrf_set_sample_rate_manual(rf->d, s->vid.sample_rate, 1);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_sample_rate_set() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(HACKTV_ERROR);
	}
	
	r = hackrf_set_baseband_filter_bandwidth(rf->d, hackrf_compute_baseband_filter_bw(s->vid.sample_rate));
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_baseband_filter_bandwidth_set() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(HACKTV_ERROR);
	}
	
	r = hackrf_set_freq(rf->d, frequency_hz);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_set_freq() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(HACKTV_ERROR);
	}
	
	r = hackrf_set_txvga_gain(rf->d, txvga_gain);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_set_txvga_gain() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(HACKTV_ERROR);
	}
	
	r = hackrf_set_amp_enable(rf->d, amp_enable);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_set_amp_enable() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(HACKTV_ERROR);
	}
	
	/* Allocate memory for the output buffers, enough for at least 400ms - minimum 4 */
	r = s->vid.sample_rate * 2 * 4 / 10 / hackrf_get_transfer_buffer_size(rf->d);
	if(r < 4) r = 4;
	_buffer_init(&rf->buffers, r, hackrf_get_transfer_buffer_size(rf->d));
	
	/* Begin transmitting */
	r = hackrf_start_tx(rf->d, _tx_callback, rf);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_start_tx() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(HACKTV_ERROR);
	}
	
	/* Register the callback functions */
	s->rf_private = rf;
	s->rf_write = _rf_write;
	s->rf_close = _rf_close;
	
	return(HACKTV_OK);
}

