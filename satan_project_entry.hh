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

/*
 * This class is used to register a top level XML node type entry for satan projects.
 *
 * A top level entry will on request generate data to be stored in a project XML file.
 *
 * A top level entry will also provide a method to parse the same data back from an XML file.
 *
 * ------------------------------------------------------
 *
 * So - if you have a class created that you want should be able to save it's data into
 * a Satan Project, please create a static object of a class that inherits this one. That object
 * will be called each time a project is to be saved or loaded.
 */

#ifndef SATAN_PROJECT_ENTRY
#define SATAN_PROJECT_ENTRY

#include <kamo_xml.hh>
#include <string>
#include <map>

class SatanProjectEntry {
public:
	class SatanProjectEntryAlreadyRegistered {} ;
	
	// generate the XML node attributes for this entry
	virtual std::string get_xml_attributes() = 0;
	// generate the XML node for this entry
	virtual void generate_xml(std::ostream &output) = 0;
	// parse a node of a specific project entry
	virtual void parse_xml(int project_interface_level, KXMLDoc &xml_node) = 0;
	// set default values (on a clear project)
	virtual void set_defaults() = 0;
	
private:
	// make sure the child class always initializes using the intended constructor
	SatanProjectEntry();

	static int project_interface_level; // used to determine minimal interface level of projects saved
	static std::map<std::string, SatanProjectEntry *> *entries;
	static std::map<int, std::vector<std::string> > *load_order;
public:
	// lower load_order loads first
	SatanProjectEntry(const std::string &type_name, unsigned int load_order, int project_interface_level);

	// static interface to get all xml from all entries
	static void get_satan_project_xml(std::ostream &output);
	// static interface to parse all xml from all entries in an xml file
	static void parse_satan_project_xml(KXMLDoc &xml);
	// static interface to clear the current project
	static void clear_satan_project();
};

#endif
