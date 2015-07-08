/*
 * VuKNOB
 *  Copyright (C) 2014 by Anton Persson
 *
 * http://www.vuknob.com/
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either version 3
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

//#define __DO_DYNLIB_DEBUG
#include "dynlib_debug.h"

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

#define MAX_CHORUS_VOICES 4
#define MAX_CHORUS_DELAY 400

typedef struct _ChorusData {
	struct delayLineMono *dl1;

	// general configuration
	FTYPE general_gain;
	int voices;

	// per voice configuration
	float depth[MAX_CHORUS_VOICES];
	float rate[MAX_CHORUS_VOICES];
	float offset[MAX_CHORUS_VOICES];
	float gain[MAX_CHORUS_VOICES];
	float pan[MAX_CHORUS_VOICES];

	// playback data
	int time[MAX_CHORUS_VOICES]; // current time index, in samples.. between 0 and period length..

	int Fs_CURRENT ;
} ChorusData;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	ChorusData *d = (ChorusData *)malloc(sizeof(ChorusData));

	if(d == NULL) return NULL;

	memset(d, 0, sizeof(ChorusData));

	d->voices = MAX_CHORUS_VOICES;
	d->general_gain = ftoFTYPE(1.0);

	int k = 0;
	for(k = 0; k < MAX_CHORUS_VOICES; k++) {
		d->depth[k] = 0.5f;
		d->rate[k] = 0.08f + (0.04f / (float)MAX_CHORUS_VOICES) * ((float)k);
		d->offset[k] = 40.0f + (((float)k) * ((float)MAX_CHORUS_DELAY - 40.0f) / ((float)MAX_CHORUS_VOICES));
		d->gain[k] = 0.9f - (0.6f / (float)MAX_CHORUS_VOICES) * ((float)k);
		d->pan[k] = (1 - 2 * (k % 2)) * (0.9f / (float)MAX_CHORUS_VOICES) * ((float)k);
	}

	SETUP_SATANS_MATH(mt);

	/* return pointer to instance data */
	return (void *)d;
}

void recreate_delayline(ChorusData *d, int frequency) {
	if(d->dl1)
		delayLineMonoFree(d->dl1);

	// Create delay line, with the maximu delay
	int dl1_length;
	dl1_length = MAX_CHORUS_DELAY * frequency;
	dl1_length /= 1000; // MAX_CHORUS_DELAY is in milliseconds, so we need to divide by 1000...
	d->dl1 = create_delayLineMono(dl1_length);
}

void delete(void *data) {
	ChorusData *d = (ChorusData *)data;
	/* free instance data here */
	if(d->dl1)
		delayLineMonoFree(d->dl1);
	free(d);
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	ChorusData *d = (ChorusData *)void_info;

	if(strcmp("General", group) == 0) {
		if(strcmp("voices", name) == 0)
			return &(d->voices);
		if(strcmp("gain", name) == 0)
			return &(d->general_gain);
		return NULL;
	}

	int voice_id = -1;
	if(strcmp("Voice 1", group)) voice_id = 0;
	if(strcmp("Voice 2", group)) voice_id = 1;
	if(strcmp("Voice 3", group)) voice_id = 2;
	if(strcmp("Voice 4", group)) voice_id = 3;

	if(voice_id >= 0 && voice_id < 4) {
		if(strcmp("depthPercent", name) == 0)
			return &(d->depth[voice_id]);
		if(strcmp("rateHz", name) == 0)
			return &(d->rate[voice_id]);
		if(strcmp("offsetMilliS", name) == 0)
			return &(d->offset[voice_id]);
		if(strcmp("gain", name) == 0)
			return &(d->gain[voice_id]);
		if(strcmp("pan", name) == 0)
			return &(d->pan[voice_id]);
	}

	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	ChorusData *d = (ChorusData *)data;

	SignalPointer *s = mt->get_input_signal(mt, "Mono");
	SignalPointer *os = mt->get_output_signal(mt, "Mono");
	SignalPointer *os_stereo = mt->get_output_signal(mt, "Stereo");

	if(os == NULL) return;

	FTYPE *ou = mt->get_signal_buffer(os);
	FTYPE *ou_stereo = mt->get_signal_buffer(os_stereo);
	int ol = mt->get_signal_samples(os);

	if(s == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < ol; t++) {
			ou[t] = itoFTYPE(0);
		}
		return;
	}

	FTYPE *in = mt->get_signal_buffer(s);

	float freq = (float)mt->get_signal_frequency(os);
	if(d->Fs_CURRENT != (int)freq) {
		d->Fs_CURRENT = (int)freq;
		recreate_delayline(d, d->Fs_CURRENT);
	}


#ifdef __SATAN_USES_FLOATS

#define DELAYT_t float
#define ftoDELAYT(A) (A)
#define itoDELAYT(A) (A)
#define DELAYTtoi(A) (A)
#define mulDELAYT(A,B) (A * B)

#else

#define DELAYT_t fp16p16_t
#define ftoDELAYT(A) ftofp16p16(A)
#define itoDELAYT(A) ftofp16p16(A)
#define DELAYTtoi(A) fp16p16toi(A)
#define mulDELAYT(A,B) mulfp16p16(A,B)

#endif

	FTYPE x, y;
	DELAYT_t y1, y2;


#define SIN(x,f) ftoDELAYT(SAT_SIN_SCALAR(((x)%(f))/(float)(f)))

	int k;

	FTYPE gain[MAX_CHORUS_VOICES];
	FTYPE pan_gain_l[MAX_CHORUS_VOICES];
	FTYPE pan_gain_r[MAX_CHORUS_VOICES];
	DELAYT_t variable_offset[MAX_CHORUS_VOICES];
	DELAYT_t base_offset[MAX_CHORUS_VOICES];
	int period[MAX_CHORUS_VOICES];

	for(k = 0; k < MAX_CHORUS_VOICES; k++) {
		if(d->pan[k] < 0) {
			pan_gain_l[k] = ftoFTYPE(1.0f);
			pan_gain_r[k] = ftoFTYPE(1.0f + d->pan[k]);
		} else {
			pan_gain_l[k] = ftoFTYPE(1.0f - d->pan[k]);
			pan_gain_r[k] = ftoFTYPE(1.0f);
		}
		gain[k] = ftoFTYPE(d->gain[k]);
		int variable_offset_i = d->offset[k] * freq * d->depth[k] / 4000.0f;
		int base_offset_i = d->offset[k] * freq / 2000.f - variable_offset_i;

		variable_offset[k] = itoDELAYT(variable_offset_i);
		base_offset[k] = itoDELAYT(base_offset_i);

		float periodf = freq / d->rate[k];
		period[k] = (int)periodf;
	}

	int i, i_s;

	for(i = 0, i_s = 0; i < ol; i++, i_s += 2) {
		x = in[i];

		// put into the delay line
		delayLineMonoPut(d->dl1, x);

		// mono + stereo data
		FTYPE current[3] = {
			ftoFTYPE(0.0f), // mono
			ftoFTYPE(0.0f), // left
			ftoFTYPE(0.0f)  // right
		};

		for(k = 0; k < d->voices; k++) {
			// calculate offset
			DELAYT_t offset = SIN(d->time[k], period[k]); d->time[k] = (d->time[k] + 1) % period[k];
			offset = base_offset[k] + mulDELAYT(offset, variable_offset[k]);
			int offset_i = DELAYTtoi(offset);
			// Get delayed sample at offset
#ifdef __SATAN_USES_FLOATS
			// we get two values to do a linear interpolation
			y1 = delayLineMonoGetOffsetNoMove(d->dl1, offset_i);
			y2 = delayLineMonoGetOffsetNoMove(d->dl1, offset_i + 1);

			y2 = y2 - y1;

			int t_offset = offset;
			y2 = y1 + (offset - (float)t_offset) * y2;

			y = y2;

#else
			// we get two values to do a linear interpolation
			y = delayLineMonoGetOffsetNoMove(d->dl1, offset_i);
			y1 = y >> 8; // shift from fp8p24 to DELAYT
			y = delayLineMonoGetOffsetNoMove(d->dl1, offset_i + 1);
			y2 = y >> 8; // shift from fp8p24 to DELAYT

			y2 = y2 - y1;

			offset = 0x0000ffff & offset; // only keep fraction
			y2 = y1 + mulDELAYT(offset, y2);
			y = y2 << 8; // shift from DELAYT to fp8p24
#endif
			current[0] += mulFTYPE(y, gain[k]);
			current[1] += mulFTYPE(current[0], pan_gain_l[k]);
			current[2] += mulFTYPE(current[0], pan_gain_r[k]);
		}
		// just step the delay line, actual value is ignored
		(void) delayLineMonoGet(d->dl1);

		// mix in dry
		ou[i] = mulFTYPE(d->general_gain, current[0] + x);
		ou_stereo[i_s + 0] = mulFTYPE(d->general_gain, current[1] + x);
		ou_stereo[i_s + 1] = mulFTYPE(d->general_gain, current[2] + x);
	}
}
