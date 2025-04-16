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
		.cc608 = { 0, 0 },
	};
}

int av_read_video(av_t *s, av_frame_t *frame)
{
	int r = AV_EOF;
	
	if(s->read_video)
	{
		r = s->read_video(s->av_source_ctx, frame);
		
		if(r != AV_OK)
		{
			/* EOF or error */
			s->read_video = NULL;
		}
	}
	else
	{
		/* Send a blank frame */
		av_frame_init(frame, 0, 0, NULL, 0, 0);
		r = AV_OK;
	}
	
	if(r == AV_OK) s->frames++;
	
	return(r);
}

int av_read_audio(av_t *s, int16_t **samples, size_t *nsamples)
{
	int r = AV_EOF;
	
	*samples = NULL;
	*nsamples = 0;
	
	if(s->read_audio)
	{
		r = s->read_audio(s->av_source_ctx, samples, nsamples);
		
		if(r != AV_OK)
		{
			/* EOF or error */
			s->read_audio = NULL;
		}
	}
	
	if(r == AV_OK) s->samples += *nsamples;
	
	return(r);
}

int av_eof(av_t *s)
{
	return(s->read_video == NULL && s->read_audio == NULL ? 1 : 0);
}

int av_close(av_t *s)
{
	int r;
	
	r = s->close ? s->close(s->av_source_ctx) : AV_ERROR;
	
	s->av_source_ctx = NULL;
	s->read_video = NULL;
	s->read_audio = NULL;
	s->close = NULL;
	
	return(r);
}

r64_t av_calculate_frame_size(av_t *av, r64_t resolution, r64_t aspect)
{
	r64_t r = { av->width, av->height };
	const r64_t fadj[][2] = {
		/* Horizontal resolution adjustment factors based on the list at:
		 * https://xpt.sourceforge.net/techdocs/media/video/dvd/dvd04-DVDAuthoringSpecwise/ar01s02.html
		 */
		{ { 720, 576 }, { 720, 702 } },   /* D1/DV/DVB/DVD/SVCD */
		{ { 704, 576 }, { 704, 702 } },   /* DVB/DVD/VCD */
		{ { 544, 576 }, { 1088, 1053 } }, /* DVB */
		{ { 480, 576 }, { 480, 468 } },   /* SVCD */
		{ { 384, 288 }, { 768, 767 } },
		{ { 352, 576 }, { 352, 351 } },   /* DVD */
		{ { 352, 288 }, { 352, 351 } },   /* VCD/DVD */
		{ { 176, 144 }, { 352, 351 } },   /* H.261/H.263 */
		
		{ { 720, 480 }, { 1600, 1587 } }, /* DVD */
		{ { 704, 480 }, { 14080, 14283 } }, /* ATSC/DVD/VCD */
		
		{ }
	};
	
	if(av->fit_mode == AV_FIT_STRETCH)
	{
		/* Mode "stretch" ignores the source aspect,
		 * always returns the active resolution */
	}
	else if(av->fit_mode == AV_FIT_NONE)
	{
		/* Mode "none" keeps the source resolution */
		return(resolution);
	}
	else
	{
		r64_t b, c;
		
		/* Use frame size if no aspect set, assume 1:1 pixel ratio */
		if(aspect.num <= 0 || aspect.den <= 0)
		{
			aspect = resolution;
		}
		
		if(av->fit_mode == AV_FIT_FILL)
		{
			/* Mode "fill" scales the source video to fill the active frame */
			
			/* Find the nearest display aspect ratio if there is more than one */
			c = av->display_aspect_ratios[0];
			
			if(av->display_aspect_ratios[1].den > 0)
			{
				c = r64_nearest(aspect, c, av->display_aspect_ratios[1]);
			}
		}
		else
		{
			c = aspect;
		}
		
		/* Enforce active ratio limits if set */
		if(av->min_display_aspect_ratio.den > 0 &&
		   r64_cmp(c, av->min_display_aspect_ratio) < 0)
		{
			c = av->min_display_aspect_ratio;
		}
		
		if(av->max_display_aspect_ratio.den > 0 &&
		   r64_cmp(c, av->max_display_aspect_ratio) > 0)
		{
			c = av->max_display_aspect_ratio;
		}
		
		/* b = display ratio */
		b = av->display_aspect_ratios[0];
		
		if(av->display_aspect_ratios[1].den > 0)
		{
			b = r64_nearest(c, b, av->display_aspect_ratios[1]);
		}
		
		/* Calculate visible resolution */
		if(r64_cmp(c, b) > 0)
		{
			/* Vertical padding (Letterbox) */
			r.den = (int64_t) r.den * ((int64_t) b.num * c.den) / ((int64_t) b.den * c.num);
		}
		else if(r64_cmp(c, b) < 0)
		{
			/* Horizontal padding (Pillarbox) */
			r.num = (int64_t) r.num * ((int64_t) c.num * b.den) / ((int64_t) c.den * b.num);
		}
		
		/* Calculate source resolution */
		if(r64_cmp(c, aspect) > 0)
		{
			/* Vertical cropping */
			r.den = (int64_t) r.den * ((int64_t) c.num * aspect.den) / ((int64_t) c.den * aspect.num);
		}
		else if(r64_cmp(c, aspect) < 0)
		{
			/* Horizontal cropping */
			r.num = (int64_t) r.num * ((int64_t) aspect.num * c.den) / ((int64_t) aspect.den * c.num);
		}
	}
	
	/* Experiment: Adjust final resolution to compensate for padding */
	for(int i = 0; fadj[i][0].num != 0; i++)
	{
		if(resolution.num == fadj[i][0].num &&
		   resolution.den == fadj[i][0].den)
		{
			r.num = (int64_t) r.num * fadj[i][1].num / fadj[i][1].den;
			break;
		}
	}
	
	return(r);
}

r64_t av_display_aspect_ratio(av_frame_t *frame)
{
	/* Helper function to return a frames display aspect ratio */
	/* DAR = SAR * PAR */
	return(r64_mul(
		(r64_t) { frame->width, frame->height },
		frame->pixel_aspect_ratio)
	);
}

void av_set_display_aspect_ratio(av_frame_t *frame, r64_t display_aspect_ratio)
{
	/* Helper function to set a frames display aspect ratio */
	/* PAR = DAR / SAR */
	frame->pixel_aspect_ratio = r64_div(
		display_aspect_ratio,
		(r64_t) { frame->width, frame->height }
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
		frame->pixel_aspect_ratio = (r64_t) {
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

