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

/* $Id: mono2stereo.c,v 1.2 2008/09/14 20:48:18 pltxtra Exp $ */

#include "dynlib.h"
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <fixedpointmath.h>

typedef struct _mono2stereo {
	float pan;
} mono2stereo_t;

void *init(MachineTable *mt, const char *name) {
	mono2stereo_t *retval = NULL;

	if((retval = (mono2stereo_t *)malloc(sizeof(mono2stereo_t))) != NULL) {
		memset(retval, 0, sizeof(mono2stereo_t));
		retval->pan = 1.0;
	}
	
	return retval;
}
 
void *get_controller_ptr(MachineTable *mt, void *data,
			 const char *name,
			 const char *group) {
	mono2stereo_t *m2s = (mono2stereo_t *)data;
	if(m2s) {
		if(strcmp("pan", name) == 0) {
			return &(m2s->pan);
		}
	}
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	// nothing to be done
}

void execute(MachineTable *mt, void *data) {
	mono2stereo_t *m2s = (mono2stereo_t *)data;
	SignalPointer *outsig = NULL;
	SignalPointer *insig = NULL;

	outsig = mt->get_output_signal(mt, "Stereo");
	if(outsig == NULL)
		return;

	FTYPE *out =
		(FTYPE *)mt->get_signal_buffer(outsig);
	int out_l = mt->get_signal_samples(outsig);

	insig = mt->get_input_signal(mt, "Mono");
	if(insig == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < out_l; t++) {
			out[t * 2 + 0] = itoFTYPE(0);
			out[t * 2 + 1] = itoFTYPE(0);
		}
		return;
	}
	
	FTYPE *in =
		(FTYPE *)mt->get_signal_buffer(insig);
	int in_l = mt->get_signal_samples(insig);

	if(in_l != out_l)
		return; // we expect equal lengths..

	int t;

	float l_f_ = m2s->pan > 1.0 ? (2.0 - m2s->pan) : 1.0;
	float r_f_ = m2s->pan < 1.0 ? (m2s->pan) : 1.0;

	FTYPE l_f = ftoFTYPE(l_f_);
	FTYPE r_f = ftoFTYPE(r_f_);
	
	for(t = 0; t < out_l; t++) {
		out[t * 2 + 0] = mulFTYPE(in[t], l_f);
		out[t * 2 + 1] = mulFTYPE(in[t], r_f);
	}
}

void delete(void *data) {
	if(data != NULL) free(data);
}
