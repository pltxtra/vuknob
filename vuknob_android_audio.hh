/*
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

#ifndef CLASS_ANDROID_AUDIO
#define CLASS_ANDROID_AUDIO

#include <stdint.h>

#include <jngldrum/jthread.hh>
#include <kamogui.hh>

#include "machine.hh"
#include "dynlib/dynlib.h"

class VuknobAndroidAudio : public jThread::Synchronizer {
private:	
	bool java_bridge_created;

	pthread_t android_audio_thread;
	static JNIEnv *android_audio_env;

public:
	static int srate, bfsiz, bytesPsample;
	bool stop_using_audiotrack;
	
private:
	jclass audio_bridge_class;
	jclass satan_audio_class;
	static jobject audio_bridge;
	static jmethodID bridge_write;
	static jmethodID bridge_stop;
	static jshort *java_target_buffer;
	static jshortArray bridge_buffer;

	void (*dynamic_callback)(void);
	
	// private functions, NON-monitor guarded (call only when inside monitor guard)
	bool setup_audio_bridge();
	
	// instanziation
	VuknobAndroidAudio();
	static VuknobAndroidAudio *audio_object;

public:
	static void *dynamic_machine_data;
	static int (*dynamic_machine_entry)(void *data);
	static int period_size, rate;

public:
	void java_thread_loop(JNIEnv *e, jclass sac);
	
	static VuknobAndroidAudio *instance();

	static int fill_buffers(fp8p24_t vol, fp8p24_t *in, int il, int ic);
	static void stop_audio();
};

#endif
