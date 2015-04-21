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

#ifdef ANDROID
#include "android_java_interface.hh"
#endif

#include "machine.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#include <kamogui.hh>

KammoEventHandler_Declare(shareUI,"ShareOgg");

virtual void on_click(KammoGUI::Widget *wid) {
	SATAN_DEBUG_("ShareOgg on click...\n");
	if(wid->get_id() == "ShareOgg") {
#ifdef ANDROID
		std::string oname = Machine::get_record_file_name();
		oname += ".ogg";
		SATAN_DEBUG_("ShareOgg %s...\n", oname.c_str());

		if(AndroidJavaInterface::share_musicfile(oname) == false) {
			KammoGUI::display_notification(
				"OGG not found:",
				"Sorry.. You must export wav to ogg before you can share...");
		}
		
#else
		KammoGUI::display_notification(
			"Not implemented:",
			"Sorry! Feature not implemented.");
#endif
	}
}

KammoEventHandler_Instance(shareUI);


