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

#ifndef SAMSUNG_PA_HH
#define SAMSUNG_PA_HH

#include <IAPAInterface.h>
#include <APACommon.h>

#include "samsung_jack.hh"

namespace android {

class APAWave : IAPAInterface {

	public:
		APAWave();
		virtual ~APAWave();
		int init();
		int sendCommand(const char* command);
		IJackClientInterface* getJackClientInterface();
		int request(const char* what, const long ext1, const long capacity, size_t &len, void* data);
	private:
		SamsungJack sjack_client;
};

DECLARE_APA_INTERFACE(APAWave)

};

#endif // SAMSUNG_PA_HH
