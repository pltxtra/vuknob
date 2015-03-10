/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include "machine_sequencer.hh"

#include <kamogui.hh>
#include <fstream>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

static void (*callback_function)(void *cbd, const std::string &fname) = NULL;
static void *callback_data = NULL;

void fileRequestUI_getFile(const std::string &message, void (*cbf)(void *data, const std::string &fname), void *cbdata) {
	static KammoGUI::Label *fmessage = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&fmessage, "fReqHelp");

	if(fmessage) fmessage->set_title(message);
	
	SATAN_DEBUG_("setting callback...\n");
	callback_function = cbf;
	callback_data = cbdata;
}

KammoEventHandler_Declare(fileRequestUi,"fReqGO");

virtual void on_click(KammoGUI::Widget *wid) {
	if(wid->get_id() == "fReqGO") {
		static KammoGUI::Entry *fname = NULL;
		KammoGUI::get_widget((KammoGUI::Widget **)&fname, "fReqValue");
		
		if(fname && callback_function) {
			SATAN_DEBUG_("calling callback...\n");
			callback_function(callback_data, fname->get_text());
		}
	}
}

KammoEventHandler_Instance(fileRequestUi);

