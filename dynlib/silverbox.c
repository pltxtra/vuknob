/*
 * VuKnob, Copyright 2014 by Anton Persson
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

#include "dynlib.h"
USE_SATANS_MATH

#include "liboscillator.c"
#include "libfilter.c"
#include "libenvelope.c"

#define SILVERBOX_CHANNEL 0

typedef struct silverbox_instance {
	// playback data
	int key;
	oscillator_t vco;
	envelope_t amp_env;
	envelope_t flt_env;
	FTYPE velocity;

	FTYPE cutoff_crnt, cutoff_target, cutoff_change_per_tick;
	int cutoff_change_ticks;

	FTYPE resonance_crnt, resonance_target, resonance_change_per_tick;
	int resonance_change_ticks;

	FTYPE envmod_crnt, envmod_target, envmod_change_per_tick;
	int envmod_change_ticks;

	FTYPE decay_crnt, decay_target, decay_change_per_tick;
	int decay_change_ticks;

	// controls
	int midi_channel, wave;
	FTYPE volume, cutoff, resonance, envmod, decay;  // envmod = envelope modulation

} silverbox_t;

#define NOTE_TABLE_LENGTH 256
FTYPE note_table[NOTE_TABLE_LENGTH]; // frequency table for standard MIDI notes

static float cutoff = 1.0f, resonance = 0.0f;

inline float swindow(float x, float margin) {
	if(x <= margin) {
		return 0.5f + 0.5f * cos((x / margin) * M_PI + M_PI);
	} else if(x < (1.0f - margin)) {
		return 1.0f;
	} else if(x < 1.0f) {
		return 0.5f + 0.5f * cos(((x - 1.0f + margin) / margin) * M_PI);
	}
	return 0.0f;
}

inline float sdrop(float x, float margin, float width) {
	if(x < margin) {
		return 1.0f;
	} else if(x > margin && x <= (margin + width)) {
		return 0.5 + 0.5 * cos(((x - margin) / width) * M_PI);
	}
	return 0.0f;
}

inline FTYPE sigSaw(FTYPE x_fixed) {
	float x = FTYPEtof(x_fixed);

	x = x - floorf(x);

	float lim = 0.25 / (1 + 19.0f * cutoff);

	float amplitude = sdrop(x, lim, 2.0f * lim);
	amplitude = ((1 - resonance) * amplitude) + resonance * (1.0f - x);
	amplitude = 0.5 + 0.5 * cos((1-amplitude) * M_PI);

	float w = swindow(x, lim);

	float output = -amplitude * sin((1 + 19.0f * cutoff) * x * 2.0f * M_PI);
	output = - w * (1 - x) * 0.2 + 0.8 * output * w;

	float sig = -sinf((powf(sinf(x * M_PI / 2.0), (0.8 - 0.4 * cutoff))) * 2.0 * M_PI) * (1.0f - powf(x, (0.8 - 0.4 * cutoff))) * (1.0f - x) * w;
	output = sig - w * (pow(1.0f - x, 4.0f)) * resonance * sinf((1.0f + cutoff * 40.0f) * 2.0f * M_PI * x);

//	printf("output: %f\n", output);

	return ftoFTYPE(output);
}

inline FTYPE sigSqr(FTYPE x_fixed) {
	FTYPE limit = SAT_FLOOR(mulFTYPE(ftoFTYPE(2.0f), (x_fixed - SAT_FLOOR(x_fixed))));
	FTYPE output = sigSaw(mulFTYPE(ftoFTYPE(2.0f), x_fixed));
	output = mulFTYPE((ftoFTYPE(1.0f) - limit), output) - mulFTYPE(limit, output);
	return output;
}

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	silverbox_t *silverbox = (silverbox_t *)malloc(sizeof(silverbox_t));

	if(silverbox == NULL) return NULL;

	memset(silverbox, 0, sizeof(silverbox_t));

	silverbox->midi_channel = SILVERBOX_CHANNEL;
	silverbox->volume = ftoFTYPE(0.35f);

	/* filter stuff */
	SETUP_SATANS_MATH(mt);
	silverbox->cutoff = ftoFTYPE(1.0f);
	silverbox->resonance = ftoFTYPE(0.0f);

	los_init_oscillator(&(silverbox->vco));
	envelope_init(&(silverbox->amp_env), 44100);
	envelope_init(&(silverbox->flt_env), 44100);

	silverbox->key = -1;

	/* return pointer to instance data */
	return (void *)silverbox;
}

void *get_controller_ptr(MachineTable *mt, void *void_silverbox,
			 const char *name,
			 const char *group) {
	silverbox_t *silverbox = (silverbox_t *)void_silverbox;
	if(strcmp(name, "midiChannel") == 0) {
		return &(silverbox->midi_channel);
	} else if(strcmp(name, "volume") == 0) {
		return &(silverbox->volume);
	} else if(strcmp(name, "wave") == 0) {
		return &(silverbox->wave);
	} else if(strcmp("cutoff", name) == 0) {
		return &(silverbox->cutoff);
	} else if(strcmp("resonance", name) == 0) {
		return &(silverbox->resonance);
	} else if(strcmp("env_mod", name) == 0) {
		return &(silverbox->envmod);
	} else if(strcmp("decay", name) == 0) {
		return &(silverbox->decay);
	}

	return NULL;
}

void reset(MachineTable *mt, void *void_silverbox) {
	silverbox_t *silverbox = (silverbox_t *)void_silverbox;
	silverbox->key = 0;
}


inline void trigger_voice_envelope(envelope_t *env, int Fs, FTYPE a, FTYPE d, FTYPE s, FTYPE r) {
	envelope_init(env, Fs);

	envelope_set_attack(env, a);
	envelope_set_hold(env, ftoFTYPE(0.0f));
	envelope_set_decay(env, d);
	envelope_set_sustain(env, s);
	envelope_set_release(env, r);

	envelope_hold(env);
}

inline void set_voice_to_key(silverbox_t *silverbox, int key, int do_glide, FTYPE glide_time) {
	FTYPE frequency = note_table[key];

	if(do_glide) {
		los_glide_frequency(
			&(silverbox->vco),
			frequency,
			glide_time);
	} else {
		los_set_base_frequency(
			&(silverbox->vco),
			frequency);
	}
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

void execute(MachineTable *mt, void *void_silverbox) {
	silverbox_t *silverbox = (silverbox_t *)void_silverbox;

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
	int out_l = mt->get_signal_samples(outsig);

	if(midi_l != out_l)
		return; // we expect equal lengths..

	int Fs = mt->get_signal_frequency(outsig);

	static int Fs_CURRENT = 0;
	if(Fs_CURRENT != Fs) {
		Fs_CURRENT = Fs;
		los_set_Fs(mt, Fs);
		libfilter_set_Fs(Fs);
		calc_note_freq(Fs);
	}

	int t;

	for(t = 0; t < out_l; t++) {
		// check for midi events
		MidiEvent *mev = (MidiEvent *)midi_in[t];
		if(
			(mev != NULL)
			&&
			((mev->data[0] & 0xf0) == MIDI_CONTROL_CHANGE)
			&&
			((mev->data[0] & 0x0f) == silverbox->midi_channel)
			) {
			FTYPE valu = itoFTYPE(mev->data[2]);
			switch(mev->data[1]) {
			case 74:
				silverbox->cutoff = divFTYPE(valu, ftoFTYPE(128.0f));
				break;
			case 73:
				silverbox->resonance = divFTYPE(valu, ftoFTYPE(128.0f));
				break;
			case 72:
				silverbox->envmod = divFTYPE(valu, ftoFTYPE(128.0f));
				break;
			case 71:
				silverbox->decay = divFTYPE(valu, ftoFTYPE(128.0f));
				break;
			}
		}

		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_ON)
		   &&
		   ((mev->data[0] & 0x0f) == silverbox->midi_channel)
			) {
			int key = mev->data[1];
			float velocity = (float)(mev->data[2]);
			velocity = velocity / 127.0;

			silverbox->velocity = ftoFTYPE(velocity);
			if(silverbox->key == -1) {
				trigger_voice_envelope(&(silverbox->amp_env),
						       Fs,
						       ftoFTYPE(0.002),
						       ftoFTYPE(0.1),
						       ftoFTYPE(0.9),
						       ftoFTYPE(0.1));
				trigger_voice_envelope(&(silverbox->flt_env),
						       Fs,
						       ftoFTYPE(0.0),
						       mulFTYPE(ftoFTYPE(2.0f), silverbox->decay),
						       ftoFTYPE(0.0),
						       ftoFTYPE(0.1));
				set_voice_to_key(silverbox,
						 key,
						 0,
						 ftoFTYPE(0.1f));
			} else {
				set_voice_to_key(silverbox,
						 key,
						 -1,
						 ftoFTYPE(0.25f));
			}
			silverbox->key = key;
		}

		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_OFF)
		   &&
		   ((mev->data[0] & 0x0f) == silverbox->midi_channel)
			) {
			int key = mev->data[1];

			if(key == silverbox->key) {
				envelope_release(&(silverbox->amp_env));
				envelope_release(&(silverbox->flt_env));
				silverbox->key = -1;
			}
		}

		{ /* update cutoff properly */
			if(silverbox->cutoff_target != silverbox->cutoff) {
				silverbox->cutoff_target = silverbox->cutoff;
				silverbox->cutoff_change_ticks = Fs / 10;
				silverbox->cutoff_change_per_tick = divFTYPE((silverbox->cutoff_target - silverbox->cutoff_crnt),
									     itoFTYPE(silverbox->cutoff_change_ticks));
			}

			if(silverbox->cutoff_change_ticks) {
				silverbox->cutoff_change_ticks--;
				silverbox->cutoff_crnt += silverbox->cutoff_change_per_tick;
				if(silverbox->cutoff_change_ticks == 0) {
					silverbox->cutoff_crnt = silverbox->cutoff_target;
				}
			}
		}
		{ /* update resonance properly */
			if(silverbox->resonance_target != silverbox->resonance) {
				silverbox->resonance_target = silverbox->resonance;
				silverbox->resonance_change_ticks = Fs / 10;
				silverbox->resonance_change_per_tick = divFTYPE((silverbox->resonance_target - silverbox->resonance_crnt),
									     itoFTYPE(silverbox->resonance_change_ticks));
			}

			if(silverbox->resonance_change_ticks) {
				silverbox->resonance_change_ticks--;
				silverbox->resonance_crnt += silverbox->resonance_change_per_tick;
				if(silverbox->resonance_change_ticks == 0) {
					silverbox->resonance_crnt = silverbox->resonance_target;
				}
			}
		}
		{ /* update envmod properly */
			if(silverbox->envmod_target != silverbox->envmod) {
				silverbox->envmod_target = silverbox->envmod;
				silverbox->envmod_change_ticks = Fs / 10;
				silverbox->envmod_change_per_tick = divFTYPE((silverbox->envmod_target - silverbox->envmod_crnt),
									     itoFTYPE(silverbox->envmod_change_ticks));
			}

			if(silverbox->envmod_change_ticks) {
				silverbox->envmod_change_ticks--;
				silverbox->envmod_crnt += silverbox->envmod_change_per_tick;
				if(silverbox->envmod_change_ticks == 0) {
					silverbox->envmod_crnt = silverbox->envmod_target;
				}
			}
		}
		{ /* update decay properly */
			if(silverbox->decay_target != silverbox->decay) {
				silverbox->decay_target = silverbox->decay;
				silverbox->decay_change_ticks = Fs / 10;
				silverbox->decay_change_per_tick = divFTYPE((silverbox->decay_target - silverbox->decay_crnt),
									     itoFTYPE(silverbox->decay_change_ticks));
			}

			if(silverbox->decay_change_ticks) {
				silverbox->decay_change_ticks--;
				silverbox->decay_crnt += silverbox->decay_change_per_tick;
				if(silverbox->decay_change_ticks == 0) {
					silverbox->decay_crnt = silverbox->decay_target;
				}
			}
		}

		/* zero out */
		out[t] = itoFTYPE(0);

		if(envelope_is_active(&(silverbox->amp_env))) {
			FTYPE vco = itoFTYPE(0);
			FTYPE amp_env = itoFTYPE(0);
			FTYPE flt_env = itoFTYPE(0);

			{ // process ENV
				amp_env = envelope_get_sample(&(silverbox->amp_env));
				flt_env = envelope_get_sample(&(silverbox->flt_env));
				envelope_step(&(silverbox->amp_env));
				envelope_step(&(silverbox->flt_env));
			}

			{ // process VCO
				resonance = FTYPEtof(silverbox->resonance_crnt);

				FTYPE crnt_cutoff =
					silverbox->cutoff_crnt + mulFTYPE(silverbox->envmod_crnt,
									  mulFTYPE(flt_env,
										   ftoFTYPE(1.0f) - silverbox->cutoff_crnt
										  )
						);

				if(silverbox->wave == 0) {
					cutoff = FTYPEtof(crnt_cutoff);
					vco = los_get_alternate_sample(&(silverbox->vco), sigSaw);
				} else {
					cutoff = FTYPEtof(mulFTYPE(ftoFTYPE(0.5f), crnt_cutoff));
					vco = los_get_alternate_sample(&(silverbox->vco), sigSqr);
				}
				los_step_oscillator(&(silverbox->vco), itoFTYPE(0), itoFTYPE(0));
			}
			out[t] += mulFTYPE(silverbox->velocity, mulFTYPE(silverbox->volume, mulFTYPE(vco, amp_env)));
		}
	}
}

void delete(void *data) {
	/* free instance data here */
	free(data);
}
