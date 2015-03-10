/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
 * Copyright (C) 2012 by Anton Persson
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
 * the GNU General Public License version 2; see COPYING for the complete License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "whistle_analyzer.hh"
#include "wavloader.hh"
#include "kiss_fftr.h"
#include "machine.hh"
#include <kamogui.hh>

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#define FFT_SIZE 512

static bool please_allocate = true;
static kiss_fftr_cfg cfg;
static kiss_fft_cpx fft[FFT_SIZE];
static FTYPE samples[FFT_SIZE];

#define TRESHOLD 7.0f
#define QUIET_TRESHOLD 1

#define WHISTLE_LOW_TONE 10
#define WHISTLE_HI_TONE 90

#define NOTE_TABLE_LENGTH 256 + 4 + 24
float note_table[NOTE_TABLE_LENGTH];

void calc_note_freq() {
	/* note table */
	int n;
	for(n = 0; n < NOTE_TABLE_LENGTH; n++) {
		note_table[n] =
			440.0 * pow(2.0, (n - 69) / 12.0);
	}
}

void WhistleAnalyzer::add_note_to_sequence(
	MachineSequencer *mseq, int loop_id,
	int sratei,
	int current_chunk, int current_note, int note_length_in_chunks) {

	MachineSequencer::Loop *loop = mseq->get_loop(loop_id);
	if(!loop) return;

	MachineSequencer::NoteEntry note;

	int posi = (current_chunk - note_length_in_chunks) * FFT_SIZE;
	int stpi = (current_chunk) * FFT_SIZE;

	float srate = (float)sratei;
	float pos = (float)posi;
	float stp = (float)stpi;

	pos /= srate; stp /= srate;

	float spb = 1.0f / ((float)Machine::get_bpm() / 60.0f); // calculate seconds per beat
	float spl = spb / ((float)Machine::get_lpb()); // calculate seconds per line

	// convert positions in seconds to position in lines
	pos = pos / spl;
	stp = stp / spl;

	int pos_i = (int)pos;
	note.on_at = PAD_TIME(pos_i, 0); note.length = stp - pos;
	note.channel = 0; note.program = 0;
	note.velocity = 0x7f;
	note.note = current_note;

	loop->note_insert(&note);

//	SATAN_DEBUG("Added note %d, at %d with length %d (%d)\n", current_note, note.on, note.length, note_length_in_chunks);
}

class Analyzor {
public:
	MachineSequencer *mseq;
	int loop_id;
	std::string input_wave;
};

void WhistleAnalyzer::analyze(MachineSequencer *mseq, int loop_id, const std::string &input_wave) {
	Analyzor *a = new Analyzor();

	a->mseq = mseq;
	a->loop_id = loop_id;
	a->input_wave = input_wave;

	KammoGUI::do_busy_work("Analyzing...", "Analyzing your whistle...", analyze_bsy, a);
}

void WhistleAnalyzer::analyze_bsy(void *ppp) {
	Analyzor *a = (Analyzor *)ppp;
	MachineSequencer *mseq = a->mseq;
	int loop_id = a->loop_id;
	std::string input_wave = a->input_wave;

	delete a;
	
	if(!mseq) return;

	if(loop_id == -1) {
		// create new loop first
		loop_id = mseq->add_new_loop();
	}
	
	WavLoader::MemoryMappedWave *mwav =
		WavLoader::MemoryMappedWave::get(input_wave);
       	
	if(please_allocate) {
		cfg = kiss_fftr_alloc(1024, 0, NULL, NULL);
		calc_note_freq();
		please_allocate = false;
	}

	if(mwav->get_channels_per_sample() != 1) {
		delete mwav;
		return;
	}

	if(mwav->get_bits_per_sample() != 16) {
		delete mwav;
		return;
	}
	
	uint16_t srate = mwav->get_sample_rate();
	uint32_t length = mwav->get_length_in_samples();
	int16_t *data = (int16_t *)mwav->get_data_pointer();

	if(!data) {
		delete mwav;
		return;
	}
	
	uint32_t chunk, max_chunks;
	int k;

	max_chunks = length / FFT_SIZE;

	int current_note = -1, current_length = -1;
	int quiet_counter = 0;
	
	for(chunk = 0; chunk < max_chunks; chunk++) {
		// convert chunk of wav to FTYPE data
		for(k = 0; k < FFT_SIZE; k++) {
#ifdef __SATAN_USES_FXP
			int32_t s = (int32_t)data[chunk * FFT_SIZE + k];
			s = s << 8;
#else
			float s = (float)data[chunk * FFT_SIZE + k];
			s /= 32768.0f;
#endif
			samples[k] = s;
		}
		
		kiss_fftr(cfg, samples, fft);

		float v;
		float maximum = 0.0f;
		int maximum_k = -1;
		
		for(k = 0; k < FFT_SIZE; k++) {
			float i = fft[k].i;
			float r = fft[k].r;
			v = i * i + r * r; v = sqrtf(v);

#ifdef __SATAN_USES_FXP
			v /= 32768.0f;
#endif
			
			if(v > maximum) {
				maximum = v;
				maximum_k = k;
			}
		}

		if(maximum_k > -1 && maximum > TRESHOLD) {
			SATAN_DEBUG("NOTE ABOVE TRESHOLD: %d (level %3.2f, threshold %3.2f)\n", maximum_k, maximum, TRESHOLD);
		} else {
			SATAN_DEBUG("NOTE BELOW TRESHOLD: %d (level %3.2f, threshold %3.2f)\n", maximum_k, maximum, TRESHOLD);
		}

		if(maximum_k < WHISTLE_LOW_TONE ||
		   maximum_k > WHISTLE_HI_TONE) maximum_k = -1; // not a probable tone while whistleing...
		
		if(maximum_k > -1 && maximum > TRESHOLD) {
			quiet_counter = 0;
			
			float freq = 44100.0f / 1024.0f;

			freq *= maximum_k;
			
			float lower, upper;
			
			int n, this_note = -1;
			for(n = 1; n < NOTE_TABLE_LENGTH - 1; n++) {
				lower = note_table[n] - ((note_table[n] - note_table[n - 1]) / 2.0f);
				upper = note_table[n] + (note_table[n + 1] - note_table[n]) / 2.0f;

				if(freq > lower && freq < upper) {
					this_note = n;
					break;
				}
			}

			int note_diff = current_note - this_note;
//			SATAN_DEBUG("Chunk %d - detected note %d, level %f (limit %f) (%d) --- %d\n", chunk, this_note, maximum, TRESHOLD, note_diff, maximum_k);

			if(current_note == -1 && this_note != -1) {
				current_note = this_note;
				current_length = 0;
			} else if(
				(current_note != this_note && this_note) &&
				(((note_diff) * (note_diff)) > 4)
				)
			{
				// it lasted at lease one step, so add up!
				current_length++;

				// then end it
				add_note_to_sequence(
					mseq, loop_id,
					(int)srate,
					chunk, current_note, current_length);

				current_length = -1;
				current_note = -1;
			} else {
				current_length++;
			}
		} else if(current_note != -1) {
			if(quiet_counter < QUIET_TRESHOLD) {
				current_length++;
				quiet_counter++;
			} else {
				quiet_counter = 0;
//				SATAN_DEBUG("Chunk %d - detected quiet level %f (limit %f)\n", chunk, maximum, TRESHOLD);
				
				// it lasted at least one step, so add upp!
				current_length++;
				
				// then end current note
				add_note_to_sequence(
					mseq, loop_id,
					(int)srate,
					chunk, current_note, current_length);
				
				current_length = -1;
				current_note = -1;
			}
		}

	}
}
