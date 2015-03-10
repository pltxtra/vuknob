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

#include "dynlib.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <fixedpointmath.h>

#ifdef ANDROID
#include <android/log.h>
#endif

#define TCUTTER_CHANNEL 0
#define POLYPHONY 6

USE_SATANS_MATH

typedef enum Resolution _Resolution;

typedef struct note_struct {
	int active; /* !0 == active, 0 == inactive */
	int note; /* midi note */
	int note_on; // boolean value. This is used to make sure we only care for the FIRST received note off

	// Amplitude
	int amp_phase;
	FTYPE amplitude; /* 0<1 */
	int amp_attack_steps;
	FTYPE amp_attack_step;
	int amp_hold_steps;
	int amp_decay_steps;
	FTYPE amp_decay_step;
	int amp_release_steps;
	FTYPE amp_release_step;
		
	int t;
} note_t;

typedef struct tcutter_instance {
	int midi_channel;
	float volume;
	note_t note[POLYPHONY]; // POLYPHONY concurrent tones
	int program; // current program, use in the future?

	float amp_attack, amp_hold, amp_decay, amp_sustain, amp_release; 

	float dry;
	
} tcutter_t;

float note_table[257]; // freqency table for standard MIDI notes, 257 is because we need a "spare" when calculating centi-notes

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	tcutter_t *tcutter = (tcutter_t *)malloc(sizeof(tcutter_t));;
	memset(tcutter, 0, sizeof(tcutter_t));

	tcutter->midi_channel = TCUTTER_CHANNEL;
	tcutter->volume = 1.0;
	tcutter->dry = 0.0;

	/* envelope */
	tcutter->amp_attack = 0.05;
	tcutter->amp_hold = 0.01;
	tcutter->amp_decay = 0.2;
	tcutter->amp_sustain = 0.6;
	tcutter->amp_release = 0.25; 
	
	/* note table */
	int n;
	for(n = 0; n < 257; n++) {
		note_table[n] =
			440.0 * pow(2.0, (n - 69) / 12.0);
	}
	
	/* return pointer to instance data */
	return (void *)tcutter;
}
 
void *get_controller_ptr(MachineTable *mt, void *void_tcutter,
			 const char *name,
			 const char *group) {
	tcutter_t *tcutter = (tcutter_t *)void_tcutter;
	if(strcmp(name, "midiChannel") == 0) {
		return &(tcutter->midi_channel);
	} else if(strcmp(name, "volume") == 0) {
		return &(tcutter->volume);
	} else if(strcmp("ampAttack", name) == 0) {
		return &(tcutter->amp_attack);
	} else if(strcmp("ampHold", name) == 0) {
		return &(tcutter->amp_hold);
	} else if(strcmp("ampDecay", name) == 0) {
		return &(tcutter->amp_decay);
	} else if(strcmp("ampSustain", name) == 0) {
		return &(tcutter->amp_sustain);
	} else if(strcmp("ampRelease", name) == 0) {
		return &(tcutter->amp_release);
	} else if(strcmp("dry", name) == 0) {
		return &(tcutter->dry);
	} 
	
	return NULL;
}

void reset(MachineTable *mt, void *void_tcutter) {
	tcutter_t *tcutter = (tcutter_t *)void_tcutter;
	int n_k;
	for(n_k = 0; n_k < POLYPHONY; n_k++) {
		note_t *n = &(tcutter->note[n_k]);
		n->active = 0;
	}
}

#define SAMPLES_PER_FILTER_UPDATE (128 - 1)
void execute(MachineTable *mt, void *void_tcutter) {
	tcutter_t *tcutter = (tcutter_t *)void_tcutter;

	SignalPointer *s = NULL;
	SignalPointer *outsig = NULL;
	SignalPointer *insig = NULL;

	outsig = mt->get_output_signal(mt, "Stereo");
	if(outsig == NULL)
		return;

	FTYPE *out =
		(FTYPE *)mt->get_signal_buffer(outsig);
	int out_l = mt->get_signal_samples(outsig);
	int oc = mt->get_signal_channels(outsig);

	s = mt->get_input_signal(mt, "Stereo");
	if(s == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < out_l; t++) {
			out[t * 2 + 0] = itoFTYPE(0);
			out[t * 2 + 1] = itoFTYPE(0);
		}
		
		return;
	}
	FTYPE *in = mt->get_signal_buffer(s);
	int ic = mt->get_signal_channels(s);
	
	insig = mt->get_input_signal(mt, "midi");
	if(insig == NULL)
		return;
	
	void **midi_in = (void **)mt->get_signal_buffer(insig);
	if(midi_in == NULL)
		return;
	
	int midi_l = mt->get_signal_samples(insig);
		
	if(midi_l != out_l)
		return; // we expect equal lengths..

	float Fs = (float)mt->get_signal_frequency(outsig);

	FTYPE volume = ftoFTYPE(tcutter->volume);
	FTYPE dry_fp = ftoFTYPE(tcutter->dry);

	int t, n_k;

	for(t = 0; t < out_l; t++) {
		// check for midi events
		MidiEvent *mev = (MidiEvent *)midi_in[t];

		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_ON)
		   &&
		   ((mev->data[0] & 0x0f) == tcutter->midi_channel)
			) {
			int note = mev->data[1];
			float velocity = (float)(mev->data[2]);
			for(n_k = 0; n_k < POLYPHONY; n_k++) {
				if(!(tcutter->note[n_k].active)) {
					note_t *n = &(tcutter->note[n_k]);

					n->active = 1;
					n->t = 0;
					n->note = note;
					n->note_on = 1;
					
					// amplitude stuff
					n->amp_phase = 0;
					n->amplitude = ftoFTYPE(0.0);
					n->amp_attack_steps = 1 +
						(int)((float)tcutter->amp_attack * (float)Fs);
					n->amp_attack_step =
						ftoFTYPE((velocity / 127.0) /
							  n->amp_attack_steps);
					n->amp_hold_steps = 1 +
						(int)((float)tcutter->amp_hold * (float)Fs);
					n->amp_decay_steps = 1 +
						(int)((float)tcutter->amp_decay * (float)Fs);
					n->amp_decay_step =
						ftoFTYPE((1.0 - tcutter->amp_sustain) *
							  (velocity / 127.0) /
							  n->amp_decay_steps);

					break;
				}
			}
		}

		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_OFF)
		   &&
		   ((mev->data[0] & 0x0f) == tcutter->midi_channel)
			) {
			int note = mev->data[1];
			float velocity = (float)(mev->data[2]);
			velocity = velocity / 127.0;
			for(n_k = 0; n_k < POLYPHONY; n_k++) {
				if((tcutter->note[n_k].active) &&
				   (tcutter->note[n_k].note == note) &&
				   (tcutter->note[n_k].note_on)) {
					note_t *n = &(tcutter->note[n_k]);

					n->note_on = 0;
					n->amp_phase = 4;
					n->amp_release_steps = 1 +
						tcutter->amp_release * Fs / (velocity + 1.0);
					n->amp_release_step =
						ftoFTYPE(FTYPEtof(n->amplitude) /
							 (float)(n->amp_release_steps));
					break;
				}
			}
		}

		/* zero out */
		out[t * oc    ] = itoFTYPE(0);
		out[t * oc + 1] = itoFTYPE(0);
		
		/* process active notes */
		for(n_k = 0; n_k < POLYPHONY; n_k++) {
			note_t *n = &(tcutter->note[n_k]);
			if(n->active) {
				{ /* amplitude parameters */
					switch(n->amp_phase) {
					case 0: // attack phase
						n->amplitude +=
							n->amp_attack_step;
						n->amp_attack_steps--;
						if(n->amp_attack_steps < 0) {
							n->amp_phase++;
						}
						break;
					case 1: // hold phase
						n->amp_hold_steps--;
						if(n->amp_hold_steps < 0) {
							n->amp_phase++;
						}
						break;
					case 2: // decay phase
						n->amplitude -=
							n->amp_decay_step;
						n->amp_decay_steps--;
						if(n->amp_decay_steps < 0) {
							n->amp_phase++;
						}
						break;
					case 3: // sustain  phase
						/* hold here until a NOTE_OFF message arrives */
						break;
					case 4: // release  phase
						n->amplitude -=
							n->amp_release_step;
						if(n->amplitude < itoFTYPE(0)) {
							n->amp_release_step = 0;
							n->amp_release_steps = 0;
						}
						
						n->amp_release_steps--;
						if(n->amp_release_steps < 0) {
							n->amplitude = itoFTYPE(0);
							n->active = 0;
						}
						break;
					}
				}
				
				{ /* signal generation */
					FTYPE val;
					int c;

					for(c = 0; c < oc; c++) {
						val = mulFTYPE(in[t * ic + c], n->amplitude);

						val = mulFTYPE(
							(itoFTYPE(1) - dry_fp),
							val);
						
						out[t * oc + c] += mulFTYPE(val, volume);
					}
				}
			}
		}
		{
			FTYPE val;
			int c;
			
			for(c = 0; c < oc; c++) {
				val = mulFTYPE(in[t * ic + c], dry_fp);
				out[t * oc + c] += mulFTYPE(val, volume);
			}
		}
	}
}

void delete(void *data) {
	/* free instance data here */
	free(data);
}
