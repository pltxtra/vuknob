/*
 * vuKNOB (c) 2015 by Anton Persson
 *
 * -------------------------------------------------
 *
 * VuKNOB is an instance of SATAN
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

package com.holidaystudios.vuknob;

import android.app.Dialog;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;
import android.view.View;
import android.util.Log;
import android.media.AudioManager;

import android.widget.ProgressBar;
import java.lang.Runnable;

import java.io.*;
import java.util.*;
import java.util.zip.*;

import com.toolkits.kamoflage.Kamoflage;
import com.toolkits.kamoflage.KamoflageActivity;

import com.holidaystudios.vuknobbase.VuknobAndroidAudio;
import com.holidaystudios.vuknobbase.JavaInterface;
import com.holidaystudios.vuknobbase.WifiControl;

public class vuKNOBnet extends KamoflageActivity
	implements 
	android.media.AudioManager.OnAudioFocusChangeListener
{
	final int BUFFER = 2048;

	private void unpack_native_files(String destdir) throws IOException {
		InputStream istream = null;
		istream = this.getResources().openRawResource(R.raw.nativefiletreearchive);
		
		if(istream == null)
			throw new IOException("Can't open native file tree archive.");
		
		ZipInputStream zis;
		
		zis = new ZipInputStream(istream);
				
		BufferedOutputStream dest = null;

		ZipEntry entry;

		while((entry = zis.getNextEntry()) != null) {
			if(entry.isDirectory()) {
				File f = new File(destdir + "/" + entry);
				f.mkdirs();				
			} else {
				int count;
				byte data[] = new byte[BUFFER];
				
				// write the files to the disk
				FileOutputStream fos = new 
					FileOutputStream(destdir + "/" + entry.getName());
				dest = new 
					BufferedOutputStream(fos, BUFFER);
				
				while ((count = zis.read(data, 0, BUFFER)) 
				       != -1) {
					dest.write(data, 0, count);
				}
				
				dest.flush();
				dest.close();
			}
		}
		zis.close();
	}

	private static TextView statusField;
	private static TextView buffersizeField;
	private static TextView exceptionType;
	private static TextView exceptionMessage;
	private static TextView exceptionStack;
	private static ProgressBar mProgress;
	private WifiControl wfctrl;
	private int mProgressStatus = 0;

	static Kamoflage k = null;
	static View result = null;
	static InputStream istream = null;

	public void onAudioFocusChange(int focusChange) {
		// ignore for now...
	}
	
	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		Log.v("VuKNOB", "Bringing up VuKNOB - entered onCreate()");

		this.requestWindowFeature(
			android.view.Window.FEATURE_NO_TITLE);
		this.getWindow().setFlags(
			android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN,
			android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN);

		Log.v("VuKNOB", "Bringing up VuKNOB - get android layout");
		k = new Kamoflage(this, R.layout.listrow, R.id.listlayout);
		Log.v("VuKNOB", "Bringing up VuKNOB - setContent");

		wfctrl = new WifiControl(this);
		wfctrl.acquire();
		
		setContentView(R.layout.main);
		Log.v("VuKNOB", "Bringing up VuKNOB - content set!");
		mProgress = (ProgressBar) findViewById(R.id.progress_bar);
		statusField = (TextView) findViewById(R.id.statusField);
		buffersizeField = (TextView) findViewById(R.id.buffersizeField);
		exceptionType = (TextView) findViewById(R.id.exceptionType);
		exceptionMessage = (TextView) findViewById(R.id.exceptionMessage);
		exceptionStack = (TextView) findViewById(R.id.exceptionStack);

		Log.v("VuKNOB", "Bringing up VuKNOB - reading Kamoflage UI layout");
		istream = this.getResources().openRawResource(R.raw.vuknobui);

		Log.v("VuKNOB", "Bringing up VuKNOB - request audio focus");
		android.media.AudioManager audioManager = (AudioManager) getSystemService(this.AUDIO_SERVICE);
		int ignored_result = audioManager.requestAudioFocus(this,
								    AudioManager.STREAM_MUSIC,
								    AudioManager.AUDIOFOCUS_GAIN);

		Log.v("VuKNOB", "Bringing up VuKNOB - prepare(audioManager);");
		VuknobAndroidAudio.prepare(audioManager);
		buffersizeField.setText(
			"Device: " + android.os.Build.DEVICE + ", " +
			"Frequency: " + String.valueOf(VuknobAndroidAudio.default_frequency) + ", " +
			"Buffer: " + String.valueOf(VuknobAndroidAudio.default_buffer_size)
			);
		Log.v("VuKNOB", "   prepare returned OK.");
		JavaInterface.SetupNativeJavaInterface(this);
		Log.v("VuKNOB", "   SetupNativeJavaInterface returned OK.");

//		Log.v("SATAN", "AutogenDate.DATE: " + AutogenDate.DATE);
		statusField.setText("Starting new thread...");
		final Activity uiAct = this;
		new Thread(new Runnable() {
				public void run() {
					try {
						String nativedatadir =
							getDir("nativedata",
							       MODE_PRIVATE).getAbsolutePath();
						
						// check if the timestamp file exists, if it does then
						// we unpacked this archive before, if it does not exist
						// then this specific version of the archive has not
						// been unpacked...
						File test = new File(nativedatadir + "/" + AutogenDate.DATE);
						if(!test.exists()) {
							uiAct.runOnUiThread(new Runnable() {
									public void run() {
										statusField.setText("Unpacking...");
									}
								});
//							Log.v("SATAN", "Unpacking fresh new files.");
							unpack_native_files(nativedatadir);
							test.createNewFile();
						} else {
							uiAct.runOnUiThread(new Runnable() {
									public void run() {
									statusField.setText("Unpacking not needed...");
									}
								});
//							Log.v("SATAN", "No need to unpack files.");
						}
//						Log.v("SATAN", "TJOOOOOZAN");
//						Log.v("SATAN", "   THREAD ID: " + Thread.currentThread().getId());
						uiAct.runOnUiThread(new Runnable() {
								public void run() {
									statusField.setText("Unpacking finished...");
								}
							});
						
						uiAct.runOnUiThread(new Runnable() {
								public void run() {
									mProgress.setProgress(10);
								}
							});
						
						uiAct.runOnUiThread(new Runnable() {
								public void run() {
									statusField.setText("Parsing UI file...");
								}
							});

						Log.v("Kamoflage", "Will parse UI...");
						result = k.parse(istream, mProgress, 10);
						Log.v("Kamoflage", "Parsed UI!");
						uiAct.runOnUiThread(new Runnable() {
								public void run() {
									statusField.setText("UI file is read, replacing view...");
								}});
						uiAct.runOnUiThread(new Runnable() {
								public void run() {
									setContentView(result);
								}
							});

					} catch(Exception e) {
						final String etyp = e.toString();
						final String emsg = e.getMessage();
						StringWriter sw = new StringWriter();
						PrintWriter pw = new PrintWriter(sw);
						e.printStackTrace(pw);
						final String stck = sw.toString();
						uiAct.runOnUiThread(new Runnable() {
								public void run() {
									exceptionType.setText(etyp);
									exceptionMessage.setText(emsg);
									exceptionStack.setText(stck);
								}
							});
						Log.v("SATAN", "Failed to unpack native data.");
						android.os.SystemClock.sleep(120 * 1000);
						java.lang.System.exit(0);
						return; // don't continue this thread...
						
					} 

				}
			}).start();
		
	}
	
	@Override
	protected void onStart() {
		super.onStart();
		// The activity is about to become visible.
	}
	@Override
	protected void onResume() {
		super.onResume();
		// The activity has become visible (it is now "resumed").
	}
	@Override
	protected void onPause() {
		super.onPause();
		// Another activity is taking focus (this activity is about to be "paused").
	}
	@Override
	protected void onStop() {
		super.onStop();
		// The activity is no longer visible (it is now "stopped")
	}
	@Override
	protected void onDestroy() {
		super.onDestroy();
		// The activity is about to be destroyed.
//		k.quit();
		JavaInterface.tearDown();
		wfctrl.release();
		android.os.SystemClock.sleep(10 * 1000);
		java.lang.System.exit(0);
	}

	static {
		Log.v("vuknob", "Loading native library for kamoflage.");
		System.loadLibrary("pathvariable");
		System.loadLibrary("kamoflage");
		Log.v("vuknob", "    Native library loaded.");
	}

}
