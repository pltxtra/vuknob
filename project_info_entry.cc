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


#include "project_info_entry.hh"
#include "machine.hh"
#include "machine_sequencer.hh"
#include "jngldrum/jthread.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/**************************************
 *                                    *
 * class ProjectInfoEntry SAVING     *
 *                                    *
 **************************************/

ProjectInfoEntry ProjectInfoEntry::this_will_register_us_as_a_project_entry;

ProjectInfoEntry::ProjectInfoEntry() : SatanProjectEntry("projectinfoentry", 0, PROJECT_INFO_PROJECT_INTERFACE_LEVEL) {}

std::string ProjectInfoEntry::get_xml_attributes() {
	return "";
}

class GetUIData_Event : public jThread::Event {
public:
	virtual ~GetUIData_Event() {
	}
	
	virtual void dummy_event_function() {
		// do nothing
	}
	
	std::string title;
	std::string artist;
	std::string genre;
};

std::string trim(const std::string& str,
                 const std::string& whitespace = " \t") {
	size_t strBegin = str.find_first_not_of(whitespace);
	if (strBegin == std::string::npos)
		return ""; // no content
	
	size_t strEnd = str.find_last_not_of(whitespace);
	size_t strRange = strEnd - strBegin + 1;
	
	return str.substr(strBegin, strRange);
}

void get_project_info(void *data) {
	jThread::EventQueue *q = (jThread::EventQueue *)data;
	GetUIData_Event *gue = new GetUIData_Event();

	static KammoGUI::Entry *title = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&title, "saveUI_titleEntry");
	static KammoGUI::Entry *artist = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&artist, "saveUI_artistEntry");
	static KammoGUI::Entry *genre = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&genre, "saveUI_genreEntry");

	gue->title = trim(title->get_text());
	gue->artist = trim(artist->get_text());
	gue->genre = trim(genre->get_text());

	Machine::set_record_file_name(std::string(DEFAULT_EXPORT_PATH) + "/" + gue->title);       

	q->push_event(gue);
}

void ProjectInfoEntry::generate_xml(std::ostream &output) {
	jThread::EventQueue q;
	
	KammoGUI::run_on_GUI_thread(get_project_info, &q);

	GetUIData_Event *gue = (GetUIData_Event *)q.wait_for_event();

	output << "<title value=\"" << gue->title << "\" />\n";
	output << "<artist value=\"" << gue->artist << "\" />\n";
	output << "<genre value=\"" << gue->genre << "\" />\n";

	SATAN_DEBUG_("GENERATING XML. %s\n", gue->title.c_str());
	
	delete gue;
}

/**************************************
 *                                    *
 * class ProjectInfoEntry LOADING    *
 *                                    *
 **************************************/

class SetUIData {
public:
	std::string title;
	std::string artist;
	std::string genre;	
};

void set_project_info(void *data) {
	SetUIData *s = (SetUIData *)data;

	SATAN_DEBUG_("Entering set_project_info()......\n");

	static KammoGUI::Entry *title = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&title, "saveUI_titleEntry");
	static KammoGUI::Entry *artist = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&artist, "saveUI_artistEntry");
	static KammoGUI::Entry *genre = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&genre, "saveUI_genreEntry");

	SATAN_DEBUG_("set_project_info(): %s\n", s->title.c_str());

	title->set_text(trim(s->title));
	artist->set_text(trim(s->artist));
	genre->set_text(trim(s->genre));

	if(s->title == "") {
		Machine::set_record_file_name("");       
	} else {
		Machine::set_record_file_name(std::string(DEFAULT_EXPORT_PATH) + "/" + trim(s->title));       
	}
	
	delete s;
}

void ProjectInfoEntry::parse_xml(int project_interface_level, KXMLDoc &xml_node) {
	// we can safely ignore the project_interface_level since we are compatible with all versions currently
	// however - if we would like we could use that value to support different stuff in different versions...
	
	SetUIData *s = new SetUIData();

	if(s == NULL)
		throw jException("Failed to allocate memory for intermediate storage in project_info_entry.cc",
				 jException::syscall_error);
	
	try {
		s->title = xml_node["title"].get_attr("value");
	} catch(...) { s->title = ""; }
	try {
		s->artist = xml_node["artist"].get_attr("value");
	} catch(...) { s->artist = ""; }
	try {
		s->genre = xml_node["genre"].get_attr("value");
	} catch(...) { s->genre = ""; }

	SATAN_DEBUG("---- will call set_project_info on GUI thread.\n");
	KammoGUI::run_on_GUI_thread_synchronized(set_project_info, s);
	SATAN_DEBUG("---- called set_project_info on GUI thread.\n");
}

void ProjectInfoEntry::set_defaults() {
	SetUIData *s = new SetUIData();

	if(s == NULL)
		throw jException("Failed to allocate memory for intermediate storage in project_info_entry.cc",
				 jException::syscall_error);
	
	s->title = "";
	s->artist = "";
	s->genre = "";
 	
	KammoGUI::run_on_GUI_thread_synchronized(set_project_info, s);
}

