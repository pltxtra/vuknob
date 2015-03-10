/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
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

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#include "machine.hh"

#include <jngldrum/jinformer.hh>

using namespace std;

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include "dynamic_machine.hh"
#include "machine_sequencer.hh"

#include <kamogui.hh>
#include <fstream>

#ifdef HAVE_LTDL
extern "C" {
#include <ltdl.h>
};
#endif

static KammoGUI::UserEvent *callback_event = NULL;

KammoEventHandler_Declare(MachineTypeHandler,"MachineType:addNode:showMachineTypeListScroller");

virtual void on_user_event(KammoGUI::UserEvent *ev, std::map<std::string, void *> data) {
	char *hint_match = NULL;

	// reset callback event
	callback_event = NULL;

	if(data.find("hint_match") != data.end())
		hint_match = (char *)data["hint_match"];

	// set callback event
	if(data.find("callback_event") != data.end())
		callback_event = (KammoGUI::UserEvent *)data["callback_event"];
	
	if(ev->get_id() == "showMachineTypeListScroller") {
		KammoGUI::List *list = NULL;
		KammoGUI::get_widget((KammoGUI::Widget **)&list, "MachineType");

		list->clear(); // flush the list before use
		
		std::set<std::string> handles;
		std::set<std::string>::iterator i;
		handles = DynamicMachine::get_handle_set();

		std::vector<std::string> row_values;

		SATAN_DEBUG("Got handles.\n");
		for(i = handles.begin(); i != handles.end(); i++) {
			std::string hint = DynamicMachine::get_handle_hint(*i);
			unsigned int p = 0;

			// if hint_match is set then we use that
			// to determine
			if(hint_match)
				p = hint.find(hint_match);
			
			SATAN_DEBUG("    - %s (%s, %d)\n", (*i).c_str(), hint.c_str(), p);
			
			if((p >= 0 && p < hint.size())) {
				// hint does contain the string hint_match
				// _OR_ hint_match was not set to begin with
				row_values.push_back(*i);
				list->add_row(row_values);
				row_values.clear();
				SATAN_DEBUG("       * ADDED\n");
			}
		}
	}

	// if hint_match was set we assume it was strdup() there fore we must free() here
	if(hint_match)
		free(hint_match);
}

void auto_connect(Machine *m) {
	if(m == NULL) return; // silently fail on NULL entry
	
	// we will try and attach m to the sink
	Machine *sink = Machine::get_sink();

	if(sink == NULL) {
		SATAN_DEBUG("auto_connect: no liveoutsink found.\n");
		goto fail;
	}

	{
		std::vector<std::string> inputs = sink->get_input_names();
		std::vector<std::string> outputs = m->get_output_names();
		
		// currently we are not smart enough to find matching sockets unless there are only one output and one input...
		if(inputs.size() != 1 || outputs.size() != 1) {
			SATAN_DEBUG("auto_connect: input.size() = %d, output.size() = %d.\n", inputs.size(), outputs.size());
			goto fail;
		}
		
		// OK, so there is only one possible connection to make.. let's try!
		try {
			sink->attach_input(m, outputs[0], inputs[0]);
			
			return; // we've suceeded
		} catch(jException e) {
			SATAN_DEBUG("auto_connect: error attaching - %s\n", e.message.c_str());
		} catch(...) {			
			// ignore the error, next stop is fail...
		}
	}
	
fail:
	{
		// automatic attempt failed, inform the user
		std::string  error_msg =
			"Automatic connection of machine '" +
			m->get_name() +
			"' failed. You must connect it manually if you want to use it.";
		jInformer::inform(error_msg);
	}
}

virtual void on_select_row(KammoGUI::Widget *wid, KammoGUI::List::iterator iter) {
	KammoGUI::List *types = (KammoGUI::List *)wid;

	if(wid->get_id() == "MachineType") {
		Machine *new_machine = NULL;
		
		try {
			new_machine = DynamicMachine::instance(types->get_value(iter, 0));
			MachineSequencer::create_sequencer_for_machine(new_machine);
		} catch (jException e) {
			if(new_machine)
				Machine::disconnect_and_destroy(new_machine);
			SATAN_DEBUG("   Failed to create machine: %s\n", e.message.c_str()); fflush(0);
		} catch (...) {
			// ignore
			SATAN_DEBUG_(" Unknown exception caught and ignored... state is un-safe and unpredictable.\n");
			fflush(0);
		}

		SATAN_DEBUG("New machin?: %p\n", new_machine);
		if(new_machine) {
			MachineSequencer *mseq;
			mseq = MachineSequencer::get_sequencer_for_machine(new_machine);
			SATAN_DEBUG("New machine sequencer?: %p\n", mseq);
			if(mseq) {
				SATAN_DEBUG("Setting loop...\n");
				mseq->set_loop_id_at(0, 0); // default to having a loop at position 0
			}

			// if the callback_event is set we will try to autoconnect the node since the user
			// will not be presented with the machine connector interface
			if(callback_event) {
				auto_connect(new_machine);
			}
		}

		if(callback_event) {
			// call the callback
			trigger_user_event(callback_event);
		} else {
			static KammoGUI::UserEvent *ue = NULL;
			KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showConnector_container");
			if(ue != NULL)
				trigger_user_event(ue);
		}
	}
}

KammoEventHandler_Instance(MachineTypeHandler);

