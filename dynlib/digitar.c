/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2011 by Anton Persson
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

/*
 * This is an implementation of the following algorithm:
 * http://en.wikipedia.org/wiki/Karplus-Strong_string_synthesis
 *
 */

#define MIDI_CHANNEL 0
#define DIGITAR_NR_OF_STRINGS 6

#define MAX_PERIOD_LEN 2048

#include "dynlib.h"
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include "dynlib.h"
USE_SATANS_MATH

#include "libfilter.c"

#include <fixedpointmath.h>

#include "liboscillator.c"

#include <math.h>
#include <stdio.h>

#define NOTE_TABLE_LENGTH 256
FTYPE note_table[NOTE_TABLE_LENGTH];
int period_table[NOTE_TABLE_LENGTH];

typedef struct _digitarNote {
	int active;

	int free; // note is "on"
	int note;

	int spread_delay;
	
	int period; // used as index into dl0
	int t;
	FTYPE voltage, voltage_step;
	
	struct xPassFilterMono *xpf0;
	struct xPassFilterMono *xpf1;
	struct delayLineMono *dl0; // digitar algorithm delay

	struct delayLineMono *dl1; // string spread delay

	FTYPE mem; // feedback
	FTYPE amplitude;

	int signal_type;
	
	int silence_length;
} digitarNote_t;

typedef struct _digitar {
	digitarNote_t note[DIGITAR_NR_OF_STRINGS];
	
	int base_signal_type;
	int preFilter;

	int midi_channel;

	FTYPE preFilterCutoff;
	FTYPE postFilterCutoff;
	FTYPE gain;

	float spread;
} digitar_t;

void calc_note_freq() {
	/* note table */
	int n;
#ifdef THIS_IS_A_MOCKERY
	printf("__LOS_Fs_INVERSE: %f\n", __LOS_Fs_INVERSE);
#endif
	for(n = 0; n < NOTE_TABLE_LENGTH; n++) {
		float f;
		f = 440.0 * pow(2.0, (n - 69) / 12.0);
		note_table[n] = LOS_CALC_FREQUENCY(f);
		period_table[n] = (1.0f / __LOS_Fs_INVERSE) / f;
#ifdef THIS_IS_A_MOCKERY
		printf("n[%3d] f[%f], note[%f], period[%d]\n", n, f, note_table[n], period_table[n]);
#endif
	}
}

void delete(void *data) {
	digitar_t *digitar = (digitar_t *)data;
	
	// clear and clean up everything here
	int x;
	for(x = 0; x < DIGITAR_NR_OF_STRINGS; x++) {
		if(digitar->note[x].xpf0 != NULL)
			xPassFilterMonoFree(digitar->note[x].xpf0);
		if(digitar->note[x].xpf1 != NULL)
			xPassFilterMonoFree(digitar->note[x].xpf1);
		if(digitar->note[x].dl0 != NULL)
			delayLineMonoFree(digitar->note[x].dl0);
		if(digitar->note[x].dl1 != NULL)
			delayLineMonoFree(digitar->note[x].dl1);
	}
	free(digitar);	
}

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	digitar_t *digitar = (digitar_t *)malloc(sizeof(digitar_t));

	if(digitar == NULL) return NULL;

	memset(digitar, 0, sizeof(digitar_t));
			
	// init digitar here
#ifdef THIS_IS_A_MOCKERY
	digitar->base_signal_type = 1;
#else
	digitar->base_signal_type = 1;
#endif
	digitar->preFilter = 1;

	digitar->spread = 0.18f;
	
	digitar->gain = ftoFTYPE(1.2);
	digitar->preFilterCutoff = ftoFTYPE(0.065f);
	digitar->postFilterCutoff = ftoFTYPE(0.147f);

	int x;
	for(x = 0; x < DIGITAR_NR_OF_STRINGS; x++) {
		digitar->note[x].free = 1;

		digitar->note[x].xpf0 = create_xPassFilterMono(mt, 0);
		if(digitar->note[x].xpf0 == NULL)
			goto fail;
		xPassFilterMonoSetResonance(digitar->note[x].xpf0, itoFTYPE(0));
		
		digitar->note[x].xpf1 = create_xPassFilterMono(mt, 0);
		if(digitar->note[x].xpf1 == NULL)
			goto fail;
		xPassFilterMonoSetResonance(digitar->note[x].xpf1, itoFTYPE(0));

		digitar->note[x].dl0 = create_delayLineMono(MAX_PERIOD_LEN);
		if(digitar->note[x].dl0 == NULL)
			goto fail;
		
		digitar->note[x].dl1 = create_delayLineMono(48000);
		if(digitar->note[x].dl1 == NULL)
			goto fail;
	}
	
	digitar->midi_channel = MIDI_CHANNEL;

	/* return pointer to instance digitar */
	return (void *)digitar;

fail:
	delete(digitar);
	
	return NULL;
}
 
void *get_controller_ptr(MachineTable *mt, void *data,
			 const char *name,
			 const char *group) {
	digitar_t *digitar = (digitar_t *)data;
	if(strcmp(name, "signalBase") == 0) {
		return &(digitar->base_signal_type);
	} else if(strcmp(name, "preFilter") == 0) {
		return &(digitar->preFilter);
	} else if(strcmp(name, "preFilterCutoff") == 0) {
		return &(digitar->preFilterCutoff);
	} else if(strcmp(name, "postFilterCutoff") == 0) {
		return &(digitar->postFilterCutoff);
	} else if(strcmp(name, "gain") == 0) {
		return &(digitar->gain);
	} else if(strcmp(name, "spread") == 0) {
		return &(digitar->spread);
	}
	
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	//do nothing
}

void execute(MachineTable *mt, void *data) {
	digitar_t *digitar = (digitar_t *)data;
	
	SignalPointer *outsig = NULL;
	
	outsig = mt->get_output_signal(mt, "Mono");
	if(outsig == NULL) {
		return;
	}

	SignalPointer *s = NULL;
      	
	void **in = NULL;
	int Fs = 44100;
	
	MidiEvent *mev = NULL;
	
	s = mt->get_input_signal(mt, "midi");
	if(s == NULL)
		return;
	
	in = (void **)mt->get_signal_buffer(s);
	Fs = mt->get_signal_frequency(s);

	static int Fs_CURRENT = 0;
	if(Fs_CURRENT != Fs) {
		los_set_Fs(mt, Fs);
		libfilter_set_Fs(Fs);
		Fs_CURRENT = Fs;
		calc_note_freq(Fs);
	}
	
	FTYPE gain = digitar->gain;
	FTYPE *out =
		(FTYPE *)mt->get_signal_buffer(outsig);
	int out_l = mt->get_signal_samples(outsig);

	int t, k;

	static int kount = 0;

	// precalc spread value
	float spread = (float)Fs;
	spread *= digitar->spread;
	spread /= DIGITAR_NR_OF_STRINGS;
	for(k = 0; k < DIGITAR_NR_OF_STRINGS; k++) {
		digitar->note[k].spread_delay = spread * k;
	}
	
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

	for(t = 0; t < out_l; t++) {
		mev = in[t];
		if(mev != NULL) {
			if(
				((mev->data[0] & 0xf0) == MIDI_NOTE_ON)
				&&
				((mev->data[0] & 0x0f) == digitar->midi_channel)
				) {

				int note = mev->data[1]; 

				FTYPE velocity;
#ifdef __SATAN_USES_FXP
				int *tmpv = &velocity;
				*tmpv = ((int)mev->data[2]) << 16;
#else
				velocity = ((FTYPE)mev->data[2]) / 256.0f;
#endif
				int selected_k = -1;
				for(k = 0; k < DIGITAR_NR_OF_STRINGS; k++) {
					digitarNote_t *n = &(digitar->note[k]);

					if(n->free) {
						n->free = 0;
						selected_k = k;
						break;
					}
				}

				if(selected_k != -1) {
					digitarNote_t *n = &(digitar->note[selected_k]);

					// clear run data
					n->active = 1;
					n->t = 0;
					n->mem = itoFTYPE(0);	
					delayLineMonoClear(n->dl0);

					// set note data
					n->note = note;

					n->voltage = itoFTYPE(0);
					n->voltage_step = note_table[note];
					
					n->period = period_table[note];
					if(n->period >= MAX_PERIOD_LEN)
						n->period = MAX_PERIOD_LEN - 1;

					n->amplitude = velocity;

					n->signal_type = digitar->base_signal_type;

					// recalc filter
					xPassFilterMonoSetCutoff(
						n->xpf0,
						digitar->postFilterCutoff
						);
					xPassFilterMonoRecalc(n->xpf0);
					
					xPassFilterMonoSetCutoff(
						n->xpf1,
						digitar->preFilterCutoff
						);
					xPassFilterMonoRecalc(n->xpf1);
				}
			}
			if(
				((mev->data[0] & 0xf0) == MIDI_NOTE_OFF)
				&&
				((mev->data[0] & 0x0f) == digitar->midi_channel)
				) {
				int note = mev->data[1];

				for(k = 0; k < DIGITAR_NR_OF_STRINGS; k++) {
					digitarNote_t *n = &(digitar->note[k]);

					if(!(n->free) && n->note == note) {
						n->free = 1;
						n->note = -1;
					}
				}
			}

		}

		out[t] = itoFTYPE(0);

		for(k = 0; k < DIGITAR_NR_OF_STRINGS; k++) {
			digitarNote_t *n = &(digitar->note[k]);
			
			if(n->active) {
				FTYPE o;					
				FTYPE v = itoFTYPE(0);

				if(n->t < n->period) {
					switch(n->signal_type) {
					case 0:
						v = los_src_sin(n->voltage);
						break;
					case 1:
						v = los_src_noise(n->voltage);
						break;
					case 2:
						v = los_src_square(n->voltage);
						break;
					}
					n->voltage += n->voltage_step;
					v = mulFTYPE(n->amplitude, v);
				} else v = itoFTYPE(0);

#ifdef THIS_IS_A_MOCKERY
				int_out_a[t] = v;
#endif
				if(digitar->preFilter) {
					xPassFilterMonoPut(n->xpf1, v);
					v = xPassFilterMonoGet(n->xpf1);
				}
				
				o = n->mem + v;
				
				delayLineMonoPut(n->dl0, mulFTYPE(o, ftoFTYPE(0.95)));
				xPassFilterMonoPut(n->xpf0, delayLineMonoGetOffset(n->dl0, n->period));
				
				n->mem = xPassFilterMonoGet(n->xpf0);
				
#ifdef THIS_IS_A_MOCKERY
				int_out_b[t] = n->mem;
#endif

				n->t += 1;

				delayLineMonoPut(n->dl1, o);
				out[t] += mulFTYPE(delayLineMonoGetOffset(n->dl1, n->spread_delay), gain);

				// deactivate after a period of silence
				if(o == itoFTYPE(0))
					n->silence_length++;
				else
					n->silence_length = 0;

				if(n->silence_length >= n->period)
					n->active = 0;
			}
			
		}

		kount = (kount + 1) % (44100 * 4);
	}
}
