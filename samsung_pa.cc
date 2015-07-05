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

#include "samsung_pa.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

namespace android {

	IMPLEMENT_APA_INTERFACE(APAWave)

	APAWave::APAWave() {}
	APAWave::~APAWave() {}

	int APAWave::init() {
		SATAN_DEBUG("wave.so initialized");
		return APA_RETURN_SUCCESS;
	}

	int APAWave::sendCommand(const char* command) {
		SATAN_DEBUG("APAWave::sendCommand: %s\n", command);
		return APA_RETURN_SUCCESS;
	}

	IJackClientInterface* APAWave::getJackClientInterface() {
		return &sjack_client;
	}

	int APAWave::request(const char* what, const long ext1, const long capacity, size_t &len, void*data) {
		return APA_RETURN_SUCCESS;
	}

};
