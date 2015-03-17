/*
 * vuKNOB (c) 2015 by Anton Persson
 *
 * -------------------------------------------------
 *
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Bjorling
 * Copyright (C) 2008 by Anton Persson
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

package com.holidaystudios.vuknobbase;

import android.media.AudioTrack;
import android.media.AudioManager;
import android.media.AudioFormat;
import java.lang.Thread;
import java.util.*;
import android.util.Log;

import com.toolkits.kamoflage.Kamoflage;

public class VuknobAndroidAudio {

	public static native void registerNativeAudioConfigurationData(int freq, int blen, int dev_class);
	public static native boolean javaThread();
	
	private static class AudioThread extends Thread {

		AudioThread() {
			super("VuknobAndroidAudioThread");
		}
		
		public void run() {
			while(VuknobAndroidAudio.javaThread()) {
				if(at != null)
					VuknobAndroidAudio.at.play();
			}
		}
	}
	
	private static AudioTrack at = null;

	private static int sampleRate, minBufferSize;

	/*** THESE VALUES MUST MATCH THE VALUES IN dynlib.h ***/
	static final int __PLAYBACK_OPENSL_DIRECT    = 14; // The device should use direct OpenSL rendering
	static final int __PLAYBACK_OPENSL_BUFFERED  = 16; // The device should use buffered OpenSL rendering
	
	private static int getPlaybackMode() {
		/********** DEBUG OUTPUT **************/
		String s="Debug-infos:";
		s += "\n OS Version: " + System.getProperty("os.version") + "(" + android.os.Build.VERSION.INCREMENTAL + ")";
		s += "\n OS API Level: " + android.os.Build.VERSION.SDK;
		s += "\n Device: " + android.os.Build.DEVICE;
		s += "\n Model (and Product): " + android.os.Build.MODEL + " ("+ android.os.Build.PRODUCT + ")";
		Log.v("VuKNOB", s);


		/********** CREATE HASHMAP OF KNOWN DEVICES / PLAYBACK TYPE **********/
		HashMap<String, Integer> known_devices = new HashMap<String, Integer>();

		// hammerhead (aka Nexus 5) does not sound good in direct mode, so we set it to buffered mode. 
		known_devices.put("hammerhead", __PLAYBACK_OPENSL_BUFFERED);
		// m0 (aka Samsung Galaxy S3) does not sound good in direct mode, so we set it to buffered mode. 
		known_devices.put("m0", __PLAYBACK_OPENSL_BUFFERED);

		/** GET PREFERRED PLAYBACK MODE OF KNOWN DEVICE  *****/
		if(known_devices.containsKey(android.os.Build.DEVICE)) {
			return known_devices.get(android.os.Build.DEVICE);
		}

		/** THE DEVICE IS UNKNOWN; RETURN DEFAULT SETTING ****/
		// Previousoly we would default to direct mode (the recommended mode by Google - that isn't working on Nexus 5 as of September 2014... go figure...)
		// But now I default to BUFFERED 
		return __PLAYBACK_OPENSL_BUFFERED;
	}
	
	public static void scanNativeAudioConfiguration(AudioManager audioManager) {
		// set default frame rate and buffer size
		int f = 44100, b = 4410;

		// then we get backward compatible defaults
		f = AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);
		b = AudioTrack.getMinBufferSize(
			f,
			AudioFormat.CHANNEL_OUT_STEREO,
			AudioFormat.ENCODING_PCM_16BIT);
		
		// then we try to use the new api for native configuration data
		try {
			Class<?> cls = audioManager.getClass();
			Class[] cArg = new Class[1];
			cArg[0] = String.class;
			
			java.lang.reflect.Method getProperty = cls.getMethod("getProperty", cArg);
			Object result = null;
			
			result = getProperty.invoke(audioManager, "android.media.property.OUTPUT_FRAMES_PER_BUFFER");
			java.lang.String framesPerBuffer = (java.lang.String)result;

			result = getProperty.invoke(audioManager, "android.media.property.OUTPUT_SAMPLE_RATE");
			java.lang.String sampleRate  = (java.lang.String)result;

			Log.v("SATAN", "**************** Frames per buffer: " + framesPerBuffer);
			Log.v("SATAN", "**************** Sample rate: " + sampleRate);

			f = Integer.parseInt(sampleRate);
			b = Integer.parseInt(framesPerBuffer);
			
		} catch(java.lang.NoSuchMethodException ignored) {
			Log.v("SATAN", "Api audioManager.getProperty() is not available.");
			// ignore
		} catch(java.lang.IllegalAccessException ignored) {
			Log.v("SATAN", "Api audioManager.getProperty() could not be accessed.");
			// ignore
		} catch(java.lang.reflect.InvocationTargetException ignored) {
			Log.v("SATAN", "Api audioManager.getProperty() reported InvocationTargetException... ignored!");
			// ignore
		}

		int playback_mode = getPlaybackMode();
		registerNativeAudioConfigurationData(f, b, playback_mode);

		default_frequency = f;
		default_buffer_size = b;
	}

	public static int default_frequency = 0, default_buffer_size = 0;
	
	public static void createThread() {
		AudioThread athread = new AudioThread();
		athread.start();
	}

	public static void stopAudioBridge() {
		if(at != null)
			at.stop();
	}
	
	public static AudioTrack getAudioBridge() {
		sampleRate = AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);
		minBufferSize = AudioTrack.getMinBufferSize(
			sampleRate,
			AudioFormat.CHANNEL_OUT_STEREO,
			AudioFormat.ENCODING_PCM_16BIT);
		
		at = new AudioTrack(
			AudioManager.STREAM_MUSIC,
			sampleRate,
			AudioFormat.CHANNEL_OUT_STEREO,
			AudioFormat.ENCODING_PCM_16BIT,
			minBufferSize,
			AudioTrack.MODE_STREAM
			);

		return at;
	}

	public static int getAudioParameter(int param_x) {
		switch(param_x) {
		case 0:
			return sampleRate;
		case 1:
			return minBufferSize;
		case 2:
			return 2;
		default:
			return -1;
		}
	}

	public static void prepare(android.media.AudioManager audioManager) {
		// first we try to scan for the optimal audio configuration
		VuknobAndroidAudio.scanNativeAudioConfiguration(audioManager);
		VuknobAndroidAudio.createThread();
	}
}
