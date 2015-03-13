/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include "machine_sequencer.hh"

#include <kamogui.hh>
#include <fstream>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "common.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#define DIR_TYPE_ID "-> "

static std::string current_directory;
static std::string current_file;
static void (*select_callback_function)(void *cbd, const std::string &fname) = NULL;
static void (*cancel_callback_function)(void *cbd) = NULL;
static void *callback_data = NULL;

static void set_current_file(const std::string &f) {
	static KammoGUI::Label *lbl = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&lbl, "filReq_target");
	lbl->set_title(f);

	current_file = f;
}

static void refresh_directory(std::string path) {
	// first we strip an eventual trailing /
	std::string::reverse_iterator rit = path.rbegin();
	if((*rit) == '/') {
		path = path.substr(0, path.size() - 1);
		
		// if the trailing slash was actually just the path pointing to the root directory, then correct the "fault" here...
		if(path.size() == 0) path = "/";
	}
	
	set_current_file(path);

	DIR *d = opendir(path.c_str());
	struct dirent *ent = NULL;

	std::vector<std::string> row_values;

	if(d == NULL) {
		throw jException("Bad directory.", jException::sanity_error);
	}

	current_directory = path;
	
	static KammoGUI::List *list = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&list, "filReq_selected");
	list->clear();

	std::vector<std::string> directories;
	std::vector<std::string> files;

#ifdef ANDROID
	// push a shortcut to / first
	directories.push_back("/");

	// also push a shortcut to the installation directory
	directories.push_back("/BACK2VUKNOB");
#endif	
	while((ent = readdir(d)) != NULL) {
		if(ent->d_type == DT_DIR) {
			directories.push_back(ent->d_name);
		}
		if(ent->d_type == DT_REG) {
			files.push_back(ent->d_name);
		}
	}

	sort(directories.begin(), directories.end());
	sort(files.begin(), files.end());
		
	std::vector<std::string>::iterator k;

	// first we add the directories
	for(k  = directories.begin();
	    k != directories.end();
	    k++) {
		if((*k) != ".") {
			row_values.push_back(DIR_TYPE_ID);
			row_values.push_back((*k));
			list->add_row(row_values);
			row_values.clear();
		}
	}

	// then we add the directories
	for(k  = files.begin();
	    k != files.end();
	    k++) {
		row_values.push_back("");
		row_values.push_back((*k));
		list->add_row(row_values);
		row_values.clear();
	}
}

KammoEventHandler_Declare(advancedFileRequestUi,"filReq_selected:filReq_select:filReq_cancel");

virtual void on_select_row(KammoGUI::Widget *widget, KammoGUI::List::iterator row) {
	KammoGUI::List *lst = (KammoGUI::List *)widget;

	std::string type = lst->get_value(row, 0);
	std::string file = lst->get_value(row, 1);

	std::string new_path;

	if(file == "/") {
		new_path = "/";
	} else if(file == "..") {
		new_path = current_directory.substr(
			0,
			current_directory.find_last_of('/',  current_directory.size()));
#ifdef ANDROID
	} else if(file == "/BACK2VUKNOB") {
		new_path = KAMOFLAGE_ANDROID_ROOT_DIRECTORY;
#endif
	} else if(current_directory == "/") {
		new_path = "/" + file;
	} else {
		new_path = current_directory + "/" + file;
	}
	
	SATAN_DEBUG("new_path: ]%s[\n", new_path.c_str());
	if(type == DIR_TYPE_ID) {
		try {
			refresh_directory(new_path);
		} catch(...) {
			// treat exceptions as a cancel
			if(cancel_callback_function)
				cancel_callback_function(callback_data);
		}
	} else {
		set_current_file(new_path);
	}
}

virtual void on_click(KammoGUI::Widget *wid) {
	if(wid->get_id() == "filReq_select") {
		if(select_callback_function) {
			SATAN_DEBUG_("calling select callback...\n");
			select_callback_function(callback_data, current_file);
		}
	}
	if(wid->get_id() == "filReq_cancel") {
		if(cancel_callback_function) {
			SATAN_DEBUG_("calling cancel callback...\n");
			cancel_callback_function(callback_data);
		}
	}
}

KammoEventHandler_Instance(advancedFileRequestUi);

void advancedFileRequestUI_getFile(
	const std::string &start_path,
	void (*cbf_select)(void *data, const std::string &fname),
	void (*cbf_cancel)(void *data),
	void *cbdata) {
	
	select_callback_function = cbf_select;
	cancel_callback_function = cbf_cancel;
	callback_data = cbdata;

	try {
		refresh_directory(start_path);
	} catch(...) {
		// treat exceptions as a cancel
		if(cbf_cancel)
			cbf_cancel(cbdata);
	}

	static KammoGUI::UserEvent *ue = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showAdvancedFileRequester");
	if(ue != NULL)
		KammoGUI::EventHandler::trigger_user_event(ue);

}

