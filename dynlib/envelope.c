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

typedef struct _EnvelopeData {
	envelope_t env_1;

	FTYPE env_1_attack;
	FTYPE env_1_hold;
	FTYPE env_1_decay;
	FTYPE env_1_sustain;
	FTYPE env_1_release;
	int env_1_polarity;
} EnvelopeData;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	EnvelopeData *d = (EnvelopeData *)malloc(sizeof(EnvelopeData));
	memset(d, 0, sizeof(EnvelopeData));

	d->env_1_attack = ftoFTYPE(0.0f);
	d->env_1_hold = ftoFTYPE(0.1f);
	d->env_1_decay = ftoFTYPE(0.1f);
	d->env_1_sustain = ftoFTYPE(0.5f);
	d->env_1_release = ftoFTYPE(0.945f);
	
	/* return pointer to instance data */
//NO_FAIL:
	return (void *)d;
}
 
void delete(void *data) {
	EnvelopeData *d = (EnvelopeData *)data;
	
/* free instance data here */
	free(d);
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	EnvelopeData *d = (EnvelopeData *)void_info;
	
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

inline void trigger_voice_envelope(envelope_t *env, int Fs,
				   FTYPE a, FTYPE h, FTYPE d, FTYPE s, FTYPE r) {
	envelope_init(env, Fs);
	
	envelope_set_attack(env, a);
	envelope_set_hold(env, h);
	envelope_set_decay(env, d);
	envelope_set_sustain(env, s);
	envelope_set_release(env, r);

	envelope_hold(env);
}

void execute(MachineTable *mt, void *data) {
	EnvelopeData *d = (EnvelopeData *)data;

	SignalPointer *os = mt->get_output_signal(mt, "Mono");

	if(os == NULL) return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);
	int Fs = mt->get_signal_frequency(os);
	float Fs_i = 1.0f / ((float)Fs);
	
	int i;

	FTYPE env1_a, env1_d, env1_h, env1_s, env1_r;
	env1_a = d->env_1_attack;
	env1_h = d->env_1_hold;
	env1_d = d->env_1_decay;
	env1_s = d->env_1_sustain;
	env1_r = d->env_1_release;
	
	static int kounter = 0;
	if(kounter == 0)
		trigger_voice_envelope(&(d->env_1),
				       Fs,
				       env1_a, env1_h, env1_d, env1_s, env1_r);
	
	for(i = 0; i < ol; i++) {
		FTYPE env_1 = itoFTYPE(0);
		
		{ // process ENV 1
			env_1 = envelope_get_sample(&(d->env_1));
			envelope_step(&(d->env_1));
			ou[i] = mulFTYPE(ftoFTYPE(0.77), env_1);
		}

		if(++kounter == 44100) {
			DYNLIB_DEBUG("released!\n");
			envelope_release(&(d->env_1));
		}
	}
}

