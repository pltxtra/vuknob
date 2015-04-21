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

#include <jngldrum/jinformer.hh>

#include "file_request_ui.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

bool exportLoops = false;
static std::string export_path;

KammoEventHandler_Declare(midiExportGui,"ExportSequence2MIDI:ExportLoops2MIDI");

static void busy_midi_export(void *cbdata_ignored) {
	SATAN_DEBUG("Will export midi (%d) to %s\n", exportLoops, export_path.c_str());
	try {
		MachineSequencer::export2MIDI(exportLoops, export_path);
	} catch(jException e) {
		SATAN_DEBUG_("CAUGHT jException...\n");
		jInformer::inform(e.message);
		SATAN_DEBUG_("jException used to inform...\n");
	} catch(...) {
		SATAN_DEBUG_("CAUGHT unknown exception...\n");
		jInformer::inform("Exception thrown while exporting to midi, cause unknown.)");
		SATAN_DEBUG_("not used to inform...\n");
	}
	
	SATAN_DEBUG_("MIDI exported...");
}

static void yes(void *ignored) {
	KammoGUI::do_busy_work("Exporting...", "to MIDI.", busy_midi_export, NULL);
}

static void no(void *ignored) {
	// do not do anything
}

virtual void on_click(KammoGUI::Widget *wid) {
	if(wid->get_id() == "ExportLoops2MIDI")
		exportLoops = true;
	else if(wid->get_id() == "ExportSequence2MIDI")
		exportLoops = false;

	std::string path = Machine::get_record_file_name();
	export_path = path + ".mid";

	if(path == "") {
		KammoGUI::display_notification(
			"Information",
			"Please save the project one time first...");
		return;
	}
	
	KammoGUI::ask_yes_no("Do you want to export to this MIDI?", export_path,
			     yes, NULL,
			     no, NULL);
}

KammoEventHandler_Instance(midiExportGui);

