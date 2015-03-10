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

// keys and voices must be equal size...
#define SUBASTARD_NR_VOICES 6
#define SUBASTARD_FILTER_RECALC_PERIOD 1024

// +4 since we need four "spare" ones for VCO modulation
// +24 since we can set the "vco_X_range" from 0 to +24
#define NOTE_TABLE_LENGTH 256 + 4 + 24
FTYPE note_table[NOTE_TABLE_LENGTH];

typedef struct subastard_voice {
	FTYPE velocity;
	
	int filter_recalc_step;

	oscillator_t vco_1;
	oscillator_t vco_2;

	xPassFilterMono_t *lpf;
	
	envelope_t env1;
	envelope_t env2;

	int active_key;
} subastardVoice_t;


typedef struct subastard_instance {
	subastardVoice_t voice[SUBASTARD_NR_VOICES];

	FTYPE general_mix;
	FTYPE general_volume;
	
	int midi_channel;

	int vco_1_sin;
	int vco_1_saw;
	int vco_1_pulse;
	FTYPE vco_1_begin_multiplier;
	FTYPE vco_1_freq_slide;

	int vco_2_sin;
	int vco_2_saw;
	int vco_2_pulse;
	FTYPE vco_2_begin_multiplier;
	FTYPE vco_2_freq_slide;
	
	FTYPE env1_attack;
	FTYPE env1_decay;
	FTYPE env1_sustain;
	FTYPE env1_release;
	
	FTYPE env2_attack;
	FTYPE env2_decay;
	FTYPE env2_sustain;
	FTYPE env2_release;

	FTYPE vcf_cutoff;
	FTYPE vcf_resonance;
	FTYPE vcf_env;
} subastard_t;

int init_voice(MachineTable *mt, subastardVoice_t *v) {
	los_init_oscillator(&(v->vco_1));
	los_init_oscillator(&(v->vco_2));

	v->lpf = create_xPassFilterMono(mt, 0);
	if(v->lpf == NULL) return -1;

	return 0;
}

void free_voice(subastardVoice_t *v) {
	if(v->lpf) xPassFilterMonoFree(v->lpf);
}

void delete(void *void_subastard) {
	subastard_t *subastard = (subastard_t *)void_subastard;
	int k;
	for(k = 0; k < SUBASTARD_NR_VOICES; k++) {
		free_voice(&(subastard->voice[k]));
	}

	/* free instance data here */
	free(subastard);
}

void reset(MachineTable *mt, void *void_subastard) {
	subastard_t *subastard = (subastard_t *)void_subastard;

	int l;
	for(l = 0; l < SUBASTARD_NR_VOICES; l++) {
		subastardVoice_t *v = &(subastard->voice[l]);

		envelope_reset(&(v->env1));
		envelope_reset(&(v->env2));
		v->active_key = -1;
	}

}

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	subastard_t *subastard = (subastard_t *)malloc(sizeof(subastard_t));;
	if(!subastard) return NULL;
	
	memset(subastard, 0, sizeof(subastard_t));

	int k;
	for(k = 0; k < SUBASTARD_NR_VOICES; k++) {
		if(init_voice(mt, &(subastard->voice[k]))) {
			goto fail;
		}
	}
	
	/* set defaults */
	subastard->general_mix = ftoFTYPE(0.5f);
	subastard->general_volume = ftoFTYPE(0.4f);

	subastard->env1_attack = ftoFTYPE(0.01f);
	subastard->env1_decay = ftoFTYPE(0.01f);
	subastard->env1_sustain = ftoFTYPE(0.5f);
	subastard->env1_release = ftoFTYPE(0.2f);

	subastard->env2_attack = ftoFTYPE(0.01f);
	subastard->env2_decay = ftoFTYPE(0.01f);
	subastard->env2_sustain = ftoFTYPE(0.5f);
	subastard->env2_release = ftoFTYPE(0.2f);

	subastard->vco_1_sin = 1;
	subastard->vco_1_saw = 0;
	subastard->vco_1_pulse = 0;
	subastard->vco_1_begin_multiplier = ftoFTYPE(4.0f);
	subastard->vco_1_freq_slide = ftoFTYPE(0.4f);

	subastard->vco_2_sin = 1;
	subastard->vco_2_saw = 0;
	subastard->vco_2_pulse = 0;
	subastard->vco_2_begin_multiplier = ftoFTYPE(4.0f);
	subastard->vco_2_freq_slide = ftoFTYPE(0.4f);

	subastard->vcf_cutoff = ftoFTYPE(0.4f);
	subastard->vcf_resonance = ftoFTYPE(0.1f);
	subastard->vcf_env = ftoFTYPE(0.4f);
	
	/* reset */
	reset(mt, subastard);
	
	/* return pointer to instance data */
	return (void *)subastard;

fail:
	delete(subastard);

	return NULL;
}
 
void *get_controller_ptr(MachineTable *mt, void *void_subastard,
			 const char *name,
			 const char *group) {
	subastard_t *subastard = (subastard_t *)void_subastard;

	if(strcmp("General", group) == 0) {
		if(strcmp("Volume", name) == 0) return &(subastard->general_volume);
		if(strcmp("Mix", name) == 0) return &(subastard->general_mix);
	}

	if(strcmp("VCO-1", group) == 0) {
		if(strcmp("Sin", name) == 0) return &(subastard->vco_1_sin);
		if(strcmp("Saw", name) == 0) return &(subastard->vco_1_saw);
		if(strcmp("Pulse", name) == 0) return &(subastard->vco_1_pulse);
		if(strcmp("Begin", name) == 0) return &(subastard->vco_1_begin_multiplier);
		if(strcmp("Slide", name) == 0) return &(subastard->vco_1_freq_slide);
	}

	if(strcmp("VCO-2", group) == 0) {
		if(strcmp("Sin", name) == 0) return &(subastard->vco_2_sin);
		if(strcmp("Saw", name) == 0) return &(subastard->vco_2_saw);
		if(strcmp("Pulse", name) == 0) return &(subastard->vco_2_pulse);
		if(strcmp("Begin", name) == 0) return &(subastard->vco_2_begin_multiplier);
		if(strcmp("Slide", name) == 0) return &(subastard->vco_2_freq_slide);
	}

	if(strcmp("ENV-1", group) == 0) {
		if(strcmp("Attack", name) == 0) return &(subastard->env1_attack);
		if(strcmp("Decay", name) == 0) return &(subastard->env1_decay);
		if(strcmp("Sustain", name) == 0) return &(subastard->env1_sustain);
		if(strcmp("Release", name) == 0) return &(subastard->env1_release);
	}

	if(strcmp("ENV-2", group) == 0) {
		if(strcmp("Attack", name) == 0) return &(subastard->env2_attack);
		if(strcmp("Decay", name) == 0) return &(subastard->env2_decay);
		if(strcmp("Sustain", name) == 0) return &(subastard->env2_sustain);
		if(strcmp("Release", name) == 0) return &(subastard->env2_release);
	}

	if(strcmp("VCF", group) == 0) {
		if(strcmp("Cutoff", name) == 0) return &(subastard->vcf_cutoff);
		if(strcmp("Resonance", name) == 0) return &(subastard->vcf_resonance);
		if(strcmp("Env", name) == 0) return &(subastard->vcf_env);
	}

	return NULL;
}

void calc_note_freq() {
	/* note table */
	int n;
	for(n = 0; n < NOTE_TABLE_LENGTH; n++) {
		float f;
		f = 440.0 * pow(2.0, (n - 69) / 12.0);
		note_table[n] = LOS_CALC_FREQUENCY(f);
	}
}

inline void trigger_voice_envelope(envelope_t *env, int Fs,
				   FTYPE a, FTYPE d, FTYPE s, FTYPE r) {
	envelope_init(env, Fs);
	
	envelope_set_attack(env, a);
	envelope_set_hold(env, ftoFTYPE(0.001));
	envelope_set_decay(env, d);
	envelope_set_sustain(env, s);
	envelope_set_release(env, r);

	envelope_hold(env);

}

inline void set_voice_to_key(subastard_t *subastard, subastardVoice_t *voice, int key) {
	FTYPE frequency_end = note_table[key];
	FTYPE frequency_1_begin = mulFTYPE(frequency_end, subastard->vco_1_begin_multiplier);
	FTYPE frequency_2_begin = mulFTYPE(frequency_end, subastard->vco_2_begin_multiplier);

	los_set_base_frequency(
		&(voice->vco_1),
		frequency_1_begin);
	
	los_set_base_frequency(
		&(voice->vco_2),
		frequency_2_begin);
	
	los_glide_frequency(
		&(voice->vco_1),
		frequency_end,
		subastard->vco_1_freq_slide);
	
	los_glide_frequency(
		&(voice->vco_2),
		frequency_end,
		subastard->vco_2_freq_slide
		);
}

void execute(MachineTable *mt, void *void_subastard) {
	subastard_t *subastard = (subastard_t *)void_subastard;

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
	
	FTYPE *out =
		(FTYPE *)mt->get_signal_buffer(outsig);
	int out_l = mt->get_signal_samples(outsig);
	int Fs = mt->get_signal_frequency(outsig);

#ifdef THIS_IS_A_MOCKERY
	SignalPointer *int_sig_a = NULL;
	SignalPointer *int_sig_b = NULL;

	int_sig_a = mt->get_mocking_signal(mt, "A");
	int_sig_b = mt->get_mocking_signal(mt, "B");
	FTYPE *int_out_a =
		(FTYPE *)mt->get_signal_buffer(int_sig_a);
	FTYPE *int_out_b =
		(FTYPE *)mt->get_signal_buffer(int_sig_b);
#endif
	
	los_set_Fs(mt, Fs);
	libfilter_set_Fs(Fs);

	static int Fs_CURRENT = 0;
	if(Fs_CURRENT != Fs) {
		Fs_CURRENT = Fs;
		calc_note_freq(Fs);
	}
	
	FTYPE vcf_cutoff = mulFTYPE(subastard->vcf_cutoff, ftoFTYPE(0.45)); // 45% of Fs
	FTYPE vcf_resonance = subastard->vcf_resonance;
	FTYPE vcf_env = subastard->vcf_env;

	FTYPE env1_a, env1_d, env1_s, env1_r;
	env1_a = subastard->env1_attack;
	env1_d = subastard->env1_decay;
	env1_s = subastard->env1_sustain;
	env1_r = subastard->env1_release;

	FTYPE env2_a, env2_d, env2_s, env2_r;
	env2_a = subastard->env2_attack;
	env2_d = subastard->env2_decay;
	env2_s = subastard->env2_sustain;
	env2_r = subastard->env2_release;

	FTYPE mix_VCO_1, mix_VCO_2, mix_d;
	mix_d = (subastard->general_mix);
	mix_d = mulFTYPE(itoFTYPE(2), mix_d) - itoFTYPE(1);
	if(mix_d < itoFTYPE(0)) {
		mix_VCO_1 = itoFTYPE(1);
		mix_VCO_2 = mix_d + itoFTYPE(1);
	} else {
		mix_VCO_2 = itoFTYPE(1);
		mix_VCO_1 = itoFTYPE(1) - mix_d;
	}

	FTYPE volume = (subastard->general_volume);

	int t; 
	for(t = 0; t < out_l; t++) {		
		MidiEvent *mev = (MidiEvent *)midi_in[t];
		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_ON)
		   &&
		   ((mev->data[0] & 0x0f) == subastard->midi_channel)
			) {
			int key = mev->data[1];
			int velocity = mev->data[2];
			FTYPE velocity_f;
#ifdef __SATAN_USES_FXP
			velocity_f = ((velocity & 0xff) << 16);
#else
			velocity_f = ((float)velocity / 127.0f);
#endif

			int voice_id;
			for(voice_id = 0; voice_id < SUBASTARD_NR_VOICES; voice_id++) {
				if(subastard->voice[voice_id].active_key == -1) {
					// set the active key of this voice
					subastard->voice[voice_id].active_key = key;

					// set velocity
					subastard->voice[voice_id].velocity = velocity_f;
					
					// trigger the envelope
					trigger_voice_envelope(&(subastard->voice[voice_id].env1),
							       Fs,
							       env1_a, env1_d, env1_s, env1_r);
					trigger_voice_envelope(&(subastard->voice[voice_id].env2),
							       Fs,
							       env2_a, env2_d, env2_s, env2_r);

					// set the frequencies for the oscillators
					set_voice_to_key(subastard,
							 &(subastard->voice[voice_id]),
							 key
						);
						
					// exit for loop
					break;
				}
			}
		}
		
		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_OFF)
		   &&
		   ((mev->data[0] & 0x0f) == subastard->midi_channel)
			) {
			int key = mev->data[1];
			int voice_id;
			for(voice_id = 0; voice_id < SUBASTARD_NR_VOICES; voice_id++) {
				if(subastard->voice[voice_id].active_key == key) {
					// mark this voice as unused
					subastard->voice[voice_id].active_key = -1;

					// release envelope
					envelope_release(
						&(subastard->voice[voice_id].env1));
					envelope_release(
						&(subastard->voice[voice_id].env2));
				}
			}						
		}

		// set the current output sample to 0
		out[t] = itoFTYPE(0);

		// process voices
		{
			int l;
			for(l = 0; l < SUBASTARD_NR_VOICES; l++) {
				subastardVoice_t *v = &(subastard->voice[l]);

				// if both envelopes are non-active, skip processing this voice
				if(
					(!envelope_is_active(&(v->env1)))
					&&
					(!envelope_is_active(&(v->env2)))
					) {
					break;
				} 
				
				// otherwise, process it hereafter!
				
				FTYPE vco_1 = itoFTYPE(0), vco_2 = itoFTYPE(0);
				FTYPE env1 = itoFTYPE(0);
				FTYPE env2 = itoFTYPE(0);

				{ // process ENV1 and ENV2
					env1 = envelope_get_sample(&(v->env1));
					envelope_step(&(v->env1));
					env2 = envelope_get_sample(&(v->env2));
					envelope_step(&(v->env2));
				}
				
				{ // process VCO-2
					FTYPE saw = los_get_alternate_sample(&(v->vco_2), los_src_saw);
					if(subastard->vco_2_sin)
						vco_2 += los_get_alternate_sample(&(v->vco_2), los_src_sin);
					if(subastard->vco_2_saw)
						vco_2 += saw;
					if(subastard->vco_2_pulse) {
						vco_2 += los_get_alternate_sample(
							&(v->vco_2), los_src_square);
					}
					
					los_step_oscillator(&(v->vco_2), itoFTYPE(0), itoFTYPE(0));
				}

				{ // process VCO-1
					FTYPE saw = los_get_alternate_sample(&(v->vco_1), los_src_saw);
					if(subastard->vco_1_sin)
						vco_1 += los_get_alternate_sample(&(v->vco_1), los_src_sin);
					if(subastard->vco_1_saw)
						vco_1 += saw;
					if(subastard->vco_1_pulse) {
						vco_1 += los_get_alternate_sample(
							&(v->vco_1), los_src_square);
					}
					los_step_oscillator(&(v->vco_1), itoFTYPE(0), itoFTYPE(0));
				}

				{ // process VCF parameters
					FTYPE cutoff;

					if(v->filter_recalc_step == 0) {
						cutoff = mulFTYPE(vcf_cutoff,
								  itoFTYPE(1) - vcf_env + mulFTYPE(env1, vcf_env));
					       

						xPassFilterMonoSetCutoff(v->lpf, cutoff);
						xPassFilterMonoSetResonance(v->lpf, vcf_resonance);
						xPassFilterMonoRecalc(v->lpf);
					}
				}

				{ // process end result
#ifdef THIS_IS_A_MOCKERY
					if(l == 0) {
						int_out_a[t] = mulFTYPE(ftoFTYPE(0.9f), vco_1);
						int_out_b[t] = mulFTYPE(ftoFTYPE(0.9f), vco_2);
					}
#endif
					
					vco_1 = mulFTYPE(vco_1, env1);
					vco_2 = mulFTYPE(vco_2, env2);
					FTYPE output_v = mulFTYPE(
						volume,
						mulFTYPE(mix_VCO_1, vco_1) + mulFTYPE(mix_VCO_2, vco_2)
						);

					output_v = mulFTYPE(output_v, v->velocity);

					xPassFilterMonoPut(v->lpf, output_v);
					output_v = xPassFilterMonoGet(v->lpf);
					
					out[t] += output_v;
				}
				v->filter_recalc_step = (v->filter_recalc_step + 1) % SUBASTARD_FILTER_RECALC_PERIOD;
			}
		}
	}
}
