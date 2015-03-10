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

// this define forces grooveiator to do additional calculations in fixed point mode that are otherwise still floating.
#define FILTER_USING_FIXED_POINT

#include "dynlib.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <fixedpointmath.h>

#define GROOVEIATOR_CHANNEL 0
#define POLYPHONY 6

USE_SATANS_MATH

typedef enum Resolution _Resolution;

typedef struct note_struct {
	int active; /* !0 == active, 0 == inactive */
	int note; /* midi note */
	int note_on; // boolean value. This is used to make sure we only care for the FIRST received note off

	int period_A, period_B;

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
	
	// Filter
	int fil_phase;
	FTYPE filter;
	int fil_attack_steps;
	FTYPE fil_attack_step;
	int fil_hold_steps;
	int fil_decay_steps;
	FTYPE fil_decay_step;
	int fil_release_steps;
	FTYPE fil_release_step;

	FTYPE hist_x[2];
	FTYPE hist_y[2];
	FTYPE coef[5];
	
	int t;
} note_t;

typedef struct grooveiator_instance {
	int midi_channel;
	float volume;
	note_t note[POLYPHONY]; // POLYPHONY concurrent tones
	int program; // current program, use in the future?

	int enable_filter; 
	
	int wave_A, wave_B;
	float wave_mix;
	int wave_transpose, wave_detune;
	
	float amp_attack, amp_hold, amp_decay, amp_sustain, amp_release; 
	float fil_attack, fil_hold, fil_decay, fil_sustain, fil_release; 
	
	float cutoff, resonance;
	float freq;

} grooveiator_t;

float note_table[257]; // freqency table for standard MIDI notes, 257 is because we need a "spare" when calculating centi-notes

inline void calc_filter(grooveiator_t *ld, note_t *coef) {
	float alpha, omega, sn, cs;

#ifdef FILTER_USING_FIXED_POINT
	FTYPE fx_b0, fx_b1, fx_b2, fx_a0, fx_a1, fx_a2, fx_alpha, fx_cs;;
#else
	float a0, a1, a2, b0, b1, b2;
#endif
	
	// These limits the cutoff frequency and resonance to
	// reasoneable values.
	if (ld->cutoff < 4.0f) { ld->cutoff = 4.0f; };
	if (ld->cutoff > 14000.0f) { ld->cutoff = 14000.0f; };
	if (ld->resonance < 1.0f) { ld->resonance = 1.0f; };
	if (ld->resonance > 127.0f) { ld->resonance = 127.0f; };

	omega = (500.0f + (FTYPEtof(coef->filter) * ld->cutoff)) / ld->freq;

//	omega = (0.5 + 0.5 * SAT_SIN_SCALAR(
//		(coef->t % 11000) / 11000.0))  * ld->cutoff / ld->freq;

//	omega = ld->cutoff / ld->freq;
	sn = SAT_SIN_SCALAR(omega);
	cs = SAT_COS_SCALAR(omega);

	alpha = sn / ld->resonance;

	if(alpha < 0.0f) {
		alpha = 0.0; // if alpha is < 0.0 we will get bad sounds...
	}


#ifdef FILTER_USING_FIXED_POINT
	fx_cs = ftoFTYPE(cs);
	fx_alpha = ftoFTYPE(alpha);
	
	fx_b1 = ftoFTYPE(1.0f) - fx_cs;
	fx_b0 = fx_b2 = divFTYPE(fx_b1, ftoFTYPE(2.0f));

	fx_a0 = ftoFTYPE(1.0f) + fx_alpha;
	fx_a1 = mulFTYPE(ftoFTYPE(-2.0), fx_cs);
	fx_a2 = ftoFTYPE(1.0f) - fx_alpha;

	coef->coef[0] = divFTYPE(fx_b0, fx_a0);
	coef->coef[1] = divFTYPE(fx_b1, fx_a0);
	coef->coef[2] = divFTYPE(fx_b2, fx_a0);
	coef->coef[3] = divFTYPE(-fx_a1, fx_a0);
	coef->coef[4] = divFTYPE(-fx_a2, fx_a0);
#else
	b1 = 1.0f - cs;
	b0 = b2 = b1 / 2.0f;

	a0 = 1.0f + alpha;
	a1 = -2.0f * cs;
	a2 = 1.0f - alpha;

	coef->coef[0] = ftoFTYPE(b0 / a0);
	coef->coef[1] = ftoFTYPE(b1 / a0);
	coef->coef[2] = ftoFTYPE(b2 / a0);
	coef->coef[3] = ftoFTYPE(-a1 / a0);
	coef->coef[4] = ftoFTYPE(-a2 / a0);
#endif
}

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	grooveiator_t *grooveiator = (grooveiator_t *)malloc(sizeof(grooveiator_t));

	if(grooveiator == NULL) return NULL;
	
	memset(grooveiator, 0, sizeof(grooveiator_t));

	grooveiator->midi_channel = GROOVEIATOR_CHANNEL;
	grooveiator->volume = 0.35;

	/* envelope */
	grooveiator->amp_attack = 0.05;
	grooveiator->amp_hold = 0.01;
	grooveiator->amp_decay = 0.2;
	grooveiator->amp_sustain = 0.6;
	grooveiator->amp_release = 0.25; 
	grooveiator->fil_attack = 0.02;
	grooveiator->fil_hold = 0.0;
	grooveiator->fil_decay = 0.1;
	grooveiator->fil_sustain = 0.1;
	grooveiator->fil_release = 0.1; 
	
	/* filter stuff */
	SETUP_SATANS_MATH(mt);
	grooveiator->enable_filter = 1;
	grooveiator->cutoff = 10000.0;
	grooveiator->resonance = 6.0;
	grooveiator->freq = 44100.0;
	
	/* note table */
	int n;
	for(n = 0; n < 257; n++) {
		note_table[n] =
			440.0 * pow(2.0, (n - 69) / 12.0);
	}
	
	/* return pointer to instance data */
	return (void *)grooveiator;
}
 
void *get_controller_ptr(MachineTable *mt, void *void_grooveiator,
			 const char *name,
			 const char *group) {
	grooveiator_t *grooveiator = (grooveiator_t *)void_grooveiator;
	if(strcmp(name, "midiChannel") == 0) {
		return &(grooveiator->midi_channel);
	} else if(strcmp(name, "volume") == 0) {
		return &(grooveiator->volume);
	} else if(strcmp("cutoff", name) == 0) {
		return &(grooveiator->cutoff);
	} else if(strcmp("enable", name) == 0) {
		return &(grooveiator->enable_filter);
	} else if(strcmp("resonance", name) == 0) {
		return &(grooveiator->resonance);
	} else if(strcmp("ampAttack", name) == 0) {
		return &(grooveiator->amp_attack);
	} else if(strcmp("ampHold", name) == 0) {
		return &(grooveiator->amp_hold);
	} else if(strcmp("ampDecay", name) == 0) {
		return &(grooveiator->amp_decay);
	} else if(strcmp("ampSustain", name) == 0) {
		return &(grooveiator->amp_sustain);
	} else if(strcmp("ampRelease", name) == 0) {
		return &(grooveiator->amp_release);
	} else if(strcmp("filAttack", name) == 0) {
		return &(grooveiator->fil_attack);
	} else if(strcmp("filHold", name) == 0) {
		return &(grooveiator->fil_hold);
	} else if(strcmp("filDecay", name) == 0) {
		return &(grooveiator->fil_decay);
	} else if(strcmp("filSustain", name) == 0) {
		return &(grooveiator->fil_sustain);
	} else if(strcmp("filRelease", name) == 0) {
		return &(grooveiator->fil_release);
	} else if(strcmp("wave_A", name) == 0) {
		return &(grooveiator->wave_A);
	} else if(strcmp("wave_B", name) == 0) {
		return &(grooveiator->wave_B);
	} else if(strcmp("wave_mix", name) == 0) {
		return &(grooveiator->wave_mix);
	} else if(strcmp("wave_transpose", name) == 0) {
		return &(grooveiator->wave_transpose);
	} else if(strcmp("wave_detune", name) == 0) {
		return &(grooveiator->wave_detune);
	} 
	
	return NULL;
}

void reset(MachineTable *mt, void *void_grooveiator) {
	grooveiator_t *grooveiator = (grooveiator_t *)void_grooveiator;
	int n_k;
	for(n_k = 0; n_k < POLYPHONY; n_k++) {
		note_t *n = &(grooveiator->note[n_k]);
		n->active = 0;
	}
}

#define SAMPLES_PER_FILTER_UPDATE (1024 - 1)
void execute(MachineTable *mt, void *void_grooveiator) {
	grooveiator_t *grooveiator = (grooveiator_t *)void_grooveiator;

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

	float Fs = (float)mt->get_signal_frequency(outsig);
	grooveiator->freq = Fs;

	FTYPE volume = ftoFTYPE(grooveiator->volume);
	FTYPE wave_mix = ftoFTYPE(grooveiator->wave_mix);

	int t, n_k;

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
		// check for midi events
		MidiEvent *mev = (MidiEvent *)midi_in[t];
		if(
			(mev != NULL)
			&&	
			((mev->data[0] & 0xf0) == MIDI_CONTROL_CHANGE)
			&&
			((mev->data[0] & 0x0f) == grooveiator->midi_channel)
			) {
			int valu = mev->data[2];
			grooveiator->cutoff =
				15000.0 *
				(1.0 - pow(1.0 - (((float)valu) / 255.0), 0.4));
		}
		
		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_PROGRAM_CHANGE)
		   &&
		   ((mev->data[0] & 0x0f) == grooveiator->midi_channel)
			) {
			grooveiator->program = mev->data[1];
		}

		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_ON)
		   &&
		   ((mev->data[0] & 0x0f) == grooveiator->midi_channel)
			) {
			int note = mev->data[1];
			float velocity = (float)(mev->data[2]);
			for(n_k = 0; n_k < POLYPHONY; n_k++) {
				if(!(grooveiator->note[n_k].active)) {
					note_t *n = &(grooveiator->note[n_k]);

					n->active = 1;
					n->t = 0;
					n->note = note;
					n->note_on = 1;

					n->period_A = (int)(Fs / (float)(note_table[note]));
					int t_note = note + grooveiator->wave_transpose;
					n->period_B = (int)(Fs / (float)(note_table[t_note] +
									 grooveiator->wave_detune *
									 ((note_table[t_note+1] - note_table[t_note])/100.0)
								    ));

					// just make sure we don't hit notes we can't possibly play...
					if(n->period_A == 0 ||
					   n->period_B == 0) {
						n->active = 0;
						DYNLIB_DEBUG("note %d out of playable range!\n", note);
					}
					
					// amplitude stuff
					n->amp_phase = 0;
					n->amplitude = ftoFTYPE(0.0);
					n->amp_attack_steps = 1 +
						(int)((float)grooveiator->amp_attack * (float)Fs);
					n->amp_attack_step =
						ftoFTYPE((velocity / 127.0) /
							  n->amp_attack_steps);
					n->amp_hold_steps = 1 +
						(int)((float)grooveiator->amp_hold * (float)Fs);
					n->amp_decay_steps = 1 +
						(int)((float)grooveiator->amp_decay * (float)Fs);
					n->amp_decay_step =
						ftoFTYPE((1.0 - grooveiator->amp_sustain) *
							  (velocity / 127.0) /
							  n->amp_decay_steps);

					// filter stuff
					n->fil_phase = 0;
					n->filter = ftoFTYPE(0.0);
					n->fil_attack_steps = 1 +
						(int)((float)grooveiator->fil_attack * (float)Fs);
					n->fil_attack_step =
						ftoFTYPE((velocity / 127.0) /
							  n->fil_attack_steps);
					n->fil_hold_steps = 1 +
						(int)((float)grooveiator->fil_hold * (float)Fs);
					n->fil_decay_steps = 1 +
						(int)((float)grooveiator->fil_decay * (float)Fs);
					n->fil_decay_step = 
						ftoFTYPE((1.0 - grooveiator->fil_sustain) *
						(velocity / 127.0) /
						n->fil_decay_steps);
					n->hist_x[0] = itoFTYPE(0);
					n->hist_x[1] = itoFTYPE(0);
					n->hist_y[0] = itoFTYPE(0);
					n->hist_y[1] = itoFTYPE(0);

					break;
				}
			}
		}

		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_OFF)
		   &&
		   ((mev->data[0] & 0x0f) == grooveiator->midi_channel)
			) {
			int note = mev->data[1];
			float velocity = (float)(mev->data[2]);
			velocity = velocity / 127.0;
			for(n_k = 0; n_k < POLYPHONY; n_k++) {
				if((grooveiator->note[n_k].active) &&
				   (grooveiator->note[n_k].note == note) &&
				   (grooveiator->note[n_k].note_on)) {
					note_t *n = &(grooveiator->note[n_k]);

					n->note_on = 0;
					n->amp_phase = 4;
					n->amp_release_steps = 1 +
						grooveiator->amp_release * Fs / (velocity + 1.0);
					n->amp_release_step =
						ftoFTYPE(FTYPEtof(n->amplitude) /
							  (float)(n->amp_release_steps));

					n->fil_phase = 4;
					n->fil_release_steps = 1 +
						grooveiator->fil_release * Fs / (velocity + 1.0);
					n->fil_release_step =
						ftoFTYPE(FTYPEtof(n->filter) /
							  (float)(n->fil_release_steps));
					break;
				}
			}
		}

		/* zero out */
		out[t] = itoFTYPE(0);

#ifdef __SATAN_USES_FXP
		// << 8 is to convert fp16p16 to fp8p24
#define SAW(r,x,f) r = (( divfp16p16( itofp16p16(((x)%(f))<<1), itofp16p16(f)) - itofp16p16(1) ) << 8)
#else
		int saw_tempura;
#define SAW(r,x,f) { saw_tempura = x % f; r = (2.0f * ((float)saw_tempura / (float)f) - 1.0f); }
#endif

#define SIN(x,f) ftoFTYPE(SAT_SIN_SCALAR(((x)%(f))/(float)(f)))
#define SQR(x,f) ((((x)%(f)) > (f>>1)) ? ftoFTYPE(-1.0) : ftoFTYPE(1.0))
#define COSHALF(x,f) ftoFTYPE(SAT_COS_SCALAR(((x)%(f))/(float)(2*f)))
		
		/* process active notes */
		for(n_k = 0; n_k < POLYPHONY; n_k++) {
			note_t *n = &(grooveiator->note[n_k]);
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
				
				{ /* filter amplitude parameters */
					switch(n->fil_phase) {
					case 0: // attack phase
						n->filter +=
							n->fil_attack_step;
						n->fil_attack_steps--;
						if(n->fil_attack_steps < 0)
							n->fil_phase++;
						break;
					case 1: // hold phase
						n->fil_hold_steps--;
						if(n->fil_hold_steps < 0)
							n->fil_phase++;
						break;
					case 2: // decay phase
						n->filter -=
							n->fil_decay_step;
						n->fil_decay_steps--;
						if(n->fil_decay_steps < 0)
							n->fil_phase++;
						break;
					case 3: // sustain  phase
						/* hold here until a NOTE_OFF message arrives */
						break;
					case 4: // release  phase
						if(n->filter > itoFTYPE(0) && n->fil_release_steps > 0) {
							n->filter -=
								n->fil_release_step;
							n->fil_release_steps--;
						}
						if(n->filter < itoFTYPE(0))
							n->filter = itoFTYPE(0);
						
						break;
					}
				}

				if(((n->t & SAMPLES_PER_FILTER_UPDATE) == 0) && (grooveiator->enable_filter)) {
					calc_filter(grooveiator, n);
				}

				{ /* signal generation and filtration */
					FTYPE filter_tmp;
					float tempura2 = 0;
					int tempura1;
					FTYPE valX = itoFTYPE(0), valY = itoFTYPE(0), val;

					switch(grooveiator->wave_A) {
					case 0:
						tempura1 = n->t + n->t % n->period_A;
						SAW(tempura2,tempura1, n->period_A);
						SAW(valX,n->t,n->period_A);
						valX = valX - mulFTYPE(ftoFTYPE(0.5), tempura2);

#ifdef THIS_IS_A_MOCKERY
						int_out_a[t] = valX;
#endif
						break;
					case 1:
						valX = SIN(n->t,n->period_A) - mulFTYPE(ftoFTYPE(0.5),
											 SIN (n->t + n->t % n->period_A, n->period_A));
						break;
					case 2:
						valX = SQR(n->t,n->period_A) - mulFTYPE(ftoFTYPE(0.5),
											 SQR (n->t + n->t % n->period_A, n->period_A));

						break;
					case 3:
						valX = COSHALF(n->t,n->period_A);
						break;
					}
					switch(grooveiator->wave_B) {
					case 0:
						tempura1 = n->t + n->t % n->period_A;
						SAW(tempura2,tempura1, n->period_A);
						SAW(valX,n->t,n->period_B);
						valX = valX - mulFTYPE(ftoFTYPE(0.5), tempura2);
						break;
					case 1:
						valY = SIN(n->t,n->period_B) - mulFTYPE(ftoFTYPE(0.5),
											 SIN (n->t + n->t % n->period_B, n->period_B));
						break;
					case 2:
						valY = SQR(n->t,n->period_B) - mulFTYPE(ftoFTYPE(0.5),
											 SQR (n->t + n->t % n->period_B, n->period_B));
						break;
					case 3:
						valY = COSHALF(n->t,n->period_B);
						break;
					}
					
					val = mulFTYPE(itoFTYPE(1) - wave_mix, valX) +
						mulFTYPE(wave_mix, valY);
					val = mulFTYPE(val, n->amplitude);
#ifdef THIS_IS_A_MOCKERY
					int_out_b[t] = valX * n->amplitude;
#endif

					if(grooveiator->enable_filter) {
						filter_tmp =
							mulFTYPE(n->coef[0], val) +
							mulFTYPE(n->coef[1], n->hist_x[0]) +
							mulFTYPE(n->coef[2], n->hist_x[1]) +
							mulFTYPE(n->coef[3], n->hist_y[0]) +
							mulFTYPE(n->coef[4], n->hist_y[1]);
						
						n->hist_y[1] = n->hist_y[0];
						n->hist_y[0] = filter_tmp;
						n->hist_x[1] = n->hist_x[0];
						n->hist_x[0] = val;
					} else {
						filter_tmp = val;
					}
					
					out[t] += mulFTYPE(filter_tmp, volume);
					
					n->t++;
				}
			}	
		}
	}
}

void delete(void *data) {
	/* free instance data here */
	free(data);
}
