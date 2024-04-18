/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2023 Philip Heron <phil@sanslogic.co.uk>                    */
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

#include <stddef.h>
#include "av.h"

void av_frame_init(av_frame_t *frame, int width, int height, uint32_t *framebuffer, int pstride, int lstride)
{
	*frame = (av_frame_t) {
		.width = width,
		.height = height,
		.framebuffer = framebuffer,
		.pixel_stride = pstride,
		.line_stride = lstride,
		.pixel_aspect_ratio = { 1, 1 },
		.interlaced = 0,
	};
}

int av_read_video(av_t *s, av_frame_t *frame)
{
	int r;
	
	if(s->read_video)
	{
		r = s->read_video(s->av_source_ctx, frame);
	}
	else
	{
		av_frame_init(frame, 0, 0, NULL, 0, 0);
		r = AV_OK;
	}
	
	if(r == AV_OK) s->frames++;
	
	return(r);
}

int16_t *av_read_audio(av_t *s, size_t *samples)
{
	int16_t *r = NULL;
	
	if(s->read_audio)
	{
		r = s->read_audio(s->av_source_ctx, samples);
	}
	
	s->samples += *samples;
	
	return(r);
}

int av_eof(av_t *s)
{
	return(s->eof ? s->eof(s->av_source_ctx) : 0);
}

int av_close(av_t *s)
{
	int r;
	
	r = s->close ? s->close(s->av_source_ctx) : AV_ERROR;
	
	s->av_source_ctx = NULL;
	s->read_video = NULL;
	s->read_audio = NULL;
	s->eof = NULL;
	s->close = NULL;
	
	return(r);
}

rational_t av_calculate_frame_size(av_t *av, rational_t resolution, rational_t aspect)
{
	rational_t r = { av->width, av->height };
	rational_t b = aspect;
	rational_t c = av->display_aspect_ratios[0];
	rational_t min, max;
	
	/* Experiment: Adjust aspect to compensate for 702x576 > 720x576 padding */
	//if(resolution.num == 720 && resolution.den == 576)
	//{
	//	b = aspect = rational_mul(aspect, (rational_t) { 720, 702 });
	//}
	
	/* Find the nearest display aspect ratio if there is more than one */
	if(av->display_aspect_ratios[1].den > 0)
	{
		c = rational_nearest(b, c, av->display_aspect_ratios[1]);
	}
	
	if(av->fit_mode == AV_FIT_STRETCH ||
	   aspect.num <= 0 || aspect.den <= 0)
	{
		/* Mode "stretch" ignores the source aspect,
		 * always returns the active resolution */
		return(r);
	}
	else if(av->fit_mode == AV_FIT_NONE)
	{
		/* Mode "none" keeps the source resolution */
		return(resolution);
	}
	else if(av->fit_mode == AV_FIT_FILL)
	{
		/* Mode "fill" scales the source video to
		 * fill the active frame */
		min = max = c;
	}
	else if(av->fit_mode == AV_FIT_FIT)
	{
		/* Mode "fit" scales the source video to
		 * fit entirely within the active frame */
		min = (rational_t) { 2, r.den };
		max = (rational_t) { r.num, 2 };
	}
	
	/* Test for min/max override */
	if(av->min_display_aspect_ratio.den > 0)
	{
		min = av->min_display_aspect_ratio;
	}
	
	if(av->max_display_aspect_ratio.den > 0)
	{
		max = av->max_display_aspect_ratio;
	}
	
	/* Restrict visible ratio */
	if(rational_cmp(b, min) < 0) b = min;
	else if(rational_cmp(b, max) > 0) b = max;
	
	/* Calculate visible resolution */
	if(rational_cmp(b, c) < 0)
	{
		r.num = r.num * (b.num * c.den) / (c.num * b.den);
	}
	else if(rational_cmp(b, c) > 0)
	{
		r.den = r.den * (c.num * b.den) / (b.num * c.den);
	}
	
	/* Calculate source resolution */
	if(rational_cmp(b, aspect) < 0)
	{
		r.num = r.num * (aspect.num * b.den) / (b.num * aspect.den);
	}
	else if(rational_cmp(b, aspect) > 0)
	{
		r.den = r.den * (b.num * aspect.den) / (aspect.num * b.den);
	}
	
	return(r);
}

rational_t av_display_aspect_ratio(av_frame_t *frame)
{
	/* Helper function to return a frames display aspect ratio */
	/* DAR = SAR * PAR */
	return(rational_mul(
		(rational_t) { frame->width, frame->height },
		frame->pixel_aspect_ratio)
	);
}

void av_set_display_aspect_ratio(av_frame_t *frame, rational_t display_aspect_ratio)
{
	/* Helper function to set a frames display aspect ratio */
	/* PAR = DAR / SAR */
	frame->pixel_aspect_ratio = rational_div(
		display_aspect_ratio,
		(rational_t) { frame->width, frame->height }
	);
}

void av_hflip_frame(av_frame_t *frame)
{
	frame->framebuffer += (frame->width - 1) * frame->pixel_stride;
	frame->pixel_stride = -frame->pixel_stride;
}

void av_vflip_frame(av_frame_t *frame)
{
	frame->framebuffer += (frame->height - 1) * frame->line_stride;
	frame->line_stride = -frame->line_stride;
}

void av_rotate_frame(av_frame_t *frame, int a)
{
	int i;
	
	/* a == degrees / 90 */
	a = a % 4;
	
	if(a == 1 || a == 3)
	{
		/* Rotate the frame 90 degrees clockwise */
		
		/* Move the origin to the bottom left of the image */
		frame->framebuffer += (frame->height - 1) * frame->line_stride;
		
		/* Reverse the image dimensions */
		i = frame->width;
		frame->width = frame->height;
		frame->height = i;
		
		/* Reverse the line and pixel strides */
		i = frame->pixel_stride;
		frame->pixel_stride = -frame->line_stride;
		frame->line_stride = i;
		
		/* Reverse the pixel aspect ratio (r = 1 / r) */
		frame->pixel_aspect_ratio = (rational_t) {
			frame->pixel_aspect_ratio.den,
			frame->pixel_aspect_ratio.num
		};
	}
	
	if(a == 2 || a == 3)
	{
		/* Rotate the frame 180 degrees */
		av_hflip_frame(frame);
		av_vflip_frame(frame);
	}
}

void av_crop_frame(av_frame_t *frame, int x, int y, int width, int height)
{
	if(x < 0) { width += x; x = 0; }
	if(y < 0) { height += y; y = 0; }
	if(x + width > frame->width) width = frame->width - x;
	if(y + height > frame->height) height = frame->height - y;
	
	frame->framebuffer += y * frame->line_stride + x * frame->pixel_stride;
	frame->width = width;
	frame->height = height;
}

