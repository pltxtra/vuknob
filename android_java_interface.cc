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

#include <sys/time.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "android_java_interface.hh"

jclass AndroidJavaInterface::JavaInterfaceClass = NULL;
jmethodID AndroidJavaInterface::call_tar_function = NULL;
jmethodID AndroidJavaInterface::sharemusicfile = NULL;

jmethodID AndroidJavaInterface::announceservice = NULL;
jmethodID AndroidJavaInterface::discoverservices = NULL;
jmethodID AndroidJavaInterface::listservices = NULL;

jmethodID AndroidJavaInterface::preview16bitwav_start = NULL;
jmethodID AndroidJavaInterface::preview16bitwav_next_buffer = NULL;
jmethodID AndroidJavaInterface::preview16bitwav_stop = NULL;

jmethodID AndroidJavaInterface::start_record = NULL;
jmethodID AndroidJavaInterface::stop_record = NULL;

std::string __ANDROID_installation_id = "not set";
std::map<std::string, std::pair<std::string, int> > current_services;

extern "C" {
	JNIEXPORT void Java_com_holidaystudios_vuknobbase_JavaInterface_SetupInterface
	(JNIEnv *env, jclass jc, jstring installation_id) {
		__setup_env_for_thread(env);

		const char *id = (*env).GetStringUTFChars(installation_id, NULL);
		__ANDROID_installation_id = id;
		(*env).ReleaseStringUTFChars(installation_id, id); 
		
		AndroidJavaInterface::setup_interface(jc);
	}	

	JNIEXPORT void Java_com_holidaystudios_vuknobbase_JavaInterface_AddService
	(JNIEnv *env, jclass jc, jstring service_name, jstring service_host, jint service_port) {
		__setup_env_for_thread(env);

		const char *sname = (*env).GetStringUTFChars(service_name, NULL);
		const char *shost = (*env).GetStringUTFChars(service_host, NULL);
		current_services[std::string(sname)] = std::pair<std::string, int>({shost, service_port});
		(*env).ReleaseStringUTFChars(service_name, sname); 
		(*env).ReleaseStringUTFChars(service_host, shost); 
		
		AndroidJavaInterface::setup_interface(jc);
	}	
};

void AndroidJavaInterface::setup_interface(jclass jc) {
	JNIEnv *env = get_env_for_thread();

	JavaInterfaceClass = (jclass)(env->NewGlobalRef((jobject)jc));

	call_tar_function = env->GetStaticMethodID(
		JavaInterfaceClass, "CallTarFunction",
		"(Ljava/util/Vector;)Ljava/lang/String;");
	sharemusicfile = env->GetStaticMethodID(
		JavaInterfaceClass, "ShareMusicFile",
		"(Ljava/lang/String;)Z");

	announceservice = env->GetStaticMethodID(
		JavaInterfaceClass, "AnnounceService",
		"(I)V");
	discoverservices = env->GetStaticMethodID(
		JavaInterfaceClass, "DiscoverServices",
		"()V");
	listservices = env->GetStaticMethodID(
		JavaInterfaceClass, "ListServices",
		"()V");

	preview16bitwav_start = env->GetStaticMethodID(
		JavaInterfaceClass, "Preview16BitWavStart",
		"(III[S)V");
	preview16bitwav_next_buffer = env->GetStaticMethodID(
		JavaInterfaceClass, "Preview16BitWavNextBuffer",
		"()Z");
	preview16bitwav_stop = env->GetStaticMethodID(
		JavaInterfaceClass, "Preview16BitWavStop",
		"()V");

	start_record = env->GetStaticMethodID(
		JavaInterfaceClass, "StartRecord",
		"(Ljava/lang/String;)Z");
	stop_record = env->GetStaticMethodID(
		JavaInterfaceClass, "StopRecord",
		"()V");


}

void AndroidJavaInterface::call_tar(std::vector<std::string > &args) {
	JNIEnv *env = get_env_for_thread();
	
	// prepare input and output Java Vectors...
	// Get vector object class
	static jclass vec_class = NULL;
	if(vec_class == NULL) {
		vec_class = env->FindClass("java/util/Vector");
		vec_class = (jclass)(env->NewGlobalRef((jobject)vec_class));
	}
	// Get vector constructor
	static jmethodID vec_new = env->GetMethodID( vec_class, "<init>","()V");
	// Get "addElement" method id of clase vector
	static jmethodID vec_add = env->GetMethodID( vec_class, "addElement", "(Ljava/lang/Object;)V");
	std::vector<std::string>::const_iterator k;
	
	///////////// CREATE JAVA args VECTOR
	// create vector object
	jobject j_args = env->NewObject( vec_class, vec_new);

	k = args.begin();
	for(
		; k != args.end(); k++) {
		env->CallVoidMethod(j_args, vec_add, env->NewStringUTF((*k).c_str()) );
	}

	/// Call tar!
	jstring exception_string;
	
	exception_string = (jstring)env->CallStaticObjectMethod(
		JavaInterfaceClass,
		call_tar_function,
		j_args);

	/// Check for JAVA exception
	if (env->ExceptionCheck()) {
		jthrowable exceptionHandle = env->ExceptionOccurred();

		env->ExceptionClear();
		
		jclass jThrowableClass = env->GetObjectClass( exceptionHandle );
		jmethodID methodID = env->GetMethodID( jThrowableClass, "getMessage", "()Ljava/lang/String;" );;
		
		exception_string = (jstring)env->CallObjectMethod(exceptionHandle, methodID);
	}

	/// Check if we received an exception message - if so, throw an exception!
	const char *_es = env->GetStringUTFChars(exception_string, NULL);
	std::string es(_es);	
	env->ReleaseStringUTFChars(exception_string, _es);

	if(es != "") {
		throw jException(es, jException::syscall_error);
	}

	// if we reached this position - we are ok!
}

bool AndroidJavaInterface::share_musicfile(const std::string &path_to_file) {
	JNIEnv *env = get_env_for_thread();
	jstring path_js = env->NewStringUTF(path_to_file.c_str());
	jboolean rval;
	
	rval = env->CallStaticBooleanMethod(
		JavaInterfaceClass,
		sharemusicfile,
		path_js);

	return rval == JNI_TRUE ? true : false;
}

void AndroidJavaInterface::announce_service(int port) {
	JNIEnv *env = get_env_for_thread();

	env->CallStaticVoidMethod(
		JavaInterfaceClass,
		announceservice,
		port);
	
}

void AndroidJavaInterface::discover_services() {
	JNIEnv *env = get_env_for_thread();

	env->CallStaticVoidMethod(
		JavaInterfaceClass,
		discoverservices
		);
	
}

std::map<std::string, std::pair<std::string, int> > AndroidJavaInterface::list_services() {
	JNIEnv *env = get_env_for_thread();

	current_services.clear();
	
	env->CallStaticVoidMethod(
		JavaInterfaceClass,
		listservices
		);

	return current_services;
}

void AndroidJavaInterface::preview_16bit_wav_start(int channels, int samples, int frequency, int16_t *data) {
	JNIEnv *env = get_env_for_thread();

	int bfsiz = channels * samples;
	
	jshortArray bridge_buffer = env->NewShortArray(bfsiz);
	
	env->SetShortArrayRegion(
		bridge_buffer,
		(jint)0, bfsiz,
		(jshort*)data);

	env->CallStaticVoidMethod(
		JavaInterfaceClass,
		preview16bitwav_start,
		channels, samples, frequency, bridge_buffer);
	
}

bool AndroidJavaInterface::preview_16bit_wav_next_buffer() {
	JNIEnv *env = get_env_for_thread();

	jboolean rval = env->CallStaticBooleanMethod(
		JavaInterfaceClass,
		preview16bitwav_next_buffer);

	return rval == JNI_TRUE ? true : false;
}

void AndroidJavaInterface::preview_16bit_wav_stop() {
	JNIEnv *env = get_env_for_thread();

	env->CallStaticVoidMethod(
		JavaInterfaceClass,
		preview16bitwav_stop);
}

bool AndroidJavaInterface::start_audio_recorder(const std::string &fname) {
	JNIEnv *env = get_env_for_thread();

	jstring path_js = env->NewStringUTF(fname.c_str());

	jboolean rval = env->CallStaticBooleanMethod(
		JavaInterfaceClass,
		start_record,
		path_js);

	return rval == JNI_TRUE ? true : false;
}

void AndroidJavaInterface::stop_audio_recorder() {
	JNIEnv *env = get_env_for_thread();

	env->CallStaticVoidMethod(
		JavaInterfaceClass,
		stop_record);
}
