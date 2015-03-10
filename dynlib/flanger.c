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

typedef struct _FlangerData {
	struct delayLineMono *dl1;

	FTYPE mem; // feedback

	float depth;
	float rate;
	float offset;

	float feedback;
	float gain;

	int time; // current time index, in samples.. between 0 and period length..
} FlangerData;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	FlangerData *d = (FlangerData *)malloc(sizeof(FlangerData));

	if(d == NULL) goto fail0;
	
	memset(d, 0, sizeof(FlangerData));

	int dl1;

	d->depth = 0.5f;
	d->rate = 0.25f;
	d->feedback = 0.0f;
	d->offset = 10.0f;
	d->gain = 0.8;
	
	// Create delay line, with the maximu delay
	dl1 = 40 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 40 is in milliseconds, so we need to divide by 1000...
	d->dl1 = create_delayLineMono(dl1);
	if(d->dl1 == NULL) goto fail1;
	
	SETUP_SATANS_MATH(mt);
	
	/* return pointer to instance data */
//NO_FAIL:
	return (void *)d;

	// fail stack
fail1:
	free(d);
fail0:
	return NULL;
}

void delete(void *data) {
	FlangerData *d = (FlangerData *)data;
	/* free instance data here */
	if(d->dl1)
		delayLineMonoFree(d->dl1);
	free(d);
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	FlangerData *d = (FlangerData *)void_info;

	if(strcmp("depthPercent", name) == 0)
		return &(d->depth);
	if(strcmp("rateHz", name) == 0)
		return &(d->rate);
	if(strcmp("feedback", name) == 0)
		return &(d->feedback);
	if(strcmp("offsetMilliS", name) == 0)
		return &(d->offset);
	if(strcmp("gain", name) == 0)
		return &(d->gain);
	
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	FlangerData *d = (FlangerData *)data;

	SignalPointer *s = mt->get_input_signal(mt, "Mono");
	SignalPointer *os = mt->get_output_signal(mt, "Mono");

	if(os == NULL) return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
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

	int i;

#ifdef __SATAN_USES_FLOATS
#undef ftofp16p16
#undef itofp16p16
#undef fp16p16_t
#undef fp16p16toi
#undef mulfp16p16
#define ftofp16p16(A) A
#define itofp16p16(A) A
#define fp16p16_t float
#define fp16p16toi(A) A
#define mulfp16p16(A,B) (A * B)
#endif

	FTYPE x, y;
	fp16p16_t y1, y2;
	FTYPE feedback = ftoFTYPE(d->feedback);
	FTYPE gain = ftoFTYPE(d->gain);

	
#define SIN(x,f) ftofp16p16(SAT_SIN_SCALAR(((x)%(f))/(float)(f)))

	int variable_offset_i = d->offset * freq * d->depth / 4000.0f;
	int base_offset_i = d->offset * freq / 2000.f - variable_offset_i;

	fp16p16_t variable_offset = itofp16p16(variable_offset_i);
	fp16p16_t base_offset = itofp16p16(base_offset_i);
	fp16p16_t offset;
	int offset_i;
	float periodf = freq / d->rate;
	int period = (int)periodf;

	
	for(i = 0; i < ol; i++) {
		x = in[i];

		// put into first pure delay
		delayLineMonoPut(d->dl1, x + mulFTYPE(d->mem, feedback));

		// calculate offset
		offset = SIN(d->time, period); d->time = (d->time + 1) % period;
		offset = base_offset + mulfp16p16(offset, variable_offset);
		offset_i = fp16p16toi(offset);

		// Get delayed sample at offset
#ifdef __SATAN_USES_FLOATS
		y1 = delayLineMonoGetOffsetNoMove(d->dl1, offset_i);
		y2 = delayLineMonoGetOffset(d->dl1, offset_i + 1);

		y2 = y2 - y1;
		
		int t_offset = offset;
		y2 = y1 + (offset - (float)t_offset) * y2;

		y = y2;
		
#else
		y = delayLineMonoGetOffsetNoMove(d->dl1, offset_i);
		y1 = y >> 8; // shift from fp8p24 to fp16p16
		y = delayLineMonoGetOffset(d->dl1, offset_i + 1);
		y2 = y >> 8; // shift from fp8p24 to fp16p16

		y2 = y2 - y1;
		
		offset = 0x0000ffff & offset; // only keep fraction
		y2 = y1 + mulfp16p16(offset, y2);
		y = y2 << 8; // shift from fp16p16 to fp8p24
#endif

		
		// mix in dry
		d->mem = ou[i] = mulFTYPE(y, gain) + x;
	}	
}

