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

//#define __DO_DYNLIB_DEBUG
#include "dynlib_debug.h"

#include <stdlib.h>

#include "dynlib.h"
USE_SATANS_MATH

#include "liboscillator.c"
#include "libfilter.c"
#include "libenvelope.c"

typedef struct _OscillatorData {
	oscillator_t vco_1;
	FTYPE frequency_1;
	FTYPE frequency_1_modulation;
} OscillatorData;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	OscillatorData *d = (OscillatorData *)malloc(sizeof(OscillatorData));
	memset(d, 0, sizeof(OscillatorData));

	los_init_oscillator(&(d->vco_1));

	d->frequency_1 = ftoFTYPE(0.001665f);
	d->frequency_1_modulation = ftoFTYPE(0.000433f);
       	
	/* return pointer to instance data */
//NO_FAIL:
	return (void *)d;
}
 
void delete(void *data) {
	OscillatorData *d = (OscillatorData *)data;
	
/* free instance data here */
	free(d);
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	OscillatorData *d = (OscillatorData *)void_info;

	if(strcmp(name, "frequency1") == 0) {
		return &(d->frequency_1);
	}
	
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	OscillatorData *d = (OscillatorData *)data;

	SignalPointer *os = mt->get_output_signal(mt, "Mono");

	if(os == NULL) return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);
	int Fs = mt->get_signal_frequency(os);
	float Fs_i = 1.0f / ((float)Fs);
	
	int i;

	static int kounter = 0;
	if(kounter == 0) {
		los_set_Fs(mt, Fs);
		los_set_frequency_modulator_depth(&(d->vco_1),
						  d->frequency_1_modulation);
		los_set_base_frequency(
			&(d->vco_1),
			d->frequency_1);
	}
	for(i = 0; i < ol; i++) {
		ou[i] = mulFTYPE(ftoFTYPE(0.1f), los_get_alternate_sample(&(d->vco_1), los_src_sawsmooth));
		los_step_oscillator(&d->vco_1, itoFTYPE(0), itoFTYPE(0));
/*		
		{ // process ENV 1
			env_1 = envelope_get_sample(&(d->env_1));
			envelope_step(&(d->env_1));
			ou[i] = mulFTYPE(ftoFTYPE(0.5), env_1);
		}
*/
	}
}

