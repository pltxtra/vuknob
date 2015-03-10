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


#include "graph_project_entry.hh"
#include "machine.hh"
#include "machine_sequencer.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/**************************************
 *                                    *
 * class GraphProjectEntry SAVING     *
 *                                    *
 **************************************/

GraphProjectEntry GraphProjectEntry::this_will_register_us_as_a_project_entry;

GraphProjectEntry::GraphProjectEntry() : SatanProjectEntry("graphentry", 1, GRAPH_PROJECT_INTERFACE_LEVEL) {}

std::string GraphProjectEntry::get_xml_attributes() {
	return "";
}

void GraphProjectEntry::generate_xml(std::ostream &output) {
	// OK, save machine graph position data
	output << "\n";

	std::vector<Machine *> machines =
		Machine::get_machine_set();
	std::vector<Machine *>::iterator m;

	for(m  = machines.begin();
	    m != machines.end();
	    m++) {
		std::pair<float, float> coords;
		
		coords.first = (*m)->get_x_position();
		coords.second = (*m)->get_y_position();
			
		output << "<graphcoords "
		       << "machine=\""
		       << (*m)->get_name()
		       << "\" "
		       << "x=\""
		       << coords.first
		       << "\" "
		       << "y=\""
		       << coords.second
		       << "\" />\n";
		
	}
	output << "\n";		
}

/**************************************
 *                                    *
 * class GraphProjectEntry LOADING    *
 *                                    *
 **************************************/

void GraphProjectEntry::parse_graphcoord(const KXMLDoc &gc) {
	std::string machine_name = gc.get_attr("machine");

	float x, y;

	KXML_GET_NUMBER(gc, "x", x, 0.0);
	KXML_GET_NUMBER(gc, "y", y, 0.0);

	Machine *m = Machine::get_by_name(machine_name);
	m->set_position(x, y);
}

void GraphProjectEntry::parse_graphcoords(const KXMLDoc &satanproject) {
	unsigned int gc, gc_max = 0;

	// Parse graph coords
	try {
		gc_max = satanproject["graphcoords"].get_count();
	} catch(jException e) { gc_max = 0;}

	for(gc = 0; gc < gc_max; gc++) {
		parse_graphcoord(satanproject["graphcoords"][gc]);
	}
}

void GraphProjectEntry::parse_xml(int project_interface_level, KXMLDoc &xml_node) {
	// we can safely ignore the project_interface_level since we are compatible with all versions currently
	// however - if we would like we could use that value to support different stuff in different versions...
	
	// move the nodes of the graph into place
	parse_graphcoords(xml_node);

}

/**************************************
 *                                    *
 * class GraphProjectEntry DEFAULTS   *
 *                                    *
 **************************************/

void GraphProjectEntry::set_defaults() {
}

