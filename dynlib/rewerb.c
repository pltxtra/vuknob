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

typedef struct _RewerbData {
	struct xPassFilterMono *xpf0;
	struct xPassFilterMono *xpf1;
	struct allPassDNestMono *apdnf;
	struct delayLineMono *dl1;
	struct allPassMono *apf;
	struct allPassMono *apf2;
	struct delayLineMono *dl2;
	struct delayLineMono *dl3;
	struct allPassSNestMono *apsnf;
	struct delayLineMono *dl4;
	struct xPassFilterMono *xpf2; // could be band pass as well
	
	FTYPE mem; // feedback

	float hipass_f;
	float lopass_f;
	float wet_level;
	float dry_level;
	float fb_gain;

	int type, last_type;
} RewerbData;

void delete_current_type(RewerbData *d) {
	/* free instance data here */
	if(d->dl4) {
		delayLineMonoFree(d->dl4);
		d->dl4 = NULL;
	}
	if(d->apsnf) {
		allPassSNestMonoFree(d->apsnf);
		d->apsnf = NULL;
	}
	if(d->dl3) {
		delayLineMonoFree(d->dl3);
		d->dl3 = NULL;
	}
	if(d->dl2) {
		delayLineMonoFree(d->dl2);
		d->dl2 = NULL;
	}
	if(d->apf) {
		allPassMonoFree(d->apf);
		d->apf = NULL;
	}
	if(d->apf2) {
		allPassMonoFree(d->apf2);
		d->apf2 = NULL;
	}
	if(d->dl1) {
		delayLineMonoFree(d->dl1);
		d-> dl1= NULL;
	}
	if(d->apdnf) {
		allPassDNestMonoFree(d->apdnf);
		d->apdnf = NULL;
	}
	if(d->xpf2) {
		xPassFilterMonoFree(d->xpf2);
		d->xpf2 = NULL;
	}
	if(d->xpf1) {
		xPassFilterMonoFree(d->xpf1);
		d->xpf1 = NULL;
	}
	if(d->xpf0) {
		xPassFilterMonoFree(d->xpf0);	
		d->xpf0 = NULL;
	}
}
 
int configure_small(MachineTable *mt, RewerbData *d) {
	int dl1, dl2, dl3;

	d->hipass_f = 500.0f;
	d->lopass_f = 6000.0f;
	d->wet_level = 1.0f;
	d->dry_level = 1.0f;
	
	// First a hi pass filter, for entry
	d->xpf0 = create_xPassFilterMono(mt, 1);
	if(d->xpf0 == NULL) goto fail;
	
	// Then a low pass filter, for entry
	d->xpf1 = create_xPassFilterMono(mt, 0);
	if(d->xpf1 == NULL) goto fail;
	
	// Then a delay line
	dl1 = 24 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 24 is in milliseconds, so we need to divide by 1000...
	d->dl1 = create_delayLineMono(dl1);
	if(d->dl1 == NULL) goto fail;
	
	// Then a double nested all pass
	dl1 = 35 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 35 is in milliseconds, so we need to divide by 1000...
	dl2 = 22 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl2 /= 1000; // 22 is in milliseconds, so we need to divide by 1000...
	dl3 = 83 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl3 /= 10000; // 83 is 8.3 in milliseconds, so we need to divide by 10000...
	
	d->apdnf = create_allPassDNestMono(mt,
					   dl1, dl2, dl3,
					   ftoFTYPE(0.15),
					   ftoFTYPE(0.25),
					   ftoFTYPE(0.30));
	if(d->apdnf == NULL) goto fail;
	
	// Then a single nested all pass
	dl1 = 66 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 66 is in milliseconds, so we need to divide by 1000...
	dl2 = 30 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl2 /= 1000; // 30 is milliseconds, so we need to divide by 1000...
	
	d->apsnf = create_allPassSNestMono(dl1, dl2,
					   ftoFTYPE(0.08),
					   ftoFTYPE(0.30)
		);
	if(d->apsnf == NULL) goto fail;

	// Then another low pass
	d->xpf2 = create_xPassFilterMono(mt, 0);
	if(d->xpf2 == NULL) goto fail;
	
	/* return pointer to instance data */
//NO_FAIL:
	return 0;

	// fail stack
fail:
	delete_current_type(d);
	return -1;
}

int configure_medium(MachineTable *mt, RewerbData *d) {
	int dl1, dl2, dl3;

	d->hipass_f = 500.0f;
	d->lopass_f = 6000.0f;
	d->wet_level = 1.0f;
	d->dry_level = 1.0f;
	
	// First a hi pass filter, for entry
	d->xpf0 = create_xPassFilterMono(mt, 1);
	if(d->xpf0 == NULL) goto fail;
	
	// Then a low pass filter, for entry
	d->xpf1 = create_xPassFilterMono(mt, 0);
	if(d->xpf1 == NULL) goto fail;
	
	// Then a double nested all pass
	dl1 = 35 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 35 is in milliseconds, so we need to divide by 1000...
	dl2 = 83 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl2 /= 10000; // 83 is 8.3 in milliseconds, so we need to divide by 10000...
	dl3 = 22 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl3 /= 1000; // 22 is in milliseconds, so we need to divide by 1000...
	
	d->apdnf = create_allPassDNestMono(mt,
					   dl1, dl2, dl3,
					   ftoFTYPE(0.25),
					   ftoFTYPE(0.35),
					   ftoFTYPE(0.45));
	if(d->apdnf == NULL) goto fail;

	// Then a delay line
	dl1 = 5 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 5 is in milliseconds, so we need to divide by 1000...
	d->dl1 = create_delayLineMono(dl1);
	if(d->dl1 == NULL) goto fail;
	
	// Then a plain all pass
	dl1 = 35 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 35 is in milliseconds, so we need to divide by 1000...	
	d->apf = create_allPassMono(dl1, ftoFTYPE(0.45));
	if(d->apf == NULL) goto fail;

	// Then a delay line
	dl1 = 67 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 67 is in milliseconds, so we need to divide by 1000...
	d->dl2 = create_delayLineMono(dl1);
	if(d->dl2 == NULL) goto fail;

	// Then another delay line
	dl1 = 15 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 15 is in milliseconds, so we need to divide by 1000...
	d->dl3 = create_delayLineMono(dl1);
	if(d->dl3 == NULL) goto fail;
	
	// Then a single nested all pass
	dl1 = 39 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 39 is in milliseconds, so we need to divide by 1000...
	dl2 = 98 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl2 /= 10000; // 98 is 9.8 in milliseconds, so we need to divide by 10000...
	
	d->apsnf = create_allPassSNestMono(dl1, dl2,
					   ftoFTYPE(0.15),
					   ftoFTYPE(0.35)
		);
	if(d->apsnf == NULL) goto fail;

	// Then another delay line
	dl1 = 108 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 108 is in milliseconds, so we need to divide by 1000...
	d->dl4 = create_delayLineMono(dl1);
	if(d->dl4 == NULL) goto fail;

	// Then another low pass
	d->xpf2 = create_xPassFilterMono(mt, 0);
	if(d->xpf2 == NULL) goto fail;
	
	/* return pointer to instance data */
//NO_FAIL:
	return 0;

	// fail stack
fail:
	delete_current_type(d);
	return -1;
}

int configure_large(MachineTable *mt, RewerbData *d) {
	int dl1, dl2, dl3;

	d->hipass_f = 500.0f;
	d->lopass_f = 6000.0f;
	d->wet_level = 1.0f;
	d->dry_level = 1.0f;
	
	// First a hi pass filter, for entry
	d->xpf0 = create_xPassFilterMono(mt, 1);
	if(d->xpf0 == NULL) goto fail;
	
	// Then a low pass filter, for entry
	d->xpf1 = create_xPassFilterMono(mt, 0);
	if(d->xpf1 == NULL) goto fail;
	
	// Then a plain all pass
	dl1 = 8 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 8 is in milliseconds, so we need to divide by 1000...	
	d->apf = create_allPassMono(dl1, ftoFTYPE(0.3));
	if(d->apf == NULL) goto fail;

	// Then a second plain all pass
	dl1 = 12 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 12 is in milliseconds, so we need to divide by 1000...	
	d->apf2 = create_allPassMono(dl1, ftoFTYPE(0.3));
	if(d->apf2 == NULL) goto fail;

	// Then a delay line
	dl1 = 4 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 4 is in milliseconds, so we need to divide by 1000...
	d->dl1 = create_delayLineMono(dl1);
	if(d->dl1 == NULL) goto fail;
	
	// Then a delay line
	dl1 = 17 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 17 is in milliseconds, so we need to divide by 1000...
	d->dl2 = create_delayLineMono(dl1);
	if(d->dl2 == NULL) goto fail;

	// Then a single nested all pass
	dl1 = 87 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 87 is in milliseconds, so we need to divide by 1000...
	dl2 = 62 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl2 /= 1000; // 62 is in milliseconds, so we need to divide by 1000...
	
	d->apsnf = create_allPassSNestMono(dl1, dl2,
					   ftoFTYPE(0.50),
					   ftoFTYPE(0.25)
		);
	if(d->apsnf == NULL) goto fail;

	// Then another delay line
	dl1 = 31 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 31 is in milliseconds, so we need to divide by 1000...
	d->dl3 = create_delayLineMono(dl1);
	if(d->dl3 == NULL) goto fail;
	
	// Then another delay line
	dl1 = 3 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 3 is in milliseconds, so we need to divide by 1000...
	d->dl4 = create_delayLineMono(dl1);
	if(d->dl4 == NULL) goto fail;

	// Then a double nested all pass
	dl1 = 120 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 120 is in milliseconds, so we need to divide by 1000...
	dl2 = 76 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl2 /= 1000; // 76 is in milliseconds, so we need to divide by 1000...
	dl3 = 30 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl3 /= 1000; // 30 is in milliseconds, so we need to divide by 1000...
	
	d->apdnf = create_allPassDNestMono(mt,
					   dl1, dl2, dl3,
					   ftoFTYPE(0.50),
					   ftoFTYPE(0.25),
					   ftoFTYPE(0.25));
	if(d->apdnf == NULL) goto fail;

	// Then another low pass
	d->xpf2 = create_xPassFilterMono(mt, 0);
	if(d->xpf2 == NULL) goto fail;
	
	/* return pointer to instance data */
//NO_FAIL:
	return 0;

	// fail stack
fail:
	delete_current_type(d);
	return -1;
}

void delete(void *data) {
	RewerbData *d = (RewerbData *)data;

	if(d) {
		delete_current_type(d);
		free(d);
	}
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	RewerbData *d = (RewerbData *)void_info;

	if(strcmp("Highpass", name) == 0)
		return &(d->hipass_f);
	if(strcmp("Lowpass", name) == 0)
		return &(d->lopass_f);
	if(strcmp("WetLevel", name) == 0)
		return &(d->wet_level);
	if(strcmp("DryLevel", name) == 0)
		return &(d->dry_level);
	if(strcmp("FeedbackGain", name) == 0)
		return &(d->fb_gain);
	if(strcmp("Type", name) == 0)
		return &(d->type);
	
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute_small(MachineTable *mt, RewerbData *d) {
	SignalPointer *s = mt->get_input_signal(mt, "Mono");
	SignalPointer *os = mt->get_output_signal(mt, "Mono");

	if(os == NULL) return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);
	int Fs = mt->get_signal_frequency(os);
	float Fs_i = 1.0f / ((float)Fs);
	
	if(s == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < ol; t++) {
			ou[t] = itoFTYPE(0);
		}
		return;		
	}
	
	FTYPE *in = mt->get_signal_buffer(s);

	libfilter_set_Fs(Fs);
	
	int i;

	FTYPE x, y;
	FTYPE t1, t2;

	FTYPE drymix = ftoFTYPE(d->dry_level);
	FTYPE wetmix = ftoFTYPE(d->wet_level);
	FTYPE fbgain = ftoFTYPE(d->fb_gain);

	xPassFilterMonoSetCutoff(d->xpf0, ftoFTYPE(d->hipass_f * Fs_i)); 
	xPassFilterMonoSetResonance(d->xpf0, ftoFTYPE(0.0));
	xPassFilterMonoSetCutoff(d->xpf1, ftoFTYPE(d->lopass_f * Fs_i)); 
	xPassFilterMonoSetResonance(d->xpf1, ftoFTYPE(0.0));
	xPassFilterMonoSetCutoff(d->xpf2, ftoFTYPE(0.036f));
	xPassFilterMonoSetResonance(d->xpf2, ftoFTYPE(0.0));
	
	xPassFilterMonoRecalc(d->xpf0);
	xPassFilterMonoRecalc(d->xpf1);
	xPassFilterMonoRecalc(d->xpf2);
	
	for(i = 0; i < ol; i++) {
		// get input
		x = in[i];
		
		// calc first filter
		xPassFilterMonoPut(d->xpf0, x);

		// calc second filter, then acquire t1
		xPassFilterMonoPut(d->xpf1, xPassFilterMonoGet(d->xpf0));

		// put into first pure delay
		delayLineMonoPut(d->dl1, mulFTYPE(fbgain, d->mem) + xPassFilterMonoGet(d->xpf1));

		// calc first all pass (double nested), then acquire t1
		allPassDNestMonoPut(d->apdnf, delayLineMonoGet(d->dl1));
		t1 = allPassDNestMonoGet(d->apdnf);

		// then put t1 as input for second all pass (single nested)
		allPassSNestMonoPut(d->apsnf, t1);

		// acquire t2
		t2 = allPassSNestMonoGet(d->apsnf);

		// Put data into second x pass filter
		xPassFilterMonoPut(d->xpf2, mulFTYPE(ftoFTYPE(0.5f), t2));

		// Extract feedback
		d->mem = xPassFilterMonoGet(d->xpf2);

		// Calculate output
		y =
			mulFTYPE(ftoFTYPE(0.5f), t1) +
			mulFTYPE(ftoFTYPE(0.5f), t2);
		
		// mix in dry
		ou[i] = mulFTYPE(y, wetmix) + mulFTYPE(x, drymix);
	}	
}

void execute_medium(MachineTable *mt, RewerbData *d) {
	SignalPointer *s = mt->get_input_signal(mt, "Mono");
	SignalPointer *os = mt->get_output_signal(mt, "Mono");

	if(os == NULL) return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);
	int Fs = mt->get_signal_frequency(os);
	float Fs_i = 1.0f / ((float)Fs);
	
	if(s == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < ol; t++) {
			ou[t] = itoFTYPE(0);
		}
		return;		
	}
	
	FTYPE *in = mt->get_signal_buffer(s);

	libfilter_set_Fs(Fs);
	
	int i;

	FTYPE x, tmp, y;
	FTYPE t1, t2, t3, t4;

	FTYPE drymix = ftoFTYPE(d->dry_level);
	FTYPE wetmix = ftoFTYPE(d->wet_level);
	FTYPE fbgain = ftoFTYPE(d->fb_gain);

	xPassFilterMonoSetCutoff(d->xpf0, ftoFTYPE(d->hipass_f * Fs_i)); 
	xPassFilterMonoSetResonance(d->xpf0, ftoFTYPE(0.0));
	xPassFilterMonoSetCutoff(d->xpf1, ftoFTYPE(d->lopass_f * Fs_i)); 
	xPassFilterMonoSetResonance(d->xpf1, ftoFTYPE(0.0));
	xPassFilterMonoSetCutoff(d->xpf2, ftoFTYPE(0.022f));
	xPassFilterMonoSetResonance(d->xpf2, ftoFTYPE(0.0));
	
	xPassFilterMonoRecalc(d->xpf0);
	xPassFilterMonoRecalc(d->xpf1);
	xPassFilterMonoRecalc(d->xpf2);
	
	for(i = 0; i < ol; i++) {
		x = in[i];

		// calc first filter
		xPassFilterMonoPut(d->xpf0, x);

		// calc second filter, then acquire t1
		xPassFilterMonoPut(d->xpf1, xPassFilterMonoGet(d->xpf0));
		t1 = xPassFilterMonoGet(d->xpf1);

		// calc first all pass (double nested), then acquire t2
		allPassDNestMonoPut(d->apdnf, t1 + mulFTYPE(fbgain,
							     d->mem));
		t2 = allPassDNestMonoGet(d->apdnf);

		// put into first pure delay
		delayLineMonoPut(d->dl1, t2);

		// then take delay output and put into second all pass (plain)
		allPassMonoPut(d->apf, delayLineMonoGet(d->dl1));

		// Then take the all pass output and put into second pure delay, then acquire t3
		delayLineMonoPut(d->dl2, allPassMonoGet(d->apf));
		t3 = delayLineMonoGet(d->dl2);

		// Then put t3 into third pure delay
		delayLineMonoPut(d->dl3, t3);
		
		// then calculate input for third all pass (single nested)
		tmp = mulFTYPE(delayLineMonoGet(d->dl3), ftoFTYPE(0.4f)) + t1;
		allPassSNestMonoPut(d->apsnf, tmp);

		// acquire t4
		t4 = allPassSNestMonoGet(d->apsnf);

		// Put t4 in fourth pure delay
		delayLineMonoPut(d->dl4, t4);

		// Put data into second x pass filter
		xPassFilterMonoPut(d->xpf2, mulFTYPE(ftoFTYPE(0.4f), delayLineMonoGet(d->dl4)));

		// Extract feedback
		d->mem = xPassFilterMonoGet(d->xpf2);

		// Calculate output
		y =
			mulFTYPE(ftoFTYPE(0.5f), t2) +
			mulFTYPE(ftoFTYPE(0.5f), t3) +
			mulFTYPE(ftoFTYPE(0.5f), t4);
			
		
		// mix in dry
		ou[i] = mulFTYPE(y, wetmix) + mulFTYPE(x, drymix);
	}	
}

void execute_large(MachineTable *mt, RewerbData *d) {

	SignalPointer *s = mt->get_input_signal(mt, "Mono");
	SignalPointer *os = mt->get_output_signal(mt, "Mono");

	if(os == NULL) return;
	
	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);
	int Fs = mt->get_signal_frequency(os);
	float Fs_i = 1.0f / ((float)Fs);
	
	if(s == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < ol; t++) {
			ou[t] = itoFTYPE(0);
		}
		return;		
	}
	
	FTYPE *in = mt->get_signal_buffer(s);

	libfilter_set_Fs(Fs);
	
	int i;

	FTYPE x, y;
	FTYPE t1, t2, t3, t4;

	FTYPE drymix = ftoFTYPE(d->dry_level);
	FTYPE wetmix = ftoFTYPE(d->wet_level);
	FTYPE fbgain = ftoFTYPE(d->fb_gain);

	xPassFilterMonoSetCutoff(d->xpf0, ftoFTYPE(d->hipass_f * Fs_i)); 
	xPassFilterMonoSetResonance(d->xpf0, ftoFTYPE(0.0));
	xPassFilterMonoSetCutoff(d->xpf1, ftoFTYPE(d->lopass_f * Fs_i)); 
	xPassFilterMonoSetResonance(d->xpf1, ftoFTYPE(0.0));
	xPassFilterMonoSetCutoff(d->xpf2, ftoFTYPE(0.022f));
	xPassFilterMonoSetResonance(d->xpf2, ftoFTYPE(0.0));
	
	xPassFilterMonoRecalc(d->xpf0);
	xPassFilterMonoRecalc(d->xpf1);
	xPassFilterMonoRecalc(d->xpf2);
	
	for(i = 0; i < ol; i++) {
		x = in[i];

		// calc first filter
		xPassFilterMonoPut(d->xpf0, x);

		// calc second filter, then acquire t1
		xPassFilterMonoPut(d->xpf1, xPassFilterMonoGet(d->xpf0));
		t1 = xPassFilterMonoGet(d->xpf1);

		// then take t1 + mem * fbgain and put into first all pass (plain)
		allPassMonoPut(d->apf, t1 + mulFTYPE(fbgain, d->mem));

		// then take that and put into second plain all pass
		allPassMonoPut(d->apf2, allPassMonoGet(d->apf));

		// put into first pure delay
		delayLineMonoPut(d->dl1, allPassMonoGet(d->apf2));

		// get t2
		t2 = delayLineMonoGet(d->dl1);

		// Then take t2 and put into second pure delay
		delayLineMonoPut(d->dl2, t2);
				
		// then calculate input for third all pass (single nested)
		allPassSNestMonoPut(d->apsnf, delayLineMonoGet(d->dl2));

		// Then put that into third pure delay
		delayLineMonoPut(d->dl3, allPassSNestMonoGet(d->apsnf));

		// then get t3
		t3 = delayLineMonoGet(d->dl3);

		// then put t3 into fourht delay
		delayLineMonoPut(d->dl4, t3);
		
		// calc first fourth all pass (double nested), then acquire t4
		allPassDNestMonoPut(d->apdnf, delayLineMonoGet(d->dl4));
		t4 = allPassDNestMonoGet(d->apdnf);

		// Put data into second x pass filter
		xPassFilterMonoPut(d->xpf2, mulFTYPE(ftoFTYPE(0.5f), t3));

		// Extract feedback
		d->mem = xPassFilterMonoGet(d->xpf2);

		// Calculate output
		y =
			mulFTYPE(ftoFTYPE(1.5f), t2) +
			mulFTYPE(ftoFTYPE(0.8f), t3) +
			mulFTYPE(ftoFTYPE(0.8f), t4);
			
		
		// mix in dry
		ou[i] = mulFTYPE(y, wetmix) + mulFTYPE(x, drymix);
	}	
}

int recalc_type(MachineTable *m, RewerbData *d) {
	delete_current_type(d);
	
	switch(d->type) {
	case 0:
		return configure_medium(m, d);
		break;

	case 1:
		return configure_small(m, d);
		break;

	case 2:
		return configure_large(m, d);
		break;
	}
	// this "shouldn't happen" - famous last words
	return -2;
}

void execute(MachineTable *mt, void *data) {	
	RewerbData *d = (RewerbData *)data;

	if(d->type != d->last_type) {
		(void) recalc_type(mt, d); // we ignore this error - we really shouldn't but well... today I am lazy...
		d->last_type = d->type;
	}
			
	switch(d->type) {
	case 0:
		execute_medium(mt, d);
		break;

	case 1:
		execute_small(mt, d);
		break;

	case 2:
		execute_large(mt, d);
		break;
	}
}

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	RewerbData *d = (RewerbData *)malloc(sizeof(RewerbData));
	if(d) {	
		memset(d, 0, sizeof(RewerbData));

		// default type is 0, medium
		d->last_type = d->type = 0;
		if(recalc_type(mt, d)) {
			// if failure - delete the rewerb object and return NULL
			delete(d);
			return NULL;
		}
	}
	return d;
}

