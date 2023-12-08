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
#include <stdint.h>
#include "rf.h"

/* RF sink callback handlers */
int rf_write(rf_t *s, int16_t *iq_data, size_t samples)
{
	if(s->write)
	{
		return(s->write(s->ctx, iq_data, samples));
	}
	
	return(RF_ERROR);
}

int rf_close(rf_t *s)
{
	if(s->close)
	{
		return(s->close(s->ctx));
	}
	
	return(RF_OK);
}

