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

#ifndef __HAVE_LIBFILTER_MAIN_INCLUDED__
#define __HAVE_LIBFILTER_MAIN_INCLUDED__

#include <string.h>
#include "liboscillator.h"
#include <math.h>
#include <stdio.h>

int __LOS_Fs_integer = 1;
float __LOS_Fs_INVERSE = 1.0f;
FTYPE __LOS_Fs_INVERSE_FTYPE = ftoFTYPE(1.0f);

void los_set_Fs(MachineTable *mt, int new_Fs) {
	if(new_Fs != __LOS_Fs_integer) {
		__LOS_Fs_integer = new_Fs;
		__LOS_Fs_INVERSE = 1.0f / (float)new_Fs;
		__LOS_Fs_INVERSE_FTYPE = ftoFTYPE(__LOS_Fs_INVERSE);
		SETUP_SATANS_MATH(mt);
	}
}

void los_init_oscillator(oscillator_t *oscillator) {
	memset(oscillator, 0, sizeof(oscillator_t));
	oscillator->amplitude = oscillator->amplitude_base = itoFTYPE(1);
	oscillator->voltage_glide_phase_count = itoFTYPE(1);
}

void los_set_oscillator_source(oscillator_t *oscillator, FTYPE (*src)(FTYPE voltage)) {
	oscillator->oscillator_source = src;
}

void los_set_base_frequency(oscillator_t *oscillator, FTYPE frequency) {
	oscillator->voltage_step_base = frequency; // these so happens to be the same since frequency is (f / Fs)!
}

void los_glide_frequency(oscillator_t *oscillator, FTYPE target_frequency, FTYPE time) {
	if(time <= itoFTYPE(0) || oscillator->voltage_step_base == itoFTYPE(0)) {
		oscillator->voltage_step_base = target_frequency;
	} else {
		oscillator->glide_target_frequency = target_frequency;
		
		// because of resolutions issues with fixed point math (when FTYPE is fp8p24)
		// we can not step the voltage_step variable every sample, the step is too small
		// to be represented using fp8p24. To solve that we do a little non audible trick -
		// we step only every 100 samples. We divide the time variable by 100.
		// Then we use the glide_count variable to count the samples.
		oscillator->voltage_step_glide_step = 
			mulFTYPE(target_frequency - oscillator->voltage_step_base,
				 divFTYPE(__LOS_Fs_INVERSE_FTYPE, divFTYPE(time, itoFTYPE(100)))
				);
		oscillator->glide_count = 0;
		oscillator->voltage_glide_phase_count = itoFTYPE(0);
		oscillator->voltage_glide_phase_step = divFTYPE(__LOS_Fs_INVERSE_FTYPE, time);
	}
}

void los_set_frequency_modulator_depth(oscillator_t *oscillator, FTYPE frequency_modulator_depth) {
	oscillator->frequency_modulator_depth = frequency_modulator_depth;
}

void los_set_amplitude_modulator_depth(oscillator_t *oscillator, FTYPE amplitude_modulator_depth) {
	oscillator->amplitude_modulator_depth = divFTYPE(amplitude_modulator_depth, itoFTYPE(2));
	oscillator->amplitude_base = ftoFTYPE(1.0) - oscillator->amplitude_modulator_depth;
}

FTYPE los_get_sample(oscillator_t *oscillator) {
	return mulFTYPE(oscillator->amplitude, oscillator->oscillator_source(oscillator->voltage));
}

FTYPE los_get_alternate_sample(oscillator_t *oscillator,
			       FTYPE (*alternate_source)(FTYPE voltage)) {
	return mulFTYPE(oscillator->amplitude, alternate_source(oscillator->voltage));
}

void los_step_oscillator(oscillator_t *oscillator,
			 FTYPE frequency_modulator_voltage,
			 FTYPE amplitude_modulator_voltage) {
	if(oscillator->voltage_glide_phase_count < itoFTYPE(1)) {
		if(oscillator->glide_count == 0) {
			oscillator->voltage_step_base += oscillator->voltage_step_glide_step;
		}
		oscillator->voltage_glide_phase_count += oscillator->voltage_glide_phase_step;
		oscillator->glide_count =
			(oscillator->glide_count + 1) % 100;

		if(oscillator->voltage_glide_phase_count >= itoFTYPE(1)) {
			// ugly correction
			oscillator->voltage_step_base = oscillator->glide_target_frequency;
		}
	}
	
	FTYPE voltage_step =
		oscillator->voltage_step_base + mulFTYPE(
			frequency_modulator_voltage,
			oscillator->frequency_modulator_depth);

	oscillator->amplitude =
		oscillator->amplitude_base + mulFTYPE(
			amplitude_modulator_voltage,
			oscillator->amplitude_modulator_depth);
	
	oscillator->voltage += voltage_step;
	if(oscillator->voltage > itoFTYPE(1)) {
		oscillator->voltage -= itoFTYPE(1);
	}
}

void los_reset_oscillator(oscillator_t *oscillator) {
	oscillator->voltage = itoFTYPE(0);
}

FTYPE los_src_saw(FTYPE voltage) {
	return mulFTYPE(voltage, itoFTYPE(2)) - itoFTYPE(1);
}

FTYPE los_src_sin(FTYPE voltage) {
	return SAT_SIN_SCALAR_FTYPE(voltage);
}

FTYPE los_src_cos(FTYPE voltage) {
	return SAT_COS_SCALAR_FTYPE(voltage);
}

FTYPE los_src_sawsmooth(FTYPE voltage) {
	voltage = mulFTYPE(voltage, ftoFTYPE(0.5f));
	return SAT_COS_SCALAR_FTYPE(voltage);
}

FTYPE los_src_square(FTYPE voltage) {
	if(voltage < ftoFTYPE(0.5))
		return ftoFTYPE(-1.0);
	else
		return ftoFTYPE(1.0);
}

FTYPE los_src_noise(FTYPE voltage) {
#ifdef __SATAN_USES_FXP
	int x = rand();
	int k = x & 0x00ffffff;
	if(x & 0x10000000)
		return -k;
	return k;
#else
	return(((float)rand()) / ((float)RAND_MAX));
#endif
}

#else
#error "You have included liboscillator.c twice, that's bad for your health! The recommended dose is ONE unit of liboscillator.c"
#endif
