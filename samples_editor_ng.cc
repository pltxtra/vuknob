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
#include <fstream>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>

#include "machine.hh"
#include "signal.hh"
#include "advanced_file_request_ui.hh"
#include "common.hh"
#include "remote_interface.hh"

#include <jngldrum/jinformer.hh>

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#ifdef ANDROID
#define SAMPLES_DIRECTORY_FILE (std::string(KAMOFLAGE_ANDROID_ROOT_DIRECTORY) + "/samples_directory_file")
#else
#define SAMPLES_DIRECTORY_FILE (std::string("~/.satan_samples_directory_file"))
#endif

class SamplesDirectory {
private:
	void parse_file_entry(const KXMLDoc &xml) {
		int id;
		KXML_GET_NUMBER(xml, "id", id, 0);

		std::string name;
		name = xml.get_attr("name");

		filenames[id] = name;
	}

	void parse_xml(const KXMLDoc &xml) {
		name = xml.get_attr("name");
		path = xml.get_attr("path");

		// Parse entries
		int k, k_max = 0;
		try {
			k_max = xml["file"].get_count();
		} catch(jException e) { k_max = 0;}

		for(k = 0; k < k_max; k++) {
			parse_file_entry(xml["file"][k]);
		}
	}

	void write_xml(std::ostream &output) {
		output << "<directory name=\"" << name << "\" path=\"" << path << "\" >\n";

		std::map<int, std::string>::iterator f;

		for(f = filenames.begin(); f != filenames.end(); f++) {
			output << "    <file id=\"" << (*f).first << "\" name=\"" << (*f).second << "\" />";
		}
		output << "</directory>\n";
	}

	static void parse_directories(const KXMLDoc &xml, std::map<std::string, SamplesDirectory *> *dirs) {
		unsigned int x, max;

		try {
			max = xml["directory"].get_count();
		} catch(jException e) { max = 0;}

		for(x = 0; x < max ; x++) {
			SamplesDirectory *new_dir = new SamplesDirectory();
			new_dir->parse_xml(xml["directory"][x]);

			(*dirs)[new_dir->name] = new_dir;
		}
	}

	bool rescan(const std::string &_path, const std::string &_name) {
		path = _path;
		name = _name;

		SATAN_DEBUG("Validating DIRECTORY: %s\n", path.c_str());
		DIR *d = opendir(path.c_str());
		struct dirent *ent = NULL;

		if(d == NULL) {
			throw jException("Bad directory.", jException::sanity_error);
		}

		std::vector<std::string> files;

		while((ent = readdir(d)) != NULL) {
			if(ent->d_type == DT_REG) {
				files.push_back(ent->d_name);
			}
		}

		std::sort(files.begin(), files.end());

		std::vector<std::string>::iterator k;

		bool one_file_found = false;

		int i = 0;
		// we scan each file and identify those we understand...
		for(k  = files.begin();
		    k != files.end();
		    k++, i++) {
			std::string pth = path + "/" + (*k);

			SATAN_DEBUG_("Validating sample at path: %s\n", pth.c_str());

			if(Machine::StaticSignalLoader::is_valid(pth)) {
				SATAN_DEBUG_("Sample found VALID.\n");
				filenames[i] = (*k);
				one_file_found = true;
			} else {
				SATAN_DEBUG_("Sample found IN-VALID.\n");
			}
		}
		// only return a new SamplesDirectory if it contain valid samples
		if(one_file_found) {
			return true;
		}

		return false;
	}

	static void scan_for_dirs_recursive(std::string path) {
		DIR *d = opendir(path.c_str());
		struct dirent *ent = NULL;

		if(d == NULL) {
			throw jException("Bad directory.", jException::sanity_error);
		}

		std::vector<std::string> dirs;

		while((ent = readdir(d)) != NULL) {
			if(ent->d_type == DT_DIR) {
				std::string ndir = ent->d_name;
				if(ndir != ".." && ndir != ".") {
					dirs.push_back(ent->d_name);
				}
			}
		}

		std::sort(dirs.begin(), dirs.end());

		std::vector<std::string>::iterator k;

		// first we add the directories
		for(k  = dirs.begin();
		    k != dirs.end();
		    k++) {
			if((*k) != ".") {
				std::string dirpath = path + "/" + (*k);

				SamplesDirectory *sdir = SamplesDirectory::scan((*k), dirpath);


				if(sdir != NULL) {
					add_directory(dirpath, sdir);
				}

				scan_for_dirs_recursive(dirpath);
			}
		}
	}

	static void save_samples_directory_file() {
		fstream file;
		file.open(SAMPLES_DIRECTORY_FILE.c_str(),
			  fstream::out);
		if(!file.fail()) {
			file << "<samplesdirectoryfile>\n";

			std::map<std::string, SamplesDirectory *>::iterator k;

			for(k  = directories.begin();
			    k != directories.end();
			    k++) {
				(*k).second->write_xml(file);
			}

			file << "</samplesdirectoryfile>\n";
		}
		file.close();
	}

	static std::map<std::string, SamplesDirectory *> directories;
public:
	std::string name;
	std::string path; // path of the directory
	std::map<int, std::string> filenames;

	int working_on_signal_index; // this indicates which signal we intend to replace by loading a sample

	void refresh() {
		(void) rescan(path, name); // ignore return value in this case
	}

	static void add_directory(const std::string &dirpath, SamplesDirectory *sdir) {
		directories[dirpath] = sdir;
		SamplesDirectory::save_samples_directory_file();
	}

	static void remove_directory(SamplesDirectory *sdir) {
		std::map<std::string, SamplesDirectory *>::iterator k;

		for(k  = directories.begin();
		    k != directories.end();
		    k++) {
			if(sdir == (*k).second) {
				directories.erase(k);
				break; // break the loop
			}
		}

		save_samples_directory_file();
	}

	static bool load_samples_directory_file() {
		fstream file;
		file.open(SAMPLES_DIRECTORY_FILE.c_str(), fstream::in);

		if(!file.fail()) {
			KXMLDoc docu;

			try {
				file >> docu;

				if(docu.get_name() == "samplesdirectoryfile") {
					parse_directories(docu, &directories);
				}
			} catch(jException e) {
				jInformer::inform(e.message);
			} catch(...) {
				jInformer::inform("Failed to load samples directory file, for unknown reasons...");
			}

			return true;
		}

		return false;
	}

	static SamplesDirectory *scan(const std::string &name, const std::string &path) {
		SamplesDirectory *retdir = new SamplesDirectory();
		if(!retdir) return NULL;

		try {
			if(!retdir->rescan(path, name)) {
				// if no valid samples where found, delete the object and reset the pointer to NULL
				delete retdir;
				retdir = NULL;
			}
		} catch(...) {
			// make sure we delete the pointer in case of an exception, then just rethrow
			delete retdir;
			throw;
		}

		return retdir;
	}

	static void create_default_directories_list() {
		/* only create this list on Android for now
		 * a PC user, for example, will not
		 * expect a default list as much as a phone/tablet user...
		 */
#ifdef ANDROID
		try {
			scan_for_dirs_recursive(std::string(KAMOFLAGE_ANDROID_ROOT_DIRECTORY) + "/app_nativedata/Samples");
			SamplesDirectory::save_samples_directory_file();
		} catch(...) {
			// silently ignore...
		}
#endif
	}

	static std::map<std::string, SamplesDirectory *>::iterator begin() {
		return directories.begin();
	}

	static std::map<std::string, SamplesDirectory *>::iterator end() {
		return directories.end();
	}
};
std::map<std::string, SamplesDirectory *> SamplesDirectory::directories;

class MyButton : public KammoGUI::Button {
public:
	int index;

	MyButton(int idx) : index(idx) {}
};

std::map<int, KammoGUI::Label *> srows;

void refresh_row(int row, std::function<std::string(int row)> callback) {
	std::string name = "<empty>";

	try {
		name = callback(row);
	} catch (...) {
		// ignore error (this should mean that no signal was in slot row)
	}

	static char bfr[128];
	snprintf(bfr, 128, "%02d : %s", row, name.c_str());

	if(srows.find(row) != srows.end()) {
		srows[row]->set_title(bfr);
		return;
	}
}

KammoEventHandler_Declare(SamplesListHandler, "SamplesListContainer:showSamplesConf:refreshProject");

void add_sample_row(KammoGUI::Container *cnt, int row, std::map<int, std::string> sample_names) {
	if(srows.find(row) == srows.end()) {
		KammoGUI::Label *lbl = new KammoGUI::Label();
		lbl->set_title("dummy");
		lbl->set_expand(true);
		lbl->set_fill(true);

		MyButton *btn = new MyButton(row);
		btn->attach_event_handler(this);
		btn->set_title("load");

		KammoGUI::Container *sub_cnt = new KammoGUI::Container(true);
		sub_cnt->add(*lbl);
		sub_cnt->add(*btn);

		cnt->add(*sub_cnt);

		srows[row] = lbl;
	}
	refresh_row(row,
		    [sample_names](int row) -> std::string {
			    auto found = sample_names.find(row);
			    if(found == sample_names.end())
				    return "<empty>";
			    return found->second;
		    }
		);
}

virtual void on_click(KammoGUI::Widget *wid) {
	if(wid->get_id() == "showSamplesConf") {
//		rebuild_samples_list();
		return;
	}

	MyButton *mbu = (MyButton *)wid;
	int *id = new int();

	if(id == NULL || mbu == NULL)
		return;

	*id = mbu->index;

	static KammoGUI::UserEvent *ue = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showSamplesDirectories");
	if(ue != NULL) {
		std::map<std::string, void *> args;
		args["id"] = id;
		trigger_user_event(ue, args);
	}
}

void rebuild_samples_list() {
	static KammoGUI::Container *cnt = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&cnt, "SamplesListContainer");

	int k;

	auto samples_map = Machine::StaticSignalLoader::get_all_signal_names();

	for(k = 0; k < MAX_STATIC_SIGNALS; k++) {
		add_sample_row(cnt, k, samples_map);
	}
}

virtual void on_init(KammoGUI::Widget *wid) {
	if(wid->get_id() == "SamplesListContainer")
		rebuild_samples_list();
}

virtual void on_user_event(KammoGUI::UserEvent *ue, std::map<std::string, void *> args) {
	// this event is triggered for example when a project is loaded.
	if(ue->get_id() == "refreshProject") {
		SATAN_DEBUG("on refreshProject - calling rebuild_samples_list()\n");
		rebuild_samples_list();
		SATAN_DEBUG("on refreshProject - returned from rebuild_samples_list()\n");
	}
}

KammoEventHandler_Instance(SamplesListHandler);

KammoEventHandler_Declare(SamplesDirectoriesHandler, "SamplesDirectoriesContainer:showSamplesDirectories:samDirs_Add");

class MyButton : public KammoGUI::Button {
public:
	SamplesDirectory *dir;
	int current_static_signal_index;

	MyButton(SamplesDirectory *_dir) : dir(_dir) {}
	MyButton(int _current_) : dir(NULL), current_static_signal_index(_current_) {}
};

static int current_static_signal_index;
static std::string current_followup_action;

static void dirSelector_select(void *id_void, const std::string &dirpath) {
	int *id = (int *)id_void;
	SamplesDirectory *sdir = SamplesDirectory::scan(
		dirpath.substr(dirpath.find_last_of('/') + 1, dirpath.size()),
		dirpath);

	if(sdir != NULL) {
		SamplesDirectory::add_directory(dirpath, sdir);
	}


	static KammoGUI::UserEvent *ue = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showSamplesDirectories");
	if(ue != NULL) {
		std::map<std::string, void *> args;
		args["id"] = id;
		trigger_user_event(ue, args);
	}

	SATAN_DEBUG("SELECTED PATH: %s\n", dirpath.c_str());
}

static void dirSelector_cancel(void *id_void) {
	int *id = (int *)id_void;
	delete id; // we don't care at this time, so we just delete it

	static KammoGUI::UserEvent *ue = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showSamplesDirectories");
	if(ue != NULL)
		trigger_user_event(ue);
}

virtual void on_click(KammoGUI::Widget *wid) {
	// do stuff here
	SATAN_DEBUG("on_click() -> %s\n", wid->get_id().c_str());

	if(wid->get_id() == "samDirs_Add") {
		// add new directory
		int *id = new int();

		if(id == NULL)
			return;

		*id = current_static_signal_index;

		advancedFileRequestUI_getFile(
			"/",
			dirSelector_select,
			dirSelector_cancel,
			id);
	} else {
		MyButton *mbu = (MyButton *)wid;

		// show directory contents
		static KammoGUI::UserEvent *ue = NULL;
		KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showSamplesDirectory");
		if(ue != NULL) {
			std::map<std::string, void *> args;
			args["sdir"] = mbu->dir;
			args["followup"] = new std::string(current_followup_action);
			trigger_user_event(ue, args);
		}
	}
}

void add_directory_row(KammoGUI::Container *cnt, SamplesDirectory *sdir) {
	KammoGUI::Label *lbl = new KammoGUI::Label();

	lbl->set_title(sdir->name);
	lbl->set_fill(true); lbl->set_expand(true);

	MyButton *btn = new MyButton(sdir);
	btn->attach_event_handler(this);
	btn->set_title(">");

	KammoGUI::Container *sub_cnt = new KammoGUI::Container(true);
	sub_cnt->add(*lbl);
	sub_cnt->add(*btn);

	cnt->add(*sub_cnt);
}

void rebuild_directories_list(int current_static_signal_index) {
	static KammoGUI::Container *cnt = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&cnt, "SamplesDirectoriesContainer");
	cnt->clear();

	std::map<std::string, SamplesDirectory *>::iterator k;

	for(k  = SamplesDirectory::begin();
	    k != SamplesDirectory::end();
	    k++) {
		(*k).second->working_on_signal_index = current_static_signal_index;
		add_directory_row(cnt, (*k).second);
	}
}

virtual void on_user_event(KammoGUI::UserEvent *ue, std::map<std::string, void *> args) {
	static int index = -1;

	int *id = (int *)NULL;

	if(args.find("id") != args.end())
		id = (int *)args["id"];

	if(id != NULL) {
		index = *id; delete id;
		current_static_signal_index = index;
	}

	current_followup_action = "showSamplesList";
	if(args.find("followup") != args.end()) {
		std::string *temp = (std::string *)args["followup"];
		current_followup_action = *temp;
		delete temp;
	}

	if(index != -1) {
		rebuild_directories_list(index);
	}
}

virtual void on_init(KammoGUI::Widget *wid) {
	if(wid->get_id() == "SamplesDirectoriesContainer") {
		if(!SamplesDirectory::load_samples_directory_file()) {
			SamplesDirectory::create_default_directories_list();
		}
	}
}

KammoEventHandler_Instance(SamplesDirectoriesHandler);
int SamplesDirectoriesHandler::SamplesDirectoriesHandler::current_static_signal_index = -1;
std::string SamplesDirectoriesHandler::SamplesDirectoriesHandler::current_followup_action = "showSamplesList";

KammoEventHandler_Declare(SamplesDirectoryHandler, "SamplesDirectoryContainer:showSamplesDirectory:samEd_Return:samEd_Cancel:samEd_Remove");

class MyButton : public KammoGUI::Button {
public:
	SamplesDirectory *sdir;
	std::string path, followup_action;
	bool preview_action;

	MyButton(SamplesDirectory *_sdir, const std::string &_path,
		 bool _preview_action, const std::string &_followup_action) :
		sdir(_sdir), path(_path), followup_action(_followup_action), preview_action(_preview_action) {}
};

static SamplesDirectory *current_sdir;

virtual void on_click(KammoGUI::Widget *wid) {
	if(wid->get_id() == "samEd_Cancel") {
		static KammoGUI::UserEvent *ue = NULL;
		KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showSamplesList");
		if(ue != NULL)
			trigger_user_event(ue);

		return;
	} else if(wid->get_id() == "samEd_Return") {
		static KammoGUI::UserEvent *ue = NULL;
		KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showSamplesDirectories");
		if(ue != NULL)
			trigger_user_event(ue);

		return;
	} else if(wid->get_id() == "samEd_Remove") {
		if(current_sdir)
			SamplesDirectory::remove_directory(current_sdir);

		static KammoGUI::UserEvent *ue = NULL;
		KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showSamplesDirectories");
		if(ue != NULL)
			trigger_user_event(ue);

		return;
	}

	MyButton *mbu = dynamic_cast<MyButton*>(wid);

	if(mbu) {
		// do stuff here
		if(mbu->preview_action) {
			Machine::StaticSignalLoader::preview_signal(mbu->path);
		} else {
			auto sbank = RemoteInterface::SampleBank::get_bank(""); // get global
			sbank->load_sample(mbu->sdir->working_on_signal_index, mbu->path);
			//Machine::StaticSignalLoader::load_signal(mbu->sdir->working_on_signal_index, mbu->path);

			refresh_row(mbu->sdir->working_on_signal_index, Machine::StaticSignalLoader::get_signal_name_for_slot);

			static KammoGUI::UserEvent *ue = NULL;
			KammoGUI::get_widget((KammoGUI::Widget **)&ue, mbu->followup_action);
			if(ue != NULL)
				trigger_user_event(ue);
		}
	}
}

void add_sample_row(KammoGUI::Container *cnt, SamplesDirectory *sdir,
		    const std::string &path, const std::string &name, const std::string &followup_action) {
	KammoGUI::Label *lbl = new KammoGUI::Label();

	lbl->set_title(name);
	lbl->set_fill(true); lbl->set_expand(true);

	MyButton *btn_load = new MyButton(sdir, path, false, followup_action);
	btn_load->attach_event_handler(this);
	btn_load->set_title("load");

	MyButton *btn_prev = new MyButton(sdir, path, true, "<this string is ignored>");
	btn_prev->attach_event_handler(this);
	btn_prev->set_title("play");

	KammoGUI::Container *sub_cnt = new KammoGUI::Container(true);
	sub_cnt->add(*lbl);
	sub_cnt->add(*btn_load);
	sub_cnt->add(*btn_prev);

	cnt->add(*sub_cnt);
}

void rebuild_directory_list(SamplesDirectory *sdir, const std::string &followup_action) {
	current_sdir = sdir;

	static KammoGUI::Container *cnt = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&cnt, "SamplesDirectoryContainer");
	cnt->clear();

	std::map<int, std::string>::iterator k;

	sdir->refresh();

	for(k  = sdir->filenames.begin();
	    k != sdir->filenames.end();
	    k++) {
		std::string pth = sdir->path + "/" + (*k).second;
		SATAN_DEBUG_("add_sample_row ]%s[ ]%s[\n",
			     pth.c_str(), (*k).second.c_str());
		add_sample_row(cnt, sdir, pth, (*k).second, followup_action);
	}


}

virtual void on_user_event(KammoGUI::UserEvent *ue, std::map<std::string, void *> args) {
	SamplesDirectory *sdir = NULL;

	if(args.find("sdir") != args.end())
		sdir = (SamplesDirectory *)(args["sdir"]);

	if(sdir == NULL) return;

	std::string followup_action = "showSamplesList";
	if(args.find("followup") != args.end()) {
		std::string *temp = (std::string *)args["followup"];
		followup_action = *temp;
		delete temp;
	}

	rebuild_directory_list(sdir, followup_action);
}


KammoEventHandler_Instance(SamplesDirectoryHandler);
SamplesDirectory *SamplesDirectoryHandler::SamplesDirectoryHandler::current_sdir = NULL;
