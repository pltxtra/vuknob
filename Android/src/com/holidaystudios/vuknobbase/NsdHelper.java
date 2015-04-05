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

/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.holidaystudios.vuknobbase;

import android.content.Context;
import android.net.nsd.NsdServiceInfo;
import android.net.nsd.NsdManager;
import android.util.Log;

public class NsdHelper {

	Context mContext;

	NsdManager mNsdManager;
	NsdManager.ResolveListener mResolveListener;
	NsdManager.DiscoveryListener mDiscoveryListener;
	NsdManager.RegistrationListener mRegistrationListener;

	public static final String SERVICE_TYPE = "_vuknob._tcp.";

	public static final String TAG = "SATAN";
	public String mServiceName = "vuKNOB";

	NsdServiceInfo mService;

	java.util.Set<NsdServiceInfo> discovery_set;
	
	public NsdHelper(Context context) {
		mContext = context;
		mNsdManager = (NsdManager) context.getSystemService(Context.NSD_SERVICE);
		discovery_set = new java.util.HashSet<NsdServiceInfo>();
	}

	public void initializeNsd() {
		initializeResolveListener();
		initializeDiscoveryListener();
		initializeRegistrationListener();
	}

	public java.util.Set<NsdServiceInfo> get_discovery_set() {
		return discovery_set;
	}

	public void initializeDiscoveryListener() {
		mDiscoveryListener = new NsdManager.DiscoveryListener() {

				@Override
				public void onDiscoveryStarted(String regType) {
					Log.d(TAG, "Service discovery started");
				}

				@Override
				public void onServiceFound(NsdServiceInfo service) {
					Log.d(TAG, "Service discovery success: " + service.getServiceName());
					if (!service.getServiceType().equals(SERVICE_TYPE)) {
						Log.d(TAG, "Unknown Service Type: " + service.getServiceType());
					} else if (service.getServiceName().equals(mServiceName)) {
						Log.d(TAG, "Same machine: ]" + mServiceName + "[ == ]" + service.getServiceName() + "[");
					} else if (service.getServiceName().contains(mServiceName)){
						mNsdManager.resolveService(service, mResolveListener);
					}
				}

				@Override
				public void onServiceLost(NsdServiceInfo service) {
					Log.e(TAG, "service lost" + service);
					if (mService == service) {
						mService = null;
					}
					if(discovery_set.contains(service)) {
						discovery_set.remove(service);
					}
				}
            
				@Override
				public void onDiscoveryStopped(String serviceType) {
					Log.i(TAG, "Discovery stopped: " + serviceType);        
				}

				@Override
				public void onStartDiscoveryFailed(String serviceType, int errorCode) {
					Log.e(TAG, "Discovery failed: Error code:" + errorCode);
					mNsdManager.stopServiceDiscovery(this);
				}

				@Override
				public void onStopDiscoveryFailed(String serviceType, int errorCode) {
					Log.e(TAG, "Discovery failed: Error code:" + errorCode);
					mNsdManager.stopServiceDiscovery(this);
				}
			};
	}

	public void initializeResolveListener() {
		mResolveListener = new NsdManager.ResolveListener() {

				@Override
				public void onResolveFailed(NsdServiceInfo serviceInfo, int errorCode) {
					Log.e(TAG, "Resolve failed" + errorCode);
				}

				@Override
				public void onServiceResolved(NsdServiceInfo serviceInfo) {
					Log.e(TAG, "Resolve Succeeded: " + serviceInfo.getHost() + ":" + String.valueOf(serviceInfo.getPort()));

					if (serviceInfo.getServiceName().equals(mServiceName)) {
						Log.d(TAG, "Same IP.");
						return;
					}
					mService = serviceInfo;

					if(!discovery_set.contains(serviceInfo)) {
						discovery_set.add(serviceInfo);
						Log.d(TAG, "   added service to set.\n");
					}
				}
			};
	}

	public void initializeRegistrationListener() {
		mRegistrationListener = new NsdManager.RegistrationListener() {

				@Override
				public void onServiceRegistered(NsdServiceInfo NsdServiceInfo) {
					mServiceName = NsdServiceInfo.getServiceName();
					Log.d(TAG, "Service registered - " + mServiceName);
				}
            
				@Override
				public void onRegistrationFailed(NsdServiceInfo arg0, int arg1) {
				}

				@Override
				public void onServiceUnregistered(NsdServiceInfo arg0) {
				}
            
				@Override
				public void onUnregistrationFailed(NsdServiceInfo serviceInfo, int errorCode) {
				}
            
			};
	}

	public void registerService(int port) {
		NsdServiceInfo serviceInfo  = new NsdServiceInfo();
		serviceInfo.setPort(port);
		serviceInfo.setServiceName(mServiceName);
		serviceInfo.setServiceType(SERVICE_TYPE);

		Log.d(TAG, "Will register service on port " + String.valueOf(port) + ".");
	
		mNsdManager.registerService(
			serviceInfo, NsdManager.PROTOCOL_DNS_SD, mRegistrationListener);
        
	}

	public void discoverServices() {
		mNsdManager.discoverServices(
			SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, mDiscoveryListener);
	}
    
	public void stopDiscovery() {
		mNsdManager.stopServiceDiscovery(mDiscoveryListener);
	}

	public NsdServiceInfo getChosenServiceInfo() {
		return mService;
	}
    
	public void tearDown() {
		mNsdManager.unregisterService(mRegistrationListener);
	}
}
