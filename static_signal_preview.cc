/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
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

#include "static_signal_preview.hh"

#include <jngldrum/jthread.hh>
#include <kamogui.hh>

#ifdef ANDROID
#include "android_java_interface.hh"
#endif

#include <fixedpointmath.h>

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"


class Preview {
public:
	Dimension d;
	Resolution r;
	int channels, samples, frequency;
	void *data;
};

static void convert_to_16bitPCM(Preview *p) {
	// since PROJECT_INTERFACE_LEVEL == 7
	// we convert all WAV files
	// to f8p24_t (i.e. fixed point values)
	// we convert these here to 16 bit PCM for
	// preview
	if(p->d == _0D && p->r == _fx8p24bit) {
		fp8p24_t *buffer = (fp8p24_t *)p->data;
		int k, k_max = p->channels * p->samples;
		int16_t *d = (int16_t *)malloc(k_max * sizeof(int16_t));
		if(d) {
			for(k = 0; k < k_max; k++) {
				float v = fp8p24tof(buffer[k]);
				v *= 32767.0;
				d[k] = (int16_t)v;
			}
			free(p->data);
			p->data = d;
			p->r = _16bit;
		}
	}
}

static void set_minimum_playback(Preview *p, float time) {
	float c, s, f;
	
	c = p->channels; s = p->samples; f = p->frequency;

	if((s / f) > time) return; // no action needed

	if(p->d == _0D && p->r == _16bit) {
		float new_nr_of_samples = f * time;
		int sizetoAlloc = (sizeof(int16_t) * c * ((int)new_nr_of_samples));
		
		int16_t *d = (int16_t *)malloc(sizetoAlloc);
		if(d == NULL) return; // the system is under strain here... just drop out.

		memset(d, 0, sizetoAlloc);
		memcpy(d, p->data, sizeof(int16_t) * c * s);

		free(p->data);
		p->data = d;
		p->samples = new_nr_of_samples;
	}
}

static void preview_in_background(void *pp, KammoGUI::CancelIndicator &cid) {
	Preview *p = (Preview *)pp;

	convert_to_16bitPCM(p);
	
	set_minimum_playback(p, 0.5); // set the buffer to be at least 0.5 seconds, no matter how short the sample is
	
#ifdef ANDROID
	if(p->d == _0D && p->r == _16bit) {
		AndroidJavaInterface::preview_16bit_wav_start(
			p->channels, p->samples,
			p->frequency, (int16_t *)p->data);

		while(AndroidJavaInterface::preview_16bit_wav_next_buffer() && !(cid.is_cancelled())) {
			// do nothing, just loop
		}
		
		AndroidJavaInterface::preview_16bit_wav_stop();		
	}

#endif

	free(p->data);
	delete p;
}

void StaticSignalPreview::preview(
	Dimension d, Resolution r,
	int channels, int samples, int frequency,
	void *data) {
	
	Preview *p = new Preview();
	if(p == NULL) {
		// failure, make sure we free the data pointer before returning!
		free(data);
		return;
	}
	
	p->d = d; p->r = r;
	p->channels = channels; p->samples = samples; p->frequency = frequency;
	p->data = data;

	KammoGUI::do_cancelable_busy_work("Preview....", "Playing sample preview....", preview_in_background, p);
}
