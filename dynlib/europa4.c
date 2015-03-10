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
#define E4_NR_VOICES 6
#define E4_NR_KEYS E4_NR_VOICES

// define how often we recalculate the filters
#define FILTER_RECALC_PERIOD 1024

// +4 since we need four "spare" ones for VCO modulation
// +24 since we can set the "vco_X_range" from 0 to +24
#define NOTE_TABLE_LENGTH 256 + 4 + 24
FTYPE note_table[NOTE_TABLE_LENGTH];

typedef struct europa4_voice {
	FTYPE velocity;

	FILTER_RECALC_PART;
	
	oscillator_t vco_1;
	oscillator_t vco_2;

	envelope_t env_1;
	envelope_t env_2;

	xPassFilterMono_t *lpf;
	xPassFilterMono_t *hpf;

	oscillator_t lfo;
	xPassFilterMono_t *lfo_lpf; // used to filter in noise mode

	FTYPE unison_detune;
	
	struct europa4_voice *next_voice;
} e4voice_t;

typedef struct europa4_key {
	FTYPE velocity;

	int key;
	
	e4voice_t *voice[E4_NR_VOICES]; // voices tied to this key

	int unison_voices; // how many voices are we using for this key, in unison mode
	
	struct europa4_key *next_key;
} e4key_t;

typedef struct europa4_instance {
	e4voice_t voice[E4_NR_VOICES];
	e4key_t key[E4_NR_KEYS]; // key "store"

	e4key_t *free_key; // non pressed keys
	e4key_t *active_key; // currently pressed keys

	// in unison mode we also need to chain voices
	e4voice_t *free_voice;
	
	FTYPE general_mix;
	FTYPE general_volume;
	int last_general_mode; // we must reset all voices in case we do a change of the general_mode parameter. We use the last_general_mode to keep track of this.
	int general_mode;
	FTYPE general_detune;

	FTYPE glide_time;
	int glide_mode;

	int vco_1_sin;
	int vco_1_sawsmooth;
	int vco_1_saw;
	int vco_1_pulse;
	int vco_1_pulse_mode;
	int vco_1_range;
	FTYPE vco_1_cross_mod_manual;
	FTYPE vco_1_cross_mod_env_1;

	int vco_2_sin;
	int vco_2_sawsmooth;
	int vco_2_saw;
	int vco_2_pulse;
	int vco_2_pulse_mode;
	int vco_2_range;

	int vcf_mode;
	FTYPE vcf_cutoff;
	FTYPE vcf_resonance;
	FTYPE vcf_env;
	int vcf_env_selector;
	FTYPE vcf_lfo;
	FTYPE vcf_kybd;

	int lfo_wave;
	FTYPE lfo_frequency;
	int lfo_sync_to_key_press;

	FTYPE vca_env_2;
	FTYPE vca_lfo;

	int vco_mod_vco_1;
	int vco_mod_vco_2;
	FTYPE vco_mod_lfo;
	FTYPE vco_mod_env_1;

	int pwm_selector;
	FTYPE pwm_pw;
	FTYPE pwm_pwm;

	FTYPE env_1_attack;
	FTYPE env_1_decay;
	FTYPE env_1_sustain;
	FTYPE env_1_release;
	int env_1_polarity;

	FTYPE env_2_attack;
	FTYPE env_2_decay;
	FTYPE env_2_sustain;
	FTYPE env_2_release;

	int midi_channel;
} europa4_t;

int init_voice(MachineTable *mt, e4voice_t *v) {
	los_init_oscillator(&(v->vco_1));
	los_init_oscillator(&(v->vco_2));
	los_init_oscillator(&(v->lfo));

	v->lpf = create_xPassFilterMono(mt, 0);
	if(v->lpf == NULL) return -1;
	v->hpf = create_xPassFilterMono(mt, 1);
	if(v->hpf == NULL) return -1;
	v->lfo_lpf = create_xPassFilterMono(mt, 1);
	if(v->lfo_lpf == NULL) return -1;
	return 0;
}

void free_voice(e4voice_t *v) {
	if(v->lpf) xPassFilterMonoFree(v->lpf);
	if(v->hpf) xPassFilterMonoFree(v->hpf);
	if(v->lfo_lpf) xPassFilterMonoFree(v->lfo_lpf);
}

void delete(void *void_europa4) {
	europa4_t *europa4 = (europa4_t *)void_europa4;
	int k;
	for(k = 0; k < E4_NR_VOICES; k++) {
		free_voice(&(europa4->voice[k]));
	}

	/* free instance data here */
	free(europa4);
}

#ifdef THIS_IS_A_MOCKERY
TimeMeasure_t *tmt_A = NULL;
TimeMeasure_t *tmt_B = NULL;
TimeMeasure_t *tmt_C = NULL;
TimeMeasure_t *tmt_D = NULL;
TimeMeasure_t *tmt_E = NULL;
TimeMeasure_t *tmt_F = NULL;
#endif

static FTYPE ctrl2cutoff_table[128];
static int ctrl2cutoff_created = 0;

void *init(MachineTable *mt, const char *name) {
	if(ctrl2cutoff_created == 0) {
		ctrl2cutoff_created = 1;

		int k;
		float y, x;
		for(k = 0; k < 128; k++) {
			x = (float)k; x /= 127.0f;
			y = sin(0.5f*(x + 3.0f) * M_PI) + 1.0f;
			ctrl2cutoff_table[k] = ftoFTYPE(y);
		}
	}
	
	/* Allocate and initiate instance data here */
	europa4_t *europa4 = (europa4_t *)malloc(sizeof(europa4_t));;
	if(!europa4) return NULL;

#ifdef THIS_IS_A_MOCKERY
	tmt_A = create_TimeMeasure("A Timer", 100);
	tmt_B = create_TimeMeasure("B Timer", 100);
	tmt_C = create_TimeMeasure("C Timer", 100);
	tmt_D = create_TimeMeasure("D Timer", 100);
	tmt_E = create_TimeMeasure("E Timer", 100);
	tmt_F = create_TimeMeasure("F Timer", 100);
#endif
	
	memset(europa4, 0, sizeof(europa4_t));

	int k;
	for(k = 0; k < E4_NR_VOICES; k++) {
		if(init_voice(mt, &(europa4->voice[k]))) {
			goto fail;
		}
	}

	// set up key chains
	for(k = 0; k < E4_NR_KEYS - 1; k++) {
		europa4->key[k].next_key = &(europa4->key[k + 1]);
	}
	europa4->key[k].next_key = NULL;
	europa4->free_key = &(europa4->key[0]);

	/* set defaults */
	europa4->general_mix = ftoFTYPE(0.5f);
	europa4->general_volume = ftoFTYPE(0.4f);
	europa4->general_detune = ftoFTYPE(0.3f);

	europa4->lfo_frequency = ftoFTYPE(0.5f);
	europa4->lfo_sync_to_key_press = 1;

	europa4->vca_env_2 = ftoFTYPE(1.0f);
	
	europa4->env_1_attack = ftoFTYPE(0.01f);
	europa4->env_1_decay = ftoFTYPE(0.01f);
	europa4->env_1_sustain = ftoFTYPE(0.5f);
	europa4->env_1_release = ftoFTYPE(0.2f);

	europa4->env_2_attack = ftoFTYPE(0.1f);
	europa4->env_2_decay = ftoFTYPE(0.1f);
	europa4->env_2_sustain = ftoFTYPE(0.5f);
	europa4->env_2_release = ftoFTYPE(0.2f);

	europa4->vco_1_saw = 1;
	europa4->vco_2_saw = 1;

	europa4->vcf_cutoff = ftoFTYPE(0.4f);
	europa4->vcf_resonance = ftoFTYPE(0.1f);
	europa4->vcf_env = ftoFTYPE(0.4f);
	europa4->vcf_env_selector = 1;
	europa4->vcf_mode = 1;

	europa4->glide_time = ftoFTYPE(0.1f);
	europa4->glide_mode = 2;

	europa4->pwm_pwm = ftoFTYPE(0.2f);
	europa4->pwm_pw = ftoFTYPE(0.5f);
	
	/* return pointer to instance data */
	return (void *)europa4;

fail:
	delete(europa4);

	return NULL;
}
 
void *get_controller_ptr(MachineTable *mt, void *void_europa4,
			 const char *name,
			 const char *group) {
	europa4_t *europa4 = (europa4_t *)void_europa4;
	if(strcmp("General", group) == 0) {
		if(strcmp("Mix", name) == 0) return &(europa4->general_mix);
		if(strcmp("Volume", name) == 0) return &(europa4->general_volume);
		if(strcmp("Mode", name) == 0) return &(europa4->general_mode);
		if(strcmp("Detune", name) == 0) return &(europa4->general_detune);
	}

	if(strcmp("Glide", group) == 0) {
		if(strcmp("Time", name) == 0) return &(europa4->glide_time);
		if(strcmp("Mode", name) == 0) return &(europa4->glide_mode);
	}

	if(strcmp("VCO-1", group) == 0) {
		if(strcmp("Sin", name) == 0) return &(europa4->vco_1_sin);
		if(strcmp("SawSmooth", name) == 0) return &(europa4->vco_1_sawsmooth);
		if(strcmp("Saw", name) == 0) return &(europa4->vco_1_saw);
		if(strcmp("Pulse", name) == 0) return &(europa4->vco_1_pulse);
		if(strcmp("Pulse Mode", name) == 0) return &(europa4->vco_1_pulse_mode);
		if(strcmp("Range", name) == 0) return &(europa4->vco_1_range);
		if(strcmp("Cross Mod Manual", name) == 0) return &(europa4->vco_1_cross_mod_manual);
		if(strcmp("Cross Mod Env 1", name) == 0) return &(europa4->vco_1_cross_mod_env_1);
	}

	if(strcmp("VCO-2", group) == 0) {
		if(strcmp("Sin", name) == 0) return &(europa4->vco_2_sin);
		if(strcmp("SawSmooth", name) == 0) return &(europa4->vco_2_sawsmooth);
		if(strcmp("Saw", name) == 0) return &(europa4->vco_2_saw);
		if(strcmp("Pulse", name) == 0) return &(europa4->vco_2_pulse);
		if(strcmp("Pulse Mode", name) == 0) return &(europa4->vco_2_pulse_mode);
		if(strcmp("Range", name) == 0) return &(europa4->vco_2_range);
	}

	if(strcmp("VCF", group) == 0) {
		if(strcmp("Mode", name) == 0) return &(europa4->vcf_mode);
		if(strcmp("Cutoff", name) == 0) return &(europa4->vcf_cutoff);
		if(strcmp("Resonance", name) == 0) return &(europa4->vcf_resonance);
		if(strcmp("Env", name) == 0) return &(europa4->vcf_env);
		if(strcmp("EnvSelector", name) == 0) return &(europa4->vcf_env_selector);
		if(strcmp("LFO", name) == 0) return &(europa4->vcf_lfo);
		if(strcmp("KYBD", name) == 0) return &(europa4->vcf_kybd);
	}

	if(strcmp("LFO", group) == 0) {
		if(strcmp("Wave", name) == 0) return &(europa4->lfo_wave);
		if(strcmp("Frequency", name) == 0) return &(europa4->lfo_frequency);
		if(strcmp("Sync to key press", name) == 0) return &(europa4->lfo_sync_to_key_press);
	}

	if(strcmp("VCA", group) == 0) {
		if(strcmp("ENV-2", name) == 0) return &(europa4->vca_env_2);
		if(strcmp("LFO", name) == 0) return &(europa4->vca_lfo);
	}

	if(strcmp("VCO MOD", group) == 0) {
		if(strcmp("VCO-1", name) == 0) return &(europa4->vco_mod_vco_1);
		if(strcmp("VCO-2", name) == 0) return &(europa4->vco_mod_vco_2);
		if(strcmp("LFO", name) == 0) return &(europa4->vco_mod_lfo);
		if(strcmp("ENV-1", name) == 0) return &(europa4->vco_mod_env_1);
	}

	if(strcmp("PWM", group) == 0) {
		if(strcmp("Selector", name) == 0) return &(europa4->pwm_selector);
		if(strcmp("PW", name) == 0) return &(europa4->pwm_pw);
		if(strcmp("PWM", name) == 0) return &(europa4->pwm_pwm);
	}

	if(strcmp("ENV-1", group) == 0) {
		if(strcmp("Attack", name) == 0) return &(europa4->env_1_attack);
		if(strcmp("Decay", name) == 0) return &(europa4->env_1_decay);
		if(strcmp("Sustain", name) == 0) return &(europa4->env_1_sustain);
		if(strcmp("Release", name) == 0) return &(europa4->env_1_release);
		if(strcmp("Polarity", name) == 0) return &(europa4->env_1_polarity);
	}

	if(strcmp("ENV-2", group) == 0) {
		if(strcmp("Attack", name) == 0) return &(europa4->env_2_attack);
		if(strcmp("Decay", name) == 0) return &(europa4->env_2_decay);
		if(strcmp("Sustain", name) == 0) return &(europa4->env_2_sustain);
		if(strcmp("Release", name) == 0) return &(europa4->env_2_release);
	}
	
	return NULL;
}

void reset(MachineTable *mt, void *void_europa4) {
	europa4_t *europa4 = (europa4_t *)void_europa4;

	int l;
	for(l = 0; l < E4_NR_VOICES; l++) {
		e4voice_t *v = &(europa4->voice[l]);

		envelope_reset(&(v->env_1));
		envelope_reset(&(v->env_2));
	}

	// set up key chains
	int k = 0;
	for(k = 0; k < E4_NR_KEYS - 1; k++) {
		europa4->key[k].next_key = &(europa4->key[k + 1]);
	}
	europa4->key[k].next_key = NULL;
	europa4->free_key = &(europa4->key[0]);
	europa4->active_key = NULL;

	// set up voice chain
	for(k = 0; k < E4_NR_VOICES - 1; k++) {
		europa4->voice[k].next_voice = &(europa4->voice[k + 1]);
	}
	europa4->voice[k].next_voice = NULL;
	europa4->free_voice = &(europa4->voice[0]);
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

inline void set_voice_to_key(europa4_t *europa4, e4voice_t *voice, int key, int do_glide, FTYPE glide_time) {
	int v1_key = key + europa4->vco_1_range;
	int v2_key = key + europa4->vco_2_range;
	FTYPE frequency_1 = note_table[v1_key];
	FTYPE frequency_1_modulation = note_table[v1_key + 4] - frequency_1;
	FTYPE frequency_2 = note_table[v2_key];
	FTYPE frequency_2_modulation = note_table[v2_key + 4] - frequency_2;

	los_set_frequency_modulator_depth(&(voice->vco_1), frequency_1_modulation);
	los_set_frequency_modulator_depth(&(voice->vco_2), frequency_2_modulation);

#ifdef THIS_IS_A_MOCKERY
	printf("modulation: %f, frequency: %f\n",
	       FTYPEtof(frequency_1_modulation),
	       FTYPEtof(frequency_1));
#endif
	
	if(do_glide) {
		los_glide_frequency(
			&(voice->vco_1),
			frequency_1,
			glide_time);
		
		los_glide_frequency(
			&(voice->vco_2),
			frequency_2,
			glide_time);
	} else {
		los_set_base_frequency(
			&(voice->vco_1),
			frequency_1);
		
		los_set_base_frequency(
			&(voice->vco_2),
			frequency_2);
	}
}

void execute(MachineTable *mt, void *void_europa4) {
	europa4_t *europa4 = (europa4_t *)void_europa4;

	// if we do a mode change, reset all.
	if(europa4->last_general_mode != europa4->general_mode) {
		reset(mt, europa4);
		europa4->last_general_mode = europa4->general_mode;
	}
	
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
	
	los_set_Fs(mt, Fs);
	libfilter_set_Fs(Fs);

	static int Fs_CURRENT = 0;
	if(Fs_CURRENT != Fs) {
		Fs_CURRENT = Fs;
		calc_note_freq(Fs);
	}

	FTYPE _10Hz = LOS_CALC_FREQUENCY(HERTZ(10.0f));
	FTYPE lfo_frequency = (europa4->lfo_frequency);
	FTYPE vco_mod_lfo = (europa4->vco_mod_lfo);
	FTYPE vco_mod_env_1 = (europa4->vco_mod_env_1);
	FTYPE glide_time = (europa4->glide_time);

	FTYPE vco_1_cross_mod_manual = (europa4->vco_1_cross_mod_manual);
	FTYPE vco_1_cross_mod_env_1 = (europa4->vco_1_cross_mod_env_1);
	
	FTYPE vca_env_2 = (europa4->vca_env_2);
	FTYPE vca_lfo = (europa4->vca_lfo);
	
	FTYPE vcf_cutoff = mulFTYPE(europa4->vcf_cutoff, ftoFTYPE(0.45)); // 45% of Fs
	FTYPE vcf_resonance = europa4->vcf_resonance;
	FTYPE vcf_env = mulFTYPE(ftoFTYPE(0.999), europa4->vcf_env);
	FTYPE vcf_lfo = europa4->vcf_lfo;
	FTYPE vcf_kybd = europa4->vcf_kybd;

	FTYPE pwm_pw = (europa4->pwm_pw);
	FTYPE pwm_pwm = (europa4->pwm_pwm);
	
	FTYPE env1_a, env1_d, env1_s, env1_r;
	env1_a = europa4->env_1_attack;
	env1_d = europa4->env_1_decay;
	env1_s = europa4->env_1_sustain;
	env1_r = europa4->env_1_release;

	FTYPE env2_a, env2_d, env2_s, env2_r;
	env2_a = europa4->env_2_attack;
	env2_d = europa4->env_2_decay;
	env2_s = europa4->env_2_sustain;
	env2_r = europa4->env_2_release;

	FTYPE mix_VCO_1, mix_VCO_2, mix_d;
	mix_d = (europa4->general_mix);
	mix_d = mulFTYPE(itoFTYPE(2), mix_d) - itoFTYPE(1);
	if(mix_d < itoFTYPE(0)) {
		mix_VCO_1 = itoFTYPE(1);
		mix_VCO_2 = mix_d + itoFTYPE(1);
	} else {
		mix_VCO_2 = itoFTYPE(1);
		mix_VCO_1 = itoFTYPE(1) - mix_d;
	}

	FTYPE volume = (europa4->general_volume);

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
	
	// pre-calculate filter coefficients, and other frame statics
	{
		int l;
		for(l = 0; l < E4_NR_VOICES; l++) {
			e4voice_t *v = &(europa4->voice[l]);
			
			los_set_base_frequency(
				&(v->lfo),
				mulFTYPE(lfo_frequency, _10Hz));
			
			xPassFilterMonoSetCutoff(v->lfo_lpf, mulFTYPE(lfo_frequency, ftoFTYPE(0.45f))); // 45% of Fs
			xPassFilterMonoSetResonance(v->lfo_lpf, ftoFTYPE(0.0f));
			xPassFilterMonoRecalc(v->lfo_lpf);
						
			los_set_frequency_modulator_depth(&(v->vco_1), _10Hz);	
			los_set_frequency_modulator_depth(&(v->vco_2), _10Hz);
		}
	}
	
#ifdef THIS_IS_A_MOCKERY
	FTYPE max_mock = itoFTYPE(0);
	start_measure(tmt_A);
#endif
	int t; 
	for(t = 0; t < out_l; t++) {		
		MidiEvent *mev = (MidiEvent *)midi_in[t];
#ifdef THIS_IS_A_MOCKERY
		if(mev) 
			printf(" midi event %d at %d\n", mev->data[0] & 0xf0, t);
#endif

		if(
			(mev != NULL)
			&&	
			((mev->data[0] & 0xf0) == MIDI_CONTROL_CHANGE)
			&&
			((mev->data[0] & 0x0f) == europa4->midi_channel)
			) {
#ifdef LINEAR_CUTOFF_CONVERSION
			FTYPE valu = itoFTYPE(mev->data[2]);
			valu = divFTYPE(valu, ftoFTYPE(127.0f));
#else
			FTYPE valu = ctrl2cutoff_table[mev->data[2]];
#endif
			europa4->vcf_cutoff = ftoFTYPE(0.01f) + mulFTYPE(ftoFTYPE(0.94f), valu);
			
			vcf_cutoff = mulFTYPE(europa4->vcf_cutoff, ftoFTYPE(0.45)); // 45% of Fs
		}

		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_ON)
		   &&
		   ((mev->data[0] & 0x0f) == europa4->midi_channel)
			) {
			int key = mev->data[1];
			int velocity = mev->data[2];
				
			if(europa4->free_key) {
				e4key_t *nfree = europa4->free_key->next_key;
				europa4->free_key->next_key = europa4->active_key;
				europa4->active_key = europa4->free_key;
				europa4->free_key = nfree;				

				e4key_t *new_key = europa4->active_key;
				new_key->key = key;

#ifdef __SATAN_USES_FXP
				new_key->velocity = ((velocity & 0xff) << 16);
#else
				new_key->velocity = ((float)velocity / (2.0f * 127.0f));  // the 2.0f is because I made a misstake in the fixed point math block.. so to make sure I don't break the behavior for current users who are transfering from the fixed point to floating point I must compensate here.. If I correct the misstake in the fixed point code the songs users have already produced will sound bad...
#endif
#ifdef THIS_IS_A_MOCKERY
				printf("  velocity: %f\n", FTYPEtof(new_key->velocity));
#endif

				if(europa4->general_mode == 1) {
					// unison mode
					FTYPE unison_detune = mulFTYPE(europa4->general_detune, ftoFTYPE(0.25f)); // 0.25 because we only want one half note detune... otherwise we will get 4.
					
					// first count active keys
					int actives = 0;
					e4key_t *a = europa4->active_key;
					while(a != NULL) {
						actives++; a = a->next_key;
					}

					// calculate voices / key
					int voices_per_key = E4_NR_VOICES / actives;
					
					// if voices_per_key is less than the unison_voices counter for each key, deactivate these voices
					a = europa4->active_key;
					while(a != NULL) {
						if(voices_per_key < a->unison_voices) {
							int v;

							for(v = voices_per_key; v < a->unison_voices; v++) {
								if(a->voice[v]) {
									envelope_release(&(a->voice[v]->env_1));
									envelope_release(&(a->voice[v]->env_2));

									a->voice[v]->next_voice = europa4->free_voice;
									europa4->free_voice = a->voice[v];
									
									a->voice[v] = NULL;
								}
							}							
						}
						a->unison_voices = voices_per_key;
						a = a->next_key;
					}

					FTYPE unison_velocity = new_key->velocity;
					FTYPE last_unison_detune = itoFTYPE(0);
					// activate voices for new key
					{
						int v;
						for(v = 0; v < voices_per_key; v++) {
							if(europa4->free_voice) {
								e4voice_t *voice = europa4->free_voice;
								new_key->voice[v] = voice;
								europa4->free_voice = voice->next_voice;
								voice->next_voice = NULL;
								
								voice->velocity = unison_velocity;
								unison_velocity = divFTYPE(unison_velocity, itoFTYPE(2));
						
								trigger_voice_envelope(&(voice->env_1),
										       Fs,
										       env1_a, env1_d, env1_s, env1_r);
								
								trigger_voice_envelope(&(voice->env_2),
										       Fs,
										       env2_a, env2_d, env2_s, env2_r);
								
								set_voice_to_key(europa4,
										 voice, new_key->key,
										 0, itoFTYPE(0));
								
								if(europa4->lfo_sync_to_key_press)
									los_reset_oscillator(&(voice->lfo));

								if(v % 2 == 0) {
									voice->unison_detune = -last_unison_detune;
								} else {
									voice->unison_detune = last_unison_detune = unison_detune;
									unison_detune = divFTYPE(unison_detune, itoFTYPE(2));
								}
							}
						}
					}
				} else {
					// solo or polyphony mode
					if(europa4->general_mode == 0 && new_key->next_key != NULL && europa4->glide_mode) {
 						new_key->voice[0] = new_key->next_key->voice[0];
						new_key->next_key->voice[0] = NULL;
						
						set_voice_to_key(europa4,
								 new_key->voice[0], new_key->key,
								 europa4->glide_mode,
								 glide_time);
					} else {
						if(europa4->general_mode == 0) {
							// solo mode
							new_key->voice[0] = &(europa4->voice[0]);
						} else {
							// polyphony mode
							int k;
							for(k = 0; k < E4_NR_KEYS; k++) {
								if(new_key == &(europa4->key[k]))
									break;
							}
							new_key->voice[0] = &(europa4->voice[k]);
						}
						new_key->voice[0]->velocity = new_key->velocity;
						
						trigger_voice_envelope(&(new_key->voice[0]->env_1),
								       Fs,
								       env1_a, env1_d, env1_s, env1_r);
						
						trigger_voice_envelope(&(new_key->voice[0]->env_2),
								       Fs,
								       env2_a, env2_d, env2_s, env2_r);

						if(europa4->general_mode == 0) {

							// solo mode
							set_voice_to_key(europa4,
									 new_key->voice[0], new_key->key,
									 europa4->glide_mode == 1 ? 1 : 0,
									 glide_time);

						} else {

							// polyphony mode
							set_voice_to_key(europa4,
									 new_key->voice[0], new_key->key,
									 0, itoFTYPE(0));

						}
						
						if(europa4->lfo_sync_to_key_press)
							los_reset_oscillator(&(new_key->voice[0]->lfo));
				
					}
				}
			}
#ifdef THIS_IS_A_MOCKERY
			{
				e4key_t *k = europa4->active_key;
				printf("NOTE ON:\n");
				while(k != NULL) {
					int u;
					printf("Active key: %d\n", k->key);
					printf("   "); // indentation
					for(u = 0; u < E4_NR_VOICES; u++) {
						if(k->voice[u]) {
							printf("voice %p ", k->voice[u]);
						}
					}
					printf("\n"); // line feed
					k = k->next_key;
				}
				
			}
#endif
		}

		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_OFF)
		   &&
		   ((mev->data[0] & 0x0f) == europa4->midi_channel)
			) {
			int key = mev->data[1];

			e4key_t *crnt_key = europa4->active_key;
			e4key_t *prev_key = NULL;

#ifdef THIS_IS_A_MOCKERY
			{
				e4key_t *k = europa4->active_key;
				printf("NOTE OFF (BEFORE):\n");
				while(k != NULL) {
					int u;
					
					printf("Active key: %d\n", k->key);
					
					printf("   "); // indentation
					for(u = 0; u < E4_NR_VOICES; u++) {
						if(k->voice[u]) {
							printf("voice %p ", k->voice[u]);
						}
					}
					printf("\n"); // line feed
					k = k->next_key;
				}
				
			}
			{
				int l;
				for(l = 0; l < E4_NR_VOICES; l++) {
					e4voice_t *v = &(europa4->voice[l]);
					
					// if both envelopes are non-active, skip processing this voice
					if(
						(!envelope_is_active(&(v->env_1)))
						&&
						(!envelope_is_active(&(v->env_2)))
						) {
						printf(" voice %p is inactive.\n", v);
						continue;
					}
					printf(" voice %p is ACTIVE.\n", v);
				}
			}
#endif
			while(crnt_key != NULL) {
				if(crnt_key->key == key) {
					if(prev_key) {
						// make sure the previous voice correctly
						// points to the next one before we proceed.
						prev_key->next_key = crnt_key->next_key;
					}
					
					if(europa4->general_mode != 1) {
						// solo or polyphony mode
						if(crnt_key->voice[0] != NULL) {
							// if general_mode = solo,
							// and we can pass the voice to the next one
							// do so...
							if(europa4->general_mode == 0
							   && crnt_key->next_key) {
								set_voice_to_key(europa4,
										 crnt_key->voice[0],
										 crnt_key->next_key->key,
										 europa4->glide_mode, glide_time);
								
								crnt_key->next_key->voice[0] = crnt_key->voice[0];
							} else {
								// if we cannot pass to the next
								// one, or if general mode is polyphony
								// we shall just release the voice.
								envelope_release(&(crnt_key->voice[0]->env_1));
								envelope_release(&(crnt_key->voice[0]->env_2));
							}
							crnt_key->voice[0] = NULL;
						}
					} else {
						// unison mode
						int v;
						for(v = 0; v < E4_NR_VOICES; v++) {

							if(crnt_key->voice[v] != NULL) {
								envelope_release(&(crnt_key->voice[v]->env_1));
								envelope_release(&(crnt_key->voice[v]->env_2));

								crnt_key->voice[v]->next_voice = europa4->free_voice;
								europa4->free_voice = crnt_key->voice[v];
								
								crnt_key->voice[v] = NULL;
							}
						}
					}

					
					// if curnt_key is first in line, we must
					// set the active_key to the next in chain..
					if(europa4->active_key == crnt_key) 
						europa4->active_key = 
							crnt_key->next_key;
					
					// put it back in the free key chain.
					crnt_key->next_key = europa4->free_key;
					europa4->free_key = crnt_key;
					
					crnt_key = NULL;
				} else {					
					prev_key = crnt_key;
					crnt_key = crnt_key->next_key;
				}
			}
#ifdef THIS_IS_A_MOCKERY
			{
				e4key_t *k = europa4->active_key;
				printf("NOTE OFF (AFTER):\n");
				while(k != NULL) {
					int u;
					
					printf("Active key: %d\n", k->key);
					
					printf("   "); // indentation
					for(u = 0; u < E4_NR_VOICES; u++) {
						if(k->voice[u]) {
							printf("voice %p ", k->voice[u]);
						}
					}
					printf("\n"); // line feed
					k = k->next_key;
				}
				
			}
#endif
						
		}
		
		// set the current output sample to 0
		out[t] = itoFTYPE(0);

		// process voices
		{
			int l;
			for(l = 0; l < E4_NR_VOICES; l++) {
				e4voice_t *v = &(europa4->voice[l]);

				// if both envelopes are non-active, skip processing this voice
				if(
					(!envelope_is_active(&(v->env_1)))
				   &&
					(!envelope_is_active(&(v->env_2)))
					) {
					continue;
				} 
				
				// otherwise, process it hereafter!
				
				FTYPE vco_1 = itoFTYPE(0), vco_2 = itoFTYPE(0);
				FTYPE lfo = itoFTYPE(0);
				FTYPE env_1 = itoFTYPE(0), env_2 = itoFTYPE(0);
				FTYPE vca = itoFTYPE(0);
				FTYPE pwm;

				{ // process ENV 1
					env_1 = envelope_get_sample(&(v->env_1));
					envelope_step(&(v->env_1));

					if(europa4->env_1_polarity) { // reverse polarity
						env_1 = itoFTYPE(1) - env_1;
					}
				}
				
				{ // process ENV 2
					env_2 = envelope_get_sample(&(v->env_2));
					envelope_step(&(v->env_2));
				}

				
				{ // process LFO
					switch(europa4->lfo_wave) {
					case 0:
						lfo = los_get_alternate_sample(&(v->lfo), los_src_sin);
						break;
					case 1:
						lfo = los_get_alternate_sample(&(v->lfo), los_src_saw);
						break;
					case 2:
						lfo = los_get_alternate_sample(&(v->lfo), los_src_square);
						break;
					case 3:
						lfo = los_get_alternate_sample(&(v->lfo), los_src_noise);
						xPassFilterMonoPut(v->lfo_lpf, lfo);
						lfo = xPassFilterMonoGet(v->lfo_lpf);
						break;
					}
					los_step_oscillator(&(v->lfo), itoFTYPE(0), itoFTYPE(0));
				}

				{ // calculate PWM here
					FTYPE mod = itoFTYPE(1);
					
					if(europa4->pwm_selector == 0) { // LFO
						mod = divFTYPE(lfo + itoFTYPE(1),
							       itoFTYPE(2));
					} else {
						mod = env_1;
					}
					mod = itoFTYPE(1) - pwm_pwm +
						mulFTYPE(
							pwm_pwm,
							mod);
					
					mod = mulFTYPE(mod, pwm_pw);
					pwm = mulFTYPE(mod, ftoFTYPE(0.5f));
				}

				FTYPE vco_mod_BASE;
				
				vco_mod_BASE = mulFTYPE(lfo, vco_mod_lfo);
				if(europa4->env_1_polarity)
					vco_mod_BASE += mulFTYPE(env_1, vco_mod_env_1);
				else
					vco_mod_BASE += mulFTYPE(env_1 - itoFTYPE(1), vco_mod_env_1);

				
				{ // process VCO-2
					FTYPE vco_mod = itoFTYPE(0);

					if(europa4->vco_mod_vco_2) {
						vco_mod += vco_mod_BASE;
					}

					FTYPE saw = los_get_alternate_sample(&(v->vco_2), los_src_saw);
					if(europa4->vco_2_sin)
						vco_2 += los_get_alternate_sample(&(v->vco_2), los_src_sin);
					if(europa4->vco_2_sawsmooth)
						vco_2 += los_get_alternate_sample(&(v->vco_2), los_src_sawsmooth);
					if(europa4->vco_2_saw)
						vco_2 += saw;
					if(europa4->vco_2_pulse) {
						if(europa4->vco_2_pulse_mode) {
							if(saw < pwm)
								vco_2 += itoFTYPE(1);
							else
								vco_2 -= itoFTYPE(1);
						} else {
							vco_2 += los_get_alternate_sample(
								&(v->vco_2), los_src_square);
						}
					}
					
					if(europa4->general_mode == 1) { // unison mode
						los_step_oscillator(&(v->vco_2), vco_mod + v->unison_detune, itoFTYPE(0));
					} else {
						los_step_oscillator(&(v->vco_2), vco_mod, itoFTYPE(0));
					}
					
#ifdef THIS_IS_A_MOCKERY
					if(l == 0) {
						int_out_a[t] = mulFTYPE(ftoFTYPE(0.5f), vco_2);
					}
#endif
//					vco_2 = mulFTYPE(v->velocity, mulFTYPE(vco_2, env_2));
				}

				{ // process VCO-1
					FTYPE vco_mod = itoFTYPE(0);

					if(europa4->vco_mod_vco_1) {
						vco_mod += vco_mod_BASE;
					}

					vco_mod += mulFTYPE(vco_2, vco_1_cross_mod_manual);
					vco_mod += mulFTYPE(vco_2, vco_1_cross_mod_env_1);

					FTYPE saw = los_get_alternate_sample(&(v->vco_1), los_src_saw);
					if(europa4->vco_1_sin)
						vco_1 += los_get_alternate_sample(&(v->vco_1), los_src_sin);
					if(europa4->vco_1_sawsmooth)
						vco_1 += los_get_alternate_sample(&(v->vco_1), los_src_sawsmooth);
					if(europa4->vco_1_saw)
						vco_1 += saw;
					if(europa4->vco_1_pulse) {
						if(europa4->vco_1_pulse_mode) {
							if(saw < pwm)
								vco_1 += itoFTYPE(1);
							else
								vco_1 -= itoFTYPE(1);
						} else {
							vco_1 += los_get_alternate_sample(
								&(v->vco_1), los_src_square);
						}
					}
					if(europa4->general_mode == 1) { // unison mode
						los_step_oscillator(&(v->vco_1), vco_mod + v->unison_detune, itoFTYPE(0));
					} else {
						los_step_oscillator(&(v->vco_1), vco_mod, itoFTYPE(0));
					}

//					vco_1 = mulFTYPE(v->velocity, mulFTYPE(vco_1, env_1));
				}

				{ // process VCF parameters
					FTYPE env;
					FTYPE kybd_env;
					FTYPE lfo_env;
					FTYPE cutoff;

					if(DO_FILTER_RECALC(v)) {
						env = europa4->vcf_env_selector ? env_2 : env_1;

						cutoff = mulFTYPE(vcf_cutoff,
								  itoFTYPE(1) - vcf_env + mulFTYPE(env, vcf_env));
					       
						kybd_env = mulFTYPE(vcf_kybd, v->velocity);
						kybd_env = itoFTYPE(1) - vcf_kybd + kybd_env;
						
						lfo_env = divFTYPE(lfo + itoFTYPE(1), itoFTYPE(2)); // make it vary from 0 to 1
						lfo_env = mulFTYPE(vcf_lfo, lfo_env);
						lfo_env = itoFTYPE(1) - vcf_lfo + lfo_env;
												
						cutoff = mulFTYPE(lfo_env, cutoff);
						cutoff = mulFTYPE(kybd_env, cutoff);

						xPassFilterMonoSetCutoff(v->lpf, cutoff);
						xPassFilterMonoSetResonance(v->lpf, vcf_resonance);
						xPassFilterMonoRecalc(v->lpf);
						xPassFilterMonoSetCutoff(v->hpf, cutoff);
						xPassFilterMonoSetResonance(v->hpf, vcf_resonance);
						xPassFilterMonoRecalc(v->hpf);
					}
				}

				{ // process VCA
					FTYPE ve2 = itoFTYPE(1);
					FTYPE vlf = itoFTYPE(1);

					ve2 = ve2 - vca_env_2 + mulFTYPE(vca_env_2, env_2);
					vlf = vlf - vca_lfo + mulFTYPE(vca_lfo, lfo);
					
					vca = mulFTYPE(v->velocity, mulFTYPE(ve2, vlf));
				}

				{ // process end result
					FTYPE output_v = mulFTYPE(
						volume,
						mulFTYPE(mix_VCO_1, vco_1) + mulFTYPE(mix_VCO_2, vco_2)
						);

					if(europa4->vcf_mode == 1 ||
					   europa4->vcf_mode == 3) {
						xPassFilterMonoPut(v->lpf, output_v);
						output_v = xPassFilterMonoGet(v->lpf);
					}

					output_v = mulFTYPE(output_v, vca);
					if(europa4->vcf_mode == 2 ||
					   europa4->vcf_mode == 3) {
						xPassFilterMonoPut(v->hpf, output_v);
						output_v = xPassFilterMonoGet(v->hpf);
					}
					
#ifdef THIS_IS_A_MOCKERY
					if(l == 0) {
						int_out_b[t] = mulFTYPE(ftoFTYPE(0.9f), output_v);
					}
#endif

					out[t] += output_v;
				}
				STEP_FILTER_RECALC(v);

#ifdef THIS_IS_A_MOCKERY
				{
					if(
						(!envelope_is_active(&(v->env_1)))
						&&
						(!envelope_is_active(&(v->env_2)))
						) {
						printf(" !!!! voice %p ended by autonomy.\n", v);
					} 
				}
#endif
			}
		}
#ifdef THIS_IS_A_MOCKERY
		if(out[t] > max_mock)
			max_mock = out[t];
#endif
	}
#ifdef THIS_IS_A_MOCKERY
	stop_measure(tmt_A, "full run");
#endif

}
