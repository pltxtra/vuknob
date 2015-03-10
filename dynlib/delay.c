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

#include "../satan_debug.hh"

#include "dynlib.h"
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

#define DELAY_LINE_LENGTH (44100 * 5)
#define FILTER_RECALC_PERIOD 1024

typedef struct _delay_t {
	xPassFilterMono_t *lpf;
	xPassFilterMono_t *hpf;

	struct delayLineMono *dl0;
	FTYPE mem;

	FTYPE cutoff, resonance;
	int filter_type;
	
	float delay, amplitude;

	FILTER_RECALC_PART;
} delay_t;

void delete(void *data) {
	delay_t *d = (delay_t *)data;
	
	/* free instance data here */
	if(d->dl0) delayLineMonoFree(d->dl0);
	if(d->lpf) xPassFilterMonoFree(d->lpf);
	if(d->hpf) xPassFilterMonoFree(d->hpf);

	// free main struct here
	free(d);
}

void recalc_filter(xPassFilterMono_t *filter, FTYPE cutoff, FTYPE resonance) {
	xPassFilterMonoSetCutoff(filter, ftoFTYPE(0.002) + mulFTYPE(cutoff, ftoFTYPE(0.45f))); // 45% of Fs
	xPassFilterMonoSetResonance(filter, resonance);
	xPassFilterMonoRecalc(filter);
}

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	delay_t *d = (delay_t *)malloc(sizeof(delay_t));
	if(d == NULL) return d;
	
	memset(d, 0, sizeof(delay_t));

	d->dl0 = create_delayLineMono(DELAY_LINE_LENGTH); // 5 sec at 44100Hz sampling frequency
	d->lpf = create_xPassFilterMono(mt, 0);
	d->hpf = create_xPassFilterMono(mt, 1);
	if(
		(d->dl0 == NULL)
		||
		(d->lpf == NULL)
		||
		(d->hpf == NULL)
		) {
		goto epic_fail;
	}
	
	d->delay = 0.25;
	d->amplitude = 0.5;

	recalc_filter(d->lpf, d->cutoff, d->resonance);
	recalc_filter(d->hpf, d->cutoff, d->resonance);
	
	/* return pointer to instance data */
	return (void *)d;

epic_fail:
	delete(d);
	return NULL;       
}
 
void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	delay_t *d = (delay_t *)void_info;
	if(strcmp(name, "delay") == 0) {
		return &(d->delay);
	}
	if(strcmp(name, "amplitude") == 0) {
		return &(d->amplitude);
	}
	if(strcmp(name, "cutoff") == 0) {
		return &(d->cutoff);
	}
	if(strcmp(name, "resonance") == 0) {
		return &(d->resonance);
	}
	if(strcmp(name, "filter") == 0) {
		return &(d->filter_type);
	}
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	delay_t *d = (delay_t *)data;

	SignalPointer *s = mt->get_input_signal(mt, "Mono");
	SignalPointer *os = mt->get_output_signal(mt, "Mono");

	if(os == NULL) return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);

	if(s == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < ol; t++) {
			ou[t] = itoFTYPE(0);
		}
		return;		
	}
	
	FTYPE *in = mt->get_signal_buffer(s);

	float freq = (float)mt->get_signal_frequency(os);

	int i;

	FTYPE amplitude = ftoFTYPE(d->amplitude);
	float delay_f = d->delay * freq;
	int delay = (int)delay_f;

	if(delay >= DELAY_LINE_LENGTH) delay = DELAY_LINE_LENGTH;
	
	for(i = 0; i < ol; i++) {
		if(DO_FILTER_RECALC(d)) {
			if(d->filter_type == 1) 
				recalc_filter(d->lpf, d->cutoff, d->resonance);
			else if(d->filter_type == 2) 
				recalc_filter(d->hpf, d->cutoff, d->resonance);
		}
		STEP_FILTER_RECALC(d);		

		if(d->filter_type == 1) {
			xPassFilterMonoPut(d->lpf, d->mem);
			d->mem = xPassFilterMonoGet(d->lpf);
		}
		if(d->filter_type == 2) {
			xPassFilterMonoPut(d->hpf, d->mem);
			d->mem = xPassFilterMonoGet(d->hpf);
		}
		
		ou[i] = in[i] + d->mem;

		delayLineMonoPut(d->dl0, mulFTYPE(ou[i], amplitude));
		d->mem = delayLineMonoGetOffset(d->dl0, delay);
	}	
}
