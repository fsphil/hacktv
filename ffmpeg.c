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

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include "hacktv.h"

/* nothing to see here */

typedef struct {
	
	AVFormatContext *format_ctx;
	AVFrame *frame;
	
	/* Packet queue */
	AVPacketList *packets;
	AVPacketList *packets_last;
	
	/* Video decoder */
	AVStream *video_stream;
	AVCodecContext *video_codec_ctx;
	struct SwsContext *sws_ctx;
	AVFrame *frame_rgb;
	
	/* Audio decoder */
	AVStream *audio_stream;
	AVCodecContext *audio_codec_ctx;
	struct SwrContext *swr_ctx;
	uint8_t **frame_s16;
	int frame_s16_samples;
	int frame_s16_linesize;
	
} av_ffmpeg_t;

static int _read_frame(av_ffmpeg_t *av, int index, AVPacket *pkt)
{
	AVPacketList *l, *p;
	int r;
	
	/* Scan queued packets for a match */
	for(l = NULL, p = av->packets; p != NULL; l = p, p = p->next)
	{
		if(p->pkt.stream_index == index)
		{
			/* Found a matching packet in the queue,
			 * return it to the user */
			av_init_packet(pkt);
			av_packet_ref(pkt, &p->pkt);
			av_packet_unref(&p->pkt);
			
			if(l == NULL)
			{
				/* Removing the first queue item */
				av->packets = p->next;
				
				/* Was it the only queue item? */
				if(av->packets == NULL)
				{
					av->packets_last = NULL;
				}
			}
			else
			{
				l->next = p->next;
				
				if(l->next == NULL)
				{
					av->packets_last = l;
				}
			}
			
			av_free(p);
			
			return(0);
		}
	}
	
	/* Nothing in the queue for this index, scan the source */
	while((r = av_read_frame(av->format_ctx, pkt)) >= 0)
	{
		/* Test for a match */
		if(pkt->stream_index == index) break;
		
		/* Not a match -- queue the packet if it's required later */
		if(pkt->stream_index == av->video_stream->index ||
		   pkt->stream_index == av->audio_stream->index)
		{
			/* Allocate memory for queue item and copy packet */
			p = av_malloc(sizeof(AVPacketList));
			
			av_copy_packet(&p->pkt, pkt);
			p->next = NULL;
			
			/* Add the item to the end of the queue */
			if(av->packets == NULL)
			{
				av->packets = p;
				av->packets_last = p;
			}
			else
			{
				av->packets_last->next = p;
				av->packets_last = p;
			}
		}
		
		av_packet_unref(pkt);
	}
	
	/* A match was found or av_read_frame() failed */
	
	return(r);
}

static uint32_t *_av_ffmpeg_read_video(void *private)
{
	av_ffmpeg_t *av = private;
	AVPacket packet;
	uint32_t *frame = NULL;
	int i;
	
	while(_read_frame(av, av->video_stream->index, &packet) >= 0)
	{
		/* Decode video frame */
		avcodec_decode_video2(av->video_codec_ctx, av->frame, &i, &packet);
		
		if(i)
		{
			/* We got a complete frame. Convert it
			 * from its native format to RGB32 */
			sws_scale(
				av->sws_ctx,
				(uint8_t const * const *) av->frame->data,
				av->frame->linesize,
				0,
				av->video_codec_ctx->height,
				av->frame_rgb->data,
				av->frame_rgb->linesize
			);
			
			frame = (uint32_t *) av->frame_rgb->data[0];
		}
		
		av_packet_unref(&packet);
		
		/* Break out if we got a frame */
		if(frame)
		{
			return(frame);
		}
	}
	
	/* No more packets or an error occured */
	return(NULL);
}

static int16_t *_av_ffmpeg_read_audio(void *private, size_t *samples)
{
	av_ffmpeg_t *av = private;
	AVPacket packet;
	int16_t *frame = NULL;
	int i;
	
	if(av->audio_stream == NULL)
	{
		*samples = 0;
		return(NULL);
	}
	
	while(_read_frame(av, av->audio_stream->index, &packet) >= 0)
	{
		/* Decode audio frame */
		avcodec_decode_audio4(av->audio_codec_ctx, av->frame, &i, &packet);
		
		if(i)
		{
			/* We got a complete frame of audio. Convert
			 * it to the required format. */
			int r = swr_convert(
				av->swr_ctx,
				av->frame_s16,
				av->frame_s16_samples,
				(const uint8_t **) av->frame->data,
				av->frame->nb_samples
			);
			
			*samples = r;
			frame = av->frame_s16[0];
		}
		
		av_packet_unref(&packet);
		
		/* Break out if we got a frame */
		if(frame)
		{
			return(frame);
		}
	}
	
	return(NULL);
}

static int _av_ffmpeg_close(void *private)
{
	av_ffmpeg_t *av = private;
	
	//av_free(av->buffer);
	av_free(av->frame_rgb);
	av_free(av->frame);
	av_freep(av->frame_s16);
	swr_free(&av->swr_ctx);
	//avcodec_close(s->vid_ctx);
	avformat_close_input(&av->format_ctx);
	free(av);
	
	avformat_network_deinit();
	
	return(HACKTV_OK);
}

int av_ffmpeg_open(vid_t *s, char *input_url)
{
	av_ffmpeg_t *av = calloc(1, sizeof(av_ffmpeg_t));
	AVCodec *codec = NULL;
	uint8_t *buffer = NULL;
	int len;
	int i;
	
	av_register_all();
	avformat_network_init();
	
	/* Open the video */
	if(avformat_open_input(&av->format_ctx, input_url, NULL, NULL) < 0)
	{
		fprintf(stderr, "Error opening file '%s'\n", input_url);
		return(HACKTV_ERROR);
	}
	
	/* Read stream info from the file */
	if(avformat_find_stream_info(av->format_ctx, NULL) < 0)
	{
		fprintf(stderr, "Error reading stream information from file\n");
		return(HACKTV_ERROR);
	}
	
	/* Dump some useful information to stderr */
	fprintf(stderr, "Opening '%s'...\n", input_url);
	av_dump_format(av->format_ctx, 0, input_url, 0);
	
	/* Find the first video and audio streams */
	av->video_stream = NULL;
	av->audio_stream = NULL;
	for(i = 0; i < av->format_ctx->nb_streams; i++)
	{
		if(av->video_stream == NULL && av->format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			av->video_stream = av->format_ctx->streams[i];
		}
		
		if(av->audio_stream == NULL && av->format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			av->audio_stream = av->format_ctx->streams[i];
		}
	}
	
	/* At minimum we need a video stream */
	if(av->video_stream == NULL)
	{
		fprintf(stderr, "No video streams found\n");
		return(HACKTV_ERROR);
	}
	
	/* Get a pointer to the codec context for the video stream */
	fprintf(stderr, "Using video stream %d.\n", av->video_stream->index);
	av->video_codec_ctx = av->video_stream->codec;
	av->video_codec_ctx->thread_count = 0; /* Let ffmpeg decide number of threads */
	
	/* Find the decoder for the video stream */
	codec = avcodec_find_decoder(av->video_codec_ctx->codec_id);
	if(codec == NULL)
	{
		fprintf(stderr, "Unsupported video codec\n");
		return(HACKTV_ERROR);
	}
	
	/* Open video codec */
	if(avcodec_open2(av->video_codec_ctx, codec, NULL) < 0)
	{
		fprintf(stderr, "Error opening video codec\n");
		return(HACKTV_ERROR);
	}
	
	/* Allocate video frame */
	av->frame = av_frame_alloc();
	
	/* Allocate an AVFrame structure */
	av->frame_rgb = av_frame_alloc();
	if(av->frame_rgb == NULL)
	{
		fprintf(stderr, "Out of memory\n");
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	/* Determine required buffer size and allocate buffer */
	len = avpicture_get_size(AV_PIX_FMT_RGB32, s->active_width, s->conf.active_lines);
	buffer = (uint8_t *) av_malloc(len);
	if(!buffer)
	{
		fprintf(stderr, "Out of memory\n");
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	/* Assign appropriate parts of buffer to image planes in frame_rgb
	 * Note that frame_rgb is an AVFrame, but AVFrame is a superset
	 * of AVPicture */
	avpicture_fill((AVPicture *) av->frame_rgb, buffer, AV_PIX_FMT_RGB32, s->active_width, s->conf.active_lines);
	
	/* initialize SWS context for software scaling */
	av->sws_ctx = sws_getContext(
		av->video_codec_ctx->width,
		av->video_codec_ctx->height,
		av->video_codec_ctx->pix_fmt,
		s->active_width,
		s->conf.active_lines,
		AV_PIX_FMT_RGB32,
		SWS_BICUBIC,
		NULL,
		NULL,
		NULL
	);
	
	if(av->audio_stream != NULL)
	{
		fprintf(stderr, "Using audio stream %d.\n", av->audio_stream->index);
		
		/* Get a pointer to the codec context for the video stream */
		av->audio_codec_ctx = av->audio_stream->codec;
		av->audio_codec_ctx->thread_count = 0; /* Let ffmpeg decide number of threads */
		
		/* Find the decoder for the audio stream */
		codec = avcodec_find_decoder(av->audio_codec_ctx->codec_id);
		if(codec == NULL)
		{
			fprintf(stderr, "Unsupported audio codec\n");
			return(HACKTV_ERROR);
		}
		
		/* Open audio codec */
		if(avcodec_open2(av->audio_codec_ctx, codec, NULL) < 0)
		{
			fprintf(stderr, "Error opening audio codec\n");
			return(HACKTV_ERROR);
		}
		
		/* Prepare the resampler */
		av->swr_ctx = swr_alloc();
		if(!av->swr_ctx)
		{
			return(HACKTV_OUT_OF_MEMORY);
		}
		
		av_opt_set_int(av->swr_ctx, "in_channel_layout", av->audio_codec_ctx->channel_layout, 0);
		av_opt_set_int(av->swr_ctx, "in_sample_rate",    av->audio_codec_ctx->sample_rate, 0);
		av_opt_set_sample_fmt(av->swr_ctx, "in_sample_fmt", av->audio_codec_ctx->sample_fmt, 0);
		
		av_opt_set_int(av->swr_ctx, "out_channel_layout",    AV_CH_LAYOUT_STEREO, 0);
		av_opt_set_int(av->swr_ctx, "out_sample_rate",       HACKTV_AUDIO_SAMPLE_RATE, 0);
		av_opt_set_sample_fmt(av->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
		
		if(swr_init(av->swr_ctx) < 0)
		{
			fprintf(stderr, "Failed to initialise the resampling context\n");
			return(HACKTV_ERROR);
		}
		
		/* Calculate the number of samples needed for output */
		av->frame_s16_samples = av_rescale_rnd(
			av->audio_codec_ctx->frame_size, /* Can I trust this? */
			HACKTV_AUDIO_SAMPLE_RATE,
			av->audio_codec_ctx->sample_rate,
			AV_ROUND_UP
		);
		if(av->frame_s16_samples == 0)
		{
			av->frame_s16_samples = HACKTV_AUDIO_SAMPLE_RATE;
		}
		
		/* Allocate memory for decoded and resampled audio */
		av_samples_alloc_array_and_samples(
			&av->frame_s16,
			&av->frame_s16_linesize,
			2, /* Number of channels */
			av->frame_s16_samples,
			AV_SAMPLE_FMT_S16,
			0
		);
	}
	else
	{
		fprintf(stderr, "No audio streams found.\n");
	}
	
	/* Register the callback functions */
	s->av_private = av;
	s->av_read_video = _av_ffmpeg_read_video;
	s->av_read_audio = _av_ffmpeg_read_audio;
	s->av_close = _av_ffmpeg_close;

	return(HACKTV_OK);
}

