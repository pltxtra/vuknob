/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2012 by Anton Persson
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

#include "libenvelope.h"

void envelope_reset(envelope_t *env) {
	env->amplitude = itoFTYPE(0);
	env->phase = 0; 
	env->phase_level = itoFTYPE(0);
}

void envelope_init(envelope_t *env, int Fs) {
	if(env->Fs != Fs) {
		env->Fs = Fs;

		float inverse = 1.0f / (float)Fs;

		env->Fs_i = ftoFTYPE(inverse);

		envelope_reset(env);
	}	
}

void envelope_set_attack(envelope_t *env, FTYPE seconds) {
	if(seconds == itoFTYPE(0))
		env->attack_level_step = itoFTYPE(1); // this will indicate that we skip the envelope attack phase
	else 
		env->attack_level_step = divFTYPE(env->Fs_i, seconds);
}

void envelope_set_hold(envelope_t *env, FTYPE seconds) {
	if(seconds == itoFTYPE(0))
		env->hold_level_step = itoFTYPE(1); // indicate that we skip the hold phase
	else
		env->hold_level_step = divFTYPE(env->Fs_i, seconds);
}

void envelope_set_decay(envelope_t *env, FTYPE seconds) {
	if(seconds == itoFTYPE(0))
		env->decay_level_step = itoFTYPE(1); // indicate that we skip the decay phase
	else
		env->decay_level_step = divFTYPE(env->Fs_i, seconds);
}

void envelope_set_release(envelope_t *env, FTYPE seconds) {
	if(seconds == itoFTYPE(0))
		env->release_level_step = itoFTYPE(1); // indicate that we skip the release phase
	else
		env->release_level_step = divFTYPE(env->Fs_i, seconds);
}

void envelope_set_sustain(envelope_t *env, FTYPE amplitude) {
	env->sustain_amplitude = amplitude;
}

void envelope_hold(envelope_t *env) {
	env->attack_amp_step = env->attack_level_step;

	// adapt the decay amp step to the sustain amplitude setting...
	env->decay_amp_step = -(env->decay_level_step);
	env->decay_amp_step = mulFTYPE(env->decay_amp_step, itoFTYPE(1) - env->sustain_amplitude);

	env->amplitude = itoFTYPE(0);

	if(env->attack_level_step == itoFTYPE(1) &&
	   env->hold_level_step == itoFTYPE(1) &&
	   env->decay_level_step == itoFTYPE(1)) {
		env->phase = 4;
		env->amplitude = env->sustain_amplitude;
	} else {
		env->phase = 1; // regular mode - start with the attack phase
	}
	env->phase_level = itoFTYPE(0);
}

void envelope_release(envelope_t *env) {
	env->phase = 5; // go to release phase
	env->phase_level = itoFTYPE(0);

	// adapt the decay and release amp steps to the current amplitude
	env->release_amp_step = -(env->release_level_step);
	env->release_amp_step = mulFTYPE(env->release_amp_step, env->amplitude);
}

FTYPE envelope_get_sample(envelope_t *env) {
	return env->amplitude;
}

inline void envelope_step(envelope_t *env) {
	switch(env->phase) {
	case 0: // no action		
		break;

	case 1: // attack phase
		env->amplitude += env->attack_amp_step;
		env->phase_level += env->attack_level_step;
		
		if(env->phase_level >= itoFTYPE(1)) {
			env->phase_level = itoFTYPE(0);
			env->phase = 2; // go to hold phase
		}
		break;

	case 2: // hold phase
		env->phase_level += env->hold_level_step;
		if(env->phase_level >= itoFTYPE(1)) {
			env->phase_level = itoFTYPE(0);
			env->phase = 3; // go to decay phase
		}
		break;

	case 3: // decay phase
		env->amplitude += env->decay_amp_step;
		env->phase_level += env->decay_level_step;
		
		if(env->phase_level >= itoFTYPE(1)) {
			env->phase_level = itoFTYPE(0);
			env->phase = 4; // go to sustain phase
		}
		break;

	case 4: // sustain phase
		// do nothing, wait for release
		break;
		
	case 5: // release phase
		env->amplitude += env->release_amp_step;
		env->phase_level += env->release_level_step;
		
		if(env->phase_level >= itoFTYPE(1)) {
			env->amplitude = itoFTYPE(0);
			env->phase_level = itoFTYPE(0);
			env->phase = 0; // go to "no action" phase
		}
		break;

	}
}

int envelope_is_active(envelope_t *env) {
	return env->phase == 0 ? 0 : -1; 
}
