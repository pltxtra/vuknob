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

typedef struct _AllpassData {
	struct allPassDNestMono *apdnf;
	struct allPassMono *apf;
	struct allPassSNestMono *apsnf;

	int type;
} AllpassData;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	AllpassData *d = (AllpassData *)malloc(sizeof(AllpassData));
	memset(d, 0, sizeof(AllpassData));

	int dl1, dl2, dl3;

	// Then a double nested all pass
	dl1 = 120 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 120 is in milliseconds, so we need to divide by 1000...
	dl2 = 76 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl2 /= 10000; // 76 is 8.3 in milliseconds, so we need to divide by 10000...
	dl3 = 30 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl3 /= 1000; // 30 is in milliseconds, so we need to divide by 1000...
	
	d->apdnf = create_allPassDNestMono(mt,
					   dl1, dl2, dl3,
					   ftoFTYPE(0.5),
					   ftoFTYPE(0.25),
					   ftoFTYPE(0.25));
	if(d->apdnf == NULL) goto fail2;

	// Then a plain all pass
	dl1 = 8 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 8 is in milliseconds, so we need to divide by 1000...	
	d->apf = create_allPassMono(dl1, ftoFTYPE(0.3));
	if(d->apf == NULL) goto fail4;

	// Then a single nested all pass
	dl1 = 87 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl1 /= 1000; // 39 is in milliseconds, so we need to divide by 1000...
	dl2 = 62 * 44100; // assume 44100 Hz sample rate... bad bad boy
	dl2 /= 10000; // 98 is 9.8 in milliseconds, so we need to divide by 10000...
	
	d->apsnf = create_allPassSNestMono(dl1, dl2,
					   ftoFTYPE(0.5),
					   ftoFTYPE(0.25)
		);
	if(d->apsnf == NULL) goto fail7;
	
	/* return pointer to instance data */
//NO_FAIL:
	return (void *)d;

	// fail stack
fail7:
	allPassMonoFree(d->apf);
fail4:
	allPassDNestMonoFree(d->apdnf);
fail2:
	return NULL;
}
 
void delete(void *data) {
	AllpassData *d = (AllpassData *)data;
	
/* free instance data here */
	if(d->apsnf)
		allPassSNestMonoFree(d->apsnf);
	if(d->apf)
		allPassMonoFree(d->apf);
	if(d->apdnf)
		allPassDNestMonoFree(d->apdnf);

	free(d);
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	AllpassData *d = (AllpassData *)void_info;
	
	if(strcmp("Type", name) == 0)
		return &(d->type);

	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	AllpassData *d = (AllpassData *)data;

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

	switch(d->type) {
	case 0:
		for(i = 0; i < ol; i++) {
			allPassMonoPut(d->apf, in[i]);
			ou[i] = allPassMonoGet(d->apf);
		}	
		break;
	case 1:
		for(i = 0; i < ol; i++) {
			allPassSNestMonoPut(d->apsnf, in[i]);
			ou[i] = allPassSNestMonoGet(d->apsnf);
		}	
		break;
	case 2:
		for(i = 0; i < ol; i++) {
			allPassDNestMonoPut(d->apdnf, in[i]);
			ou[i] = allPassDNestMonoGet(d->apdnf);
		}	
		break;
	}
}

