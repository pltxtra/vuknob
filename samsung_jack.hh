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

#ifndef SAMSUNG_JACK_HH
#define SAMSUNG_JACK_HH

#include <jack/jack.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>

#include <APACommon.h>
#include <IJackClientInterfaceV2.h>

namespace android {

	class SamsungJack : public IJackClientInterfaceV2 {
	public:
		SamsungJack();
		virtual ~SamsungJack();

		virtual int setUp(int argc, char *argv[]);
		virtual int tearDown();
		virtual int activate();
		virtual int deactivate();
		virtual int transport(TransportType type);
		virtual int sendMidi(char* midi);
		virtual char* getJackClientName(const char* name) {
			static char* name_ptr = NULL;
			if(name_ptr == NULL) name_ptr = strdup("vuKNOB");
			return name_ptr;
		}
		virtual void setByPassEnabled(bool /* ignore enable */) {}

	private:
		jack_port_t* outp_l;
		jack_port_t* outp_r;
		static jack_client_t* jackClient;

		static int frequency;
		static int fillerup(jack_nframes_t nframes, void *arg);
};

};

#endif
