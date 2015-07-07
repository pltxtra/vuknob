/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
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

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <jngldrum/jinformer.hh>
#include <kamogui.hh>

#include "dynamic_machine.hh"
#include "machine_sequencer.hh"
#include "common.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/*********************************************************
 *                                                       *
 * class DynamicMachine::ProjectEntry SAVING/LOADING XML *
 *                                                       *
 *********************************************************/

DynamicMachine::ProjectEntry DynamicMachine::ProjectEntry::this_will_register_us_as_a_project_entry;

DynamicMachine::ProjectEntry::ProjectEntry() : SatanProjectEntry("dynamicmachineentry", 1, DYNAMIC_MACHINE_PROJECT_INTERFACE_LEVEL) {}

std::string DynamicMachine::ProjectEntry::get_xml_attributes() {
	return "";
}

void DynamicMachine::ProjectEntry::generate_xml(std::ostream &output) {
}

void DynamicMachine::ProjectEntry::parse_xml(int project_interface_level, KXMLDoc &xml_node) {
	// we can safely ignore the project_interface_level since we are compatible with all versions currently
	// however - if we would like we could use that value to support different stuff in different versions...
}

void DynamicMachine::ProjectEntry::set_defaults() {
	// refresh handles
	SATAN_DEBUG("DynamicMachine::ProjectEntry::set_defaults() - refresh handles...\n");
	try {
		DynamicMachine::refresh_handle_set();
	} catch(jException e) {
		SATAN_DEBUG("exception: %s\n", e.message.c_str());
		exit(1);
	}

	// Create base machines
	SATAN_DEBUG("DynamicMachine::ProjectEntry::set_defaults() - create base machines...\n");
	try {
		(void) DynamicMachine::instance("liveoutsink");

		std::vector<Machine *> machines =
			Machine::get_machine_set();
		std::vector<Machine *>::iterator m;

		for(m  = machines.begin();
		    m != machines.end();
		    m++) {
			(*m)->set_position(0.0f, 0.0f);
		}
	} catch(jException e) {
		SATAN_DEBUG("exception: %s\n", e.message.c_str());
		exit(-1);
	}
	SATAN_DEBUG("DynamicMachine::ProjectEntry::set_defaults() - Finished.\n");
}

/***************************
 *                         *
 *      class Handle       *
 *                         *
 ***************************/

DynamicMachine::HandleSetException::HandleSetException(std::string &bname, KXMLDoc &hset) : handle_set(hset), base_name(bname) {
}

DynamicMachine::Handle::Handle(std::string _base_name, KXMLDoc decl, Handle *sparent) :
	name(_base_name), hint(""), dynlib_is_loaded(false), set_parent(sparent)
{
	parse_machine_declaration(decl);
}

DynamicMachine::Handle::Handle(std::string dynamic_name) : name(""), hint(""), dynlib_is_loaded(false), set_parent(NULL) {

	std::ifstream file(dynamic_name.c_str());

	KXMLDoc xml_proto;
	try {
		file >> xml_proto;
	} catch(jException e) {
		std::string error = "Failed to load declaration for ";
		error += dynamic_name + ". (" + e.message + ")";

		throw jException(error, jException::sanity_error);
	} catch(...) {
		std::string error = "Failed to load declaration for ";
		error += dynamic_name + ".";

		throw jException(error, jException::sanity_error);
	}
	std::cout << "Loaded declaration for " << dynamic_name << " successfully.\n";

	std::string proto_name = "";
	try {
		proto_name = xml_proto.get_name();
	} catch(...) {
		throw jException("XML format error in DynamicMachine::Handle::Handle().", jException::sanity_error);
	}

	if(proto_name == "machine") {
		parse_machine_declaration(xml_proto);
	} else if(proto_name == "machineset") {
		std::string base_name = "";
		try {
			base_name = xml_proto["name"].get_value();

			if(base_name == "")
				throw 1;
		} catch(...) {
			throw jException("No name declared for connected set.", jException::sanity_error);
		}

		throw HandleSetException(base_name, xml_proto);
	} else {
		std::string error = "XML data selected for DynamicMachine Handle creation was incorret.";
		error += " (";
		error += proto_name + ")";
		throw jException(error, jException::sanity_error);
	}
}

DynamicMachine::Handle::~Handle() {
	if(module)
#ifdef ANDROID
		dlclose(module);
#else
		lt_dlclose(module);
#endif
}

void DynamicMachine::Handle::prep_dynlib() {
	if(dynlib_is_loaded) return; // dynlib is already loaded.

	if(set_parent != NULL) {
		set_parent->prep_dynlib();

		dynlib_is_loaded = true;

		init = set_parent->init;
		exct = set_parent->exct;
		rset = set_parent->rset;
		delt = set_parent->delt;
		cptr = set_parent->cptr;

		return;
	}

	module = NULL;

	std::vector<std::string> candidate_dir;
	std::vector<std::string>::iterator candidate_dir_entry;

#ifdef ANDROID
	candidate_dir.push_back(handle_directory + "/../../lib/");
#else
	candidate_dir.push_back(handle_directory + "/");
	candidate_dir.push_back(handle_directory + "/.libs/");
#endif
	std::string error_message;

	for(candidate_dir_entry  = candidate_dir.begin();
	    candidate_dir_entry != candidate_dir.end() && module == NULL;
	    candidate_dir_entry++) {

		std::string extension = "lib";
		std::string dynamic_file = *candidate_dir_entry;
		std::string dynamic_file_fallback;

#ifdef __CYGWIN__
		dynamic_file = dynamic_file + "cyg";
#else
		dynamic_file = dynamic_file + "lib";
#endif

		dynamic_file = dynamic_file + dl_name;
		dynamic_file_fallback = dynamic_file + "_fallback";

#ifdef __CYGWIN__
		extension = "-0.dll";
#else
		extension = ".so";
#endif
		dynamic_file = dynamic_file + extension;
		dynamic_file_fallback = dynamic_file_fallback + extension;

		std::ostringstream stream;

		int retries = 1;
		module = NULL;

		for(; (module == NULL) && (retries >= 0); retries--) {
			std::string f = dynamic_file;
			if(retries == 0)
				f = dynamic_file_fallback;
#ifdef ANDROID
#define DLSYM_M dlsym
#define DLERROR_M dlerror
			module = dlopen(f.c_str(), RTLD_LAZY);
#else
#define DLSYM_M lt_dlsym
#define DLERROR_M lt_dlerror
			module = lt_dlopenext(f.c_str());
#endif
		}
		stream << "Failed to load dynamic machine from file [" << dynamic_file << "] (" << DLERROR_M () << ")\n";
		error_message = stream.str();
		std::cout << "Trying to load ]" << dynamic_file << "[ yielded ]" << module <<"[\n";
	}
	if(!module) throw jException(error_message, jException::syscall_error);

	dynlib_is_loaded = true;

	if((init = (init_dynamic *) DLSYM_M (module, "init"))
	   == NULL)
		throw jException(std::string("Module ") + get_name() + " defined no init function.\n", jException::sanity_error);
	if((exct = (execute_dynamic *) DLSYM_M (module, "execute"))
	   == NULL)
		throw jException(std::string("Module ") + get_name() + " defined no execute function.\n", jException::sanity_error);
	if((rset = (reset_dynamic *) DLSYM_M (module, "reset"))
	   == NULL)
		throw jException(std::string("Module ") + get_name() + " defined no reset function.\n", jException::sanity_error);
	if((delt = (delete_dynamic *) DLSYM_M (module, "delete"))
	   == NULL)
		throw jException(std::string("Module ") + get_name() + " defined no delete function.\n", jException::sanity_error);
	if((cptr = (controller_ptr_dynamic *) DLSYM_M (module, "get_controller_ptr"))
	   == NULL)
		throw jException(std::string("Module ") + get_name() + " defined no get_controller_ptr function.\n", jException::sanity_error);
}

void DynamicMachine::Handle::parse_iodeclaration(const KXMLDoc &io, int &dimension, int &channels, bool &premix) {
	std::string d;

	d = io.get_attr("dimension");
	std::istringstream sin(d);
	if(d != "midi")
		sin >> dimension;
	else
		dimension = _MIDI;

	d = io.get_attr("channels");
	std::istringstream san(d);
	san >> channels;

	try {
		d = io.get_attr("premix");
		if(d == "true")
			premix = true;
		else premix = false;
	} catch(...) {
		premix = false;
	}

}

void DynamicMachine::Handle::parse_controller(const KXMLDoc &ctr_xml) {

	std::string title = "";
	std::string name = "", type_name = "", min = "", max = "", step = "", group = "";
	std::string controller_has_midi_s = "";

	Controller::Type tp = Controller::c_int;

	name = ctr_xml.get_attr("name");
	type_name = ctr_xml.get_attr("type");

	try { group = ctr_xml.get_attr("group"); } catch(...) { /* ignore */ }
	try { min = ctr_xml.get_attr("min"); } catch(...) { /* ignore */ }
	try { max = ctr_xml.get_attr("max"); } catch(...) { /* ignore */ }
	try { step = ctr_xml.get_attr("step"); } catch(...) { /* ignore */ }
	try { controller_has_midi_s = ctr_xml.get_attr("has_midi"); } catch(...) { /* ignore */ }

	// title is the user displayed name of the controller
	title = name; // we set that to the "name" given by the XML
	// however - the name variable MUST BE UNIQUE so we concatenate the group name with the name from the XML
	name = group + ":" + name;

	controller_group[name] = group;

	{ // only add if the group did not exist before
		bool group_existed = false;
		for(auto k : groups) if(k == group) group_existed = true;
		if(!group_existed) groups.push_back(group);
	}

	controllers.push_back(name);

	int cr = -1, fn = -1;

	KXML_GET_NUMBER(ctr_xml,"coarse",cr,-1);
	KXML_GET_NUMBER(ctr_xml,"fine",fn,-1);

	if(controller_has_midi_s == "yes") {
		controller_has_midi[name] = true;
		coarse_midi_controller[name] = cr;
		fine_midi_controller[name] = fn;
	} else controller_has_midi[name] = false;

	if(type_name == "integer") tp = Machine::Controller::c_int;
	else if(type_name == "FTYPE") tp = Machine::Controller::c_float;
	else if(type_name == "float") tp = Machine::Controller::c_float;
	else if(type_name == "boolean") tp = Machine::Controller::c_bool;
	else if(type_name == "string") tp = Machine::Controller::c_string;
	else if(type_name == "enumerated") tp = Machine::Controller::c_enum;
	else if(type_name == "signalid") tp = Machine::Controller::c_sigid;

	std::map<int, std::string> enumnames;
	if(tp == Machine::Controller::c_enum) {
		// we need to parse the enumerated values as well.
		int nr_enums, i;
		int hi = -1;
		bool low_is_zero = false;

		std::string evl, nam;
		int val;

		nr_enums = ctr_xml["enum"].get_count();
		for(i = 0; i < nr_enums; i++) {
			KXMLDoc e = ctr_xml["enum"][i];
			evl = ""; nam = "";

			try { evl = e.get_attr("value"); } catch(...) { /* ignore */ }
			try { nam = e.get_attr("name"); } catch(...) { /* ignore */ }

			if(evl == "" || name == "") {
				std::string emsg = "Enumerated controller ";
				emsg = emsg + name + " did not define a proper value name pair.";
				throw jException(emsg, jException::sanity_error);
			}

			std::istringstream st(evl);
			st >> val;

			if(val == 0) low_is_zero = true;
			if(val > hi) {
				hi = val;
				max = evl;
			}

			if(enumnames.find(val) != enumnames.end()) {
				std::string emsg = "Enumerated controller ";
				emsg = emsg + name + " tries to define a value twice";
				throw jException(emsg, jException::sanity_error);
			}
			enumnames[val] = nam;
		}

		if(low_is_zero == false || hi == -1) {
				std::string emsg = "Enumerated controller ";
				emsg = emsg + name + " did not define all required value name pairs.";
				throw jException(emsg, jException::sanity_error);
		}

		for(i = 0; i < hi; i++) {
			if(enumnames.find(i) == enumnames.end()) {
				std::string emsg = "Enumerated controller ";
				emsg = emsg + name + " does not define all values in series";
				throw jException(emsg, jException::sanity_error);
			}
		}

		min = "0"; step = "1";

	}

	controller_title[name] = title;
	controller_type[name] = tp;

	if(type_name == "FTYPE") controller_is_FTYPE[name] = true;
	else controller_is_FTYPE[name] = false;

	if(min != "") controller_min[name] = min;
	if(max != "") controller_max[name] = max;
	if(step != "") controller_step[name] = step;
	controller_enumnames[name] = enumnames;
}

void DynamicMachine::Handle::parse_machine_declaration(const KXMLDoc &xml_proto) {
	try {
		dl_name = name;
		name += xml_proto["name"].get_value();
		if(dl_name == "") dl_name = name;

		try {
			std::string sink;
			sink = xml_proto.get_attr("sink");
			if(sink == "true") {
				act_as_sink = true;
			} else act_as_sink = false;
		} catch(...) {
			act_as_sink = false;
		}

		try {
			hint = xml_proto.get_attr("hint");
		} catch(...) {
			hint = ""; // no hint about what this is...
		}

		int i;
		int dimension, channels;
		int nr_ios;
		bool premix;

		try {
			nr_ios = xml_proto["output"].get_count();
			for(i = 0; i < nr_ios; i++) {
				parse_iodeclaration(xml_proto["output"][i], dimension, channels, premix);
				output[xml_proto["output"][i].get_value()] =
					Signal::Description(dimension, channels, false); // premix is ignored on outputs
			}
		} catch(...) {
			// ignore
		}
		try {
			nr_ios = xml_proto["input"].get_count();
			for(i = 0; i < nr_ios; i++) {
				parse_iodeclaration(xml_proto["input"][i], dimension, channels, premix);
				input[xml_proto["input"][i].get_value()] =
					Signal::Description(dimension, channels, premix);
			}
		} catch(...) {
			// ignore
		}

		int nr_ctr = 0;

		try {
			nr_ctr = xml_proto["controller"].get_count();
		} catch(...) { /* ignore */ }
		try {
			for(i = 0; i < nr_ctr; i++) {
				parse_controller(xml_proto["controller"][i]);
			}
		} catch(jException e) {
			throw e;
		} catch(...) {
			/// ignore
		}
	} catch(jException e) {
		throw e;
	} catch(...) {
		throw jException("Declaration error.", jException::sanity_error);
	}
}

std::string DynamicMachine::Handle::get_name() {
	MonitorGuard g(this);
	return name;
}

bool DynamicMachine::Handle::is_sink() {
	MonitorGuard g(this);
	return act_as_sink;
}

std::map<std::string, Machine::Signal::Description> DynamicMachine::Handle::get_output_descriptions() {
	MonitorGuard g(this);
	return output;
}

std::map<std::string, Machine::Signal::Description> DynamicMachine::Handle::get_input_descriptions() {
	MonitorGuard g(this);
	return input;
}

std::vector<std::string> DynamicMachine::Handle::get_controller_groups() {
	MonitorGuard g(this);

	std::vector<std::string> result;

	for(auto k : groups) result.push_back(k);

	return result;
}

std::vector<std::string> DynamicMachine::Handle::get_controller_names() {
	MonitorGuard g(this);
	std::vector<std::string> result;

	for(auto k : controllers) {
		result.push_back(k);
	}

	return result;
}

std::string DynamicMachine::Handle::get_controller_title(const std::string &ctrl) {
	MonitorGuard g(this);
	if(controller_title.find(ctrl) == controller_title.end())
		throw jException("No such controller.\n",
				 jException::sanity_error);
	return controller_title[ctrl];
}

std::string DynamicMachine::Handle::get_controller_group(const std::string &ctrl) {
	MonitorGuard g(this);
	if(controller_group.find(ctrl) == controller_group.end())
		throw jException("No such controller.\n",
				 jException::sanity_error);
	return controller_group[ctrl];
}

Machine::Controller::Type DynamicMachine::Handle::get_controller_type(const std::string &ctrl) {
	MonitorGuard g(this);
	if(controller_type.find(ctrl) == controller_type.end())
		throw jException("No such controller.\n",
				 jException::sanity_error);
	return controller_type[ctrl];
}

bool DynamicMachine::Handle::get_controller_is_FTYPE(const std::string &ctrl) {
	MonitorGuard g(this);
	if(controller_is_FTYPE.find(ctrl) == controller_is_FTYPE.end())
		throw jException("No such controller.\n",
				 jException::sanity_error);
	return controller_is_FTYPE[ctrl];
}

std::string DynamicMachine::Handle::get_controller_min(const std::string &ctrl) {
	MonitorGuard g(this);
	if(controller_min.find(ctrl) == controller_min.end())
		return "";
	return controller_min[ctrl];
}

std::string DynamicMachine::Handle::get_controller_max(const std::string &ctrl) {
	MonitorGuard g(this);
	if(controller_max.find(ctrl) == controller_max.end())
		return "";
	return controller_max[ctrl];
}

std::string DynamicMachine::Handle::get_controller_step(const std::string &ctrl) {
	MonitorGuard g(this);
	if(controller_step.find(ctrl) == controller_step.end())
		return "";
	return controller_step[ctrl];
}

std::map<int, std::string> DynamicMachine::Handle::get_controller_enumnames(const std::string &ctrl) {
	MonitorGuard g(this);
	if(controller_enumnames.find(ctrl) == controller_enumnames.end()) {
		std::map<int, std::string> emptyrval;
		return emptyrval;
	}
	return controller_enumnames[ctrl];
}

bool DynamicMachine::Handle::get_controller_has_midi(const std::string &ctrl) {
	MonitorGuard g(this);
	if(controller_has_midi.find(ctrl) == controller_has_midi.end())
		return false;
	return controller_has_midi[ctrl];
}

int DynamicMachine::Handle::get_coarse_midi_controller(const std::string &ctrl) {
	MonitorGuard g(this);
	if(coarse_midi_controller.find(ctrl) == coarse_midi_controller.end())
		return -1;
	return coarse_midi_controller[ctrl];
}

int DynamicMachine::Handle::get_fine_midi_controller(const std::string &ctrl) {
	MonitorGuard g(this);
	if(fine_midi_controller.find(ctrl) == fine_midi_controller.end())
		return -1;
	return fine_midi_controller[ctrl];
}

std::string DynamicMachine::Handle::get_hint() {
	return hint;
}

/* static handle data/functions */
std::map<std::string, DynamicMachine::Handle *> DynamicMachine::dynamic_file_handle;
std::map<std::string, DynamicMachine::Handle *> DynamicMachine::handle_map;

std::map<std::string, DynamicMachine::Handle *> DynamicMachine::Handle::declaration2handle;
std::map<std::string, DynamicMachine::Handle *> DynamicMachine::Handle::name2handle;

std::string DynamicMachine::Handle::handle_directory;

void DynamicMachine::Handle::load_handle(std::string fname) {
	std::map<std::string, Handle *>::iterator i;
	i = declaration2handle.find(fname);

	if(i != declaration2handle.end()) {
		// ignore, already loaded
		return;
	}

	Handle *nh = NULL;
	KXMLDoc handle_set;
	std::string base_name = "";
	try {
		nh = new Handle(fname);
	} catch(HandleSetException hse) {
		base_name = hse.base_name;
		handle_set = hse.handle_set;
	}

	if(base_name == "") { /* standard procedure */
		declaration2handle[fname] = nh;
		name2handle[nh->get_name()] = nh;
	} else { /* we have ourselves a set... */
		Handle *set_parent = NULL, *current = NULL;
		int nr_machines, i;

		try {
			nr_machines = handle_set["machine"].get_count();
			for(i = 0; i < nr_machines; i++) {
				current = new Handle(
					base_name, handle_set["machine"][i], set_parent);
				name2handle[current->get_name()] = set_parent = current;
			}
			declaration2handle[fname] = current;
		} catch(jException e) {
			std::cerr << "Set error: " << e.message << "\n";
		} catch(...) {
			// ignore
		}
	}
}

void DynamicMachine::Handle::refresh_handle_set() {
	std::string candidate;
	std::vector<std::string> try_list;
	std::vector<std::string>::iterator try_list_entry;

#ifdef ANDROID
	candidate = KAMOFLAGE_ANDROID_ROOT_DIRECTORY;
	candidate += "/app_nativedata/dynlib";
#else
	char *dir_bfr, _dir_bf[2048];
	dir_bfr = getcwd(_dir_bf, sizeof(_dir_bf));
	if(dir_bfr == NULL)
		throw jException("Failed to get current working directory.",
				 jException::syscall_error);
	candidate = dir_bfr;
	candidate += "/dynlib";
#endif
	try_list.push_back(candidate);

#ifndef ANDROID
	candidate = std::string(CONFIG_DIR) + "/dynlib";
	try_list.push_back(candidate);
#endif

	for(try_list_entry  = try_list.begin();
	    try_list_entry != try_list.end();
	    try_list_entry++) {

		handle_directory = *try_list_entry;
		std::cout << "Trying to read handles in ]" << handle_directory << "[ ...\n";

		DIR *dir = opendir(handle_directory.c_str());
		if(dir != NULL) {
			struct dirent *dire;
			struct stat stb;
			std::string fnpth;
			while((dire = readdir(dir)) != NULL) {
				std::string filename = dire->d_name;
				filename = handle_directory + "/" + filename;

				if(lstat(filename.c_str(), &stb) != 0) {
					closedir(dir);
					throw jException("Failed to stat file during refresh.", jException::syscall_error);
				}

				if((stb.st_mode & S_IFMT) == S_IFREG) {
					// get extension..
					const char *f = filename.c_str();
					const char *last4 = &f[filename.size() - 4];
					if(last4[0] == '.' &&
					   last4[1] == 'x' &&
					   last4[2] == 'm' &&
					   last4[3] == 'l') { // OK, let's trust it. XXX perhaps better sequrity?
						load_handle(filename);
					}
				}
			}
			closedir(dir);
			std::cout << "Handles read successfully from ]" << *try_list_entry << "[.\n";
			return;
		}
	}
	if(try_list_entry == try_list.end())
		throw jException("Failed to refresh handles", jException::syscall_error);
}

std::set<std::string> DynamicMachine::Handle::get_handle_set() {
	std::set<std::string> results;
	std::map<std::string, DynamicMachine::Handle *>::iterator m;

	for(m = name2handle.begin(); m != name2handle.end(); m++) {
		results.insert((*m).first);
	}
	return results;
}

DynamicMachine::Handle *DynamicMachine::Handle::get_handle(std::string name) {
	std::map<std::string, DynamicMachine::Handle *>::iterator m;

	m = name2handle.find(name);
	if(m != name2handle.end()) {
		(*m).second->prep_dynlib();
		return (*m).second;
	}

	std::cout << "   Handles:\n";
	for(m = name2handle.begin(); m != name2handle.end(); m++) {
		std::cout << "[" << ((*m).first) << "]\n";
	}

	throw jException(std::string("No such machine handle. : ") + name ,
			 jException::sanity_error);
}

std::string DynamicMachine::Handle::get_handle_hint(std::string name) {
	std::map<std::string, DynamicMachine::Handle *>::iterator m;

	m = name2handle.find(name);
	if(m != name2handle.end()) {
		return (*m).second->get_hint();
	}

	std::cout << "   Handles:\n";
	for(m = name2handle.begin(); m != name2handle.end(); m++) {
		std::cout << "[" << ((*m).first) << "]\n";
	}

	throw jException(std::string("No such machine handle. : ") + name ,
			 jException::sanity_error);
}

/*****************************
 *                           *
 *      class DynamicMachine *
 *                           *
 *****************************/

DynamicMachine::~DynamicMachine() {
	/*******************
	 *
	 *
	 * Check if the Baseclass' ~-function is run!
	 *
	 *
	 *******************/
}

std::vector<std::string> DynamicMachine::internal_get_controller_groups() {
	return dh->get_controller_groups();
}

std::vector<std::string> DynamicMachine::internal_get_controller_names() {
	return dh->get_controller_names();
}

std::vector<std::string> DynamicMachine::internal_get_controller_names(const std::string &group_name) {
	std::vector<std::string> result;

	for(auto k : dh->get_controller_names()) {
		if(dh->get_controller_group(k) == group_name) {
			result.push_back(k);
		}
	}

	return result;
}

Machine::Controller *DynamicMachine::internal_get_controller(const std::string &name) {
	void *ptr = controller_ptr[name];

	if(ptr == NULL)
		throw jException(
			std::string("No such controller [") + name + "] available.",
			jException::sanity_error);

	Controller::Type tp =
		dh->get_controller_type(name);
	bool is_FTYPE =
		dh->get_controller_is_FTYPE(name);
	std::string title =
		dh->get_controller_title(name);
	std::string min =
		dh->get_controller_min(name);
	std::string max =
		dh->get_controller_max(name);
	std::string step =
		dh->get_controller_step(name);

	std::map<int, std::string> enumnames =
		dh->get_controller_enumnames(name);

	bool has_midi_controller = dh->get_controller_has_midi(name);

	Controller *ctr = create_controller(tp, name, title, ptr, min, max, step, enumnames, is_FTYPE);

	if(has_midi_controller) {
		int crs, fn;
		crs = dh->get_coarse_midi_controller(name);
		fn = dh->get_fine_midi_controller(name);

		set_midi_controller(ctr, crs, fn);
	}

	return ctr;
}

std::string DynamicMachine::internal_get_hint() {
	return dh->get_hint();
}

std::string DynamicMachine::get_handle_name() {
	return dh->get_name();
}

#define DYN_MACHINE_SETUP_BLOCK()		\
	{ \
		setup_machine(); \
		setup_dynamic_machine(); \
	}

DynamicMachine::DynamicMachine(Handle *dhandle, float _xpos, float _ypos) : Machine(dhandle->get_name(), false, _xpos, _ypos) {
	input_descriptor = dhandle->get_input_descriptions();
	output_descriptor = dhandle->get_output_descriptions();
	dh = dhandle;

	std::cout << "  dynamic created with " << output_descriptor.size() << " outputs, not using base name as name.\n";

	DYN_MACHINE_SETUP_BLOCK();
}

DynamicMachine::DynamicMachine(
	Handle *dhandle,
	const std::string &new_name) : Machine(new_name, true, 0.0f, 0.0f) {
	input_descriptor = dhandle->get_input_descriptions();
	output_descriptor = dhandle->get_output_descriptions();
	dh = dhandle;

	std::cout << "  dynamic created with " << output_descriptor.size() << " outputs.\n";

	DYN_MACHINE_SETUP_BLOCK();
}

#ifdef ANDROID
extern "C" void VuknobAndroidAudio__CLEANUP_STUFF();
extern "C" void VuknobAndroidAudio__SETUP_STUFF(
	int *period_size, int *rate,
	int (*__entry)(void *data),
	void *__data,
	int (**android_audio_callback)(FTYPE vol, FTYPE *in, int il, int ic),
	void (**android_audio_stop_f)(void)
	);
extern "C" int VuknobAndroidAudio__get_native_audio_configuration_data(int *frequency, int *buffersize);
#endif

/* Prepares the dynamic table, and calls the dynamic machines init function.
 */
void DynamicMachine::setup_dynamic_machine() {
	if(dh->is_sink()) {
		register_this_sink(this);
	}

	std::cout << "MachineTable: " << &dt << " points to " << this << "\n";
	dt.mp = (MachinePointer *)this;

	dt.fill_sink = &(DynamicMachine::fill_sink);

	dt.enable_low_latency_mode = &(DynamicMachine::enable_low_latency_mode);
	dt.disable_low_latency_mode = &(DynamicMachine::disable_low_latency_mode);

	dt.set_signal_defaults = &(DynamicMachine::set_signal_defaults);

	dt.get_output_signal = &(DynamicMachine::get_output_signal);
	dt.get_input_signal = &(DynamicMachine::get_input_signal);
	dt.get_next_signal = &(DynamicMachine::get_next_signal);
	dt.get_static_signal = &(DynamicMachine::get_static_signal);

	dt.get_signal_dimension = &(DynamicMachine::get_signal_dimension);
	dt.get_signal_channels = &(DynamicMachine::get_signal_channels);
	dt.get_signal_resolution = &(DynamicMachine::get_signal_resolution);
	dt.get_signal_frequency = &(DynamicMachine::get_signal_frequency);
	dt.get_signal_samples = &(DynamicMachine::get_signal_samples);
	dt.get_signal_buffer = &(DynamicMachine::get_signal_buffer);

	dt.run_simple_thread = &(DynamicMachine::run_simple_thread);

	dt.get_recording_filename = &(DynamicMachine::get_recording_filename);

	dt.register_failure = &(DynamicMachine::register_failure);

	dt.get_math_tables = &(DynamicMachine::get_math_tables);

	dt.prepare_fft = &(DynamicMachine::prepare_fft);
	dt.do_fft = &(DynamicMachine::do_fft);
	dt.inverse_fft = &(DynamicMachine::inverse_fft);
	dt.run_async_operation = &(Machine::run_async_operation);

#ifdef ANDROID
	dt.VuknobAndroidAudio__CLEANUP_STUFF = VuknobAndroidAudio__CLEANUP_STUFF;
	dt.VuknobAndroidAudio__SETUP_STUFF = VuknobAndroidAudio__SETUP_STUFF;
	dt.VuknobAndroidAudio__get_native_audio_configuration_data = VuknobAndroidAudio__get_native_audio_configuration_data;
#endif

	init_dynamic *init = ((Handle *)dh)->init;

	dynamic_data =
		(void *)(*init)(&dt, ((Handle *)dh)->get_name().c_str());
	if(dynamic_data == NULL)
		throw jException("Failed to initiate dynamic machine.", jException::sanity_error);
	module_name = ((Handle *)dh)->get_name().c_str();

	for(auto k : dh->get_controller_names()) {
		controller_ptr[k]
			= ((Handle *)dh)->cptr(&dt,
					       dynamic_data,
					       dh->get_controller_title(k).c_str(), // the "name" tag in the XML is actually called title in our internal structure... while the "name" variable is actually group + ":" + title
					       dh->get_controller_group(k).c_str()
				);
	}
}


void DynamicMachine::fill_buffers() {
	(*(((Handle *)dh)->exct))(&dt, dynamic_data);
}

void DynamicMachine::reset() {
	(*(((Handle *)dh)->rset))(&dt, dynamic_data);
}

bool DynamicMachine::detach_and_destroy() {
	detach_all_inputs();
	detach_all_outputs();

	SATAN_DEBUG("Deleting dynamic machine data\n"); fflush(0);
	(*(((Handle *)dh)->delt))(dynamic_data);
	SATAN_DEBUG("Dynamic machine data DELETED\n"); fflush(0);

	return true;
}

std::string DynamicMachine::get_class_name() {
	return "DynamicMachine";
}

std::string DynamicMachine::get_descriptive_xml() {
	return std::string("<dynamicmachine handle=\"") + get_handle_name() + "\" />\n";
}

std::set<std::string> DynamicMachine::get_handle_set() {
	return Handle::get_handle_set();
}

std::string DynamicMachine::get_handle_hint(std::string dynamic_machine) {
	return Handle::get_handle_hint(dynamic_machine);
}

void DynamicMachine::refresh_handle_set() {
	Handle::refresh_handle_set();
}

Machine *DynamicMachine::instance(const std::string &_dmname, float _xpos, float _ypos) {
	Machine *retval;

	Machine::machine_operation_enqueue(
		[&retval, &_dmname, _xpos, _ypos] (void* /*d*/) {
			retval = new DynamicMachine(Handle::get_handle(_dmname), _xpos, _ypos);
		},
		NULL, true);

	return retval;
}

void DynamicMachine::instance_from_xml(const KXMLDoc &_dyn_xml) {
	typedef struct {
		const KXMLDoc &dxml;
	} Param;
	Param param = {
		.dxml = _dyn_xml
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			const KXMLDoc &dyn_xml = p->dxml;

			std::string machine_name = dyn_xml.get_attr("name");
			std::string handle_name = dyn_xml["dynamicmachine"].get_attr("handle");

			DynamicMachine *dm = NULL;
			dm = new DynamicMachine(
				Handle::get_handle(handle_name),
				machine_name);

			dm->setup_using_xml(dyn_xml);
		},
		&param, true);
}

/* NOTE! This is for _ALL_ functions below this comment.
 *
 * called from the dynamic library itself, do not call these from within satan
 *
 */

int DynamicMachine::fill_sink(MachineTable *mt,
			      int (*fill_sink_callback)(int status, void *cbd),
			      void *cbdata) {
//	DECLARE_TIME_MEASURE_OBJECT_STATIC(filler_sinker, "fill_sink", 5000);
	int retval;
//	START_TIME_MEASURE(filler_sinker);
	retval = fill_this_sink((DynamicMachine *)(mt->mp), fill_sink_callback, cbdata);
//	STOP_TIME_MEASURE(filler_sinker, "sink filled");

	return retval;
}

void DynamicMachine::enable_low_latency_mode() {
	activate_low_latency_mode();
}

void DynamicMachine::disable_low_latency_mode() {
	deactivate_low_latency_mode();
}

void DynamicMachine::set_signal_defaults(MachineTable *mt,
					 int dim,
					 int len, int res,
					 int frequency) {
	DynamicMachine *m = (DynamicMachine *)(mt->mp);

	set_ssignal_defaults((Machine *)m, (Dimension)dim, len, (Resolution)res, frequency);
}

SignalPointer *DynamicMachine::get_output_signal(MachineTable *mt, const char *nam) {
	DynamicMachine *m = (DynamicMachine *)(mt->mp);

	std::map<std::string, Signal *>::iterator i;
	i = m->output.find(nam);

	if(i == m->output.end()) {
		printf(" XXXXXXXXXXXXXXX: ]%s[ - no such signal!\n", nam);
		for(i = m->output.begin(); i!= m->output.end(); i++) {
			printf("     ]%s[\n", (*i).first.c_str());
			if(strcmp((*i).first.c_str(), nam) == 0) {
				printf("Something is broke here...\n");
				exit(0);
			}
		}
		return NULL;
	}

	return (SignalPointer *)((*i).second);
}

SignalPointer *DynamicMachine::get_input_signal(MachineTable *mt, const char *nam) {
	DynamicMachine *m = (DynamicMachine *)(mt->mp);

	std::map<std::string, Signal *>::iterator i;
	if((i = m->premixed_input.find(nam)) ==
	   m->premixed_input.end()) {
		i = m->input.find(nam);

		if(i == m->input.end()) {
			return NULL;
		}
	}
	return (SignalPointer *)((*i).second);
}

SignalPointer *DynamicMachine::get_next_signal(MachineTable *mt, SignalPointer *s) {
	DynamicMachine *m = (DynamicMachine *)(mt->mp);
	Signal *sig = (Signal *)s;
	if(s == NULL) return NULL;
	return (SignalPointer *)sig->get_next(m);
}

SignalPointer *DynamicMachine::get_static_signal(int index) {
	return (SignalPointer *)StaticSignal::get_signal(index);
}

int DynamicMachine::get_signal_dimension(SignalPointer *s) {
	SignalBase *sig = (SignalBase *)s;
	return sig->get_dimension();
}

int DynamicMachine::get_signal_channels(SignalPointer *s) {
	SignalBase *sig = (SignalBase *)s;
	return sig->get_channels();
}

int DynamicMachine::get_signal_resolution(SignalPointer *s) {
	SignalBase *sig = (SignalBase *)s;
	return sig->get_resolution();
}

int DynamicMachine::get_signal_frequency(SignalPointer *s) {
	SignalBase *sig = (SignalBase *)s;
	return sig->get_frequency();
}

int DynamicMachine::get_signal_samples(SignalPointer *s) {
	SignalBase *sig = (SignalBase *)s;
	return sig->get_samples();
}

void *DynamicMachine::get_signal_buffer(SignalPointer *s) {
	SignalBase *sig = (SignalBase *)s;
	return sig->get_buffer();
}

class DynamicMachineSimpleThread : public jThread {
private:
	void (*funcbod)(void *);
	void *data;

public:
	DynamicMachineSimpleThread(void (*f)(void *), void *d) : jThread("DynamicMachineSimpleThread"), funcbod(f), data(d) {}

	virtual void thread_body() {
		(void)funcbod(data);
		delete this; // hmmm... delete your self, Atari Teenage Riot
	}
};

void DynamicMachine::run_simple_thread(void (*thread_function)(void *), void *data) {
	DynamicMachineSimpleThread *t = new DynamicMachineSimpleThread(thread_function, data);

	t->run();
}

int DynamicMachine::get_recording_filename(
	struct _MachineTable *, char *dst, unsigned int len) {
	std::string fnm;
	internal_get_rec_fname(&fnm);

	if(fnm.size() >= len) return -1;

	strncpy(dst, fnm.c_str(), len);

	return 0;
}

void DynamicMachine::register_failure(
	void *machine_instance, const char *message) {
	/* do nothing at this time */
}

#ifdef __SATAN_USES_FXP
#include <math.h>

#include "fixedpointmath.h"
FTYPE satan_powfp8p24(FTYPE x, FTYPE y);

bool __internal_satan_sine_table_initiated = false;
float __internal_satan_sine_table[SAT_SIN_TABLE_LEN];
FTYPE __internal_satan_sine_table_FTYPE[SAT_SIN_FTYPE_TABLE_LEN << 1];
#endif

void DynamicMachine::get_math_tables(
	float **sine_table,
	FTYPE (**__pow_fun)(FTYPE x, FTYPE y),
	FTYPE **sine_table_FTYPE
	) {
#ifdef __SATAN_USES_FXP
	if(!__internal_satan_sine_table_initiated) {
		int x;
		float x_f;
		for(x = 0; x < SAT_SIN_TABLE_LEN; x++) {
			x_f = x;
			x_f /= (float)SAT_SIN_TABLE_LEN;
			__internal_satan_sine_table[x] =
				sin(x_f * 2 * M_PI);
		}
		for(x = 0; x < SAT_SIN_FTYPE_TABLE_LEN; x++) {
			FTYPE v = x;
			v = v << 12;
			float t = fp8p24tof(v);
			float y = sinf(t * 2.0f * M_PI);
			__internal_satan_sine_table_FTYPE[SAT_SIN_FTYPE_TABLE_LEN + x] =
				__internal_satan_sine_table_FTYPE[x] = ftoFTYPE(y);
		}
		__internal_satan_sine_table_initiated = true;
	}

	 *sine_table = __internal_satan_sine_table;
	 *__pow_fun = satan_powfp8p24;
	 *sine_table_FTYPE = __internal_satan_sine_table_FTYPE;
#else
	 *sine_table = NULL;
	 *__pow_fun = NULL;
	 *sine_table_FTYPE = NULL;
#endif
}

/************************
 *
 * KISS FFTr interface
 *
 ************************/

kiss_fftr_cfg DynamicMachine::prepare_fft(int samples, int inverse_fft) {
	return kiss_fftr_alloc(samples, inverse_fft, NULL, NULL);
}

void DynamicMachine::do_fft(kiss_fftr_cfg cfg, FTYPE *timedata, kiss_fft_cpx *freqdata) {
	kiss_fftr(cfg, timedata, freqdata);
}

void DynamicMachine::inverse_fft(kiss_fftr_cfg cfg, kiss_fft_cpx *freqdata, FTYPE *timedata) {
	kiss_fftri(cfg, freqdata, timedata);
}
