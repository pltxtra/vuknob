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

// this define forces grooveiator to do additional calculations in fixed point mode that are otherwise still floating.
#define FILTER_USING_FIXED_POINT

#include <math.h>
#include "dynlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

USE_SATANS_MATH

typedef struct _FboxData {
	int midiC;

	int filter_type; // 0 = lopass, 1 = hipass
	
	float cutoff, resonance;
	FTYPE coef[5];
	float freq;
	FTYPE hist_x[2*2];
	FTYPE hist_y[2*2];
} FboxData;

#define __DO_NON_LINEAR_CUTOFF

void calc_filter(FboxData *ld) {
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

#ifdef __DO_NON_LINEAR_CUTOFF
	omega = (ld->cutoff / 118.32); omega *= omega;
	omega /= ld->freq;
#else
	omega = ld->cutoff / ld->freq;
#endif
	
	sn = SAT_SIN_SCALAR(omega);
	cs = SAT_COS_SCALAR(omega);

	alpha = sn / ld->resonance;

	if(alpha < 0.0f) {
		alpha = 0.0; // if alpha is < 0.0 we will get bad sounds...
	}

#ifdef FILTER_USING_FIXED_POINT
	fx_cs = ftoFTYPE(cs);
	fx_alpha = ftoFTYPE(alpha);
	
	switch(ld->filter_type) {
	case 0:
	default:
		fx_b1 = ftoFTYPE(1.0f) - fx_cs;
		fx_b0 = fx_b2 = divFTYPE(fx_b1, ftoFTYPE(2.0f));
		break;
	case 1:
		fx_b1 = -(ftoFTYPE(1.0f) + fx_cs);
		fx_b0 = fx_b2 = -(divFTYPE(fx_b1, ftoFTYPE(2.0f)));
		break;
	}		
	
	fx_a0 = ftoFTYPE(1.0f) + fx_alpha;
	fx_a1 = mulFTYPE(ftoFTYPE(-2.0), fx_cs);
	fx_a2 = ftoFTYPE(1.0f) - fx_alpha;

	ld->coef[0] = divFTYPE(fx_b0, fx_a0);
	ld->coef[1] = divFTYPE(fx_b1, fx_a0);
	ld->coef[2] = divFTYPE(fx_b2, fx_a0);
	ld->coef[3] = divFTYPE(-fx_a1, fx_a0);
	ld->coef[4] = divFTYPE(-fx_a2, fx_a0);
#else
	switch(ld->filter_type) {
	case 0:
	default:
		b1 = 1.0f - cs;
		b0 = b2 = b1 / 2.0f;
		break;
	case 1:
		b1 = -(1.0f + cs);
		b0 = b2 = -b1 / 2.0f;
		break;
	}
	
	a0 = 1.0f + alpha;
	a1 = -2.0f * cs;
	a2 = 1.0f - alpha;

	ld->coef[0] = ftoFTYPE(b0 / a0);
	ld->coef[1] = ftoFTYPE(b1 / a0);
	ld->coef[2] = ftoFTYPE(b2 / a0);
	ld->coef[3] = ftoFTYPE(-a1 / a0);
	ld->coef[4] = ftoFTYPE(-a2 / a0);
#endif
}

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	FboxData *data = (FboxData *)malloc(sizeof(FboxData));
	memset(data, 0, sizeof(FboxData));

	data->midiC = 0;

	// filter stuff
	SETUP_SATANS_MATH(mt);
	data->cutoff = 10000.0;
	data->resonance = 2.0;
	data->freq = 44100.0;
	data->filter_type = 0;
	calc_filter(data);
	
	/* return pointer to instance data */
	return (void *)data;
}
 
void *get_controller_ptr(MachineTable *mt, void *data,
			 const char *name,
			 const char *group) {
	FboxData *ld = (FboxData *)data;

	if(ld == NULL) return NULL;
	
	if(strcmp("midiChannel", name) == 0)
		return &(ld->midiC);

	if(strcmp("cutoff", name) == 0)
		return &(ld->cutoff);

	if(strcmp("resonance", name) == 0)
		return &(ld->resonance);
	
	if(strcmp("type", name) == 0)
		return &(ld->filter_type);
	
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	FboxData *ld = (FboxData *)data;

	SignalPointer *midiS = mt->get_input_signal(mt, "midi");
	SignalPointer *s = mt->get_input_signal(mt, "Stereo");
	SignalPointer *os = mt->get_output_signal(mt, "Stereo");

	if(os == NULL )
		return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);
	int oc = mt->get_signal_channels(os);

	if(s == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < ol; t++) {
			ou[t * 2 + 0] = itoFTYPE(0);
			ou[t * 2 + 1] = itoFTYPE(0);
		}
		return;		
	}
	
	void **midi = NULL;

	FTYPE *in = mt->get_signal_buffer(s);
	int ic = mt->get_signal_channels(s);

	if(midiS != NULL) midi = mt->get_signal_buffer(midiS);
	
	ld->freq = (float)mt->get_signal_frequency(os);

	int c;
	int i;
	int new_valu = -1;
	FTYPE d, tmp;

	calc_filter(ld);

	for(i = 0; i < ol; i++) {		
		if(midi != NULL && midi[i] != NULL) {
			MidiEvent *mev = (MidiEvent *)midi[i];

			if(
				(mev != NULL)
				&&	
				((mev->data[0] & 0xf0) == MIDI_CONTROL_CHANGE)
				&&
				((mev->data[0] & 0x0f) == ld->midiC)
				) {
				new_valu = mev->data[2];				
			}
		}

		for(c = 0; c < oc; c++) {
			d = in[i * ic + c];

			tmp =
				mulFTYPE(ld->coef[0], d) +
				mulFTYPE(ld->coef[1], ld->hist_x[c * 2]) +
				mulFTYPE(ld->coef[2], ld->hist_x[c * 2 + 1]) +
				mulFTYPE(ld->coef[3], ld->hist_y[c * 2]) +
				mulFTYPE(ld->coef[4], ld->hist_y[c * 2 + 1]);

			ld->hist_y[c * 2 + 1] = ld->hist_y[c * 2];
			ld->hist_y[c * 2] = tmp;
			ld->hist_x[c * 2 + 1] = ld->hist_x[c * 2];
			ld->hist_x[c * 2] = d;

			ou[i * oc + c] = tmp;
		}
	}
	if(new_valu != -1) {
		ld->cutoff =
			15000.0 *
			(1.0 - pow(1.0 - (((float)new_valu) / 255.0), 0.4));
	}
}

void delete(void *data) {
	/* free instance data here */
	free(data);
}
