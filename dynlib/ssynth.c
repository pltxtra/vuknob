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

#include <math.h>
#include "dynlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fixedpointmath.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MIDI_CHANNEL 0

typedef struct _sline {
	unsigned char note;
	int type;

	char hold_on;
	fp8p24_t volume;
	fp16p16_t attack;
	fp16p16_t decay;
	fp16p16_t sustain;
	fp16p16_t release;
} sline;

typedef struct _ssynths {
	sline line;

	int index; // time index of waveform
	int period; // Lenght of one period of the oscillator
	fp8p24_t wa_step; // scaling factor so we can calculate the correct
			// frequency of the
	fp8p24_t amplitude; // current amplitude
	fp8p24_t am_step; // change of amplitude / sample
	int elimit; // at wich index in the future that
		    // we should recalculate the am_step
	int envelope; // 0 = attack, 1 = delay, 2 = sustain, 3 = release, 4 = zero until next note
	fp16p16_t decay, sustain, release;

	float volume;
	int midi_channel;
} ssynths;

USE_SATANS_MATH
#define PI M_PI

fp16p16_t note_table[256]; // frequency table for standard MIDI notes

#define SSYNTH_TABLE_LENGTH 22050
fp8p24_t ssynth_table[SSYNTH_TABLE_LENGTH];

float ssynth_a(float x) {
	if(x < 0.0) x = 0.0f;
	if(x > 1.0) x = 1.0f;
	float x2 = x * 2;
	float doub = (1 - x) * (1 - x) * (SAT_SIN_SCALAR(x2) + (1 - x2) * 0.5 * SAT_SIN_SCALAR(x2 * 4) + 0.25 * SAT_SIN_SCALAR(x2 * 8));
	float x4 = x * 4 + 0.25;
	float quad = (1 - x) * (1 - x) * (SAT_SIN_SCALAR(x4) + (1 - x4) * 0.5 * SAT_SIN_SCALAR(x4 * 4) + 0.25 * SAT_SIN_SCALAR(x4 * 8));
	return 0.5 * (
		(SAT_SIN_SCALAR(x) * doub) +
		(SAT_SIN_SCALAR(x4) * quad) +
		(1 - x) * (1 - x) * (SAT_SIN_SCALAR(x) + (1 - x) * 0.5 * SAT_SIN_SCALAR(x * 4) + 0.25 * SAT_SIN_SCALAR(x * 8))
		);
}

void *init(MachineTable *mt, const char *name) {
	float divisor, factor;
	ssynths *info = (ssynths *)malloc(sizeof(ssynths));;
	memset(info, 0, sizeof(ssynths));

	SETUP_SATANS_MATH(mt);

	info->midi_channel = MIDI_CHANNEL;
	info->volume = 1.0;
	
	int n;

	for(n = 0; n < 256; n++) {
		note_table[n] =
			ftofp16p16(440.0 * pow(2.0, (n - 69) / 12.0));
	}

	divisor = 1.0 / (float)SSYNTH_TABLE_LENGTH;
	for(n = 0; n < SSYNTH_TABLE_LENGTH; n++) {
		factor = (float)n;
		factor *= divisor;
		ssynth_table[n] = ftofp8p24(ssynth_a(factor));
	}

	int frequency = 0;
	frequency = mt->get_signal_frequency(mt->get_output_signal(mt, "Mono"));
	info->index = 0;
	info->wa_step = ftofp8p24(1.0f / (frequency / 10.0));
	info->amplitude = 0.0;
	info->decay = ftofp16p16(1.0);
	info->release = ftofp16p16(1.0);
	info->am_step = ftofp8p24(0.0);
	info->elimit = 101;
	info->envelope = 0;
	info->period = frequency / 10.0;

	return (void *)info;
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	ssynths *info = (ssynths *)void_info;
	if(strcmp(name, "midiChannel") == 0) {
		return &(info->midi_channel);
	} else if(strcmp(name, "volume") == 0) {
		return &(info->volume);
	}
	
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *void_info) {
	ssynths *info = (ssynths *)void_info;
	int x;
	fp16p16_t wa_step, amplitude, am_step;

	int bfrsize, index, elimit, envelope, period;

	sline *line;

	SignalPointer *s = NULL;
	SignalPointer *sig = NULL;
      	
	void **in = NULL;
	MidiEvent *mev = NULL;
	
	s = mt->get_input_signal(mt, "midi");
	if(s == NULL)
		return;
	
	in = (void **)mt->get_signal_buffer(s);

	sig = mt->get_output_signal(mt, "Mono");
	if(sig == NULL) return;
	
	bfrsize = mt->get_signal_samples(sig);

	//thing that change
	index = info->index;
	wa_step = info->wa_step;
	amplitude = info->amplitude;
	am_step = info->am_step;
	elimit = info->elimit;
	envelope = info->envelope;
	period = info->period;

	float Fs = (float)mt->get_signal_frequency(sig);

	float timeSlice_f = ((float)SSYNTH_TABLE_LENGTH) / Fs;
	fp16p16_t timeSlice = ftofp16p16(timeSlice_f);
	FTYPE *data = (FTYPE *)mt->get_signal_buffer(sig);
	fp8p24_t volume = ftofp8p24(info->volume);

	line = &(info->line);

	for(x = 0; x < bfrsize; x++) {
		int k;

		mev = in[x];
		if(mev != NULL) {
			int note;
			
			if(
				((mev->data[0] & 0xf0) == MIDI_NOTE_ON)
				&&
				((mev->data[0] & 0x0f) == info->midi_channel)
				) {
				line->note = mev->data[1]; 
 
				fp8p24_t velocity;
				int *tmpv = &velocity;
				*tmpv = ((int)mev->data[2]) << 16;
				
				line->attack = ftofp16p16(0.05);
				line->decay = ftofp16p16(0.1);
				line->sustain = ftofp16p16(0.5);
				line->release = ftofp16p16(0.1);
				line->volume = mulfp8p24(volume, velocity);
				line->hold_on = 1;
				
				note = line->note;
				
 				elimit = fp16p16toi(mulfp16p16(line->attack, itofp16p16(4000)));
				am_step = divfp16p16(line->volume, itofp16p16(elimit));
				amplitude = itofp16p16(0);
				wa_step = mulfp16p16(
						timeSlice,
						note_table[note]);
				envelope = 0;

				period = (int)((float)(Fs / fp16p16tof(note_table[note])));

				info->decay = line->decay;
				info->sustain = line->sustain;
				info->release = line->release;
			} 
			if(
				((mev->data[0] & 0xf0) == MIDI_NOTE_OFF)
				&&
				((mev->data[0] & 0x0f) == info->midi_channel)
				&&
				((mev->data[1] == line->note))
				) {
				line->hold_on = 0;
			} 
		}
		
		k = index % period;

		fp8p24_t out;
		fp16p16_t table_index_f = mulfp16p16(itofp16p16(k), wa_step);
		int table_index = fp16p16toi(table_index_f);

		table_index = table_index % SSYNTH_TABLE_LENGTH;
		out = mulfp8p24(amplitude, ssynth_table[table_index]);
//		table_index = index;
//		index = (index + 1) % SSYNTH_TABLE_LENGTH;
//		out = ssynth_table[x * d];//table_index];

#ifdef __SATAN_USES_FXP
		data[x] = out;
#else
		data[x] = fp8p24tof(out);
#endif
		
		amplitude += am_step;

		/* Hold Comment A:
		 * This is a special case for sustain (hold)
		 * when the MIDI note off message is received
		 * the hold flag is set to zero, when that has
		 * happened and the envelope has reached 1
		 * the elimit is set to 1 and envelope to two
		 * which will cause the execution to enter
		 * the following if()-block (Hold Comment B)
		 */
		if((envelope == 2) && (line->hold_on == 0)) {
			envelope = 3;
			elimit = 0;
		}
			
		/* Hold Comment B:
		 * This block is reached before the envelope is "sustain",
		 * or after hold_on has been set to zero, se comment A.
		 */
		if((envelope != 2) && (elimit-- == 0)) { // new envelope mode
			envelope++;
			switch(envelope) {
			case 1: // Decay
				elimit = fp8p24toi(mulfp16p16(itofp16p16(4000), info->decay));
			{
				fp16p16_t tmp = ftofp16p16(0.1);
				am_step = mulfp16p16((-line->volume), tmp);

				tmp = itofp16p16(elimit);
				am_step = divfp16p16(am_step, tmp);
			}
//				am_step = divfp16p16(mulfp16p16((-line->volume), ftofp16p16(0.1)), itofp16p16(elimit));

				break;
			case 2: // Sustain
				elimit = 0;
				am_step = 0;
				break;

			case 3: // this is actually never reached
				// because of the new "hold"
				// stuff in Hold Comment A
				break;
				
			case 4: // Release
				elimit = fp16p16toi(mulfp16p16(itofp16p16(4000), info->release));
			{
				fp16p16_t tmp = ftofp16p16(0.9);
				am_step = mulfp16p16((-amplitude), tmp);
				tmp = itofp16p16(elimit);
				am_step = divfp16p16(am_step, tmp);
			}
				break;
			case 5: // Silence
				elimit = 4000;
				am_step = itofp16p16(0);
				amplitude = itofp16p16(0);
				break;
			}
		}
		index = (index + 1) % (1000 * period);
	}
	info->index = index;
	info->wa_step = wa_step;
	info->amplitude = amplitude;
	info->am_step = am_step;
	info->elimit = elimit;
	info->envelope = envelope;
	info->period = period;
}

void delete(void *data) {
	free(data);
}

