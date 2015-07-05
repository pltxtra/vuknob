/*
 * vuKNOB (c) 2015 by Anton Persson
 *
 * -------------------------------------------------
 *
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
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

#include "samsung_jack.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

namespace android {

	jack_client_t *SamsungJack::jackClient = NULL;
	int SamsungJack::frequency = 1;

	void (*internal_pbfunc)(unsigned int frames,
				unsigned int frequency,
				void *data_ptr,
				float *buffer_left,
				float *buffer_right) = NULL;
	void* internal_pbdata = NULL;

	extern "C" {
		extern void (*samsung_jack_set_playbackfunction)(void(*func)(unsigned int framesize,
							  unsigned int frequency,
							  void *data_ptr,
							  float *buffer_left,
							  float *buffer_right), void *data_ptr);

		void actual_samsung_jack_set_playbackfunction(void(*func)(unsigned int framesize,
								   unsigned int frequency,
								   void *data_ptr,
								   float *buffer_left,
								   float *buffer_right), void *data_ptr) {
			internal_pbfunc = func;
			internal_pbdata = data_ptr;
			SATAN_DEBUG("Samsung Jack playback function registered: %p\n", func);
		}
	};

	SamsungJack::SamsungJack() {
		outp_l = NULL;
	}

	SamsungJack::~SamsungJack() {
		jackClient = NULL;
	}

	int SamsungJack::fillerup(jack_nframes_t frames, void *arg) {
		SamsungJack *thiz = (SamsungJack*)arg;
		jack_default_audio_sample_t *out_l = (jack_default_audio_sample_t*)jack_port_get_buffer (thiz->outp_l, frames);
		jack_default_audio_sample_t *out_r = (jack_default_audio_sample_t*)jack_port_get_buffer (thiz->outp_r, frames);

		for(unsigned int i=0; i<frames; i++ )
		{
			out_l[i] = 0.0f;
			out_r[i] = 0.0f;
		}

		if(internal_pbfunc) internal_pbfunc(frames, frequency, internal_pbdata, out_l, out_r);

		return 0;
	}

	int SamsungJack::setUp (int argc, char *argv[]) {
		SATAN_DEBUG("setUp argc %d", argc);
		for(int i = 0;i< argc; i++){
			SATAN_DEBUG("setup argv %s", argv[i]);
		}

		// You have to use argv[0] as the jack client name like below.
		jackClient = jack_client_open (argv[0], JackNullOption, NULL, NULL);
		if (jackClient == NULL) {
			return APA_RETURN_ERROR;
		}

		frequency = jack_get_sample_rate (jackClient);

		jack_set_process_callback (jackClient, fillerup, this);

		outp_l = jack_port_register (jackClient, "outl",
					     JACK_DEFAULT_AUDIO_TYPE,
					     JackPortIsOutput, 0);
		if(outp_l == NULL){
			return APA_RETURN_ERROR;
		}
		outp_r = jack_port_register (jackClient, "outr",
					     JACK_DEFAULT_AUDIO_TYPE,
					     JackPortIsOutput, 0);
		if(outp_r == NULL){
			return APA_RETURN_ERROR;
		}

		samsung_jack_set_playbackfunction = actual_samsung_jack_set_playbackfunction;

		return APA_RETURN_SUCCESS;
	}

	int SamsungJack::tearDown() {
		jack_client_close(jackClient);
		return APA_RETURN_SUCCESS;
	}

	int SamsungJack::activate() {
		jack_activate(jackClient);
		SATAN_DEBUG("SamsungJack::activate");

		jack_connect(jackClient, jack_port_name(outp_l), "out:playback_1");
		jack_connect(jackClient, jack_port_name(outp_r), "out:playback_2");

		return APA_RETURN_SUCCESS;
	}

	int SamsungJack::deactivate() {
		SATAN_DEBUG("SamsungJack::deactivate");
		jack_deactivate (jackClient);
		return APA_RETURN_SUCCESS;
	}

	int SamsungJack::transport(TransportType type) {
		return APA_RETURN_SUCCESS;
	}

	int SamsungJack::sendMidi(char* midi) {
		return APA_RETURN_SUCCESS;
	}
};
