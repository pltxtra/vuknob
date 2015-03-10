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

/* frequency parameters of type FTYPE are given as percentage of sampling frequency, or f / Fs.
 *
 * so, 500Hz given an Fs of 44100Hz should be (double)~0.01133786848072562358.
 *
 * use the LOS_CALC_FREQUENCY macro for convenience.
 */

#ifndef __HAVE_LIBOSCILLATOR_INCLUDED__
#define __HAVE_LIBOSCILLATOR_INCLUDED__

#include "dynlib.h"

#include "liboscillator_internal.h"

typedef struct __struct_oscillator oscillator_t;

#define HERTZ(x) (x)
#define LOS_CALC_FREQUENCY(x) ftoFTYPE((float)(x * __LOS_Fs_INVERSE ))

// this MUST be called first to set the sample frequency to be used!
inline void los_set_Fs(MachineTable *mt, int newFS);

// oscillator source functions
FTYPE los_src_saw(FTYPE voltage); // saw
FTYPE los_src_sin(FTYPE voltage); // sin
FTYPE los_src_cos(FTYPE voltage); // cos
FTYPE los_src_coshalf(FTYPE voltage); // cos (half wave)
FTYPE los_src_square(FTYPE voltage); // square wave
FTYPE los_src_noise(FTYPE voltage); // noise ("white")

inline void los_set_oscillator_source(oscillator_t *oscillator, FTYPE (*)(FTYPE voltage));
inline void los_set_base_frequency(oscillator_t *oscillator, FTYPE frequency);
inline void los_glide_frequency(oscillator_t *oscillator, FTYPE target_frequency, FTYPE time);
inline void los_set_frequency_modulator_depth(oscillator_t *oscillator, FTYPE frequency_modulator_depth);
inline void los_set_amplitude_modulator_depth(oscillator_t *oscillator, FTYPE amplitude_modulator_depth);

// get a sample from the oscillator
inline FTYPE los_get_sample(oscillator_t *oscillator);
// get a sample from the oscillator, using the alternate source function
inline FTYPE los_get_alternate_sample(oscillator_t *oscillator, FTYPE (*alternate_source)(FTYPE));
// step the oscillator one time step
inline void los_step_oscillator(oscillator_t *oscillator,
				FTYPE frequency_modulator_voltage,
				FTYPE amplitude_modulator_voltage);

// reset period to zero
inline void los_reset_oscillator(oscillator_t *oscillator);

#endif
