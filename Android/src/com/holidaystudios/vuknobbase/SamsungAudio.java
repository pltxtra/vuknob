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

package com.holidaystudios.vuknobbase;

import android.app.Activity;
import android.os.Bundle;
import android.util.AndroidRuntimeException;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import com.samsung.android.sdk.SsdkUnsupportedException;
import com.samsung.android.sdk.professionalaudio.Sapa;
import com.samsung.android.sdk.professionalaudio.SapaProcessor;
import com.samsung.android.sdk.professionalaudio.SapaService;

public class SamsungAudio {

	private final static String TAG = "VuKNOB";

	SapaService mService;
	SapaProcessor mClient;
	Activity creator;

	public boolean startUp(Activity _creator) {
		creator = _creator;

		try {
			Sapa sapa = new Sapa();
			sapa.initialize(creator);
			mService = new SapaService();
			mService.stop(true);
			mService.start(SapaService.START_PARAM_DEFAULT_LATENCY);
			mClient = new SapaProcessor(creator, null, new SapaProcessor.StatusListener() {

				@Override
				public void onKilled() {
					Log.v(TAG, "vuKNOB will be closed, because Samsung Professional Audio was closed.");
					mService.stop(true);
					creator.finish();
				}
			});
			mService.register(mClient);

		} catch (SsdkUnsupportedException e) {
			Log.v(TAG, "Phone does not support Samsung Professional Audio");
			return false;
		} catch (InstantiationException e) {
			Log.v(TAG, "Samsung Professional Audio failed to instantiate");
			return false;
		} catch (AndroidRuntimeException e){
			Log.v(TAG, "Samsung Professional Audio failed to start");
			return false;
		}

		if(mClient != null)
			mClient.activate();

		return true;
	}

	protected void stop() {
		if(mService != null){
			if(mClient != null){
				mService.unregister(mClient);
			}
			mService.stop(true);
		}
	}
}
