/* Nagravision Syster encoder for hacktv                                 */
/*=======================================================================*/
/* Copyright 2018 Alex L. James                                          */
/* Copyright 2018 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _SYSTER_H
#define _SYSTER_H

#include <stdint.h>
#include "video.h"
#include "vbidata.h"

#define NG_SAMPLE_RATE 4437500

#define NG_VBI_WIDTH 284
#define NG_VBI_BYTES 28

#define NG_MSG_BYTES 84

#define NG_FIELD_1_START   23
#define NG_FIELD_2_START   336
#define NG_LINES_PER_FIELD 287

#define D11_FIELD_1_START   23
#define D11_FIELD_2_START   335
#define D11_LINES_PER_FIELD 286
#define D11_FIELDS 6

#define NG_ENCRYPT 1
#define NG_DECRYPT 0

/* Cut and rotate defines */
#define SCNR_WIDTH (NG_SAMPLE_RATE / 25 / 625) /* 284 */
#define SCNR_LEFT          46
#define SCNR_TOTAL_CUTS    230

#define RANDOM_ECM 0
#define STATIC_ECM 1

/* NG_DELAY_LINES needs to be long enough for the scrambler to access any
 * line in the next field from at least the last 32 lines of the current.
 * This is a safe amount and can probably be reduced. */

#define NG_DELAY_LINES (625 + NG_FIELD_1_START + NG_LINES_PER_FIELD - (NG_FIELD_2_START + NG_LINES_PER_FIELD - 32))

/* Entitlement control messages */
typedef struct {
	uint64_t cw;
	uint8_t ecm[16];
} ng_ecm_t;

typedef struct {
	char *id;			/* Provider string */
	uint8_t key[8];		/* DES decryption key */
	uint8_t data[8];	/* Programme provider data */
	
	/*
	data[0] = window (operator?)
	data[1] = channel
	data[2] = audience (0x11 = free access)
	data[3] = ??
	data[4] = date
	data[5] = date
	data[6] = PPV date
	data[7] = PPV date
	*/
	
	char *date;			/* Broadcast date */
	int vbioffset;		/* VBI offset */
	int t;				/* Key table to use */
} ng_mode_t;

typedef struct {

	
	uint8_t flags;

	/* ECM */
	ng_ecm_t *blocks;
	ng_mode_t *mode;
	int id;
	
	/* Permute tables */
	const uint8_t *table;

	/* VBI */
	vbidata_lut_t *lut;
	uint8_t vbi[10][NG_VBI_BYTES];
	int vbi_seq;
	int block_seq;

	/* EMM */
	int next_ppua;

	/* PRBS state */
	uint64_t cw;
	uint32_t sr1;
	uint32_t sr2;

	/* PRNG seed values */
	int s; /* 0, ..., 127 */
	int r; /* 0, ..., 255 */

	/* The line order for the next field (0-287) */
	int order[NG_LINES_PER_FIELD];

	/* Syster delay line */
	int16_t *delay;
	int16_t *delay_line[NG_DELAY_LINES];

	/* D11 delay values */
	int ng_delay;
	int d11_line_delay[D11_LINES_PER_FIELD * D11_FIELDS];

	/* Audio inversion FIR filter */
	int16_t *firli, *firlq; /* Left channel, I + Q */
	int16_t *firri, *firrq; /* Right channel, I + Q */
	int mixx;
	int firx;
	
	int video_scale[8520];

} ng_t;

extern int ng_init(ng_t *s, vid_t *vs);
extern void ng_free(ng_t *s);
extern void ng_invert_audio(ng_t *s, int16_t *audio, size_t samples);
extern int ng_render_line(vid_t *s, void *arg, int nlines, vid_line_t **lines);
extern int d11_init(ng_t *s, vid_t *vid, char *mode);
extern int d11_render_line(vid_t *s, void *arg, int nlines, vid_line_t **lines);
extern int systercnr_init(ng_t *s, vid_t *vid, char *mode);
extern int systercnr_render_line(vid_t *s, void *arg, int nlines, vid_line_t **lines);

#endif
