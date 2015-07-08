/* hexter DSSI software synthesizer plugin
 * VuKNOB/SATAN port
 *
 * Copyright (C) 2004, 2009, 2011, 2012 Sean Bolton and others.
 * Copyright (C) 2014 by Anton Persson
 *
 * Portions of this file may have come from Peter Hanappe's
 * Fluidsynth, copyright (C) 2003 Peter Hanappe and others.
 * Portions of this file may have come from Chris Cannam and Steve
 * Harris's public domain DSSI example code.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

//#define __DO_DYNLIB_DEBUG
#include "dynlib_debug.h"

#include "dynlib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "hexter_src/satan_ladspa.h"

#include "hexter_src/hexter_types.h"
#include "hexter_src/hexter.h"
#include "hexter_src/hexter_synth.h"
#include "hexter_src/dx7_voice.h"
#include "hexter_src/dx7_voice_data.h"

void delete(void *data);

hexter_synth_t hexter_synth = {
	.initialized = 0
};

//#include "time_debug.c"
//TimeMeasure timebob;

void *init(MachineTable *mt, const char *name) {
	hexter_instance_t *instance;
	int i;

//	clear_stats(&timebob);

	if (!hexter_synth.initialized) {

		hexter_synth.instance_count = 0;
		hexter_synth.instances = NULL;
		hexter_synth.note_id = 0;
		hexter_synth.global_polyphony = HEXTER_DEFAULT_POLYPHONY;

		for (i = 0; i < HEXTER_MAX_POLYPHONY; i++) {
			hexter_synth.voice[i] = dx7_voice_new();
			if (!hexter_synth.voice[i]) {
				DEBUG_MESSAGE(-1, " hexter_instantiate: out of memory!\n");
				delete(NULL);
				return NULL;
			}
		}

		hexter_synth.initialized = 1;
	}

	instance = (hexter_instance_t *)calloc(1, sizeof(hexter_instance_t));
	if (!instance) {
		delete(NULL);
		return NULL;
	}
	instance->next = hexter_synth.instances;
	hexter_synth.instances = instance;
	hexter_synth.instance_count++;

	if (!(instance->patches = (dx7_patch_t *)malloc(128 * DX7_VOICE_SIZE_PACKED))) {
		DEBUG_MESSAGE(-1, " hexter_instantiate: out of memory!\n");
		delete(instance);
		return NULL;
	}

	instance->sample_rate = (float)44100.0f;
	dx7_eg_init_constants(instance);  /* depends on sample rate */

	instance->polyphony = HEXTER_DEFAULT_POLYPHONY;
	instance->monophonic = DSSP_MONO_MODE_OFF;
	instance->max_voices = instance->polyphony;
	instance->current_voices = 0;
	instance->last_key = 0;
	instance->current_program = 0;
	instance->overlay_program = -1;
	hexter_data_performance_init(instance->performance_buffer);
	hexter_data_patches_init(instance->patches);
	hexter_instance_select_program(instance, 0, 0);
	hexter_instance_init_controls(instance);

	instance->tuning = 440.0f;
	instance->volume = 1.0f;

	return (LADSPA_Handle)instance;
}

void *get_controller_ptr(MachineTable *mt, void *void_instance,
			 const char *name,
			 const char *group) {
	hexter_instance_t *instance = (hexter_instance_t *)void_instance;

	if(strcmp(group, "general") == 0) {
		if(strcmp(name, "program") == 0) {
			return &(instance->new_program);
		}
		if(strcmp(name, "volume") == 0) {
			return &(instance->volume);
		}
	}

	return NULL;
}

void reset(MachineTable *mt, void *void_instance) {
	hexter_instance_t *instance = (hexter_instance_t *)void_instance;
	hexter_instance_all_voices_off(instance);  /* stop all sounds immediately */
	instance->current_voices = 0;
	dx7_lfo_reset(instance);
}

void execute(MachineTable *mt, void *void_instance) {
	static int tables_initiated = 0;
	if(!tables_initiated) {
		tables_initiated = 1;
		dx7_voice_init_tables();
	}

	hexter_instance_t *instance = (hexter_instance_t *)void_instance;

	SignalPointer *outsig = NULL;
	SignalPointer *insig = NULL;

	insig = mt->get_input_signal(mt, "midi");
	if(insig == NULL)
		return;
	outsig = mt->get_output_signal(mt, "Mono");
	if(outsig == NULL)
		return;

	void **midi_in = (void **)mt->get_signal_buffer(insig);
	if(midi_in == NULL)
		return;

	int midi_l = mt->get_signal_samples(insig);

	FTYPE *out =
		(FTYPE *)mt->get_signal_buffer(outsig);
	int sample_count = mt->get_signal_samples(outsig);

       	if(midi_l != sample_count)
		return; // we expect equal lengths..

	int Fs = mt->get_signal_frequency(outsig);

	if(instance->sample_rate != Fs) {
		instance->sample_rate = (float)Fs;
		dx7_eg_init_constants(instance);  /* depends on sample rate */
		hexter_instance_all_voices_off(instance);  /* stop all sounds immediately */
		instance->current_voices = 0;
		dx7_lfo_reset(instance);
	}

	if(instance->new_program != instance->current_program) {
		hexter_instance_select_program(instance,
					       0 /* the bank is ignored */,
					       instance->new_program);
	}

#ifdef __SATAN_USES_FXP
	static int temp_buffer_size = 0;
	static float *temp_buffer = NULL;
	if(temp_buffer == NULL || temp_buffer_size != sample_count) {
		if(temp_buffer != NULL) free(temp_buffer);

		temp_buffer = (float *)malloc(sizeof(float) * sample_count);
		if(temp_buffer == NULL) {
			mt->register_failure(instance, "Failed to allocate temporary buffer in dx7.c.");
			return;
		}
		temp_buffer_size = sample_count;
	}
	instance->output = temp_buffer;
#else
	instance->output = out;
#endif
	unsigned int samples_done = 0, samples_left = sample_count;
	MidiEvent *mev = NULL;

	// clear output buffer
	memset(instance->output, 0, sizeof(float) * sample_count);

	while(samples_left > 0) {
		unsigned int burst_size = samples_left < HEXTER_NUGGET_SIZE ? samples_left : HEXTER_NUGGET_SIZE;
		unsigned int k, k_max = samples_done + burst_size;

		for(k = samples_done; k < k_max; k++) {
			mev = midi_in[k];
			if(mev != NULL) {
				switch((mev->data[0] & 0xf0)) {
				case MIDI_NOTE_ON:
				{
					int note = mev->data[1];
					int velocity = mev->data[2];
//					DYNLIB_DEBUG("---------------------- HEXTER note on: %d, %d\n", note, velocity);
					hexter_instance_note_on(instance, note, velocity);
//					DYNLIB_DEBUG("---------------------- END note on: %d, %d\n", note, velocity);
				}
				break;
				case MIDI_NOTE_OFF:
				{
					int note = mev->data[1];
					int velocity = mev->data[2];
//					DYNLIB_DEBUG("---------------------- HEXTER note off: %d, %d (%d samples)\n", note, velocity, on_counter);
					hexter_instance_note_off(instance, note, velocity);
//					DYNLIB_DEBUG("---------------------- END note off: %d, %d\n", note, velocity);
				}
				break;
				case MIDI_CONTROL_CHANGE:
				{
					int param = mev->data[1];
					int value = mev->data[2];
					hexter_instance_control_change(instance, param, value);
				}
				break;
				default:
					DYNLIB_DEBUG("    NOT PROCESSED.\n");
					break;
				}
			}
		}

		/* render the burst */
//		start_measure(&timebob);
		hexter_synth_render_instance_voices(instance, samples_done, burst_size, 0 == 0);
//		stop_measure(&timebob);

#if 0
		if(timebob.last_time > 1000) {
			print_stats(&timebob);
			DYNLIB_DEBUG("   Failure: %p, %d, %d\n",
				     instance, samples_done, burst_size);
			exit(0);
		}
#endif

		samples_done += burst_size;
		samples_left -= burst_size;
	}

	// if __SATAN_USES_FXP we will have to do a time consuming float to fixed conversion here
	// currently..
#ifdef __SATAN_USES_FXP
	{
		int i;
		for(i = 0; i < sample_count; i++) {
			out[i] = ftoFTYPE(temp_buffer[i]);
		}
	}
#endif

	{
		out = &out[sample_count];
		char *bfr = (char *)out;
		if(strcmp(bfr, "THIS_PATTERN_IS_RIGHT") != 0) {
			DYNLIB_DEBUG("Verification pattern violated!.\n");
			DYNLIB_DEBUG("   pattern: %s\n", bfr);
			exit(0);
		}
	}
}

void delete(void *data) {
	hexter_instance_t *instance = (hexter_instance_t *)data;

	if (instance) {
		hexter_instance_t *inst, *prev;

		reset(NULL, instance);  /* stop all sounds immediately */

		prev = NULL;
		for (inst = hexter_synth.instances; inst; inst = inst->next) {
			if (inst == instance) {
				if (prev)
					prev->next = inst->next;
				else
					hexter_synth.instances = inst->next;
				break;
			}
			prev = inst;
		}
		hexter_synth.instance_count--;

		if (instance->patches) free(instance->patches);
		free(instance);

	}

	if (!hexter_synth.instance_count && hexter_synth.initialized) {
		int i;
		for (i = 0; i < HEXTER_MAX_POLYPHONY; i++) {
			if (hexter_synth.voice[i]) {
				free(hexter_synth.voice[i]);
				hexter_synth.voice[i] = NULL;
			}
		}

		hexter_synth.initialized = 0;
	}
}
