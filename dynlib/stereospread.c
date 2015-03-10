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

/* $Id$ */

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

typedef struct _StereospreadData {
	FTYPE delay_line[DELAY_LINE_LEN];
	int offset;

	float delay;
	int invert;
} StereospreadData;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	StereospreadData *d = (StereospreadData *)malloc(sizeof(StereospreadData));
	memset(d, 0, sizeof(StereospreadData));
	/* return pointer to instance data */
	return (void *)d;
}
 
void delete(void *data) {
	StereospreadData *d = (StereospreadData *)data;
	/* free instance data here */
	free(d);
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	StereospreadData *d = (StereospreadData *)void_info;
	if(strcmp(name, "delay") == 0) {
		return &(d->delay);
	}
	if(strcmp(name, "invert") == 0) {
		return &(d->invert);
	}
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	StereospreadData *d = (StereospreadData *)data;

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
	int Fs = mt->get_signal_frequency(s);
	
	int c;
	int i, k_now, k_later;
	
	float delay_f = d->delay * (float)Fs;
	int delay = (int)delay_f;

	if(delay < 0) {
		c = 0;
		delay = -delay;
	} else c = 1;
	
	for(i = 0; i < ol; i++) {
		k_now = ((i + d->offset) % DELAY_LINE_LEN);
		
		k_later = ((i + delay + d->offset) % DELAY_LINE_LEN);
		d->delay_line[k_later] =
			d->invert ?
			(-in[i * oc + c]) : (in[i * oc + c]);		

		ou[i * oc + c] =
			d->delay_line[k_now];
		ou[i * oc + 1 - c] =
			in[i * ic + 1 - c];
		
		d->delay_line[k_now] = 0;
		
	}

	d->offset = (d->offset + il) % (DELAY_LINE_LEN);
}

