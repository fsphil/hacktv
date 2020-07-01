/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2020 Alex L. James                                          */
/* Copyright 2020 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _EUROCRYPT_H
#define _EUROCRYPT_H

typedef struct {
	const char *id;		/* Mode id */
	int emode;		/* Eurocrypt M or S2 mode */
	uint8_t key[7];		/* Decryption key */
	uint8_t ppid[3];	/* Programme provider identifier */
	uint8_t cdate[4];	/* CDATE + THEME/LEVEL */
} ec_mode_t;

typedef struct {
	
	const ec_mode_t *mode;
	
	/* Encrypted even and odd control words */
	uint8_t ecw[2][8];
	
	/* Decrypted even and odd control words */
	uint8_t cw[2][8];
	
	/* ECM packet */
	int ecm_addr;
	uint8_t ecm_pkt[MAC_PAYLOAD_BYTES];
	
} eurocrypt_t;

extern int eurocrypt_init(vid_t *s, const char *mode);
extern void eurocrypt_next_frame(vid_t *s);

#endif

