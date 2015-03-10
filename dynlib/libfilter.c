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

#include "dynlib_debug.h"
#include "libfilter.h"
#include <math.h>
#include <stdio.h>

/*********************************************
 *
 *  Generical
 *
 *********************************************/

int __libfilter_Fs = 0;
FTYPE __libfilter_Fs_i; // inverse of Fs

void libfilter_set_Fs(int Fs) {
	if(__libfilter_Fs != Fs) {
		float Fs_i = 1.0f / Fs;
		__libfilter_Fs = Fs;
		__libfilter_Fs_i = ftoFTYPE(Fs_i);
	}
}

/*********************************************
 *
 *  bandPass filter mono class
 *
 *********************************************/

struct bandPassFilterMono *create_bandPassFilterMono(MachineTable *mt) {
	struct bandPassFilterMono *r = (struct bandPassFilterMono *)malloc(sizeof(struct bandPassFilterMono));

	if(r != NULL) {
		memset(r, 0, sizeof(struct bandPassFilterMono));

		SETUP_SATANS_MATH(mt);
	}
	
	return r;
}

void bandPassFilterMonoSetFrequencies(bandPassFilterMono_t *ld, float center, float Q) {
	ld->center = center;
	ld->Q = Q;
}

void bandPassFilterMonoRecalc(struct bandPassFilterMono *xpf) {
	float omega = 0, Q = 0, beta = 0, ypsilon = 0, alpha = 0;                       
	
	Q = xpf->Q;
	omega = 2.0f * M_PI * xpf->center;

	DYNLIB_DEBUG("0 - center = %f - Q %f\n",
		     (xpf->center), (xpf->Q));
	DYNLIB_DEBUG("1 - omega = %f - Q = %f - alpha = %f - beta = %f - ypsilon = %f\n",
		     (omega),
		     (Q),
		     (alpha),
		     (beta),
		     (ypsilon));
	beta = tanf(omega / (2 * Q));
	DYNLIB_DEBUG("2 - omega = %f - Q = %f - alpha = %f - beta = %f - ypsilon = %f\n",
		     (omega),
		     (Q),
		     (alpha),
		     (beta),
		     (ypsilon));
	beta = 0.5 * ((1 - beta) / (1 + beta));
	DYNLIB_DEBUG("3 - omega = %f - Q = %f - alpha = %f - beta = %f - ypsilon = %f\n",
		     (omega),
		     (Q),
		     (alpha),
		     (beta),
		     (ypsilon));
	ypsilon = (0.5 + beta) * cosf(omega);
	DYNLIB_DEBUG("4 - omega = %f - Q = %f - alpha = %f - beta = %f - ypsilon = %f\n",
		     (omega),
		     (Q),
		     (alpha),
		     (beta),
		     (ypsilon));
	alpha = (0.5 - beta) / 2.0;
	DYNLIB_DEBUG("5 - omega = %f - Q = %f - alpha = %f - beta = %f - ypsilon = %f\n",
		     (omega),
		     (Q),
		     (alpha),
		     (beta),
		     (ypsilon));
	
	xpf->alpha = ftoFTYPE(alpha);
	xpf->beta = ftoFTYPE(beta);
	xpf->ypsilon = ftoFTYPE(ypsilon);

	xpf->X_Nm2 =
		xpf->X_Nm1 =
		xpf->Y_Nm2 =
		xpf->Y_Nm1 = itoFTYPE(0);
		
}

#if 0
inline void bandPassFilterMonoPut(void *vd, FTYPE d) {
	struct bandPassFilterMono *xpf = (struct bandPassFilterMono *)vd;
	
	xpf->output = mulFTYPE(ftoFTYPE(2.0f), mulFTYPE(xpf->alpha, d - xpf->X_Nm2) + 
			       mulFTYPE(xpf->ypsilon, xpf->Y_Nm1) -
			       mulFTYPE(xpf->beta, xpf->Y_Nm2));

	xpf->X_Nm2 = xpf->X_Nm1; xpf->X_Nm1 = d; xpf->Y_Nm2 = xpf->Y_Nm1; xpf->Y_Nm1 = xpf->output;
}

inline FTYPE bandPassFilterMonoGet(void *vd) {
	struct bandPassFilterMono *xpf = (struct bandPassFilterMono *)vd;
	return xpf->output;
}
#endif

void bandPassFilterMonoFree(void *vd) {
	struct bandPassFilterMono *xpf = (struct bandPassFilterMono *)vd;
	free(xpf);
}

/*********************************************
 *
 *  delayLine class
 *
 *********************************************/

struct delayLineMono *create_delayLineMono(int delayLength) {
	struct delayLineMono *d = (struct delayLineMono *)malloc(sizeof(struct delayLineMono));
	if(d == NULL)
		return NULL;
	
	FTYPE *p = (FTYPE *)malloc(sizeof(FTYPE) * delayLength);
	if(p == NULL) {
		free(d); return NULL;
	}

	memset(p, 0, sizeof(FTYPE) * delayLength);

	d->delayLine_tail = 0;
	d->delayLine_head = delayLength - 1;
	d->delayLine_length = delayLength;
	d->delayLine = p;

	return d;
}

inline void delayLineMonoPut(void *vd, FTYPE value) {
	struct delayLineMono *d = (struct delayLineMono *)vd;
	d->delayLine[d->delayLine_head] = value;
	
	d->delayLine_head = (d->delayLine_head + 1) % d->delayLine_length;
}

inline FTYPE delayLineMonoGet(void *vd) {
	struct delayLineMono *d = (struct delayLineMono *)vd;
	FTYPE value = d->delayLine[d->delayLine_tail];
	d->delayLine_tail = (d->delayLine_tail + 1) % d->delayLine_length;
	return value;
}

inline FTYPE delayLineMonoGetOffsetNoMove(void *vd, int offset) {
	struct delayLineMono *d = (struct delayLineMono *)vd;
	FTYPE value = d->delayLine[(d->delayLine_tail + d->delayLine_length - offset - 1) % d->delayLine_length];
	return value;
}

inline FTYPE delayLineMonoGetOffset(void *vd, int offset) {
	struct delayLineMono *d = (struct delayLineMono *)vd;
	FTYPE value = d->delayLine[(d->delayLine_tail + d->delayLine_length - offset - 1) % d->delayLine_length];
	d->delayLine_tail = (d->delayLine_tail + 1) % d->delayLine_length;
	return value;
}

void delayLineMonoClear(void *vd) {
	struct delayLineMono *d = (struct delayLineMono *)vd;

	memset(d->delayLine, 0, d->delayLine_length * sizeof(FTYPE));
}

void delayLineMonoFree(void *vd) {
	struct delayLineMono *d = (struct delayLineMono *)vd;

	free(d->delayLine);
	free(d);
}

/*********************************************
 *
 *  allPass class
 *
 *********************************************/

struct allPassMono *create_allPassMono(int delayLength, FTYPE _g) {
	struct allPassMono *apm = (struct allPassMono *)malloc(sizeof(struct allPassMono));
	if(apm == NULL)
		return NULL;;
	
	apm->g = _g;

	apm->delayLineStruct = create_delayLineMono(delayLength);
	if(apm->delayLineStruct == NULL) {
		free(apm); return NULL;
	}

	apm->mem1 = ftoFTYPE(0.0f);

	return apm;
}

inline void allPassMonoPut(void *vd, FTYPE input) {
	struct allPassMono *apm = (struct allPassMono *)vd;
	delayLineMonoPut(apm->delayLineStruct, input + mulFTYPE(apm->mem1, apm->g)); 
	apm->output = mulFTYPE(input, -(apm->g)) + delayLineMonoGet(apm->delayLineStruct);	
	apm->mem1 = apm->output;
}

inline FTYPE allPassMonoGet(void *vd) {
	struct allPassMono *apm = (struct allPassMono *)vd;
	return apm->output;
}

void allPassMonoFree(void *vd) {
	struct allPassMono *apm = (struct allPassMono *)vd;
	delayLineMonoFree(apm->delayLineStruct);
	free(apm);
}

/*********************************************
 *
 *  allPass Single Nested class
 *
 *********************************************/

struct allPassSNestMono *create_allPassSNestMono(int delayLength, int delayLength1, FTYPE _g, FTYPE _g1) {
	struct allPassSNestMono *apm = (struct allPassSNestMono *)malloc(sizeof(struct allPassSNestMono));
	if(apm == NULL)
		return NULL;;
	
	apm->g = _g;

	apm->delayLineStruct = create_delayLineMono(delayLength);
	if(apm->delayLineStruct == NULL) {
		free(apm); return NULL;
	}

	apm->allPassStruct = create_allPassMono(delayLength1, _g1);
	if(apm->allPassStruct == NULL) {
		delayLineMonoFree(apm->delayLineStruct);
		free(apm); return NULL;
	}

	apm->mem1 = ftoFTYPE(0.0f);

	return apm;
}

inline void allPassSNestMonoPut(void *vd, FTYPE input) {
	struct allPassSNestMono *apm = (struct allPassSNestMono *)vd;
	
	delayLineMonoPut(apm->delayLineStruct, input + mulFTYPE(apm->mem1, apm->g));
	allPassMonoPut(apm->allPassStruct, delayLineMonoGet(apm->delayLineStruct));
	
	apm->mem1 = apm->output = allPassMonoGet(apm->allPassStruct) - mulFTYPE(input, apm->g);
}

inline FTYPE allPassSNestMonoGet(void *vd) {
	struct allPassSNestMono *apm = (struct allPassSNestMono *)vd;
	return apm->output;
}

void allPassSNestMonoFree(void *vd) {
	struct allPassSNestMono *apm = (struct allPassSNestMono *)vd;
	delayLineMonoFree(apm->delayLineStruct);
	allPassMonoFree(apm->allPassStruct);
	free(apm);
}

/*********************************************
 *
 *  allPass Double Nested class
 *
 *********************************************/

struct allPassDNestMono *create_allPassDNestMono(
	MachineTable *mt,
	int delayLength, int delayLength1, int delayLength2, FTYPE _g, FTYPE _g1, FTYPE _g2) {
	
	struct allPassDNestMono *apm = (struct allPassDNestMono *)malloc(sizeof(struct allPassDNestMono));
	if(apm == NULL)
		return NULL;;
	
	apm->g = _g;

	apm->delayLineStruct = create_delayLineMono(delayLength);
	if(apm->delayLineStruct == NULL) {
		free(apm); return NULL;
	}

	apm->allPassStruct1 = create_allPassMono(delayLength1, _g1);
	if(apm->allPassStruct1 == NULL) {
		delayLineMonoFree(apm->delayLineStruct);
		free(apm); return NULL;
	}

	apm->allPassStruct2 = create_allPassMono(delayLength2, _g2);
	if(apm->allPassStruct2 == NULL) {
		delayLineMonoFree(apm->delayLineStruct);
		allPassMonoFree(apm->allPassStruct1);
		free(apm); return NULL;
	}

	apm->lpf = create_xPassFilterMono(mt, 0);
	if(apm->lpf == NULL) {
		delayLineMonoFree(apm->delayLineStruct);
		allPassMonoFree(apm->allPassStruct1);
		allPassMonoFree(apm->allPassStruct2);
		free(apm); return NULL;
	}
	
	apm->mem1 = ftoFTYPE(0.0f);

	return apm;
}

inline void allPassDNestMonoPut(void *vd, FTYPE input) {
	struct allPassDNestMono *apm = (struct allPassDNestMono *)vd;
	if(apm->__internal_FS != __libfilter_Fs) {
		xPassFilterMonoSetCutoff(apm->lpf, ftoFTYPE(0.30)); // 30% of Fs
		xPassFilterMonoSetResonance(apm->lpf, ftoFTYPE(0.0f));
		
		apm->__internal_FS = __libfilter_Fs;
		xPassFilterMonoRecalc(apm->lpf);
	}

	delayLineMonoPut(apm->delayLineStruct, input + mulFTYPE(apm->mem1, apm->g));
	allPassMonoPut(apm->allPassStruct1, delayLineMonoGet(apm->delayLineStruct));
	allPassMonoPut(apm->allPassStruct2, allPassMonoGet(apm->allPassStruct1));
	
	apm->output = allPassMonoGet(apm->allPassStruct2) - mulFTYPE(input, apm->g);

	// due to numerical instability (=high frequency feedback), we do a low pass filter here...	
	xPassFilterMonoPut(apm->lpf, apm->output);
	apm->output = xPassFilterMonoGet(apm->lpf);
	
	apm->mem1 = apm->output;
}

inline FTYPE allPassDNestMonoGet(void *vd) {
	struct allPassDNestMono *apm = (struct allPassDNestMono *)vd;
	return apm->output;
}

void allPassDNestMonoFree(void *vd) {
	struct allPassDNestMono *apm = (struct allPassDNestMono *)vd;
	delayLineMonoFree(apm->delayLineStruct);
	allPassMonoFree(apm->allPassStruct1);
	allPassMonoFree(apm->allPassStruct2);
	free(apm);
}

/*********************************************
 *
 *  xPass filter mono class
 *
 *********************************************/

struct xPassFilterMono *create_xPassFilterMono(MachineTable *mt, int type) {
	struct xPassFilterMono *r = (struct xPassFilterMono *)malloc(sizeof(struct xPassFilterMono));

	if(r != NULL) {
		memset(r, 0, sizeof(struct xPassFilterMono));

		SETUP_SATANS_MATH(mt);
		r->filter_type = type;
	}
	
	return r;
}

void xPassFilterMonoSetCutoff(xPassFilterMono_t *xpf, FTYPE cutoff) {
	xpf->cutoff_ft = cutoff;
}

void xPassFilterMonoSetResonance(xPassFilterMono_t *xpf, FTYPE resonance) {
	xpf->resonance_ft = resonance;
}

void xPassFilterMonoRecalc(struct xPassFilterMono *xpf) {
	FTYPE fx_sn;
	FTYPE fx_omega;
	FTYPE fx_b0, fx_b1, fx_b2, fx_a0, fx_a1, fx_a2, fx_alpha, fx_cs;

	// These limits the cutoff frequency and resonance to
	// reasoneable values.
	if (xpf->cutoff_ft < ftoFTYPE(0.0f)) { xpf->cutoff_ft = ftoFTYPE(0.0f); };
	if (xpf->cutoff_ft > ftoFTYPE(1.0f)) { xpf->cutoff_ft = ftoFTYPE(1.0f); };
	if (xpf->resonance_ft < ftoFTYPE(0.0f)) { xpf->resonance_ft = ftoFTYPE(0.0f); };
	if (xpf->resonance_ft > ftoFTYPE(1.0f)) { xpf->resonance_ft = ftoFTYPE(1.0f); };

	fx_omega = xpf->cutoff_ft;
	
	fx_sn = SAT_SIN_SCALAR_FTYPE(fx_omega);
	fx_cs = SAT_COS_SCALAR_FTYPE(fx_omega);

	fx_alpha = mulFTYPE(fx_sn, itoFTYPE(1) - (xpf->resonance_ft));

	if(fx_alpha < ftoFTYPE(0.0f)) {
		fx_alpha = ftoFTYPE(0.0f); // if alpha is < 0.0 we will get bad sounds...
	}
	
	switch(xpf->filter_type) {
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

	xpf->coef[0] = divFTYPE(fx_b0, fx_a0);
	xpf->coef[1] = divFTYPE(fx_b1, fx_a0);
	xpf->coef[2] = divFTYPE(fx_b2, fx_a0);
	xpf->coef[3] = divFTYPE(-fx_a1, fx_a0);
	xpf->coef[4] = divFTYPE(-fx_a2, fx_a0);
}

#if 1
inline void xPassFilterMonoPut(xPassFilterMono_t *xpf, FTYPE d) {

	FTYPE tmp;
	
	tmp =
		mulFTYPE(xpf->coef[0], d) +
		mulFTYPE(xpf->coef[1], xpf->hist_x[0]) +
		mulFTYPE(xpf->coef[2], xpf->hist_x[1]) +
		mulFTYPE(xpf->coef[3], xpf->hist_y[0]) +
		mulFTYPE(xpf->coef[4], xpf->hist_y[1]);
	
	xpf->hist_y[1] = xpf->hist_y[0];
	xpf->hist_y[0] = tmp;
	xpf->hist_x[1] = xpf->hist_x[0];
	xpf->hist_x[0] = d;

	xpf->output = tmp;
}

inline FTYPE xPassFilterMonoGet(xPassFilterMono_t *xpf) {
	return xpf->output;
}
#endif

void xPassFilterMonoFree(xPassFilterMono_t *xpf) {
	free(xpf);
}
