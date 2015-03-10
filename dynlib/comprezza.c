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

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <math.h>
#include <fixedpointmath.h>
#include "dynlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI M_PI

USE_SATANS_MATH

typedef struct _XpData {
	float comp, dry;

	// target amplitude, current amplitude
	FTYPE target_a, current_a;

	float decay; // how fast will the amplitude decay, in seconds
	
	float freq;
	
} XpData;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	XpData *data = (XpData *)malloc(sizeof(XpData));
	memset(data, 0, sizeof(XpData));

	data->dry = 0.0;
	data->comp = -0.2;
	data->freq = 44100.0;

	data->decay = 0.25;
	
	SETUP_SATANS_MATH(mt);

	/* return pointer to instance data */
	return (void *)data;
}
 
void *get_controller_ptr(MachineTable *mt, void *data,
			 const char *name,
			 const char *group) {
	XpData *xd = (XpData *)data;

	if(strcmp("comp", name) == 0)
		return &(xd->comp);

	if(strcmp("dry", name) == 0)
		return &(xd->dry);

	if(strcmp("decay", name) == 0)
		return &(xd->decay);

	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	XpData *xd = (XpData *)data;

	SignalPointer *s = mt->get_input_signal(mt, "Stereo");
	SignalPointer *os = mt->get_output_signal(mt, "Stereo");

	if(os == NULL)
		return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);
	int oc = mt->get_signal_channels(os);

	if(s == NULL) {
		// just clear output, then return
		int t, c;
		for(t = 0; t < ol; t++) {
			for(c = 0; c < oc; c++) {
				ou[t * oc + c] = itoFTYPE(0);
			}
		}
		return;		
	}

		
	FTYPE *in = mt->get_signal_buffer(s);
	int ic = mt->get_signal_channels(s);
	
	xd->freq = mt->get_signal_frequency(os);

	int c;
	int i;

	FTYPE d, dry_fp, comp_fp;
	dry_fp = ftoFTYPE(xd->dry);
	comp_fp = ftoFTYPE(xd->comp);

	float decay_samples_f = 1.0f / ((float)(xd->decay) * (xd->freq));

	FTYPE decay_samples = ftoFTYPE(decay_samples_f);

	FTYPE a_step_per_sample;

	for(i = 0; i < ol; i++) {
		for(c = 0; c < oc; c++) {
			d = in[i * ic + c];
			if(d < 0) d = -d;
		   
			xd->target_a = d;

			if(xd->target_a > xd->current_a) {
				xd->current_a = xd->target_a;
				a_step_per_sample = ftoFTYPE(0.0);
			} else {
				a_step_per_sample = mulFTYPE(-(xd->current_a), decay_samples);
			}

			
			d = 
				mulFTYPE(in[i * ic + c], dry_fp) +
				mulFTYPE(
					(itoFTYPE(1) - dry_fp),
					mulFTYPE(
						in[i * ic + c],
						SAT_POW_FTYPE(xd->current_a, comp_fp))
					);
						
			ou[i * oc + c] = d;

			xd->current_a = xd->current_a + a_step_per_sample;
		}
	}
}

void delete(void *data) {
	/* free instance data here */
	free(data);
}
