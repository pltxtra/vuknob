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

#ifdef ANDROID
#include "android_java_interface.hh"
#endif

#include "satan_project_entry.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

KammoEventHandler_Declare(SaveUIHandler, "saveUI_saveButton");

struct save_project_busy_data {
	std::string pname;
	std::string ppath;
};

static void save_project_file_busy_func(void *data) {
	struct save_project_busy_data *spbdU = (save_project_busy_data *)data;
	std::string pname = spbdU->pname;
	std::string ppath = spbdU->ppath;
	delete spbdU; spbdU = NULL;
	
	std::string dirpath = ppath + "/" + pname + "_dir";
	std::string target_name = ppath + "/" + pname;
	
	SATAN_DEBUG_("     DIRNAME: %s\n", dirpath.c_str());
	SATAN_DEBUG_("     TARGET_NAME: %s\n", target_name.c_str());
	fflush(0);
	
	// remember working directory
	char old_wd[2048];
	char *owd;
	
	owd = getcwd(old_wd, sizeof(old_wd));
	if(owd == NULL) {
		throw jException("Path name too long, I'm sorry dave.",
				 jException::sanity_error);
	}

	// create storage dir
	SATAN_DEBUG("CREATING DIR TO SAVE: %s\n", dirpath.c_str());
	if(mkdir(dirpath.c_str(), 0755) == -1) {
		throw jException(
			std::string("Failed to create storage directory : ") + dirpath,
				 jException::syscall_error);
	}

	// change dir
	SATAN_DEBUG_("Changing directory to new dir.\n");
	if(chdir(dirpath.c_str())) SATAN_DEBUG_("\n\n\n!!! FAILED TO CHANGE DIRECTORY in loadsave.cc (1) !!!\n\n\n");

	// Do the actual project savin' here!
	std::ostringstream output;       	
	try {
		SATAN_DEBUG_("Before get XML\n");
		// save all data from all registered satan project entries
		SatanProjectEntry::get_satan_project_xml(output);
		SATAN_DEBUG_("After get XML\n");
		
	} catch(jException je) {
		// change back to old working directory
		if(chdir(owd)) SATAN_DEBUG_("!!! FAILED TO CHANGE DIRECTORY in loadsave.cc (2) !!!");
		SATAN_DEBUG("Exception caught during XML generation: %s\n", je.message.c_str());
		throw;
	}

	SATAN_DEBUG_("Writing XML\n");
	// OK, write to file
	fstream file;
	file.open("lcf.xml", fstream::out);
	if(!file.fail()) {
		file << output.str();
	}
	file.close();
	SATAN_DEBUG_("WROTE XML\n");
	
	// OK, change to directory containing the storage directory
	if(chdir(ppath.c_str())) SATAN_DEBUG_("\n\n\n!!! FAILED TO CHANGE DIRECTORY in loadsave.cc (3) !!!\n\n\n");
	
	// compress directory
	std::ostringstream cmd_line;
#ifdef ANDROID
	SATAN_DEBUG("tar -C ]%s[ -czf ]%s[ ]%s_dir[\n", ppath.c_str(), target_name.c_str(), dirpath.c_str());
	fflush(0);

	try {
		std::vector<std::string> args;
		args.push_back(std::string("-C"));
		args.push_back(ppath);
		args.push_back(std::string("-czf"));
		args.push_back(target_name);
		args.push_back(pname + "_dir");
		AndroidJavaInterface::call_tar(args);
	} catch(...) {
#else
	cmd_line
		<< "tar -czf " << target_name << " " << pname + "_dir ;";
	if(system(cmd_line.str().c_str()) != 0) {
#endif
		throw jException(
			"Failed to compress storage directory, save failed.",
			jException::syscall_error);
	}
	SATAN_DEBUG_("Remove temporary directory...\n");
	// Remove storage directory
	if(!remove_directory(dirpath)) {
		throw jException(
			"Failed to cleanup after compressing storage directory, save failed.",
			jException::syscall_error);
	}
}

void save_project_file() {		
	static KammoGUI::Entry *pname = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&pname, "saveUI_titleEntry");

	struct save_project_busy_data *spbd = new save_project_busy_data();

	spbd->pname = pname->get_text();
	spbd->ppath = DEFAULT_PROJECT_SAVE_PATH;

	KammoGUI::do_busy_work("Saving project...", "Please wait...", save_project_file_busy_func, spbd);	
}


virtual void on_click(KammoGUI::Widget *wid) {
	if(wid->get_id() == "saveUI_saveButton") {
		save_project_file();
	}
}

KammoEventHandler_Instance(SaveUIHandler);
