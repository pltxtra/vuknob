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

// if we have a fixed point implementation
// we use the IIR implementation
// otherwise the default is to use the FFT implementation
#ifdef __SATAN_USES_FXP
#define VOCODER_IIR_IMPLEMENTATION
#define VOCODER_CHANNELS 10
#else
#define FFT_OPTIMAL_LENGTH 512
#define VOCODER_CHANNELS 16
#endif


typedef struct _VocoderData {

#ifdef VOCODER_IIR_IMPLEMENTATION
	bandPassFilterMono_t **bp_carrier;
	bandPassFilterMono_t **bp_modulator;
	xPassFilterMono_t **lp_modulator; // for envelope detector
#else
	FTYPE *prev_carrier, *prev_modulator;
	FTYPE *prev_output;
	FTYPE *next_output;
	FTYPE *output;
	FTYPE *hanning;
	FTYPE *carrier, *modulator;
	kiss_fftr_cfg fft_fwd_cfg;
	kiss_fftr_cfg fft_rev_cfg;
	kiss_fft_cpx *m_fft;
	kiss_fft_cpx *c_fft;
	int fft_len, fft_lap, channel_width;

	// in case the machine input and output buffers
	// are smaller than FFT_OPTIMAL_LENGTH we need to use
	// these indirect buffers to save up enough data to process..
	// this causes extra latency in the output but it shouldn't be significant to actually hear it
	FTYPE *indirect_carrier;
	FTYPE *indirect_modulator;
	FTYPE *indirect_output;
	int indirect_index, indirect_limit, indirect_size;
#endif

	float volume;

} VocoderData;

#ifdef VOCODER_IIR_IMPLEMENTATION

void cleanup_filters(VocoderData *d) {
	int k = 0;

	if(d->bp_carrier) {
		for(k = 0; k < VOCODER_CHANNELS; k++) {
			if(d->bp_carrier[k])
				bandPassFilterMonoFree(d->bp_carrier[k]);
		}
		free(d->bp_carrier);
		d->bp_carrier = NULL;
	}
	if(d->bp_modulator) {
		for(k = 0; k < VOCODER_CHANNELS; k++) {
			if(d->bp_modulator[k])
				bandPassFilterMonoFree(d->bp_modulator[k]);
		}
		free(d->bp_modulator);
		d->bp_modulator = NULL;
	}
	if(d->lp_modulator) {
		for(k = 0; k < VOCODER_CHANNELS; k++) {
			if(d->lp_modulator[k])
				xPassFilterMonoFree(d->lp_modulator[k]);
		}
		free(d->lp_modulator);
		d->lp_modulator = NULL;
	}

}

int setup_filters(MachineTable *mt, VocoderData *d, float Fs) {
	cleanup_filters(d);

	d->bp_carrier = (bandPassFilterMono_t **)calloc(1, sizeof(bandPassFilterMono_t *) * VOCODER_CHANNELS);
	d->bp_modulator = (bandPassFilterMono_t **)calloc(1, sizeof(bandPassFilterMono_t *) * VOCODER_CHANNELS);
	d->lp_modulator = (xPassFilterMono_t **)calloc(1, sizeof(xPassFilterMono_t *) * VOCODER_CHANNELS);

	if(d->bp_carrier == NULL || d->bp_modulator == NULL || d->lp_modulator == NULL) return 1;

	int k;
	for(k = 0; k < VOCODER_CHANNELS; k++) {
		d->bp_carrier[k] = create_bandPassFilterMono(mt);
		d->bp_modulator[k] = create_bandPassFilterMono(mt);
		d->lp_modulator[k] = create_xPassFilterMono(mt, 0);

		if(d->bp_carrier[k] == NULL || d->bp_modulator[k] == NULL ||d->lp_modulator[k] == NULL)
			return 1;

		xPassFilterMonoSetResonance(d->lp_modulator[k], ftoFTYPE(0.0f));
	}

	bandPassFilterMonoSetFrequencies(d->bp_carrier[0], 31.0f / Fs, 1.4);
	bandPassFilterMonoSetFrequencies(d->bp_modulator[0], 31.0f / Fs, 1.4);
	xPassFilterMonoSetCutoff(d->lp_modulator[0], ftoFTYPE(31.0f / (2.0f * Fs)));

	bandPassFilterMonoSetFrequencies(d->bp_carrier[1], 62.0f / Fs, 1.4);
	bandPassFilterMonoSetFrequencies(d->bp_modulator[1], 62.0f / Fs, 1.4);
	xPassFilterMonoSetCutoff(d->lp_modulator[1], ftoFTYPE(62.0f / (2.0f * Fs)));

	bandPassFilterMonoSetFrequencies(d->bp_carrier[2], 125.0f / Fs, 1.4);
	bandPassFilterMonoSetFrequencies(d->bp_modulator[2], 125.0f / Fs, 1.4);
	xPassFilterMonoSetCutoff(d->lp_modulator[2], ftoFTYPE(125.0f / (2.0f * Fs)));

	bandPassFilterMonoSetFrequencies(d->bp_carrier[3], 250.0f / Fs, 1.4);
	bandPassFilterMonoSetFrequencies(d->bp_modulator[3], 250.0f / Fs, 1.4);
	xPassFilterMonoSetCutoff(d->lp_modulator[3], ftoFTYPE(250.0f / (4.0f * Fs)));

	bandPassFilterMonoSetFrequencies(d->bp_carrier[4], 500.0f / Fs, 1.4);
	bandPassFilterMonoSetFrequencies(d->bp_modulator[4], 500.0f / Fs, 1.4);
	xPassFilterMonoSetCutoff(d->lp_modulator[4], ftoFTYPE(500.0f / (4.0f * Fs)));

	bandPassFilterMonoSetFrequencies(d->bp_carrier[5], 1000.0f / Fs, 1.4);
	bandPassFilterMonoSetFrequencies(d->bp_modulator[5], 1000.0f / Fs, 1.4);
	xPassFilterMonoSetCutoff(d->lp_modulator[5], ftoFTYPE(1000.0f / (4.0f * Fs)));

	bandPassFilterMonoSetFrequencies(d->bp_carrier[6], 2000.0f / Fs, 1.4);
	bandPassFilterMonoSetFrequencies(d->bp_modulator[6], 2000.0f / Fs, 1.4);
	xPassFilterMonoSetCutoff(d->lp_modulator[6], ftoFTYPE(2000.0f / (4.0f * Fs)));

	bandPassFilterMonoSetFrequencies(d->bp_carrier[7], 4000.0f / Fs, 1.4);
	bandPassFilterMonoSetFrequencies(d->bp_modulator[7], 4000.0f / Fs, 1.4);
	xPassFilterMonoSetCutoff(d->lp_modulator[7], ftoFTYPE(4000.0f / (4.0f * Fs)));

	bandPassFilterMonoSetFrequencies(d->bp_carrier[8], 8000.0f / Fs, 1.4);
	bandPassFilterMonoSetFrequencies(d->bp_modulator[8], 8000.0f / Fs, 1.4);
	xPassFilterMonoSetCutoff(d->lp_modulator[8], ftoFTYPE(8000.0f / (4.0f * Fs)));

	bandPassFilterMonoSetFrequencies(d->bp_carrier[9], 16000.0f / Fs, 1.4);
	bandPassFilterMonoSetFrequencies(d->bp_modulator[9], 16000.0f / Fs, 1.4);
	xPassFilterMonoSetCutoff(d->lp_modulator[9], ftoFTYPE(16000.0f / (4.0f * Fs)));

	for(k = 0; k < VOCODER_CHANNELS; k++) {
		bandPassFilterMonoRecalc(d->bp_carrier[k]);
		bandPassFilterMonoRecalc(d->bp_modulator[k]);
		xPassFilterMonoRecalc(d->lp_modulator[k]);
	}

	return 0;
}

#else

void cleanup_filters(VocoderData *d) {
	if(d->indirect_carrier) {
		free(d->indirect_carrier);
		d->indirect_carrier = NULL;
	}
	if(d->indirect_modulator) {
		free(d->indirect_modulator);
		d->indirect_modulator = NULL;
	}
	if(d->indirect_output) {
		free(d->indirect_output);
		d->indirect_output = NULL;
	}
	if(d->prev_carrier) {
		free(d->prev_carrier);
		d->prev_carrier = NULL;
	}
	if(d->prev_modulator) {
		free(d->prev_modulator);
		d->prev_modulator = NULL;
	}

	if(d->carrier) {
		free(d->carrier);
		d->carrier = NULL;
	}
	if(d->modulator) {
		free(d->modulator);
		d->modulator = NULL;
	}
	if(d->output) {
		free(d->output);
		d->output = NULL;
	}
	if(d->prev_output){
		free(d->prev_output);
		d->prev_output = NULL;
	}
	if(d->next_output) {
		free(d->next_output);
		d->next_output = NULL;
	}

	if(d->fft_fwd_cfg) {
		free(d->fft_fwd_cfg);
		d->fft_fwd_cfg = NULL;
	}
	if(d->fft_rev_cfg) {
		free(d->fft_rev_cfg);
		d->fft_rev_cfg = NULL;
	}

	if(d->c_fft) {
		free(d->c_fft);
		d->c_fft = NULL;
	}
	if(d->m_fft) {
		free(d->m_fft);
		d->m_fft = NULL;
	}

	if(d->hanning) {
		free(d->hanning);
		d->hanning = NULL;
	}
}

void calculate_window_size(float length_f, int *fft_len, int *fft_lap) {
	float optimal = (float)FFT_OPTIMAL_LENGTH;
	if(optimal > length_f) optimal = length_f;
	int length = length_f;

	*fft_len = optimal;
	*fft_lap = optimal * 3 / 4; // 75% overlap

	// now we need to make sure we get an even number of pieces
	int k, l;

	// so first we calculate how many pieces we have, this is k. k can be uneven...
	k = (*fft_len) - (*fft_lap);
	k = length / k;

	// then we calculate a value for l, starting at k, such that length / l is an even integer value (no remainder)
	for(l = k; l > 1; l--) {
		if(length % l == 0) { // as soon as we find a l which divides length in even pieces we stop
			break;
		}
	}
	// if we cannot find an l which is larger than 1, then just let go... it will sound bad, but what can we do...
	if(l > 1) {
		*fft_lap = *fft_len - ((length) / l);
	}

}

void calculate_hanning(FTYPE *d, int len) {
	int n;
	float N = (float)len - 1;

	for(n = 0; n < len; n ++) {
		d[n] = 0.5 * (1.0f - cosf((2.0f * M_PI * ((float)n)) / N));
	}
}

int setup_filters(MachineTable *mt, VocoderData *d, int ol) {
	calculate_window_size((float)ol, &(d->fft_len), &(d->fft_lap));

#ifdef THIS_IS_A_MOCKERY
	printf("fft_len: %d - fft_lap: %d\n", d->fft_len, d->fft_lap);
#endif

	d->prev_carrier = (FTYPE *)calloc(ol, sizeof(FTYPE));
	d->prev_modulator = (FTYPE *)calloc(ol, sizeof(FTYPE));

	d->carrier = (FTYPE *)calloc(d->fft_len, sizeof(FTYPE));
	d->modulator = (FTYPE *)calloc(d->fft_len, sizeof(FTYPE));

	d->output = (FTYPE *)calloc(d->fft_len, sizeof(FTYPE));
	d->prev_output = (FTYPE *)calloc(ol, sizeof(FTYPE));
	d->next_output = (FTYPE *)calloc(ol, sizeof(FTYPE));

	d->fft_fwd_cfg = mt->prepare_fft(d->fft_len, 0);
	d->fft_rev_cfg = mt->prepare_fft(d->fft_len, 1);

	d->c_fft = (kiss_fft_cpx *)calloc(d->fft_len, sizeof(kiss_fft_cpx));
	d->m_fft = (kiss_fft_cpx *)calloc(d->fft_len, sizeof(kiss_fft_cpx));

	d->hanning = (FTYPE *)calloc(d->fft_len, sizeof(FTYPE));

	d->channel_width = (d->fft_len) / (VOCODER_CHANNELS * 2);

	if(
		(d->prev_carrier == NULL) ||
		(d->prev_modulator == NULL) ||
		(d->carrier == NULL) ||
		(d->modulator == NULL) ||
		(d->output == NULL) ||
		(d->prev_output == NULL) ||
		(d->next_output == NULL) ||
		(d->fft_fwd_cfg == NULL) ||
		(d->fft_rev_cfg == NULL) ||
		(d->c_fft == NULL) ||
		(d->m_fft == NULL) ||
		(d->hanning == NULL)) {
		return 1;
	}

	calculate_hanning(d->hanning, (d->fft_len));
	return 0;
}

#endif

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	VocoderData *d = (VocoderData *)calloc(1, sizeof(VocoderData));

	if(d == NULL) return NULL; // failed to allocate

	memset(d, 0, sizeof(VocoderData));

	SETUP_SATANS_MATH(mt);

	d->volume = 1.0;

	/* return pointer to instance data */
	return (void *)d;
}

void delete(void *data) {
	VocoderData *d = (VocoderData *)data;
	/* free instance data here */
	cleanup_filters(d);
	free(d);
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	VocoderData *d = (VocoderData *)void_info;
	if(strcmp("volume", name) == 0)
		return &(d->volume);

	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

#ifdef VOCODER_IIR_IMPLEMENTATION
void execute(MachineTable *mt, void *data) {
	VocoderData *d = (VocoderData *)data;

	SignalPointer *cs = mt->get_input_signal(mt, "Carrier");
	SignalPointer *ms = mt->get_input_signal(mt, "Modulator");
	SignalPointer *os = mt->get_output_signal(mt, "Mono");

	if(os == NULL) return;

	FTYPE *ou = mt->get_signal_buffer(os);
	int Fs = mt->get_signal_frequency(os);
	int ol = mt->get_signal_samples(os);

	libfilter_set_Fs(Fs);

	if(d->bp_carrier == NULL) {
		if(setup_filters(mt, d, (float)Fs)) {
			// failed - cleanup and set cs and ms to NULL (this triggers a clear of the output signal, and a return);
			cleanup_filters(d);
			cs = ms = NULL;
		}
	}

	if(cs == NULL || ms == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < ol; t++) {
			ou[t] = itoFTYPE(0);
		}
		return;
	}

	FTYPE *c_in = mt->get_signal_buffer(cs);
	FTYPE *m_in = mt->get_signal_buffer(ms);

	int i, k;
	FTYPE m_base, c_base, output;

	FTYPE volume = ftoFTYPE(d->volume);

	for(i = 0; i < ol; i++) {
		m_base = m_in[i];
		c_base = c_in[i];
		output = itoFTYPE(0.0);

		for(k = 0; k < VOCODER_CHANNELS; k++) {
			FTYPE m, c;

			// modulator
			bandPassFilterMonoPut(d->bp_modulator[k], m_base);
			m = bandPassFilterMonoGet(d->bp_modulator[k]);
			m = ABS_FTYPE(m);
			xPassFilterMonoPut(d->lp_modulator[k], m);
			m = xPassFilterMonoGet(d->lp_modulator[k]);
			m = ABS_FTYPE(m);

			// carrier
			bandPassFilterMonoPut(d->bp_carrier[k], c_base);
			c = bandPassFilterMonoGet(d->bp_carrier[k]);

			// combine
			output += mulFTYPE(m, c);
		}

		ou[i] = mulFTYPE(output, volume);
	}
}

#else

inline void do_work(MachineTable *mt, VocoderData *d, int std_len, FTYPE *c, FTYPE *m, FTYPE *o) {
	float factor =  d->volume * 1.0f / ((float)std_len * 20);
	int u = (d->channel_width) * VOCODER_CHANNELS;
	// hanning
	{
		int k;

		for(k = 0; k < std_len; k++) {
			c[k] *= d->hanning[k];
			m[k] *= d->hanning[k];
		}
	}

	// flip to frequency space
	mt->do_fft(d->fft_fwd_cfg, c, d->c_fft);
	mt->do_fft(d->fft_fwd_cfg, m, d->m_fft);

	// do the work
	{
		int k, l;

		for(k = 0; k < VOCODER_CHANNELS; k++) {
			float avg = 0;
			// calculate modulator channel average amplitude
			for(l = 0; l < d->channel_width; l++) {
				float i = d->m_fft[k * d->channel_width + l].i;
				float r = d->m_fft[k * d->channel_width + l].r;
				float v;

				v = i * i + r * r; v = sqrtf(v);
				avg += v;
			}
			// we have to divide by the channel width to get the average
			avg /= (float)(d->channel_width);

			float midval_i = d->c_fft[u].i * d->m_fft[u].i;
			float midval_r = d->c_fft[u].r * d->m_fft[u].r;
			// modulate the carrier
			for(l = 0; l < d->channel_width; l++) {
				d->c_fft[k * d->channel_width + l].i *= avg * factor;
				d->c_fft[k * d->channel_width + l].r *= avg * factor;
				d->c_fft[u + k * d->channel_width + l].i = -(d->c_fft[k * d->channel_width + l].i);
				d->c_fft[u + k * d->channel_width + l].r = (d->c_fft[k * d->channel_width + l].r);;
			}
			d->c_fft[u].i = midval_i;
			d->c_fft[u].r = midval_r;
		}
	}

	// inverse into time space
	mt->inverse_fft(d->fft_rev_cfg, d->c_fft, o);
}

inline void build_input_buffer(int source_size, int offset, int buffer_size, FTYPE *old_source, FTYPE *new_source, FTYPE *destination) {
	if(offset < 0) {
		offset = -offset; // absolute offset
		int to_copy_from_old = buffer_size < (offset) ? buffer_size : offset;
		int to_copy_from_new = buffer_size - to_copy_from_old;
		memcpy(destination, &old_source[source_size - offset], (to_copy_from_old) * sizeof(FTYPE));
		if(to_copy_from_new)
			memcpy(&destination[to_copy_from_old], new_source, (to_copy_from_new) * sizeof(FTYPE));
	} else {
		memcpy(destination, &new_source[offset], (buffer_size) * sizeof(FTYPE));
	}
}

inline void build_output_buffer(int output_size, int offset, int buffer_size,
				FTYPE *buffer, FTYPE *dest, FTYPE *next_dest) {
	int i, k;

	for(i = 0, k = offset; i < buffer_size && k < output_size; i++, k++) {
		dest[k] += buffer[i];
	}
	k -= output_size;
	for(; i < buffer_size; i++, k++) {
		next_dest[k] += buffer[i];
	}
}

inline void execute_fft(MachineTable *mt, VocoderData *d, int ol, FTYPE *c_in, FTYPE *m_in, FTYPE *ou) {
	int piece_len = d->fft_len - d->fft_lap;
	int pieces = ol / piece_len;

	int read_offset = piece_len - (piece_len * pieces);
	int write_offset = 0;

	int p;

	// clear next output buffer and copy the old one into the current
	memcpy(ou, d->prev_output, ol * sizeof(FTYPE));
	memset(d->next_output, 0, ol * sizeof(FTYPE));

	for(p = 0; p < pieces; p++) {
		build_input_buffer(ol, read_offset, d->fft_len, d->prev_carrier, c_in, d->carrier);
		build_input_buffer(ol, read_offset, d->fft_len, d->prev_modulator, m_in, d->modulator);

		do_work(mt, d, d->fft_len, d->carrier, d->modulator, d->output);

		build_output_buffer(ol, write_offset, d->fft_len,
				    d->output, ou, d->next_output);
		read_offset += piece_len;
		write_offset += piece_len;
	}

	memcpy(d->prev_carrier, c_in, ol * sizeof(FTYPE));
	memcpy(d->prev_modulator, m_in, ol * sizeof(FTYPE));

	// swap previous and next output buffers
	FTYPE *t = d->next_output;
	d->next_output = d->prev_output;
	d->prev_output = t;
}

void execute(MachineTable *mt, void *data) {
	VocoderData *d = (VocoderData *)data;

	SignalPointer *cs = mt->get_input_signal(mt, "Carrier");
	SignalPointer *ms = mt->get_input_signal(mt, "Modulator");
	SignalPointer *os = mt->get_output_signal(mt, "Mono");

	if(os == NULL) return;

	FTYPE *ou = mt->get_signal_buffer(os);
	int ol = mt->get_signal_samples(os);

	if(d->fft_len == 0) {
		int k = 1;

		// we must have at least FFT_OPTIMAL_LENGTH of data to do FFTs
		if(ol < FFT_OPTIMAL_LENGTH) {
			for(k = 1; k * ol < FFT_OPTIMAL_LENGTH; k++) { /* find multiple of ol that is larger than FFT_OPTIMAL_LENGTH */ }

			d->indirect_carrier = (FTYPE *)calloc(k * ol, sizeof(FTYPE));
			d->indirect_modulator = (FTYPE *)calloc(k * ol, sizeof(FTYPE));
			d->indirect_output = (FTYPE *)calloc(k * ol, sizeof(FTYPE));

			if(
				(d->indirect_carrier == NULL) ||
				(d->indirect_modulator == NULL) ||
				(d->indirect_output == NULL)
				) {
				cs = ms = NULL;
			}
		}
		if((cs == NULL) || (ms == NULL) || (setup_filters(mt, d, ol * k))) {
			// failed - cleanup and set cs and ms to NULL (this triggers a clear of the output signal, and a return);
			cleanup_filters(d);
			cs = ms = NULL;
		}

		d->indirect_index = 1;
		d->indirect_limit = k;
		d->indirect_size = k * ol;

#ifdef THIS_IS_A_MOCKERY
		printf("indirect_limit: %d\n", d->indirect_limit);
#endif
	}

	if(cs == NULL || ms == NULL) {
		// just clear output, then return
		int t;
		for(t = 0; t < ol; t++) {
			ou[t] = itoFTYPE(0);
		}
		return;
	}

	FTYPE *c_in = mt->get_signal_buffer(cs);
	FTYPE *m_in = mt->get_signal_buffer(ms);

	if(d->indirect_limit > 1) {
		if(d->indirect_index == 0)
			execute_fft(mt, d, d->indirect_size, d->indirect_carrier, d->indirect_modulator, d->indirect_output);

		int offset = d->indirect_index * ol;

		memcpy(&(d->indirect_carrier[offset]), c_in, ol * sizeof(FTYPE));
		memcpy(&(d->indirect_modulator[offset]), m_in, ol * sizeof(FTYPE));
		memcpy(ou, &(d->indirect_output[offset]), ol * sizeof(FTYPE));

		d->indirect_index = (d->indirect_index + 1) % (d->indirect_limit);
	} else {
		execute_fft(mt, d, ol, c_in, m_in, ou);
	}
}
#endif
