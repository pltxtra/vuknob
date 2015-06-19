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
 * the GNU General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "machine.hh"
#include "dynamic_machine.hh"
#include "machine_sequencer.hh"
#include "remote_interface.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/**************************************************
 *                                                *
 * class Machine::ProjectEntry SAVING/LOADING XML *
 *                                                *
 **************************************************/

Machine::ProjectEntry Machine::ProjectEntry::this_will_register_us_as_a_project_entry;

Machine::ProjectEntry::ProjectEntry() : SatanProjectEntry("machineentry", 0, MACHINE_PROJECT_INTERFACE_LEVEL) {}

std::string Machine::ProjectEntry::get_xml_attributes() {
	std::ostringstream output;
	output << "do_loop=\"" << (Machine::get_loop_state() ? "true" : "false") << "\" "
	       << "start=\"" << Machine::get_loop_start() << "\" "
	       << "length=\"" << Machine::get_loop_length() << "\" "
	       << "shuffle=\"" << Machine::get_shuffle_factor() << "\" "
	       << "bpm=\"" << Machine::get_bpm() << "\" "
	       << "lpb=\"" << Machine::get_lpb() << "\" ";
	return output.str();
}

void Machine::ProjectEntry::generate_xml(std::ostream &output) {
	std::vector<Machine *> machines = Machine::get_machine_set();
	std::vector<Machine *>::iterator m;

	// OK, save machine base data
	for(m  = machines.begin();
	    m != machines.end();
	    m++) {
		output << (*m)->get_base_xml_description();
		output << "\n";
	}

	// OK, save machine connection data
	for(m  = machines.begin();
	    m != machines.end();
	    m++) {
		output << (*m)->get_connection_xml();
		output << "\n";
	}
}

void Machine::ProjectEntry::parse_xml(int project_interface_level, KXMLDoc &xml_node) {
	// if project_interface_level < 3 we should treat the shuffle factor as a float [0.0-1.0], otherwise an integer [0-100]
	// no other considerations needed so far.

	// if project_interface_level < 5 we should parse the MachineSequencer::Pad in the old way with
	// absolute tick values. If it's >= 5 we should parse them as relative tick values

	// first we clear out the previous project by disconnecting and destroying all machines
	{
		std::vector<Machine *> all_machines = Machine::get_machine_set();
		std::vector<std::string> machine_names;

		std::vector<Machine *>::iterator k;
		for(k = all_machines.begin(); k != all_machines.end(); k++) {
			machine_names.push_back((*k)->get_name());
		}

		std::vector<std::string>::iterator i;
		for(i = machine_names.begin(); i != machine_names.end(); i++) {
			Machine *m = Machine::get_by_name(*i);
			if(m != NULL)
				(void)Machine::disconnect_and_destroy(m);
		}
		all_machines = Machine::get_machine_set();
	}

	// first then is it time to parse the new project settings
	int start, length, bpm, lpb;
	std::string do_loop_s; bool do_loop = false;

	try {
		do_loop_s = xml_node.get_attr("do_loop");

		if(do_loop_s == "true") do_loop = true;
	} catch(...) { }

	KXML_GET_NUMBER(xml_node, "start", start, 0);
	KXML_GET_NUMBER(xml_node, "length", length, 16);
	KXML_GET_NUMBER(xml_node, "bpm", bpm, 120);
	KXML_GET_NUMBER(xml_node, "lpb", lpb, 4);

	if(project_interface_level < 3) {
		float shuffle_factor;
		KXML_GET_NUMBER(xml_node, "shuffle", shuffle_factor, 0.0f);
		shuffle_factor *= 100.0f;
		Machine::set_shuffle_factor((int)shuffle_factor);
	} else {
		int shuffle_factor;
		KXML_GET_NUMBER(xml_node, "shuffle", shuffle_factor, 0);
		Machine::set_shuffle_factor(shuffle_factor);
	}
	Machine::set_loop_state(do_loop);
	Machine::set_loop_start(start);
	Machine::set_loop_length(length);

	if(auto gco = RemoteInterface::GlobalControlObject::get_global_control_object()) {
		gco->set_bpm(bpm);
		gco->set_lpb(lpb);
	}

	parse_machines(project_interface_level, xml_node);

	// connect machines
	parse_connections_entries(xml_node);

	// finalize machine sequencers
	MachineSequencer::finalize_xml_initialization();
}

void Machine::ProjectEntry::set_defaults() {
       // default state is to disconnect and destroy all machines
       {
	       // first stop playback
	       Machine::stop();

	       // then we proceed
#if 1
	       Machine::destroy_all_machines();
#else
               std::vector<Machine *> all_machines = Machine::get_machine_set();
               std::vector<std::string> machine_names;

               std::vector<Machine *>::iterator k;
               for(k = all_machines.begin(); k != all_machines.end(); k++) {
                       machine_names.push_back((*k)->get_name());
               }

               std::vector<std::string>::iterator i;
               for(i = machine_names.begin(); i != machine_names.end(); i++) {
                       Machine *m = Machine::get_by_name(*i);
                       if(m != NULL)
                               (void)Machine::disconnect_and_destroy(m);
               }
#endif
               SATAN_DEBUG("    AFTER CLEAR MACHINES: %d\n", Machine::get_machine_set().size());
       }
}

void Machine::ProjectEntry::parse_machine(int project_interface_level, const KXMLDoc &machine_x) {
	std::string class_name = machine_x.get_attr("class");
	std::string machine_name = machine_x.get_attr("name");

	if(class_name == "MachineSequencer") {
		MachineSequencer::presetup_from_xml(project_interface_level, machine_x);

		/* don't add to graph - already there */

	} else if(class_name == "DynamicMachine") {
		DynamicMachine::instance_from_xml(machine_x);
	}
}

void Machine::ProjectEntry::parse_machines(int project_interface_level, const KXMLDoc &satanproject) {
	unsigned int m, m_max = 0;

	// Parse machines
	try {
		m_max = satanproject["machine"].get_count();
	} catch(jException e) { m_max = 0;}

	std::cout << "Machines in project: " << m_max << "\n";

	for(m = 0; m < m_max; m++) {
		parse_machine(project_interface_level, satanproject["machine"][m]);
	}
}

void Machine::ProjectEntry::parse_connection_entry(
	const std::string machine_name,
	const KXMLDoc &con) {

	Machine *destination = Machine::get_by_name(machine_name);
	Machine *source =
		Machine::get_by_name(con.get_attr("sourcemachine"));

	std::string input_name = con.get_attr("input");
	std::string output_name = con.get_attr("output");

	destination->attach_input(source, output_name, input_name);
}

void Machine::ProjectEntry::parse_connections_entry(const KXMLDoc &cons) {
	std::string machine_name = cons.get_attr("name");
	unsigned int ce, ce_max = 0;

	// Parse graph coords
	try {
		ce_max = cons["connection"].get_count();
	} catch(jException e) { ce_max = 0;}

	std::cout << "Connection entries in connections: " << ce_max << "\n";

	for(ce = 0; ce < ce_max; ce++) {
		parse_connection_entry(
			machine_name,
			cons["connection"][ce]);
	}
}

void Machine::ProjectEntry::parse_connections_entries(const KXMLDoc &satanproject) {
	unsigned int c, c_max = 0;

	// Parse graph connections
	try {
		c_max = satanproject["connections"].get_count();
	} catch(jException e) { c_max = 0;}

	std::cout << "Connections in project: " << c_max << "\n";

	for(c = 0; c < c_max; c++) {
		parse_connections_entry(satanproject["connections"][c]);
	}
}
