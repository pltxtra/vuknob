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

/* $Id: sampler.c,v 1.4 2008/09/14 20:48:18 pltxtra Exp $ */

#include "dynlib.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

// this define forces grooveiator to do additional calculations in fixed point mode that are otherwise still floating.
#define FILTER_USING_FIXED_POINT

#include <fixedpointmath.h>

#define SAMPLER_CHANNEL 0
#define POLYPHONY 12

USE_SATANS_MATH

typedef enum Resolution _Resolution;

typedef struct note_struct {
	int active; /* !0 == active, 0 == inactive */
	int note; /* midi note */
	int note_on; // boolean value. This is used to make sure we only care for the FIRST received note off

	_Resolution resolution;
	
	int channels;
	void *data;

	int i; /* which static signal to play */

	ufp24p8_t t, t_max, t_step; /* static signal time, length and time step/output sample */

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

} note_t;

typedef struct sampler_instance {
	int midi_channel;
	float volume;
	fp16p16_t frequency[256];
	note_t note[POLYPHONY]; // POLYPHONY concurrent samples
	int program; // current program, used as index to static signal table
	int fil_enable; // if 1, then enable filter envelope, otherwise disable filtering
	
	float amp_attack, amp_hold, amp_decay, amp_sustain, amp_release; 
	float fil_attack, fil_hold, fil_decay, fil_sustain, fil_release; 
	
	float cutoff, resonance;
	float freq;
} sampler_t;

inline void calc_filter(sampler_t *ld, note_t *coef) {
	float alpha, omega, sn, cs;
//	float a0, a1, a2, b0, b1, b2;

#ifdef FILTER_USING_FIXED_POINT
	FTYPE fx_b0, fx_b1, fx_b2, fx_a0, fx_a1, fx_a2, fx_alpha, fx_cs;;
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
	sampler_t *sampler = (sampler_t *)malloc(sizeof(sampler_t));;
	memset(sampler, 0, sizeof(sampler_t));

	sampler->midi_channel = SAMPLER_CHANNEL;
	sampler->volume = 0.9;
	
	/* Calculate frequencies */
	int n;
	for(n = 0; n < 256; n++) {
		sampler->frequency[n] =
			ftofp16p16(440.0 * pow(2, (n - 69.0) / 12.0));
	}

	/* envelope */
	sampler->amp_attack = 0.05;
	sampler->amp_hold = 0.01;
	sampler->amp_decay = 0.2;
	sampler->amp_sustain = 0.6;
	sampler->amp_release = 0.25;
	sampler->fil_enable = 0;
	sampler->fil_attack = 0.01;
	sampler->fil_hold = 0.01;
	sampler->fil_decay = 1.0;
	sampler->fil_sustain = 1.0;
	sampler->fil_release = 5.0; 
	
	/* filter stuff */
	SETUP_SATANS_MATH(mt);
	sampler->cutoff = 11000.0;
	sampler->resonance = 0.0;
	sampler->freq = 44100.0;
	
	/* return pointer to instance data */
	return (void *)sampler;
}
 
void *get_controller_ptr(MachineTable *mt, void *void_sampler,
			 const char *name,
			 const char *group) {
	sampler_t *sampler = (sampler_t *)void_sampler;
	if(strcmp(name, "midiChannel") == 0) {
		return &(sampler->midi_channel);
	} else if(strcmp(name, "volume") == 0) {
		return &(sampler->volume);
	} else if(strcmp(name, "program") == 0) {
		return &(sampler->program);
	} else if(strcmp("cutoff", name) == 0) {
		return &(sampler->cutoff);
	} else if(strcmp("resonance", name) == 0) {
		return &(sampler->resonance);
	} else if(strcmp("ampAttack", name) == 0) {
		return &(sampler->amp_attack);
	} else if(strcmp("ampHold", name) == 0) {
		return &(sampler->amp_hold);
	} else if(strcmp("ampDecay", name) == 0) {
		return &(sampler->amp_decay);
	} else if(strcmp("ampSustain", name) == 0) {
		return &(sampler->amp_sustain);
	} else if(strcmp("ampRelease", name) == 0) {
		return &(sampler->amp_release);
	} else if(strcmp("enableFilter", name) == 0) {
		return &(sampler->fil_enable);
	} else if(strcmp("filAttack", name) == 0) {
		return &(sampler->fil_attack);
	} else if(strcmp("filHold", name) == 0) {
		return &(sampler->fil_hold);
	} else if(strcmp("filDecay", name) == 0) {
		return &(sampler->fil_decay);
	} else if(strcmp("filSustain", name) == 0) {
		return &(sampler->fil_sustain);
	} else if(strcmp("filRelease", name) == 0) {
		return &(sampler->fil_release);
	} 

	return NULL;
}

void reset(MachineTable *mt, void *void_sampler) {
	sampler_t *sampler = (sampler_t *)void_sampler;
	int n_k;
	for(n_k = 0; n_k < POLYPHONY; n_k++) {
		note_t *n = &(sampler->note[n_k]);
		n->active = 0;
	}
}

#define SAMPLES_PER_FILTER_UPDATE (1024 - 1)
void execute(MachineTable *mt, void *void_sampler) {
	sampler_t *sampler = (sampler_t *)void_sampler;

	SignalPointer *outsig = NULL;
	SignalPointer *insig = NULL;

	insig = mt->get_input_signal(mt, "midi");
	if(insig == NULL)
		return;
	outsig = mt->get_output_signal(mt, "Mono");
	if(outsig == NULL)
		return;
	
	void **midi_in = (void **)mt->get_signal_buffer(insig);
	int midi_l = mt->get_signal_samples(insig);
	MidiEvent *mev = NULL;
			
	FTYPE *out =
		(FTYPE *)mt->get_signal_buffer(outsig);
	int out_l = mt->get_signal_samples(outsig);

	if(midi_l != out_l)
		return; // we expect equal lengths..

	float Fs = (float)mt->get_signal_frequency(outsig);
	sampler->freq = Fs;

	FTYPE volume = ftoFTYPE(sampler->volume);

	int t, n_k;

	for(n_k = 0; n_k < POLYPHONY; n_k++) {
		if(sampler->note[n_k].active) {
			SignalPointer *sp = mt->get_static_signal(sampler->note[n_k].i);
			sampler->note[n_k].data = (sp == NULL) ? NULL : mt->get_signal_buffer(sp);
		}
	}

#ifdef THIS_IS_A_MOCKERY
	int mock_max_d = 0;
	FTYPE mock_max_val = itoFTYPE(0);
#endif
					
	for(t = 0; t < out_l; t++) {
		// check for midi events
		mev = midi_in[t];
		
		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_PROGRAM_CHANGE)
		   &&
		   ((mev->data[0] & 0x0f) == sampler->midi_channel)
			) {
			sampler->program = mev->data[1];
		}

		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_ON)
		   &&
		   ((mev->data[0] & 0x0f) == sampler->midi_channel)
			) {
			int note = mev->data[1];

			float velocity;
			velocity = (float)mev->data[2];
#ifdef THIS_IS_A_MOCKERY
			printf("  velocity: %f\n", FTYPEtof(velocity));
#endif


			for(n_k = 0; n_k < POLYPHONY; n_k++) {
				if(!(sampler->note[n_k].active)) {
					SignalPointer *sample = NULL;
					
					note_t *n = &(sampler->note[n_k]);

					n->active = 1;
					n->note_on = 1;
					n->note = note;
					n->amplitude = mulFTYPE(velocity, volume);

					n->i = sampler->program;
					sample =
						mt->get_static_signal(n->i);

					if(sample != NULL) {
						float sf = (float)mt->get_signal_frequency(sample);

						n->t = itoufp24p8(0);
						
						n->channels =
							mt->get_signal_channels(sample);

						n->resolution = mt->get_signal_resolution(sample);
						n->t_max = itoufp24p8(mt->get_signal_samples(sample));
						sf /= Fs;
						
						n->t_step =
							mulfp16p16(
								ftofp16p16(sf),
								divfp16p16(sampler->frequency[note],
									   sampler->frequency[60]) /* C4 == note 60 */
								);
						n->t_step = n->t_step >> 8; // we calculated using fp16p16, we need to shift it to fp24p8..
						n->data =
							mt->get_signal_buffer(sample);

						// amplitude stuff
						n->amp_phase = 0;
						n->amplitude = ftoFTYPE(0.0);
						n->amp_attack_steps = 1 +
							(int)((float)sampler->amp_attack * (float)Fs);
						n->amp_attack_step =
							ftoFTYPE((velocity / 127.0) /
								  n->amp_attack_steps);
						n->amp_hold_steps = 1 +
							(int)((float)sampler->amp_hold * (float)Fs);
						n->amp_decay_steps = 1 +
							(int)((float)sampler->amp_decay * (float)Fs);
						n->amp_decay_step =
							ftoFTYPE((1.0 - sampler->amp_sustain) *
								  (velocity / 127.0) /
								  n->amp_decay_steps);
						
						// filter stuff
						n->fil_phase = 0;
						n->filter = ftoFTYPE(0.0);
						n->fil_attack_steps = 1 +
							(int)((float)sampler->fil_attack * (float)Fs);
						n->fil_attack_step =
							ftoFTYPE((velocity / 127.0) /
								  n->fil_attack_steps);
						n->fil_hold_steps = 1 +
							(int)((float)sampler->fil_hold * (float)Fs);
						n->fil_decay_steps = 1 +
							(int)((float)sampler->fil_decay * (float)Fs);
						n->fil_decay_step = 
							ftoFTYPE((1.0 - sampler->fil_sustain) *
								  (velocity / 127.0) /
								  n->fil_decay_steps);
						n->hist_x[0] = itoFTYPE(0);
						n->hist_x[1] = itoFTYPE(0);
						n->hist_y[0] = itoFTYPE(0);
						n->hist_y[1] = itoFTYPE(0);

					} else {
						n->active = 0;
					}
					
					break;
				}
			}
		}

		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_OFF)
		   &&
		   ((mev->data[0] & 0x0f) == sampler->midi_channel)
			) {
			int note = mev->data[1];
			float velocity = (float)(mev->data[2]);
			velocity = velocity / 127.0;
			for(n_k = 0; n_k < POLYPHONY; n_k++) {
				if((sampler->note[n_k].active) &&
				   (sampler->note[n_k].note == note) &&
				   (sampler->note[n_k].note_on == 1)) {
					note_t *n = &(sampler->note[n_k]);

					n->note_on = 0;

					n->amp_phase = 4;
					n->amp_release_steps = 1 +
						sampler->amp_release * Fs / (velocity + 1.0);
					n->amp_release_step =
						ftoFTYPE(FTYPEtof(n->amplitude) /
							  (float)(n->amp_release_steps));

					n->fil_phase = 4;
					n->fil_release_steps = 1 +
						sampler->fil_release * Fs / (velocity + 1.0);
					n->fil_release_step =
						ftoFTYPE(FTYPEtof(n->filter) /
							  (float)(n->fil_release_steps));
					break;
				}
			}
		}

		/* zero out */
		out[t] = itoFTYPE(0);

		/* process active notes */
		for(n_k = 0; n_k < POLYPHONY; n_k++) {
			note_t *n = &(sampler->note[n_k]);
			int channels = n->channels;
			if(n->active && n->data != NULL) {
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

				if((n->t & SAMPLES_PER_FILTER_UPDATE) == 0)
					calc_filter(sampler, n);

				FTYPE filter_tmp;
				FTYPE val = itoFTYPE(0);
				switch(n->resolution) {
					// never care
				case _PTR: case _MAX_R: break;

					// the rest
				case _8bit:
				case _32bit:
					// not implemented
					break;
				case _fl32bit:
				{
					float *d = (float *)(n->data);
					val = ftoFTYPE(d[ufp24p8toi(n->t) * (int)channels]);
				}
					break;
				case _fx8p24bit:
				{
					fp8p24_t *d = (fp8p24_t *)(n->data);
					fp8p24_t tmp = d[ufp24p8toi(n->t) * (int)channels];

#ifdef __SATAN_USES_FXP
					val = tmp;
#else
					val = fp8p24tof(tmp);
#endif
				}
					break;
				case _16bit:
				{
#ifdef __SATAN_USES_FXP
					int16_t *d = (int16_t *)(n->data);
					int32_t *tmp = (int32_t *)&val;
					*tmp = (int32_t)d[ufp24p8toi(n->t) * (int)channels];
					*tmp = *tmp << 7;

#ifdef THIS_IS_A_MOCKERY
					mock_max_d = d[ufp24p8toi(n->t) * (int)channels] > mock_max_d ?
						d[ufp24p8toi(n->t) * (int)channels] : mock_max_d;
					mock_max_val = val > mock_max_val ? val : mock_max_val;
#endif

#else
					int16_t *d = (int16_t *)(n->data);
					val = (float)d[ufp24p8toi(n->t) * (int)channels];

					val = val / (4.0f * 32768.0f);  

#ifdef THIS_IS_A_MOCKERY
					mock_max_d = d[ufp24p8toi(n->t) * (int)channels] > mock_max_d ?
						d[ufp24p8toi(n->t) * (int)channels] : mock_max_d;
					mock_max_val = val > mock_max_val ? val : mock_max_val;
#endif
#endif
				}
				break;
				}

				val = mulFTYPE(val, n->amplitude);

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
				
				out[t] += sampler->fil_enable ? 
					mulFTYPE(filter_tmp, volume) :
					mulFTYPE(val, volume);
				
				n->t += n->t_step;
				if(n->t >= n->t_max) {
					n->active = 0;
				}
			}
		}
	}

#ifdef THIS_IS_A_MOCKERY
	printf(" max d: %d\n", mock_max_d);
	printf(" max val: %f\n", FTYPEtof(mock_max_val));
#endif

}

void delete(void *data) {
	/* free instance data here */
	free(data);
}
