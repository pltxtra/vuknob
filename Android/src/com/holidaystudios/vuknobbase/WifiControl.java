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

import android.content.Context;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.util.Log;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.Arrays;
import java.util.Enumeration;

public class WifiControl {
	private android.net.wifi.WifiManager.WifiLock wifi_lock;

	private final android.net.wifi.WifiManager wifiManager;

	private Enumeration<InetAddress> getWifiInetAddresses() {
		final WifiInfo wifiInfo = wifiManager.getConnectionInfo();
		final String macAddress = wifiInfo.getMacAddress();
		final String[] macParts = macAddress.split(":");
		final byte[] macBytes = new byte[macParts.length];
		for (int i = 0; i< macParts.length; i++) {
			macBytes[i] = (byte)Integer.parseInt(macParts[i], 16);
		}
		try {
			final Enumeration<NetworkInterface> e =  NetworkInterface.getNetworkInterfaces();
			while (e.hasMoreElements()) {
				final NetworkInterface networkInterface = e.nextElement();
				if (Arrays.equals(networkInterface.getHardwareAddress(), macBytes)) {
					return networkInterface.getInetAddresses();
				}
			}
		} catch (SocketException e) {
			Log.wtf("WIFIIP", "Unable to NetworkInterface.getNetworkInterfaces()");
		}

		// we did not find any match, default to first
		try {
			final Enumeration<NetworkInterface> e =  NetworkInterface.getNetworkInterfaces();
			while (e.hasMoreElements()) {
				final NetworkInterface networkInterface = e.nextElement();
				return networkInterface.getInetAddresses();
			}
		} catch (SocketException e) {
			Log.wtf("WIFIIP", "Unable to NetworkInterface.getNetworkInterfaces()");
		}
	
		return null;
	}

	@SuppressWarnings("unchecked")
	private <T extends InetAddress> T getWifiInetAddress(final Class<T> inetClass) {
		final Enumeration<InetAddress> e = getWifiInetAddresses();
		if(e == null) return null;
	
		while (e.hasMoreElements()) {
			final InetAddress inetAddress = e.nextElement();
			if (inetAddress.getClass() == inetClass) {
				return (T)inetAddress;
			}
		}
		return null;
	}

	public WifiControl(Context context) {
		wifiManager = (android.net.wifi.WifiManager)context.getSystemService(android.content.Context.WIFI_SERVICE);

		wifi_lock = wifiManager.createWifiLock(android.net.wifi.WifiManager.WIFI_MODE_FULL_HIGH_PERF, "mywifilockthereturn");
		wifi_lock.setReferenceCounted(true);
	}

	public void acquire() {
		wifi_lock.acquire();
	}
	
	public void release() {
		wifi_lock.release();
	}

	public String getIp() throws SocketException {
		final Inet4Address inet4Address = getWifiInetAddress(Inet4Address.class);
		final Inet6Address inet6Address = getWifiInetAddress(Inet6Address.class);

		if(inet6Address != null) return inet6Address.getHostAddress();
		if(inet4Address != null) return inet4Address.getHostAddress();

		throw new SocketException("No local network address found.");
	}
}
