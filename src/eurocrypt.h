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

#define ECM_PAYLOAD_BYTES 45
#define EMMU 0x00
#define EMMS 0xF8
#define EMMC 0xC7
#define EMMG 0x3F

typedef struct {
	const char *id;   /* Mode id */
	int des_algo;     /* Eurocrypt M or S2 algo */
	int packet_type;  /* Eurocrypt M or S2 packet  */
	uint8_t key[16];  /* Decryption keys */
	uint8_t ppid[3];  /* Programme provider identifier */
	char date[10];    /* Broadcast date */
	uint8_t theme[2]; /* Theme */
	char channame[32];/* Channel name to display */
} ec_mode_t;

typedef struct {
	const char *id;   /* Mode id */
	int des_algo;     /* Eurocrypt M or S2 algo */
	int packet_type;  /* Eurocrypt M or S2 packet  */
	uint8_t key[16];  /* Decryption key */
	uint8_t ppid[3];  /* Programme provider identifier */
	uint8_t sa[3];    /* Shared Address */
	uint8_t ua[5];    /* Unique Address */
	int emmtype;
} em_mode_t;

typedef struct {
	
	const ec_mode_t *mode;
	const em_mode_t *emmode;
	
	/* Encrypted even and odd control words */
	uint8_t ecw[2][8];
	
	/* Decrypted even and odd control words */
	uint8_t cw[2][8];
	
	/* Hash */
	uint8_t ecm_hash[8];
	uint8_t emm_hash[8];
	
	/* ECM packet */
	int ecm_addr;
	uint8_t ecm_pkt[MAC_PAYLOAD_BYTES * 2];
	
	/* Packet continuities */
	int ecm_cont;
	int emm_cont;
	
	/* EMM packet */
	int emm_addr;
	uint8_t emms_pkt[MAC_PAYLOAD_BYTES];
	uint8_t emmu_pkt[MAC_PAYLOAD_BYTES * 2];
	uint8_t emmg_pkt[MAC_PAYLOAD_BYTES * 2];
	uint8_t enc_data[8];
	
} eurocrypt_t;

extern int eurocrypt_init(vid_t *s, const char *mode);
extern void eurocrypt_next_frame(vid_t *s, int frame);

#endif

