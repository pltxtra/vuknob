/*
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

#include <iostream>
using namespace std;

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include "machine.hh"

#include <kamogui.hh>
#include <kamo_xml.hh>
#include <jngldrum/jinformer.hh>

#include <typeinfo>
#include <fstream>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "dynamic_machine.hh"
#include "machine_sequencer.hh"

#include "general_tools.hh"

#include "advanced_file_request_ui.hh"

#ifdef ANDROID
#include "android_java_interface.hh"
#endif

#include "satan_project_entry.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

KammoEventHandler_Declare(NewProjectUIHandler, "NEW_PROJECT_BT");

static void yes(void *ignored) {
	SatanProjectEntry::clear_satan_project();

	SATAN_DEBUG("Will trigger refreshProject.\n");
	// make sure that everyone gets that the project has been loaded
	static KammoGUI::UserEvent *ue = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&ue, "refreshProject");
	if(ue != NULL)
		trigger_user_event(ue);
	SATAN_DEBUG("refreshProject returned OK.\n");
}

virtual void on_click(KammoGUI::Widget *wid) {
	if(wid->get_id() == "NEW_PROJECT_BT") {
		KammoGUI::ask_yes_no("Are you sure?", "Do you really want to start all new?",
				     yes, NULL,
				     NULL, NULL);
	}
}

KammoEventHandler_Instance(NewProjectUIHandler);
