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

struct __struct_oscillator {
	FTYPE voltage;
	FTYPE voltage_step_base;
	FTYPE glide_target_frequency;
	
	FTYPE voltage_step_glide_step;
	FTYPE voltage_glide_phase_step;
	FTYPE voltage_glide_phase_count;
	int glide_count; // we will only glide every 100 steps, this is used to count...
	// the reason we do this is that the ammount to glide per sample is too small to represent
	// using the 24 bits we have if the FTYPE is fp8p24
	
	FTYPE amplitude;
	FTYPE amplitude_base;

	struct __struct_oscillator *frequency_modulator;
	FTYPE frequency_modulator_depth; // in f / Fs
	
	struct __struct_oscillator *amplitude_modulator;
	FTYPE amplitude_modulator_depth;
	
	FTYPE (*oscillator_source)(FTYPE _voltage_source);
};

