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

#ifndef __HAVE_LIBENVELOPE_INCLUDED__
#define __HAVE_LIBENVELOPE_INCLUDED__

#include "dynlib.h"

typedef struct __libenvelope_struct {
	int Fs;
	FTYPE Fs_i; // 1.0f / Fs
	
	FTYPE amplitude; /* current generated amplitude between 0 and 1 */

	FTYPE phase_modulation_depth;
	
	int phase; // attack, hold, decay, sustain or release phase
	FTYPE phase_level; // zero at start of each phase, increasing by step. When 1 is reached, proceed to next phase. (sustain phase is never levt, until envelope_release is called..)
	
	FTYPE attack_level_step;
	FTYPE hold_level_step;	
	FTYPE decay_level_step;
	FTYPE release_level_step;

	FTYPE attack_amp_step;	
	FTYPE decay_amp_step;
	FTYPE release_amp_step;

	FTYPE sustain_amplitude; // amplitude level during sustain phase
} envelope_t;

inline void envelope_init(envelope_t *env, int Fs);
inline void envelope_reset(envelope_t *env); // reset to initial state (silence)

inline void envelope_set_attack(envelope_t *env, FTYPE seconds);
inline void envelope_set_hold(envelope_t *env, FTYPE seconds);
inline void envelope_set_decay(envelope_t *env, FTYPE seconds);
inline void envelope_set_release(envelope_t *env, FTYPE seconds);
inline void envelope_set_sustain(envelope_t *env, FTYPE amplitude); // amplitude between 0 and 1

inline void envelope_hold(envelope_t *env);
inline void envelope_release(envelope_t *env);

inline FTYPE envelope_get_sample(envelope_t *env);

inline void envelope_step(envelope_t *env); 

inline int envelope_is_active(envelope_t *env);

#endif
