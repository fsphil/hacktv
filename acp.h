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

#ifndef _ACP_H
#define _ACP_H

#include <stdint.h>
#include "video.h"

typedef struct {
	
	int left[6];
	
	int16_t psync_level;
	int16_t pagc_level;
	
	int psync_width;
	int pagc_width;
	
} acp_t;

extern int acp_init(acp_t *s, vid_t *vid);
extern void acp_free(acp_t *s);
extern int acp_render_line(vid_t *s, void *arg, int nlines, vid_line_t **lines);

#endif

