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
#include "hacktv.h"

/* nothing to see here */

typedef struct {
	
	AVFormatContext *format_ctx;
	AVCodecContext *codec_ctx;
	AVFrame *frame;
	AVFrame *frame_rgb;
	struct SwsContext *sws_ctx;
	
	int video_stream;
	
} av_ffmpeg_t;

static uint32_t *_av_ffmpeg_read_video(void *private)
{
	av_ffmpeg_t *av = private;
	AVPacket packet;
	uint32_t *frame = NULL;
	int i;
	
	while(av_read_frame(av->format_ctx, &packet) >= 0)
	{
		/* Only handle frames from the video stream */
		if(packet.stream_index == av->video_stream)
		{
			/* Decode video frame */
			avcodec_decode_video2(av->codec_ctx, av->frame, &i, &packet);
			
			if(i)
			{
				/* We got a complete frame. Convert it
				 * from its native format to RGB32 */
				sws_scale(
					av->sws_ctx,
					(uint8_t const * const *) av->frame->data,
					av->frame->linesize,
					0,
					av->codec_ctx->height,
					av->frame_rgb->data,
					av->frame_rgb->linesize
				);
				
				frame = (uint32_t *) av->frame_rgb->data[0];
			}
		}
		
		av_free_packet(&packet);
		
		/* Break out if we got a frame */
		if(frame)
		{
			return(frame);
		}
	}
	
	/* No more packets or an error occured */
	return(NULL);
}

static int16_t *_av_ffmpeg_read_audio(void *private, size_t samples)
{
	av_ffmpeg_t *av = private;
	return(NULL);
}

static int _av_ffmpeg_close(void *private)
{
	av_ffmpeg_t *av = private;
	
	//av_free(av->buffer);
	av_free(av->frame_rgb);
	av_free(av->frame);
	//avcodec_close(s->vid_ctx);
	avformat_close_input(&av->format_ctx);
	free(av);
	
	return(HACKTV_OK);
}

int av_ffmpeg_open(hacktv_t *s, char *input_url)
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
	
	/* Find the first video stream */
	for(i = 0; i < av->format_ctx->nb_streams; i++)
	{
		if(av->format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			break;
		}
	}
	
	if(i == av->format_ctx->nb_streams)
	{
		fprintf(stderr, "No video streams found\n");
		return(HACKTV_ERROR);
	}
	
	av->video_stream = i;
	
	/* Get a pointer to the codec context for the video stream */
	av->codec_ctx = av->format_ctx->streams[i]->codec;
	
	/* Find the decoder for the video stream */
	codec = avcodec_find_decoder(av->codec_ctx->codec_id);
	if(codec == NULL)
	{
		fprintf(stderr, "Unsupported video codec\n");
		return(HACKTV_ERROR);
	}
	
	/* Open codec */
	if(avcodec_open2(av->codec_ctx, codec, NULL) < 0)
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
	len = avpicture_get_size(AV_PIX_FMT_RGB32, s->vid.active_width, s->vid.conf.active_lines);
	buffer = (uint8_t *) av_malloc(len);
	if(!buffer)
	{
		fprintf(stderr, "Out of memory\n");
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	/* Assign appropriate parts of buffer to image planes in frame_rgb
	 * Note that frame_rgb is an AVFrame, but AVFrame is a superset
	 * of AVPicture */
	avpicture_fill((AVPicture *) av->frame_rgb, buffer, AV_PIX_FMT_RGB32, s->vid.active_width, s->vid.conf.active_lines);
	
	/* initialize SWS context for software scaling */
	av->sws_ctx = sws_getContext(
		av->codec_ctx->width,
		av->codec_ctx->height,
		av->codec_ctx->pix_fmt,
		s->vid.active_width,
		s->vid.conf.active_lines,
		AV_PIX_FMT_RGB32,
		SWS_BICUBIC,
		NULL,
		NULL,
		NULL
	);
	
	/* Register the callback functions */
	s->av_private = av;
	s->av_read_video = _av_ffmpeg_read_video;
	s->av_read_audio = _av_ffmpeg_read_audio;
	s->av_close = _av_ffmpeg_close;

	return(HACKTV_OK);
}

