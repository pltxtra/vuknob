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

#include "satan_project_entry.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

// we need to set this number to the value 2 at least. The value was not defined before interface level 2 (it was then implicit..)
int SatanProjectEntry::project_interface_level = 7;
std::map<std::string, SatanProjectEntry *> *SatanProjectEntry::entries = NULL;
std::map<int, std::vector<std::string> > *SatanProjectEntry::load_order = NULL;

SatanProjectEntry::SatanProjectEntry() {} 

SatanProjectEntry::SatanProjectEntry(const std::string &type_name, unsigned int _l_order,
				     int minimal_project_interface_level) {
	if(entries == NULL) {
		entries = new std::map<std::string, SatanProjectEntry *>();
	}
	if(load_order == NULL) {
		load_order = new std::map<int, std::vector<std::string> >();
	}
	
	if(entries->find(type_name) != entries->end())
		throw SatanProjectEntryAlreadyRegistered();

	(*entries)[type_name] = this;

	(*load_order)[_l_order].push_back(type_name);

	if(project_interface_level < minimal_project_interface_level) {
		project_interface_level = minimal_project_interface_level;
	}
}

void SatanProjectEntry::get_satan_project_xml(std::ostream &os) {
	std::map<std::string, SatanProjectEntry *>::iterator k;

	os << "<satanprojectv2 projectinterfacelevel=\"" << project_interface_level << "\" >\n"; 
	for(k = entries->begin(); k != entries->end(); k++) {
		os << "<" << (*k).first << " ";
		os << (*k).second->get_xml_attributes();
		os << ">\n";
		
		(*k).second->generate_xml(os);

		os << "</" << (*k).first << ">\n";		
	}
	os << "</satanprojectv2>\n";
}

void SatanProjectEntry::parse_satan_project_xml(KXMLDoc &xml) {
	if(xml.get_name() != "satanproject" &&
	   xml.get_name() != "satanprojectv2") {
		throw jException("Not an Satan Project File.\n",
				 jException::sanity_error);
	}

	// assume old interface level unless otherwise specified in the file.
	int parsing_interface_level = 1;
	KXML_GET_NUMBER(xml,"projectinterfacelevel",parsing_interface_level,1);

	if(parsing_interface_level > project_interface_level) {
		throw jException("You need a newer version of this application to load the project you tried to load.",
				 jException::sanity_error);
	}
	
	std::map<std::string, SatanProjectEntry *>::iterator k;
	std::map<int, std::vector<std::string> >::iterator l;
	std::vector<std::string>::iterator U;

	for(l = load_order->begin(); l != load_order->end(); l++) {
		SATAN_DEBUG("Order %d - %d entry types.\n",
			    (*l).first, (*l).second.size());
		for(U = (*l).second.begin(); U != (*l).second.end(); U++) {
			k = entries->find(*U);
			
			int e_max = 0;
			try {
				e_max = xml[(*k).first].get_count();
			} catch(jException e) { e_max = 0;}
			SATAN_DEBUG("Found %d entries of type %s\n",
				    e_max, (*k).first.c_str());
			if(e_max == 1) {
				KXMLDoc node;				
				node = xml[(*k).first];
				SATAN_DEBUG("Parsing node: %s\n", (*k).first.c_str());
				(*k).second->parse_xml(parsing_interface_level, node);
				SATAN_DEBUG_("    finished parsing entry...\n");
			} else if(e_max != 0) {
				throw jException(
					std::string("Unexpected number of nodes in satan project. [")
					+ (*k).first + "]", jException::sanity_error);
			}
		}
	}
	SATAN_DEBUG_("Done parsing project KAZUMBA...\n");
}

void SatanProjectEntry::clear_satan_project() {
	std::map<std::string, SatanProjectEntry *>::iterator k;
	std::map<int, std::vector<std::string> >::iterator l;
	std::vector<std::string>::iterator U;

	for(l = load_order->begin(); l != load_order->end(); l++) {
		SATAN_DEBUG("Order %d - %d entry types.\n",
			    (*l).first, (*l).second.size());
		for(U = (*l).second.begin(); U != (*l).second.end(); U++) {
			k = entries->find(*U);

			SATAN_DEBUG("   Set defaults for ]%s[.\n",
				    (*k).first.c_str());
			(*k).second->set_defaults();
			SATAN_DEBUG("   ---> done setting defaults for ]%s[.\n", (*k).first.c_str());
		}
	}
	SATAN_DEBUG("clear_satan_project() finished.\n");
}
