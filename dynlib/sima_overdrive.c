/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2011 by Anton Persson & Sima Baymani
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

typedef struct _sima_overdrive {
	FTYPE knee; // where to put the knee
	FTYPE level;//between 0 and 1, amplitude (roof/floor)
	FTYPE dry_mix;//how to intermix dry and wet signals. Dry = no effect, wet = with effect
	FTYPE volume; // the output volume
} sima_overdrive_t;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	sima_overdrive_t *data = (sima_overdrive_t *)malloc(sizeof(sima_overdrive_t));
	if(data == NULL) return NULL;

	// init data here
	data->knee = ftoFTYPE(0.5f);
	data->level = ftoFTYPE(0.5f);
	data->dry_mix = ftoFTYPE(0.5f);
	data->volume = ftoFTYPE(1.0f);
	/* return pointer to instance data */
	return (void *)data;
}
 
void *get_controller_ptr(MachineTable *mt, void *data,
			 const char *name,
			 const char *group) {
	sima_overdrive_t *so = (sima_overdrive_t *)data;

	if(strcmp("knee",name)==0) {
		return &(so->knee);
	} else if(strcmp("level",name)==0) {
		return &(so->level);
	} else if(strcmp("dry_mix",name)==0) {
		return &(so->dry_mix);
	} else if(strcmp("volume",name)==0) {
		return &(so->volume);
	} else {
		return NULL;
	}
}

void reset(MachineTable *mt, void *data) {
	//do nothing
}

void execute(MachineTable *mt, void *data) {
	sima_overdrive_t *so = (sima_overdrive_t *)data;
	SignalPointer *outsig = NULL;
	SignalPointer *insig = NULL;

	outsig = mt->get_output_signal(mt, "Mono");
	if(outsig == NULL) {
		return;
	}

	FTYPE *out =
		(FTYPE *)mt->get_signal_buffer(outsig);
	int out_l = mt->get_signal_samples(outsig);

	insig = mt->get_input_signal(mt, "Mono");
	if(insig == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < out_l; t++) {
			out[t] = itoFTYPE(0);
		}
		return;
	}
	
	FTYPE *in =
		(FTYPE *)mt->get_signal_buffer(insig);
	int in_l = mt->get_signal_samples(insig);

	if(in_l != out_l)
		return; // we expect equal lengths..

	int t;

	FTYPE fx_d = so->dry_mix;
	FTYPE fx_d_inv = ftoFTYPE(1.0f) - so->dry_mix;

	FTYPE sign, value;

	FTYPE k1, k2, y2;

	k1 = divFTYPE(so->level, so->knee);
	k2 = divFTYPE((ftoFTYPE(1.0f) - so->level), ftoFTYPE(1.0f) - so->knee);
	y2 = so->level - mulFTYPE(k2, so->knee);
	
	for(t = 0; t < out_l; t++) {
		if(in[t] < itoFTYPE(0)) {
			value = -in[t];
			sign = itoFTYPE(-1);
		} else {
			value = in[t];
			sign = itoFTYPE(1);
		}
		
		if(value > itoFTYPE(1)) {
			value = itoFTYPE(1);
		} else if(value > so->knee) {
			value = mulFTYPE(value, k2) + y2;
		} else {
			value = mulFTYPE(value, k1);
		}

		value = mulFTYPE(value, sign);
		
		out[t] = mulFTYPE(so->volume, mulFTYPE(fx_d, in[t]) + mulFTYPE(fx_d_inv, value));
	}
}

void delete(void *data) {
	// clear and clean up everything here
	
	/* then free instance data here */
	free(data);
}
