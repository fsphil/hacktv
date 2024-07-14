/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2024 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _SIS_H
#define _SIS_H

typedef struct {
	vbidata_lut_t *lut;
	int16_t audio[NICAM_AUDIO_LEN * 2];
	nicam_enc_t nicam;
	uint8_t frame[NICAM_FRAME_BYTES];
	int frame_bit;
	int re;
	
	int blank_left;
	int blank_width;
	int16_t *blank_win;
	int16_t blank_level;
	
} sis_t;

extern int sis_init(sis_t *s, const char *sismode, vid_t *vid, uint8_t mode, uint8_t reserve);
extern void sis_free(sis_t *s);
extern int sis_render(vid_t *s, void *arg, int nlines, vid_line_t **lines);

extern int sis_write_audio(sis_t *s, const int16_t *audio);

#endif

