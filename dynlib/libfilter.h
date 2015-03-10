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

#ifndef __HAVE_LIBFILTER_INCLUDED__
#define __HAVE_LIBFILTER_INCLUDED__

#include "dynlib.h"

/*********************************************
 *
 *  generical
 *
 *********************************************/

inline void libfilter_set_Fs(int Fs);

/*********************************************
 *
 *  bandPass class
 *
 *********************************************/

struct bandPassCoeff {
	FTYPE alpha, beta, ypsilon;
};
struct bandPassMem {
	FTYPE X_Nm2, X_Nm1; // X[N-2], X[N-1]
	FTYPE Y_Nm2, Y_Nm1; // Y[N-2], Y[N-1]
	
};
#define BANDPASS_IIR_FTYPE_CALC(X,B,M)					\
	mulFTYPE(itoFTYPE(2), mulFTYPE(B.alpha, X - M.X_Nm2) +	\
		mulFTYPE(B.ypsilon, M.Y_Nm1) - \
		  mulFTYPE(B.beta, M.Y_Nm2))
#define BANDPASS_IIR_FTYPE_SAVE(X,Y,M)		\
	{ M.X_Nm2 = M.X_Nm1; M.X_Nm1 = X; M.Y_Nm2 = M.Y_Nm1; M.Y_Nm1 = Y; }

typedef struct bandPassFilterMono {
	FTYPE alpha, beta, ypsilon;
	FTYPE X_Nm2, X_Nm1;
	FTYPE Y_Nm2, Y_Nm1;
	FTYPE output;

	float center, Q;
} bandPassFilterMono_t;

struct bandPassFilterMono *create_bandPassFilterMono(MachineTable *mt);
// Q = center / (left - right) ; left and right are the half-power points, where the gain is equal to 1 / sqrt(2)
void bandPassFilterMonoSetFrequencies(bandPassFilterMono_t *ld, float center, float Q);
void bandPassFilterMonoRecalc(bandPassFilterMono_t *ld);
//inline void bandPassFilterMonoPut(void *vd, FTYPE d);
//inline FTYPE bandPassFilterMonoGet(void *vd);
void bandPassFilterMonoFree(void *vd);

#define bandPassFilterMonoPut(A,B) { \
	A->output = mulFTYPE(itoFTYPE(2), mulFTYPE(A->alpha, B - A->X_Nm2) +  \
		mulFTYPE(A->ypsilon, A->Y_Nm1) - \
			       mulFTYPE(A->beta, A->Y_Nm2)); \
A->X_Nm2 = A->X_Nm1; A->X_Nm1 = B; A->Y_Nm2 = A->Y_Nm1; A->Y_Nm1 = A->output; \
}

#define bandPassFilterMonoGet(A) (A->output)

/*********************************************
 *
 *  delayLine class
 *
 *********************************************/

struct delayLineMono {
	FTYPE *delayLine;
	int delayLine_length, delayLine_head, delayLine_tail;
};

struct delayLineMono *create_delayLineMono(int delayLength);
inline void delayLineMonoPut(void *vd, FTYPE value);
inline FTYPE delayLineMonoGet(void *vd);
inline FTYPE delayLineMonoGetOffsetNoMove(void *vd, int offset);
inline FTYPE delayLineMonoGetOffset(void *vd, int offset);
void delayLineMonoClear(void *vd);
void delayLineMonoFree(void *vd);

/*********************************************
 *
 *  allPass class
 *
 *********************************************/

struct allPassMono {
	FTYPE mem1;
	FTYPE g;
	FTYPE output;

	struct delayLineMono *delayLineStruct;
};

struct allPassMono *create_allPassMono(int delayLength, FTYPE _g);
inline void allPassMonoPut(void *vd, FTYPE input);
inline FTYPE allPassMonoGet(void *vd);
void allPassMonoFree(void *vd);

/*********************************************
 *
 *  allPass Single Nested class
 *
 *********************************************/

struct allPassSNestMono {
	FTYPE mem1;
	FTYPE g;
	FTYPE output;

	struct delayLineMono *delayLineStruct;
	struct allPassMono *allPassStruct;
};

struct allPassSNestMono *create_allPassSNestMono(int delayLength, int delayLength1, FTYPE _g, FTYPE _g1);
inline void allPassSNestMonoPut(void *vd, FTYPE input);
inline FTYPE allPassSNestMonoGet(void *vd);
void allPassSNestMonoFree(void *vd);

/*********************************************
 *
 *  allPass Double Nested class
 *
 *********************************************/

struct xPassFilterMono;

struct allPassDNestMono {
	int __internal_FS;
	FTYPE mem1;
	FTYPE g;
	FTYPE output;

	struct delayLineMono *delayLineStruct;
	struct allPassMono *allPassStruct1;
	struct allPassMono *allPassStruct2;
	struct xPassFilterMono *lpf;
};

struct allPassDNestMono *create_allPassDNestMono(
	MachineTable *mt,
	int delayLength, int delayLength1, int delayLength2, FTYPE _g, FTYPE _g1, FTYPE _g2);
inline void allPassDNestMonoPut(void *vd, FTYPE input);
inline FTYPE allPassDNestMonoGet(void *vd);
void allPassDNestMonoFree(void *vd);

/*********************************************
 *
 *  xPass filter mono class
 *
 *********************************************/

typedef struct xPassFilterMono {
	int filter_type; // 0 = lopass, 1 = hipass

	FTYPE output;
	
	FTYPE cutoff_ft, resonance_ft;
	
	FTYPE coef[5];
	FTYPE hist_x[2];
	FTYPE hist_y[2];
} xPassFilterMono_t;

struct xPassFilterMono *create_xPassFilterMono(MachineTable *mt, int type);
void xPassFilterMonoSetCutoff(xPassFilterMono_t *xpf, FTYPE cutoff);
void xPassFilterMonoSetResonance(xPassFilterMono_t *xpf, FTYPE resonance);
void xPassFilterMonoRecalc(xPassFilterMono_t *ld);
inline void xPassFilterMonoPut(xPassFilterMono_t *vd, FTYPE d);
inline FTYPE xPassFilterMonoGet(xPassFilterMono_t *vd);
void xPassFilterMonoFree(xPassFilterMono_t *vd);
/*
#define xPassFilterMonoPut(xpf,d) { \
	FTYPE tmp; \
	\
	tmp = \
		mulFTYPE(xpf->coef[0], d) + \
		mulFTYPE(xpf->coef[1], xpf->hist_x[0]) + \
		mulFTYPE(xpf->coef[2], xpf->hist_x[1]) + \
		mulFTYPE(xpf->coef[3], xpf->hist_y[0]) + \
		mulFTYPE(xpf->coef[4], xpf->hist_y[1]); \
	\
	xpf->hist_y[1] = xpf->hist_y[0]; \
	xpf->hist_y[0] = tmp; \
	xpf->hist_x[1] = xpf->hist_x[0]; \
	xpf->hist_x[0] = d; \
 \
	xpf->output = tmp; \
}

#define xPassFilterMonoGet(xpf) (xpf->output)
*/
#endif
