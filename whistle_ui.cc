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

#include "machine.hh"

#include "dynamic_machine.hh"
#include "machine_sequencer.hh"
#include "whistle_analyzer.hh"
#include "general_tools.hh"

#ifdef ANDROID
#include "android_java_interface.hh"
#endif

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#define TEMP_WHISTLE_FILE (DEFAULT_SATAN_ROOT "/satan_temporary_whistle.wav")

static int completed_selection_process = 0; // 0 = not completed, 1 = selected machine, 2 = selected loop and process is complete
static MachineSequencer *selected_machine = NULL;
static int selected_loop = -1; // -1 = create new loop, >= 0 overwrite existing loop

static void prepare_machines_list() {
	static KammoGUI::List *machines = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&machines, "whistleMachineSelector");

	std::map<std::string, MachineSequencer *, ltstr> sequencers = MachineSequencer::get_sequencers();
	std::map<std::string, MachineSequencer *>::iterator k;
	
	std::vector<std::string> row_contents;

	machines->clear();

	row_contents.clear();
	row_contents.push_back("<SELECT>");
	machines->add_row(row_contents);
	
	for(k  = sequencers.begin();
	    k != sequencers.end();
	    k++) {
		row_contents.clear();
		row_contents.push_back((*k).first);
		machines->add_row(row_contents);
	}
}

static void prepare_loops_list(MachineSequencer *m) {
	selected_machine = m;
	
	static KammoGUI::List *loops = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&loops, "whistleLoopSelector");

	int max_loop_id = m->get_nr_of_loops();

	SATAN_DEBUG("max_loop_id: %d\n", max_loop_id);
	
	std::vector<std::string> row_contents;

	loops->clear();

	row_contents.clear();
	row_contents.push_back("<SELECT>");
	loops->add_row(row_contents);
	
	row_contents.clear();
	row_contents.push_back("<CREATE NEW>");
	loops->add_row(row_contents);

	int k;
	char bfr[16];
	for(k = 0;
	    k < max_loop_id;
	    k++) {
		snprintf(bfr, 16, "%02d", k);
		
		row_contents.clear();
		row_contents.push_back(bfr);
		loops->add_row(row_contents);
	}
}

KammoEventHandler_Declare(WhistleUIHandler, "whistleStart:whistleStop:whistleConvert:whistleMachineSelector:whistleLoopSelector");

virtual void on_select_row(KammoGUI::Widget *widget, KammoGUI::List::iterator row) {
	if(widget->get_id() == "whistleMachineSelector") {
		std::string machine_name = ((KammoGUI::List *)widget)->get_value(row, 0);
		SATAN_DEBUG("--- SELECTED MACHINE: %s\n", machine_name.c_str());

		if(machine_name != "<SELECT>") {
			std::map<std::string, MachineSequencer *, ltstr> sequencers = MachineSequencer::get_sequencers();
			std::map<std::string, MachineSequencer *>::iterator k;
			
			for(k  = sequencers.begin();
			    k != sequencers.end();
			    k++) {
				if(machine_name == (*k).first) {
					completed_selection_process = 1;
					
					prepare_loops_list((*k).second);
				}
			}
		}
	}
	if(widget->get_id() == "whistleLoopSelector") {
		std::string loop_id = ((KammoGUI::List *)widget)->get_value(row, 0);
		SATAN_DEBUG("--- SELECTED LOOP: %s\n", loop_id.c_str());

		if(loop_id != "<SELECT>") {
			if(loop_id == "<CREATE NEW>") {
				selected_loop = -1;
			} else if(selected_machine) {
				int max_k = selected_machine->get_nr_of_loops();
				int k;
				char bfr[16];
				
				for(k = 0; k < max_k; k++) {
					snprintf(bfr, 16, "%02d", k);

					if(loop_id == std::string(bfr)) {
						completed_selection_process = 2;

						selected_loop = k;
					}
				}
			}
		}
	}
}

virtual void on_click(KammoGUI::Widget *wid) {
	if(wid->get_id() == "whistleStart") {
		
		if(!(AndroidJavaInterface::start_audio_recorder(TEMP_WHISTLE_FILE))) {
			jInformer::inform("Failed to start whistle recorder, sorry...");
		}
	}
	if(wid->get_id() == "whistleStop") {
		AndroidJavaInterface::stop_audio_recorder();

		// prepare machine selector list
		prepare_machines_list();

		// clear loop selector list
		static KammoGUI::List *loops = NULL;
		KammoGUI::get_widget((KammoGUI::Widget **)&loops, "whistleLoopSelector");
		loops->clear();

		// we've completed nothing of the selection process thus far.
		completed_selection_process = 0;
	}
	if(wid->get_id() == "whistleConvert") {
		if(completed_selection_process != 2) {
			jInformer::inform("You must select machine and loop first...");
		} else {
			WhistleAnalyzer::analyze(selected_machine, selected_loop, TEMP_WHISTLE_FILE);
		}
	}
}

KammoEventHandler_Instance(WhistleUIHandler);
