#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hacktv.h"
#include "test.h"
#include "ffmpeg.h"

int chans_init(chans_t *c, unsigned int sample_rate, const vid_config_t * const conf)
{
	av_ffmpeg_init();

	memset(c, 0, sizeof(chans_t));
	memcpy(&c->conf, conf, sizeof(vid_config_t));

	/* Force filtering on */
	c->conf.vfilter = 1;

	c->sample_rate = sample_rate;
	c->width = round((double) sample_rate / ((double) c->conf.frame_rate_num / c->conf.frame_rate_den) / c->conf.lines);

	c->outline32 = malloc(c->width * 2 * sizeof(int32_t));
	if(c->outline32 == NULL)
	{
		return (VID_OUT_OF_MEMORY);
	}

	c->outline = malloc(c->width * 2 * sizeof(int16_t));
	if(c->outline == NULL)
	{
		free(c->outline32);
		return (VID_OUT_OF_MEMORY);
	}

	return (VID_OK);
}

static void _chan_free_lines(_channel_t *chan)
{
	for(int i = 0; i < CHANNEL_OUTPUT_BUFFER_LINES; i++)
	{
		free(chan->lines[i]);
	}
}

static void _chan_free(_channel_t *chan)
{
	pthread_mutex_lock(&chan->mutex);
	chan->thread_abort = 1;
	pthread_cond_signal(&chan->free_cond);
	pthread_mutex_unlock(&chan->mutex);

	pthread_join(chan->process_thread, NULL);

	pthread_mutex_destroy(&chan->mutex);
	pthread_cond_destroy(&chan->ready_cond);
	pthread_cond_destroy(&chan->free_cond);

	vid_av_close(&chan->vid);
	vid_free(&chan->vid);
	_chan_free_lines(chan);
	free(chan);
}

void chans_free(chans_t *c)
{
	_channel_t *chan = c->chans;

	if(chan == NULL)
	{
		av_ffmpeg_deinit();

		return;
	}

	chan = chan->next;
	_chan_free(chan);

	c->chans = chan;
	chans_free(c);
}

static void *_chan_process(void *ch)
{
	_channel_t *chan = ch;

	pthread_mutex_lock(&chan->mutex);
	while(chan->thread_abort == 0)
	{
		while(chan->thread_abort == 0 && chan->lines_filled == CHANNEL_OUTPUT_BUFFER_LINES)
		{
			pthread_cond_wait(&chan->free_cond, &chan->mutex);
		}

		if(chan->thread_abort != 0)
		{
			break;
		}

		int32_t *outline = chan->lines[chan->lines_filled];

		pthread_mutex_unlock(&chan->mutex);

		/* Ready to process */

		size_t samples;
		int16_t *data = vid_next_line(&chan->vid, &samples);
		if(data == NULL)
		{
			return NULL;
		}

		for(int i = 0; i < samples; i++)
		{
			cint32_mul(&chan->offset_phase, &chan->offset_phase, &chan->offset_delta);

			cint32_t data_sample;
			data_sample.i = data[i * 2];
			data_sample.q = data[i * 2 + 1];

			cint32_mul(&data_sample, &data_sample, &chan->offset_phase);

			outline[i * 2] = data_sample.i;
			outline[i * 2 + 1] = data_sample.q;

			/* Correct the amplitude after INT16_MAX samples */
			if(--chan->offset_counter == 0)
			{
				double ra = atan2(chan->offset_phase.q, chan->offset_phase.i);

				chan->offset_phase.i = lround(cos(ra) * INT32_MAX);
				chan->offset_phase.q = lround(sin(ra) * INT32_MAX);

				chan->offset_counter = INT16_MAX;
			}
		}

		pthread_mutex_lock(&chan->mutex);

		chan->lines_filled++;
		pthread_cond_signal(&chan->ready_cond);
	}

	pthread_mutex_unlock(&chan->mutex);

	return NULL;
}

static int _chan_init(chans_t *c, _channel_t **channel, int offset_freq)
{
	int r;
	_channel_t *chan = malloc(sizeof(_channel_t));

	if(chan == NULL)
	{
		return (VID_OUT_OF_MEMORY);
	}

	memset(chan, 0, sizeof(_channel_t));

	for(int i = 0; i < CHANNEL_OUTPUT_BUFFER_LINES; i++)
	{
		chan->lines[i] = malloc(c->width * 2 * sizeof(uint32_t));
		if(chan->lines[i] == NULL)
		{
			_chan_free_lines(chan);
			free(chan);
			return (VID_OUT_OF_MEMORY);
		}
	}

	chan->offset_counter = INT16_MAX;

	chan->offset_phase.i = INT16_MAX;
	chan->offset_phase.q = 0;

	double phase_delta = 2.0 * M_PI / c->sample_rate * offset_freq;
	chan->offset_delta.i = lround(cos(phase_delta) * INT32_MAX);
	chan->offset_delta.q = lround(sin(phase_delta) * INT32_MAX);

	r = vid_init(&chan->vid, c->sample_rate, &c->conf);
	if (r != VID_OK)
	{
		_chan_free_lines(chan);
		free(chan);
		return r;
	}

	pthread_mutex_init(&chan->mutex, NULL);
	pthread_cond_init(&chan->ready_cond, NULL);
	pthread_cond_init(&chan->free_cond, NULL);

	r = pthread_create(&chan->process_thread, NULL, _chan_process, (void *) chan);
	if(r != 0)
	{
		fprintf(stderr, "Error starting channel thread.\n");
		pthread_mutex_destroy(&chan->mutex);
		pthread_cond_destroy(&chan->ready_cond);
		pthread_cond_destroy(&chan->free_cond);
		vid_free(&chan->vid);
		_chan_free_lines(chan);
		free(chan);
		return(HACKTV_ERROR);
	}

	vid_info(&chan->vid);

	*channel = chan;
	return (VID_OK);
}

int chans_test_add(chans_t *c, int offset_freq)
{
	int r;
	_channel_t *chan;
	r = _chan_init(c, &chan, offset_freq);
	if (r != VID_OK)
	{
		return r;
	}

	r = av_test_open(&chan->vid);
	if (r != VID_OK)
	{
		_chan_free(chan);
		return r;
	}

	chan->next = c->chans;
	c->chans = chan;

	return(VID_OK);
}

int chans_ffmpeg_add(chans_t *c, int offset_freq, char *input_url)
{
	int r;
	_channel_t *chan;
	r = _chan_init(c, &chan, offset_freq);
	if (r != VID_OK)
	{
		return r;
	}

	r = av_ffmpeg_open(&chan->vid, input_url);
	if (r != VID_OK)
	{
		_chan_free(chan);
		return r;
	}

	chan->next = c->chans;
	c->chans = chan;

	return(VID_OK);
}

static int _chan_add_next_line(chans_t *c, _channel_t *chan)
{
	pthread_mutex_lock(&chan->mutex);

	while(chan->thread_abort == 0 && chan->lines_filled == 0)
	{
		pthread_cond_wait(&chan->ready_cond, &chan->mutex);
	}

	if(chan->thread_abort)
	{
		pthread_mutex_unlock(&chan->mutex);
		return VID_ERROR;
	}

	/* We know there is a line, so safe to unlock again */
	pthread_mutex_unlock(&chan->mutex);

	if(chan->thread_abort)
	{
		return VID_ERROR;
	}

	int32_t *out = chan->lines[0];
	for(int i = 0; i < c->width * 2; i++)
	{
		c->outline32[i] += out[i];
	}

	/* Get lock to rotate lines */
	pthread_mutex_lock(&chan->mutex);

	int original_lines_filled = chan->lines_filled;
	chan->lines_filled--;

	/* Rotate filled output lines */
	memmove(chan->lines, chan->lines + 1, (CHANNEL_OUTPUT_BUFFER_LINES - 1) * sizeof(chan->lines[0]));
	chan->lines[CHANNEL_OUTPUT_BUFFER_LINES - 1] = out;

	if(original_lines_filled == CHANNEL_OUTPUT_BUFFER_LINES - 1)
	{
		pthread_cond_signal(&chan->free_cond);
	}

	pthread_mutex_unlock(&chan->mutex);

	return VID_OK;
}

int16_t *chans_next_line(chans_t *c, size_t *samples)
{
	memset(c->outline32, 0, c->width * 2 * sizeof(int32_t));

	int num_channels = 0;
	int r;

	_channel_t *chan = c->chans;
	while(chan)
	{
		num_channels++;

		r = _chan_add_next_line(c, chan);
		if(r != VID_OK)
		{
			return NULL;
		}

		chan = chan->next;
	}

	if(num_channels == 0)
	{
		return NULL;
	}

	for(int i = 0; i < c->width * 2; i++)
	{
		c->outline[i] = c->outline32[i] / num_channels;
	}

	*samples = c->width;

	return c->outline;
}
