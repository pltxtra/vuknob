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


#include <config.h>

#include <fixedpointmath.h>

#include "vuknob_android_audio.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

JNIEnv *VuknobAndroidAudio::android_audio_env = NULL;

int VuknobAndroidAudio::srate = 0;
int VuknobAndroidAudio::bfsiz = 0;
int VuknobAndroidAudio::bytesPsample = 0;

jobject VuknobAndroidAudio::audio_bridge = NULL;
jmethodID VuknobAndroidAudio::bridge_write = NULL;
jmethodID VuknobAndroidAudio::bridge_stop = NULL;
jshort *VuknobAndroidAudio::java_target_buffer = NULL;
jshortArray VuknobAndroidAudio::bridge_buffer = NULL;

VuknobAndroidAudio *VuknobAndroidAudio::audio_object = new VuknobAndroidAudio();
void *VuknobAndroidAudio::dynamic_machine_data = NULL;
int (*VuknobAndroidAudio::dynamic_machine_entry)(void *data) = NULL;

/* main android audio machine class */

void VuknobAndroidAudio::java_thread_loop(JNIEnv *e, jclass sac) {
	enter();
	if(android_audio_env != e) {
		android_audio_env = e;
		satan_audio_class = (jclass)(e->NewGlobalRef((jobject)sac));
	}
	if(!java_bridge_created && (!setup_audio_bridge())) {
		leave();
		return;
	}

	int (*__dme)(void *data);
	__dme = dynamic_machine_entry;
	void *__dmd = dynamic_machine_data;

	leave();

	fill_buffers(0, NULL, 0, 0);
	if(__dme) {
		// XXX check for error status (return value != 0)
		(void)__dme(__dmd);
	}

	// move data to java array
	android_audio_env->SetShortArrayRegion(
		bridge_buffer, (jint)0, (jint)bfsiz , (jshort*)java_target_buffer);

	// write data to android
	int written = 0, result;

	while(written < bfsiz) {
		result = android_audio_env->CallIntMethod(
			audio_bridge, bridge_write,
			bridge_buffer, written, bfsiz - written);

		if(result < 0) {
			printf("Failure in VuknobAndroidAudio::java_thread_loop()\n");
			fflush(0);
		} else if(result == 0) {
			fflush(0);
		} else written += result;
	}
}

// this is actually an internal kamoflage routine that's not aimed
// to be used directly, so this is a hack... yes.. a hack!
void __setup_env_for_thread(JNIEnv *env);

extern "C" {

	static volatile int native_frequency = 44100;
	static volatile int native_buffer_size = 4410;
	static volatile int playback_mode = __PLAYBACK_OPENSL_DIRECT;

	JNIEXPORT void Java_com_holidaystudios_vuknobbase_VuknobAndroidAudio_registerNativeAudioConfigurationData
	(JNIEnv *env, jclass jc, jint freq, jint blen, jint _playback_mode) {
		native_frequency = freq;
		native_buffer_size = blen;
		playback_mode = _playback_mode;

		SATAN_DEBUG("  !!!! Frequency: %d\n", native_frequency);
		SATAN_DEBUG("  !!!! Buffer Size: %d\n", native_buffer_size);
		SATAN_DEBUG("  !!!! Playback mode: %d\n", playback_mode);
	}

	int VuknobAndroidAudio__get_native_audio_configuration_data(int *frequency, int *buffersize) {
		*frequency = native_frequency;
		*buffersize = native_buffer_size;
		VuknobAndroidAudio *i = VuknobAndroidAudio::instance();

		i->enter();
		i->stop_using_audiotrack = true;
		i->leave();

		return playback_mode;
	}

	JNIEXPORT jboolean Java_com_holidaystudios_vuknobbase_VuknobAndroidAudio_javaThread
	(JNIEnv *env, jclass jc) {
		VuknobAndroidAudio *i = VuknobAndroidAudio::instance();

		// call this kamoflage internal routine to register this thread.. yes, a bit ugly.

		__setup_env_for_thread(env);

		if(i->stop_using_audiotrack)
			return JNI_FALSE;

		i->java_thread_loop(env, jc);

		return JNI_TRUE;
	}

	void VuknobAndroidAudio__CLEANUP_STUFF() {
		VuknobAndroidAudio *i = VuknobAndroidAudio::instance();
		i->enter();
		i->dynamic_machine_entry = NULL;
		i->dynamic_machine_data = NULL;
		i->leave();
	}

	void VuknobAndroidAudio__SETUP_STUFF(int *period_size, int *rate,
				       int (*__entry)(void *data),
				       void *__data,
				       int (**android_audio_callback)
				       (fp8p24_t vol, fp8p24_t *in, int il, int ic),
				       void (**android_audio_stop_f)(void)
		) {
		VuknobAndroidAudio *i = VuknobAndroidAudio::instance();

		i->enter();
		i->dynamic_machine_entry = __entry;
		i->dynamic_machine_data = __data;
		*(android_audio_callback) = i->fill_buffers;
		*(android_audio_stop_f) = i->stop_audio;

		*period_size = i->bfsiz/2;
		*rate = i->srate;
		i->leave();
	}

	void (*samsung_jack_set_playbackfunction)(void(*func)(unsigned int framesize,
							      unsigned int frequency,
							      void *data_ptr,
							      float *buffer_left,
							      float *buffer_right), void *data_ptr) = NULL;
};

VuknobAndroidAudio::VuknobAndroidAudio() : java_bridge_created(false), stop_using_audiotrack(false) {
	std::cout << "************** CREATING ANDROID AUDIO MACHINE **************\n";


}

VuknobAndroidAudio *VuknobAndroidAudio::instance() {
	if(!VuknobAndroidAudio::audio_object)
		VuknobAndroidAudio::audio_object = new VuknobAndroidAudio();

	return VuknobAndroidAudio::audio_object;
}

bool VuknobAndroidAudio::setup_audio_bridge() {
	printf("   VuknobAndroidAudio::setup_audio_bridge()\n"); fflush(0);

	android_audio_thread = pthread_self();

	jmethodID getAudioBridge = android_audio_env->GetStaticMethodID(
		satan_audio_class, "getAudioBridge","()Landroid/media/AudioTrack;");
	jmethodID getAudioParameter = android_audio_env->GetStaticMethodID(
		satan_audio_class, "getAudioParameter","(I)I");

	audio_bridge = android_audio_env->CallStaticObjectMethod(
		satan_audio_class,
		getAudioBridge);

	srate = android_audio_env->CallStaticIntMethod(
		satan_audio_class,
		getAudioParameter,
		0);
	bfsiz = android_audio_env->CallStaticIntMethod(
		satan_audio_class,
		getAudioParameter,
		1);
	bytesPsample = android_audio_env->CallStaticIntMethod(
		satan_audio_class,
		getAudioParameter,
		2);

	bfsiz /= 2; // we want to know number of shorts, not number of bytes...

	if(bytesPsample != 2) return false;

	SATAN_DEBUG("    ANDROID AUDIO BUFFER: %d\n", bfsiz);

	java_target_buffer = (jshort *)malloc(bfsiz * 2);
	if(java_target_buffer == NULL) return false;
	bridge_buffer = android_audio_env->NewShortArray(bfsiz);
	bridge_buffer = (jshortArray)android_audio_env->NewGlobalRef((jobject)bridge_buffer);

	audio_bridge =
		android_audio_env->NewGlobalRef(audio_bridge);

	audio_bridge_class = android_audio_env->GetObjectClass(audio_bridge);
	audio_bridge_class = (jclass)(android_audio_env->NewGlobalRef((jobject)audio_bridge_class));
	bridge_write = android_audio_env->GetMethodID(
		audio_bridge_class, "write",
		"([SII)I");
	bridge_stop = android_audio_env->GetMethodID(
		audio_bridge_class, "stop",
		"()V");

	java_bridge_created = true;

	return true;
}

int VuknobAndroidAudio::fill_buffers(fp8p24_t vol, fp8p24_t *in, int il, int ic) {
	if(in == NULL) {
		// no attached signals, just zero out
		memset(java_target_buffer, 0, bfsiz * 2);
	} else {
		if(ic != 2) {
			printf("VuknobAndroidAudio expects stereo output"
			       ", found mono or multi-channel.\n");
			fflush(0);
			return -1;
		}

		// mix input into android buffer
		int i;

		for(i = 0; i < il * ic; i++) {
			int32_t out;

			in[i] = mulfp8p24(in[i], vol);

			if(in[i] < itofp8p24(-1)) in[i] = itofp8p24(-1);
			if(in[i] > itofp8p24(1)) in[i] = itofp8p24(1);

			out = (in[i] << 7);
			out = (out & 0xffff0000) >> 16;

			java_target_buffer[i] = (int16_t)out;
		}

	}
	return 0;
}

void VuknobAndroidAudio::stop_audio() {
	VuknobAndroidAudio *i = instance();

	SATAN_DEBUG_("WILL STOP AUDIO PLAYBACK.");
	if(i != NULL) {
		i->android_audio_env->CallVoidMethod(
			audio_bridge, bridge_stop);
	}

}
