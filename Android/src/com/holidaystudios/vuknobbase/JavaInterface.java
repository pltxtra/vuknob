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

package com.holidaystudios.vuknobbase;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;
import android.view.View;
import java.io.InputStream;
import android.media.AudioTrack;
import android.media.AudioManager;
import android.media.AudioFormat;
import java.lang.Thread;
import android.util.Log;
import java.util.Vector;
import android.content.Intent;
import java.io.File;
import java.io.RandomAccessFile;
import android.net.Uri;

import com.ice.tar.*;

import com.toolkits.kamoflage.Kamoflage;

import java.io.File;
import java.io.IOException;

import android.media.AudioRecord;
import android.os.Environment;

public class JavaInterface {

	private static class AudioRecorder extends Thread {
		int minBufferSize;
		AudioRecord ar;
		File targetFile;
		RandomAccessFile target;
		byte[] buffer;
		
		/**
		 * Creates a new audio recording at the given path
		 */
		public AudioRecorder(String _path) throws IOException {
			String path = _path;
			
			minBufferSize = AudioRecord.getMinBufferSize(
				44100,
				android.media.AudioFormat.CHANNEL_IN_MONO,
				android.media.AudioFormat.ENCODING_PCM_16BIT);
			minBufferSize *= 2;
			
			ar = new AudioRecord(
				android.media.MediaRecorder.AudioSource.MIC,
				44100,
				android.media.AudioFormat.CHANNEL_IN_MONO,
				android.media.AudioFormat.ENCODING_PCM_16BIT,
				minBufferSize);
			
			String state = android.os.Environment.getExternalStorageState();
			if(!state.equals(android.os.Environment.MEDIA_MOUNTED))  {
				throw new IOException("SD Card is not mounted.  It is " + state + ".");
			}
			
			// make sure the directory we plan to store the recording in exists
			File directory = new File(path).getParentFile();
			if (!directory.exists() && !directory.mkdirs()) {
				throw new IOException("Path to file could not be created.");
			}

			targetFile = new File(path);
			targetFile.createNewFile();
			target = new RandomAccessFile(targetFile, "rw");
			
			buffer = new byte[minBufferSize];

			write_wav_header(0);
		}

		private void write_wav_header(int current_data_block_size_in_bytes) throws IOException {
			long data_length = current_data_block_size_in_bytes;
			long total_length = data_length + 44; // 44 is the size of the RIFF/WAV header we are writing...

			byte[] riff_header = new byte[44];

			// write "RIFF"
			riff_header[0] = (byte)0x52; riff_header[1] = 0x49; riff_header[2] = 0x46; riff_header[3] = 0x46;

			// write total length, little endian
			riff_header[4] = (byte)((total_length & 0x000000ff) >>  0);
			riff_header[5] = (byte)((total_length & 0x0000ff00) >>  8);
			riff_header[6] = (byte)((total_length & 0x00ff0000) >> 16);
			riff_header[7] = (byte)((total_length & 0xff000000) >> 24);

			// write "WAVE"
			riff_header[8] = 0x57; riff_header[9] = 0x41; riff_header[10] = 0x56; riff_header[11] = 0x45;

			// write "fmt "
			riff_header[12] = 0x66; riff_header[13] = 0x6d; riff_header[14] = 0x74; riff_header[15] = 0x20;

			// write "fmt " chunk length, which is 16 byte (0x0010 in little endian)
			riff_header[16] = 0x10; riff_header[17] = 0x00; riff_header[18] = 0x00; riff_header[19] = 0x00;

			// write "fmt " contents, lookup meaning of each byte from the RIFF/WAV specification please...
			riff_header[20] = 0x01; riff_header[21] = 0x00; riff_header[22] = 0x01; riff_header[23] = 0x00;
			riff_header[24] = 0x44; riff_header[25] = (byte)0xac; riff_header[26] = 0x00; riff_header[27] = 0x00;
			riff_header[28] = (byte)0x88; riff_header[29] = 0x58; riff_header[30] = 0x01; riff_header[31] = 0x00;
			riff_header[32] = 0x02; riff_header[33] = 0x00; riff_header[34] = 0x10; riff_header[35] = 0x00;

			// write "data"
			riff_header[36] = 0x64; riff_header[37] = 0x61; riff_header[38] = 0x74; riff_header[39] = 0x61;

			// write data block length, little endian
			riff_header[40] = (byte)((data_length & 0x000000ff) >>  0);
			riff_header[41] = (byte)((data_length & 0x0000ff00) >>  8);
			riff_header[42] = (byte)((data_length & 0x00ff0000) >> 16);
			riff_header[43] = (byte)((data_length & 0xff000000) >> 24);

			// write it to disk
			target.seek(0);
			target.write(riff_header);
		}

		private boolean do_record;
		public void run() {
			int max_limit = 44100 * 45; // limit to 45 seconds
			int written_size = 0;
			try {
				write_wav_header(0);
				ar.startRecording();
			} catch(IOException e) {
				// error, no use to record anything...
				do_record = false;
			}

			while(do_record) {
				if(max_limit > 0) {
					ar.read(buffer, 0, minBufferSize);
					try {
						target.write(buffer);
						written_size += minBufferSize;
						max_limit -= minBufferSize;
					} catch(IOException e) {
						// error - perhaps disk is full
						max_limit = 0;						
					}
				} else {
					do_record = false;
				}
			}
			Log.v("SATAN", "Recording stopped...\n");
			if(ar != null) {
				ar.stop();
				ar.release();
				ar = null;

				try {
					write_wav_header(written_size);
					target.close();
				} catch(IOException e) {
					// something went wrong with the file... just ignore it
				}
			}
			// end of the line
		}
		
		public void begin_record() {
			do_record = true;
			start();
		}

		public void stop_record() {
			do_record = false;
			try {
				join();
			} catch(java.lang.InterruptedException e) {
				// just ignore this fault
			}
		}
	}

	private static AudioRecorder ard = null;

	public static boolean StartRecord(String filename) {
		if(ard != null)
			return false;
		try {
			ard = new AudioRecorder(filename);
			ard.begin_record();
		} catch(IOException e) {
			return false;
		}
		return true;
	}

	public static void StopRecord() {
		if(ard != null) {
			ard.stop_record();
			ard = null;
		}
	}
	
	private static Activity creator;
	private static NsdHelper nsd_helper;	

	public static native void SetupInterface(String installation_identifier);
	public static native void AddService(String service_name, String host, int port);
	
	public static void SetupNativeJavaInterface(Activity _creator) {
		creator = _creator;

		nsd_helper = new NsdHelper(creator);
		nsd_helper.initializeNsd();

		SetupInterface(Installation.id(_creator));
	}

	public static void tearDown() {
		nsd_helper.tearDown();
	}

	public static String CallTarFunction(Vector<String> argvec) {
		String argv[] = new String[argvec.size()];
		argvec.toArray(argv);
	
		Log.v("SATAN", "Calling taring");
		tar app = new tar();
		try {
			app.instanceMain(argv);
		} catch(com.ice.tar.InvalidHeaderException ex) {
			return ex.getMessage();
		} catch(java.io.IOException ex) {
			return ex.getMessage();
		}

		return "";
	}

	public static boolean ShareMusicFile(String path_to_file) {
		File audio = new File(path_to_file);

		if(!audio.exists()) return false;
		
		Intent intent = new Intent(Intent.ACTION_SEND).setType("audio/*");
		intent.putExtra(Intent.EXTRA_STREAM, Uri.fromFile(audio));
		creator.startActivity(Intent.createChooser(intent, "Share to"));

		return true;
	}

	public static void AnnounceService(int port) {
		nsd_helper.registerService(port);
	}

	public static void TakedownService() {
		nsd_helper.tearDown();
	}

	public static void DiscoverServices() {
		nsd_helper.discoverServices();
	}

	public static void ListServices() {
		for(android.net.nsd.NsdServiceInfo service : nsd_helper.get_discovery_set()) {
			Log.v("SATAN", "JAVA - calling AddService(" + service.getServiceName() + ", " + service.getHost().getHostAddress() + ", " + String.valueOf(service.getPort()) + ") ...\n");
			AddService(service.getServiceName(), service.getHost().getHostAddress(), service.getPort());
		}
	}

	private static short[] empty_preview_data;
	private static int preview_buffer_size;
	private static AudioTrack at;
	private static short[] preview_data;
	private static int preview_index, preview_samples_left;

	public static boolean Preview16BitWavNextBuffer() {
		int samples_to_play = preview_samples_left < preview_buffer_size ? preview_samples_left : preview_buffer_size;

		at.write(preview_data, preview_index, samples_to_play);

		if(samples_to_play < preview_buffer_size)
			at.write(empty_preview_data, 0, preview_buffer_size - samples_to_play);

		preview_samples_left -= samples_to_play;
		preview_index += samples_to_play;
		
		if(preview_samples_left <= 0) return false;

		return true;
	}
	
	public static void Preview16BitWavStop() {
		at.stop();
		at.release();
	}
	
	public static void Preview16BitWavStart(int channels, int samples, int frequency, short[] data) {
		// we ignore frequency, currently
		int sampleRate = AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);
		
		int channel_mode = channels == 2 ? AudioFormat.CHANNEL_OUT_STEREO : AudioFormat.CHANNEL_OUT_MONO;

		int bfsize = 0;
		int minBufferSize = AudioTrack.getMinBufferSize(
			sampleRate,
			channel_mode,
			AudioFormat.ENCODING_PCM_16BIT);		
		
		at = new AudioTrack(
			AudioManager.STREAM_MUSIC,
			sampleRate,
			channel_mode,
			AudioFormat.ENCODING_PCM_16BIT,
			minBufferSize,
			AudioTrack.MODE_STREAM
			);
		
		preview_data = data;
		preview_buffer_size = minBufferSize / 2; // we want it in samples, not bytes
		empty_preview_data = new short[preview_buffer_size];
		int k;
		for(k = 0; k < preview_buffer_size; k++) empty_preview_data[k] = 0;
		preview_index = 0;
		preview_samples_left = samples * channels; // we want number of samples for all channels 
		
		
		at.play();

	}
}

