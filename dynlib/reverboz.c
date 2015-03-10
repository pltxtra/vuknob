/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 *
 * About SATAN:
 *   Originally developed as a small subproject in
 *   a course about applied signal processing.
 * Original Developers:
 *   Anton Persson
 *   Johan Thim
 *
 * http://www.733kru.org/
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "dynlib.h"
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <fixedpointmath.h>

#define DELAY_LINE_LEN 44100 * 2

typedef struct _ReverbozData {
	FTYPE delay_line[DELAY_LINE_LEN * 2];
	int offset;
} ReverbozData;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	ReverbozData *d = (ReverbozData *)malloc(sizeof(ReverbozData));
	memset(d, 0, sizeof(ReverbozData));
	/* return pointer to instance data */
	return (void *)d;
}
 
void delete(void *data) {
	ReverbozData *d = (ReverbozData *)data;
	/* free instance data here */
	free(d);
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
//	ReverbozData *d = (ReverbozData *)void_info;
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	ReverbozData *d = (ReverbozData *)data;

	SignalPointer *s = mt->get_input_signal(mt, "Stereo");
	SignalPointer *os = mt->get_output_signal(mt, "Stereo");

	if(os == NULL) return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);
	int oc = mt->get_signal_channels(os);

	if(s == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < ol; t++) {
			ou[t * 2 + 0] = itoFTYPE(0);
			ou[t * 2 + 1] = itoFTYPE(0);
		}
		return;		
	}
	
	FTYPE *in = mt->get_signal_buffer(s);
	int il = mt->get_signal_samples(s);
	int ic = mt->get_signal_channels(s);

	int c;
	int i, k;
	for(i = 0; i < ol; i++) {
		for(c = 0; c < 2; c++) {
			// dry + reverb
			k = ((i + d->offset) % DELAY_LINE_LEN) * oc + c;
			ou[i * oc + c] = in[i * ic + c] +
				d->delay_line[k];
			d->delay_line[k] = 0;

			// eight echoes
			k = ((i + 3000 + d->offset) % DELAY_LINE_LEN) * oc + c;
			d->delay_line[k] += mulFTYPE(in[i * oc + c], ftoFTYPE(0.8));
			k = ((i + 4200 + d->offset) % DELAY_LINE_LEN) * oc + c;
			d->delay_line[k] += mulFTYPE(in[i * oc + c], ftoFTYPE(0.7));
			k = ((i + 5400 + d->offset) % DELAY_LINE_LEN) * oc + c;
			d->delay_line[k] += mulFTYPE(in[i * oc + c], ftoFTYPE(0.6));
			k = ((i + 7800 + d->offset) % DELAY_LINE_LEN) * oc + c;
			d->delay_line[k] += mulFTYPE(in[i * oc + c], ftoFTYPE(0.5));
			k = ((i + 9200 + d->offset) % DELAY_LINE_LEN) * oc + c;
			d->delay_line[k] += mulFTYPE(in[i * oc + c], ftoFTYPE(0.4));
			k = ((i + 11000 + d->offset) % DELAY_LINE_LEN) * oc + c;
			d->delay_line[k] += mulFTYPE(in[i * oc + c], ftoFTYPE(0.3));
			k = ((i + 12800 + d->offset) % DELAY_LINE_LEN) * oc + c;
			d->delay_line[k] += mulFTYPE(in[i * oc + c], ftoFTYPE(0.2));
			k = ((i + 14000 + d->offset) % DELAY_LINE_LEN) * oc + c;
			d->delay_line[k] += mulFTYPE(in[i * oc + c], ftoFTYPE(0.1));
		}
	}	

	d->offset = (d->offset + il) % (DELAY_LINE_LEN);
}

