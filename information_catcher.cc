/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
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

#include "information_catcher.hh"
#include <kamogui.hh>

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"


InformationCatcher *InformationCatcher::ICatch = NULL;

void InformationCatcher::init() {
	ICatch = new InformationCatcher();
	ICatch->start_to_run();
}

void InformationCatcher::start_to_run() {
	if(ICatch != NULL) {
		thrd->run();
	}	
}

InformationCatcher::InformationCatcher() : jInformer() {
	if(ICatch == NULL) {
		thrd = new IThread();
	}
}
InformationCatcher::~InformationCatcher() {}

InformationCatcher::IThread::IThread() : jThread("InformationCatcher") {}
InformationCatcher::IThread::~IThread() {}

void InformationCatcher::IThread::display_message(void *d) {
	jInformer::Message *m = (jInformer::Message *)d;
	std::string msg = m->get_content();
	delete m;

	KammoGUI::display_notification("Information:", msg);
}

void InformationCatcher::IThread::thread_body() {
	while(1) {
		jInformer::Message *m = (jInformer::Message *)ICatch->eq.wait_for_event();

		KammoGUI::run_on_GUI_thread(display_message, m);
	}
}
