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

#include "machine.hh"

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

#include "dynamic_machine.hh"
#include "machine_sequencer.hh"

#include "general_tools.hh"

#include "advanced_file_request_ui.hh"

#ifdef ANDROID
#include "android_java_interface.hh"
#endif

#include "satan_project_entry.hh"
#include "common.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

KammoEventHandler_Declare(LoadUIHandler, "LOAD_PROJECT_BT:loadExampleSelected:loadExample2");

static void refresh_SATAN(void *ignored) {
	// make sure that everyone gets that the project has been loaded
	static KammoGUI::UserEvent *ue = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&ue, "refreshProject");
	if(ue != NULL)
		trigger_user_event(ue);
}

template<class charT, class Traits>
static void load_lcf_file(std::basic_istream<charT, Traits> &stream) {
	KXMLDoc satanproject;

	// stop playback during loading...
	Machine::stop();

	std::cout << "Time to stream that devil!\n"; fflush(0);
	stream >> satanproject;
	
	std::cout << "Type: " << satanproject.get_name() << "\n"; fflush(0);
	
	// indicate that we are loading, so that machines can reduce their chatter...
	Machine::set_load_state(true);

	// load the xml data
	SatanProjectEntry::parse_satan_project_xml(satanproject);

	// indicate that we are no longer loading, so that machines can continue their chatter
	Machine::set_load_state(false);

	SATAN_DEBUG_("***************");
	SATAN_DEBUG_("* PARSER DONE *");
	SATAN_DEBUG_("***************"); fflush(0);

	KammoGUI::run_on_GUI_thread(refresh_SATAN, NULL);
}

struct __busy_load_data {
	std::string archive_path;
	std::string archive_name;
};

static void busy_load_project_file(std::string lcf_fname, std::string owd, std::string dirpath) {
	// open and load the lcf file
	fstream lcf_handle;
	lcf_handle.open(lcf_fname.c_str(), fstream::in);

	if(!lcf_handle.fail()) {		
		try {
			SATAN_DEBUG_("\n\n*************** LOADING PROJECT FILE ****************\n\n"); 
			load_lcf_file(lcf_handle);
			SATAN_DEBUG_("\n\n*************** PROJECT LOADED ****************\n\n"); 
		} catch(jException e) {
			jInformer::inform(e.message);
		} catch(...) {
			jInformer::inform("Exception thrown while loading, but no handler available. (This is a bug.)");
		}
	}
	lcf_handle.close();

	// if we changed to another wd, change back to old wd
	if(owd != "") {
		if(chdir(owd.c_str()))
			SATAN_DEBUG_("\n\n\n!!! FAILED TO CHANGE DIRECTORY in loadsave.cc (4) !!!\n\n\n");
	
		// Remove storage directory
		if(!remove_directory(dirpath)) {
			throw jException(
				std::string("Failed to cleanup temporary storage directory: ") + dirpath,
				jException::syscall_error);
		}
	}
}

static void busy_unpack_and_load(void *data) {
	struct __busy_load_data *bld = (struct __busy_load_data *)data;	
	std::string archive_path = bld->archive_path;
	std::string archive_name = bld->archive_name;
	delete bld; bld = NULL;

	std::string dirname = archive_name + "_dir";
	std::string dirpath = archive_path + "_dir";

	// remember working directory
	char old_wd[2048];
	char *owd;
	
	owd = getcwd(old_wd, sizeof(old_wd));
	if(owd == NULL) {
		throw jException("Path name too long, I'm sorry dave.",
				 jException::sanity_error);
	}

	// define lcf-file name
	std::string lcf_fname = archive_path;
	
	// try to uncompress storage directory
	std::ostringstream cmd_line;

#ifdef ANDROID
	try {
		// xxx - this is a hack/workaround ... even though I remove the archive_name + "_dir" directory
		// it seems the android java system "remembers" it and fails to create it a second time.. for
		// example when loading a project.. so if a project is loaded twice it fails the second time...
		// so, using the counter I make sure that we don't create the same path multiple times...
		// and a user is highly unlikely to exhaust the available numbers during a run of Satan...
		std::stringstream stream;
		static int counter = 0;
		stream << "/sdcard/satan_unpack_" << counter++ << "/"; 
		
		std::string unpack_path = stream.str();
		
		std::vector<std::string> args;
		args.push_back(std::string("-C"));
		args.push_back(unpack_path);
		args.push_back(std::string("-zxf"));
		args.push_back(archive_path);
		AndroidJavaInterface::call_tar(args);
		dirname = std::string(unpack_path) + archive_name + "_dir";
		dirpath = std::string(unpack_path);
#else
	cmd_line
		<< "tar -xzf " << archive_path << ";";
	if(system(cmd_line.str().c_str()) == 0) {
#endif
		// OK, it seems to be an archive alright.
		std::cout << "OK, lcf archive opened! (" + dirname + ")\n";
		// change dir
		if(chdir(dirname.c_str()) != 0) {
			throw jException(
				"Failed to enter storage directory, file in "
				"wrong format? No cleanup done, sorry.",
				jException::sanity_error);
		}
	
		lcf_fname = "lcf.xml";
#ifdef ANDROID
	} catch(...) {
#else
	} else {
#endif
		std::cout
			<< "OK, not an lcf archive, trying to "
			<< "load it as a pure lcf xml file.\n";
		owd = NULL;
	}
		
	busy_load_project_file(lcf_fname, owd, dirpath);	
}

static void load_project_file(const std::string &archive_path, const std::string &archive_name) {

	struct __busy_load_data *bld = new __busy_load_data();
	bld->archive_path = archive_path;
	bld->archive_name = archive_name;

	KammoGUI::do_busy_work("Loading project...", "Please wait...", busy_unpack_and_load, bld);
}

static void select(void *data, const std::string &fname) {
	std::string archive_path = fname;
	std::string archive_name = fname.substr(
		fname.find_last_of('/') + 1, fname.size());

	static KammoGUI::UserEvent *ue = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showProjectContainer");
	if(ue != NULL)
		trigger_user_event(ue);

	load_project_file(archive_path, archive_name);
}

static void cancel(void *data) {
	static KammoGUI::UserEvent *ue = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showProjectContainer");
	if(ue != NULL)
		trigger_user_event(ue);
}

virtual void on_click(KammoGUI::Widget *wid) {
	if(wid->get_id() == "LOAD_PROJECT_BT") {
		advancedFileRequestUI_getFile(
			DEFAULT_PROJECT_SAVE_PATH,
			select, cancel, NULL);
	}
	if(wid->get_id() == "loadExample2") {
		static KammoGUI::UserEvent *ue = NULL;
		KammoGUI::get_widget((KammoGUI::Widget **)&ue, "loadExampleSelected");
		if(ue != NULL)
			trigger_user_event(ue);
	}
}

virtual void on_user_event(KammoGUI::UserEvent *ev, std::map<std::string, void *> data) {
	if(ev->get_id() == "loadExampleSelected") {
#ifdef ANDROID
		try {
			std::string path, file;
			
			file = "EXAMPLE_ONE";
			path = KAMOFLAGE_ANDROID_ROOT_DIRECTORY;
			path += "/app_nativedata/Examples/" + file;
			
			load_project_file(path, file);			
		} catch(jException e) {
			std::cout << "exception caught: " << e.message << "\n";
			throw;
		}		
#endif
	}
}

KammoEventHandler_Instance(LoadUIHandler);
