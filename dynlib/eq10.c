/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2011 by Anton Persson
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

#include <math.h>
#include <fixedpointmath.h>
#include "dynlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libfilter.h"

USE_SATANS_MATH

#define CHANNELS 2

typedef struct _EQ10Data {
	int midiC;
	float B0, B1, B2, B3, B4, B5, B6, B7, B8, B9;

	struct bandPassCoeff bpc0, bpc1, bpc2, bpc3, bpc4, bpc5, bpc6 ,bpc7, bpc8, bpc9;

	struct bandPassMem
	bpm0[CHANNELS], bpm1[CHANNELS], bpm2[CHANNELS], bpm3[CHANNELS], bpm4[CHANNELS],
		bpm5[CHANNELS], bpm6[CHANNELS], bpm7[CHANNELS], bpm8[CHANNELS], bpm9[CHANNELS];
	
} EQ10Data;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	EQ10Data *data = (EQ10Data *)malloc(sizeof(EQ10Data));
	memset(data, 0, sizeof(EQ10Data));

	data->midiC = 0;

	// filter stuff
	SETUP_SATANS_MATH(mt);

/* 31 Hz */
	data->B0 = 1.0;
	data->bpc0.alpha = ftoFTYPE(0.000723575);
	data->bpc0.beta = ftoFTYPE(0.49855285);
	data->bpc0.ypsilon = ftoFTYPE(0.998544628);
/* 62 Hz */
	data->B1 = 1.0;
	data->bpc1.alpha = ftoFTYPE(0.001445062);
	data->bpc1.beta = ftoFTYPE(0.497109876);
	data->bpc1.ypsilon = ftoFTYPE(0.997077038);
/* 125 Hz */
	data->B2 = 1.0;
	data->bpc2.alpha = ftoFTYPE(0.002904926);
	data->bpc2.beta = ftoFTYPE(0.494190149);
	data->bpc2.ypsilon = ftoFTYPE(0.994057064);
/* 250 Hz */
	data->B3 = 1.0;
	data->bpc3.alpha = ftoFTYPE(0.005776487);
	data->bpc3.beta = ftoFTYPE(0.488447026);
	data->bpc3.ypsilon = ftoFTYPE(0.987917799);
/* 500 Hz */
	data->B4 = 1.0;
	data->bpc4.alpha = ftoFTYPE(0.011422552);
	data->bpc4.beta = ftoFTYPE(0.477154897);
	data->bpc4.ypsilon = ftoFTYPE(0.975062733);
/* 1000 Hz */
	data->B5 = 1.0;
	data->bpc5.alpha = ftoFTYPE(0.02234653);
	data->bpc5.beta = ftoFTYPE(0.455306941);
	data->bpc5.ypsilon = ftoFTYPE(0.947134157);
/* 2000 Hz */
	data->B6 = 1.0;
	data->bpc6.alpha = ftoFTYPE(0.04286684);
	data->bpc6.beta = ftoFTYPE(0.414266319);
	data->bpc6.ypsilon = ftoFTYPE(0.88311345);
/* 4000 Hz */
	data->B7 = 1.0;
	data->bpc7.alpha = ftoFTYPE(0.079552886);
	data->bpc7.beta = ftoFTYPE(0.340894228);
	data->bpc7.ypsilon = ftoFTYPE(0.728235763);
/* 8000 Hz */
	data->B8 = 1.0;
	data->bpc8.alpha = ftoFTYPE(0.1199464);
	data->bpc8.beta = ftoFTYPE(0.2601072);
	data->bpc8.ypsilon = ftoFTYPE(0.3176087);
/* 16000 Hz */
	data->B9 = 1.0;
	data->bpc9.alpha = ftoFTYPE(0.159603);
	data->bpc9.beta = ftoFTYPE(0.1800994);
	data->bpc9.ypsilon = ftoFTYPE(0.4435172);
	
	/* return pointer to instance data */
	return (void *)data;
}
 
void *get_controller_ptr(MachineTable *mt, void *data,
			 const char *name,
			 const char *group) {
	EQ10Data *ld = (EQ10Data *)data;

	if(ld == NULL) return NULL;
	
	if(strcmp("midiChannel", name) == 0)
		return &(ld->midiC);

	if(strcmp("B0", name) == 0)
		return &(ld->B0);
	if(strcmp("B1", name) == 0)
		return &(ld->B1);
	if(strcmp("B2", name) == 0)
		return &(ld->B2);
	if(strcmp("B3", name) == 0)
		return &(ld->B3);
	if(strcmp("B4", name) == 0)
		return &(ld->B4);
	if(strcmp("B5", name) == 0)
		return &(ld->B5);
	if(strcmp("B6", name) == 0)
		return &(ld->B6);
	if(strcmp("B7", name) == 0)
		return &(ld->B7);
	if(strcmp("B8", name) == 0)
		return &(ld->B8);
	if(strcmp("B9", name) == 0)
		return &(ld->B9);
	
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	EQ10Data *ld = (EQ10Data *)data;

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
	
	FTYPE *in = mt->get_signal_buffer(s);
	int ic = mt->get_signal_channels(s);

	int c;
	int i;
	FTYPE d;

	FTYPE y0, y1, y2, y3, y4, y5, y6, y7, y8, y9;
	FTYPE
		fxB0,
		fxB1,
		fxB2,
		fxB3,
		fxB4,
		fxB5,
		fxB6,
		fxB7,
		fxB8,
		fxB9;
	
	fxB0 = ftoFTYPE(ld->B0);
	fxB1 = ftoFTYPE(ld->B1);
	fxB2 = ftoFTYPE(ld->B2);
	fxB3 = ftoFTYPE(ld->B3);
	fxB4 = ftoFTYPE(ld->B4);
	fxB5 = ftoFTYPE(ld->B5);
	fxB6 = ftoFTYPE(ld->B6);
	fxB7 = ftoFTYPE(ld->B7);
	fxB8 = ftoFTYPE(ld->B8);
	fxB9 = ftoFTYPE(ld->B9);
	
	for(i = 0; i < ol; i++) {		
		for(c = 0; c < oc; c++) {
			d = in[i * ic + c];

			y0 = BANDPASS_IIR_FTYPE_CALC(d,ld->bpc0,ld->bpm0[c]);
			BANDPASS_IIR_FTYPE_SAVE(d,y0,ld->bpm0[c]);
			
			y1 = BANDPASS_IIR_FTYPE_CALC(d,ld->bpc1,ld->bpm1[c]);
			BANDPASS_IIR_FTYPE_SAVE(d,y1,ld->bpm1[c]);

			y2 = BANDPASS_IIR_FTYPE_CALC(d,ld->bpc2,ld->bpm2[c]);
			BANDPASS_IIR_FTYPE_SAVE(d,y2,ld->bpm2[c]);

			y3 = BANDPASS_IIR_FTYPE_CALC(d,ld->bpc3,ld->bpm3[c]);
			BANDPASS_IIR_FTYPE_SAVE(d,y3,ld->bpm3[c]);

			y4 = BANDPASS_IIR_FTYPE_CALC(d,ld->bpc4,ld->bpm4[c]);
			BANDPASS_IIR_FTYPE_SAVE(d,y4,ld->bpm4[c]);

			y5 = BANDPASS_IIR_FTYPE_CALC(d,ld->bpc5,ld->bpm5[c]);
			BANDPASS_IIR_FTYPE_SAVE(d,y5,ld->bpm5[c]);

			y6 = BANDPASS_IIR_FTYPE_CALC(d,ld->bpc6,ld->bpm6[c]);
			BANDPASS_IIR_FTYPE_SAVE(d,y6,ld->bpm6[c]);

			y7 = BANDPASS_IIR_FTYPE_CALC(d,ld->bpc7,ld->bpm7[c]);
			BANDPASS_IIR_FTYPE_SAVE(d,y7,ld->bpm7[c]);

			y8 = BANDPASS_IIR_FTYPE_CALC(d,ld->bpc8,ld->bpm8[c]);
			BANDPASS_IIR_FTYPE_SAVE(d,y8,ld->bpm8[c]);

			y9 = BANDPASS_IIR_FTYPE_CALC(d,ld->bpc9,ld->bpm9[c]);
			BANDPASS_IIR_FTYPE_SAVE(d,y9,ld->bpm9[c]);
			
			ou[i * oc + c]  = mulFTYPE(fxB0, y0);
			ou[i * oc + c] += mulFTYPE(fxB1, y1);
			ou[i * oc + c] += mulFTYPE(fxB2, y2);
			ou[i * oc + c] += mulFTYPE(fxB3, y3);
			ou[i * oc + c] += mulFTYPE(fxB4, y4);
			ou[i * oc + c] += mulFTYPE(fxB5, y5);
			ou[i * oc + c] += mulFTYPE(fxB6, y6);
			ou[i * oc + c] += mulFTYPE(fxB7, y7);
			ou[i * oc + c] += mulFTYPE(fxB8, y8);
			ou[i * oc + c] += mulFTYPE(fxB9, y9);
		}
	}
}

void delete(void *data) {
	/* free instance data here */
	free(data);
}
