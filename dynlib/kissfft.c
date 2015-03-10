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

//#define __DO_DYNLIB_DEBUG
#include "dynlib_debug.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include "dynlib.h"
USE_SATANS_MATH

#include "libfilter.c"

#define FIXED_POINT 32

#include "kiss_fftr.h"

typedef struct _KissFFTData {
	kiss_fftr_cfg cfg;

	kiss_fft_cpx temp[1024];
} KissFFTData;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	KissFFTData *d = (KissFFTData *)malloc(sizeof(KissFFTData));

	if(d) {
		memset(d, 0, sizeof(KissFFTData));

		d->cfg = kiss_fftr_alloc(1024, 0, NULL, NULL);
	}
	
	return (void *)d;
}
 
void delete(void *data) {
	KissFFTData *d = (KissFFTData *)data;
	if(d) 
		free(d);
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	KissFFTData *d = (KissFFTData *)void_info;
	
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	KissFFTData *d = (KissFFTData *)data;

	SignalPointer *s = mt->get_input_signal(mt, "Mono");
	SignalPointer *os = mt->get_output_signal(mt, "Mono");

	if(os == NULL) return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);
	int Fs = mt->get_signal_frequency(os);
	float Fs_i = 1.0f / ((float)Fs);
	
	if(s == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < ol; t++) {
			ou[t] = itoFTYPE(0);
		}
		return;		
	}
	
	FTYPE *in = mt->get_signal_buffer(s);

	libfilter_set_Fs(Fs);
	
	int i, k;
	int maximum_k;;
	int32_t maximum = 0;
	// this is STUPID in a real project, but we will bluntly assume the buffers are a multiple of 1024...
	for(i = 0; i < ol; i += 1024) {
		kiss_fftr(d->cfg, &in[i], d->temp);

		maximum = 0;
		
		for(k = 0; k < 1024; k++) {
			ou[i + k] = d->temp[k].i > d->temp[k].r ? d->temp[k].i : d->temp[k].r;
//			printf("fft[%4d] = (%d, %d)\n", k, d->temp[k].i, d->temp[k].r);

			if(ou[i + k] < 0)
				ou[i + k] = -ou[i + k];

			if(ou[i + k] > maximum) {
				maximum_k = k;
				maximum = ou[i + k];
			}
			
		}
		printf("maximum fft[%4d] = (%f)\n", maximum_k, FTYPEtof(ou[i + maximum_k]));
		printf("   estimated frequency: %.2fHz\n", (44100.0f / 1024.0f) * (float)maximum_k);
	}
}

