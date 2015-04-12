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

#ifndef CLASS_ANDROID_JAVA_INTERFACE
#define CLASS_ANDROID_JAVA_INTERFACE

#include <stdint.h>

#include <iostream>
#include <string>
#include <set>

#include <jngldrum/jthread.hh>
#include <kamogui.hh>

class AndroidJavaInterface{
private:
	static jclass JavaInterfaceClass;
	static jmethodID call_tar_function;
	static jmethodID sharemusicfile;

	static jmethodID announceservice;
	static jmethodID takedownservice;
	static jmethodID discoverservices;
	static jmethodID listservices;

	static jmethodID preview16bitwav_start;
	static jmethodID preview16bitwav_next_buffer;
	static jmethodID preview16bitwav_stop;

	static jmethodID start_record;
	static jmethodID stop_record;
public:
	static void setup_interface(jclass jc);
	static void call_tar(std::vector<std::string > &args);
	static bool share_musicfile(const std::string &path_to_file);

	static void announce_service(int port);
	static void takedown_service();
	static void discover_services();
	static std::map<std::string, std::pair<std::string, int> > list_services();
	
	static void preview_16bit_wav_start(int channels, int samples, int frequency, int16_t *data);
	static bool preview_16bit_wav_next_buffer();
	static void preview_16bit_wav_stop();

	static bool start_audio_recorder(const std::string &fname);
	static void stop_audio_recorder();
};

#endif
