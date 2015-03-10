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

typedef struct _XEchoData {
	int echo_len;
	int offset;
	int bsize;
	float delay;
	float amplitude;
	float amp_left;
	float amp_right;
	FTYPE *buffer;
} XEchoData;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	XEchoData *d = (XEchoData *)malloc(sizeof(XEchoData));
	memset(d, 0, sizeof(XEchoData));
	d->delay = 0.25;
	d->amplitude = 0.5;
	d->echo_len = 3;
	d->amp_left = 0.75;
	d->amp_right = 0.25;
	/* return pointer to instance data */
	return (void *)d;
}
 
void delete(void *data) {
	XEchoData *d = (XEchoData *)data;
	/* free instance data here */
	free(d->buffer);
	free(d);
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	XEchoData *d = (XEchoData *)void_info;
	if(strcmp(name, "delay") == 0) {
		return &(d->delay);
	}
	if(strcmp(name, "amplitude") == 0) {
		return &(d->amplitude);
	}
	if(strcmp(name, "amp_left") == 0) {
		return &(d->amp_left);
	}
	if(strcmp(name, "amp_right") == 0) {
		return &(d->amp_right);
	}
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	XEchoData *d = (XEchoData *)data;

	SignalPointer *s = mt->get_input_signal(mt, "Mono");
	SignalPointer *s_stereo = mt->get_input_signal(mt, "Stereo");
	SignalPointer *os = mt->get_output_signal(mt, "Stereo");

	if(os == NULL) return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);
	int oc = mt->get_signal_channels(os);

	if(s == NULL && s_stereo == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < ol; t++) {
			ou[t * 2 + 0] = itoFTYPE(0);
			ou[t * 2 + 1] = itoFTYPE(0);
		}
		return;		
	}
	
	FTYPE *in = NULL;
	int il = 0;
	int ic = 0;
	int Fs = 0;

	FTYPE *in_s = NULL;
	int il_s = 0;
	int ic_s = 0;

	if(s != NULL) {
		in = mt->get_signal_buffer(s);
		il = mt->get_signal_samples(s);
		ic = mt->get_signal_channels(s);
		Fs = mt->get_signal_frequency(s);
	}
	if(s_stereo != NULL) {
		in_s = mt->get_signal_buffer(s_stereo);
		il_s = mt->get_signal_samples(s_stereo);
		ic_s = mt->get_signal_channels(s_stereo);
		Fs = mt->get_signal_frequency(s_stereo);
	}
	
	int c;
	int i;
	int p1, p2;

	int bsize = sizeof(FTYPE) * 500000; // "enough bytes".. (>44100*5 ..)
	if(d->buffer == NULL) {
		d->bsize = bsize;
		d->buffer = (FTYPE *)malloc(bsize);
		memset(d->buffer, 0, bsize);
	} else if(d->bsize != bsize) {
		d->bsize = bsize;
		d->buffer = (FTYPE *)realloc(d->buffer, bsize);
		memset(d->buffer, 0, bsize);
	}

	FTYPE stereo_amp_left = ftoFTYPE(d->amp_left);
	FTYPE stereo_amp_right = ftoFTYPE(d->amp_right);
	
	FTYPE *bf = d->buffer;

	float delay_f = d->delay * (float)Fs;
	int delay = (int)delay_f;
	FTYPE delay_amplitude = ftoFTYPE(d->amplitude);
	// mix "now"
	if(il == 0 && il_s > 0) il = il_s; // this happens if the mono input is not connected.

	
	for(i = 0; i < il; i++) {
		c = 0;
		// mix "left"
		p1 = ((i + d->offset) % (delay * 5)) * oc + c;
		if(in != NULL)
			bf[p1] += in[i * ic];
		if(in_s != NULL)
			bf[p1] += mulFTYPE(in_s[i * ic_s], stereo_amp_left);
		p2 = ((i + 3 * delay + d->offset) % (delay * 5)) * oc + (1 - c);
		bf[p2] = mulFTYPE(bf[p1], delay_amplitude);

		c = 1;
		// mix "right"
		p1 = ((i + d->offset) % (delay * 5)) * oc + c;
		if(in_s != NULL)
			bf[p1] += mulFTYPE(in_s[i * ic_s + 1], stereo_amp_right);
		p2 = ((i + 3 * delay + d->offset) % (delay * 5)) * oc + (1 - c);
		bf[p2] = mulFTYPE(bf[p1], delay_amplitude);		
	}
	// mix "delayed"
	// echo-through
	int p;
		
	for(i = 0; i < ol; i++) {
		for(c = 0; c < oc; c++) {
			p = ((i + d->offset) % (delay * 5)) * oc + c;
			ou[i * oc + c] = bf[p];
			bf[p] = 0;		
		}
	}
	
	d->offset = (d->offset + il) % (delay * 5);
		
}

