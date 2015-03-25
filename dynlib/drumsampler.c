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

#include <fixedpointmath.h>

//#define __DO_SATAN_DEBUG
#include "../satan_debug.hh"

#define SAMPLER_CHANNEL 0
#define POLYPHONY 12

typedef enum Resolution _Resolution;

typedef struct note_struct {
	int active; /* !0 == active, 0 == inactive */
	int note; /* midi note */

	_Resolution resolution;
	
	FTYPE amplitude; /* 0<1 */

	int channels;
	void *data;

	int i; /* which static signal to play */

	ufp24p8_t t, t_max, t_step; /* static signal time, length and time step/output sample */
} note_t;

typedef struct sampler_instance {
	int midi_channel;
	float volume;
	fp16p16_t frequency[256];
	note_t note[POLYPHONY]; // POLYPHONY concurrent samples

	int sample_index[12];
	int sample_frequency[12];
	float sample_volume[12];
} sampler_t;

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
	
	/* init default values for sample configuration */
	for(n = 0; n < 12; n++) {
		sampler->sample_index[n] = n;
		sampler->sample_frequency[n] = 44100;
		sampler->sample_volume[n] = 1.0;
	}
	
	/* return pointer to instance data */
	return (void *)sampler;
}
 
int get_sample_number(const char *bfr) {
	if(bfr[0] == 'C' && bfr[1] == '-') return 0;
	if(bfr[0] == 'C' && bfr[1] == '#') return 1;
	if(bfr[0] == 'D' && bfr[1] == '-') return 2;
	if(bfr[0] == 'D' && bfr[1] == '#') return 3;

	if(bfr[0] == 'E' && bfr[1] == '-') return 4;
	if(bfr[0] == 'F' && bfr[1] == '-') return 5;
	if(bfr[0] == 'F' && bfr[1] == '#') return 6;
	if(bfr[0] == 'G' && bfr[1] == '-') return 7;

	if(bfr[0] == 'G' && bfr[1] == '#') return 8;
	if(bfr[0] == 'A' && bfr[1] == '-') return 9;
	if(bfr[0] == 'A' && bfr[1] == '#') return 10;
	if(bfr[0] == 'B' && bfr[1] == '-') return 11;

	return 0;
}
 
void *get_controller_ptr(MachineTable *mt, void *void_sampler,
			 const char *name,
			 const char *group) {
	sampler_t *sampler = (sampler_t *)void_sampler;
	if(strcmp(name, "midiChannel") == 0) {
		return &(sampler->midi_channel);
	} else if(strcmp(name, "volume") == 0) {
		return &(sampler->volume);
	}

	if(strncmp("sampleIndex", name, strlen("sampleIndex")) == 0) {
		int n = get_sample_number(&name[strlen("sampleIndex")]);
		return &(sampler->sample_index[n]);
	}
	if(strncmp("sampleFrequency", name, strlen("sampleFrequency")) == 0) {
		int n = get_sample_number(&name[strlen("sampleFrequency")]);
		return &(sampler->sample_frequency[n]);
	}
	if(strncmp("sampleVolume", name, strlen("sampleVolume")) == 0) {
		int n = get_sample_number(&name[strlen("sampleVolume")]);
		return &(sampler->sample_volume[n]);
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
	
	if(mt->get_signal_resolution(outsig) != FTYPE_RESOLUTION)
		return; // we only work with the current FTYPE resolution
		
	FTYPE *out =
		(FTYPE *)mt->get_signal_buffer(outsig);
	int out_l = mt->get_signal_samples(outsig);

	if(midi_l != out_l)
		return; // we expect equal lengths..

	float Fs = (float)mt->get_signal_frequency(outsig);
	FTYPE volume = ftoFTYPE(sampler->volume);
	fp24p8_t FS_q = ftofp24p8(Fs);

	int _t_unit_, n_k;

	for(n_k = 0; n_k < POLYPHONY; n_k++) {
		if(sampler->note[n_k].active) {
			SignalPointer *sp = mt->get_static_signal(
				sampler->sample_index[sampler->note[n_k].i]
				);

			sampler->note[n_k].data = (sp == NULL) ? NULL : mt->get_signal_buffer(sp);
		}
	}

#ifdef THIS_IS_A_MOCKERY
	FTYPE max_mock = itoFTYPE(0);
#endif
	
	for(_t_unit_ = 0; _t_unit_ < out_l; _t_unit_++) {
		// check for midi events
		mev = midi_in[_t_unit_];
		
		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_ON)
		   &&
		   ((mev->data[0] & 0x0f) == sampler->midi_channel)
			) {
			int note = mev->data[1];
			FTYPE velocity;
			int i_velocity = mev->data[2];
#ifdef __SATAN_USES_FXP
			velocity = ((i_velocity & 0xff) << 16);
#else
			velocity = ((float)i_velocity / 127.0f);
#endif
			
			for(n_k = 0; n_k < POLYPHONY; n_k++) {
				if(!(sampler->note[n_k].active)) {
					SignalPointer *sample = NULL;
					
					note_t *n = &(sampler->note[n_k]);

					n->active = 1;

					n->i = note % 12;
					n->note = note;
					n->amplitude = mulFTYPE(ftoFTYPE(sampler->sample_volume[n->i]), mulFTYPE(velocity, volume));

					sample =
						mt->get_static_signal(sampler->sample_index[n->i]);

					if(sample != NULL && (mt->get_signal_dimension(sample) == _0D)) {
						n->t = itoufp24p8(0);
						
						n->channels =
							mt->get_signal_channels(sample);

						n->resolution = mt->get_signal_resolution(sample);
						n->t_max = itoufp24p8(mt->get_signal_samples(sample));

						fp24p8_t A = itofp24p8(sampler->sample_frequency[n->i]);
						
						n->t_step =
							divfp24p8(
								A,
								FS_q
								);

						n->data =
							mt->get_signal_buffer(sample);
					} else {
						n->active = 0;
					}
					break;
				}
			}
		}
#if 0
		if((mev != NULL)
		   &&
		   ((mev->data[0] & 0xf0) == MIDI_NOTE_OFF)
		   &&
		   ((mev->data[0] & 0x0f) == sampler->midi_channel)
			) {
			int note = mev->data[1];
			for(n_k = 0; n_k < POLYPHONY; n_k++) {
				if((sampler->note[n_k].active) &&
				   (sampler->note[n_k].note == note)) {
					note_t *n = &(sampler->note[n_k]);

					n->active = 0;

					break;
				}
			}
		}
#endif
		/* zero out */
		out[_t_unit_] = 0;

		/* process active notes */
		for(n_k = 0; n_k < POLYPHONY; n_k++) {
			note_t *n = &(sampler->note[n_k]);
			int channels = n->channels;
			if(n->active && n->data != NULL) {

				FTYPE val = 0;
				switch(n->resolution) {
				case _PTR:
				case _MAX_R:
					/* don't care */
					break;
					
				case _8bit:
				case _32bit:
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
#else
					int16_t *d = (int16_t *)(n->data);
					val = (float)d[ufp24p8toi(n->t) * (int)channels];
					val = val / (8.0f * 32768.0f); // the 8.0f is because I made a misstake in the fixed point math block.. so to make sure I don't break the behavior for current users who are transfering from the fixed point to floating point I must compensate here.. If I correct the misstake in the fixed point code the songs users have already produced will sound bad...
#endif
				}
				break;
				}

				out[_t_unit_] += 
					mulFTYPE(
						val,
						n->amplitude);

				n->t += n->t_step;
				if(n->t >= n->t_max) {
					n->active = 0;
				}
			}
		}
#ifdef THIS_IS_A_MOCKERY
		if(out[_t_unit_] > max_mock)
			max_mock = out[_t_unit_];
#endif
	}
#ifdef THIS_IS_A_MOCKERY
	printf(" max_mock: %f\n", FTYPEtof(max_mock));
#endif
}

void delete(void *data) {
	/* free instance data here */
	free(data);
}
