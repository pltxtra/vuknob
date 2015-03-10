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

#ifndef __PROJECT_INFO_ENTRY
#define __PROJECT_INFO_ENTRY

#define PROJECT_INFO_PROJECT_INTERFACE_LEVEL 4

#include <kamogui.hh>
#include <kamo_xml.hh>
#include "satan_project_entry.hh"

// this class will make sure that we store information like title, artist and genre
// loaded from and saved to Satan project files
class ProjectInfoEntry : public SatanProjectEntry {
public:
	ProjectInfoEntry();
	
	virtual std::string get_xml_attributes();
	virtual void generate_xml(std::ostream &output);
	virtual void parse_xml(int project_interface_level, KXMLDoc &xml_node);
	virtual void set_defaults();

private:
	static ProjectInfoEntry this_will_register_us_as_a_project_entry;
       	
	
};

#endif
