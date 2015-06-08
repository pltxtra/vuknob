/*
 * VuKNOB
 * (C) 2015 by Anton Persson
 *
 * http://www.vuknob.com/
 *
 * Based on SATAN:
 *
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

#include <kamogui.hh>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>

#include "controller_handler.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

static bool sample_editor_shortcut_enabled = true;

KammoEventHandler_Declare(ControllerHandlerOld,"showControlsContainer:cgroups");

std::shared_ptr<ControllerHandler> chndl = std::make_shared<ControllerHandler>();
std::shared_ptr<RemoteInterface::RIMachine> current_ri_machine;

std::vector<KammoGUI::Widget *> erasable_widgets;
class MyScale : public KammoGUI::Scale {
public:
	KammoGUI::Label *v_lbl;
	std::shared_ptr<RemoteInterface::RIMachine::RIController> ctr;
	double min, max,step;

	MyScale(double _min,
		double _max,
		double _step,
		std::shared_ptr<RemoteInterface::RIMachine::RIController> _ctr,
		KammoGUI::Label *_v_lbl) :
		KammoGUI::Scale(true, _min, _max, _step), v_lbl(_v_lbl),
		ctr(_ctr), min(_min), max(_max), step(_step) {
	}
};

class MyBaseButton : public KammoGUI::Button {
public:
	MyScale *scale;

	MyBaseButton(MyScale *_scale) : scale(_scale) {}

	virtual void do_action() = 0;
};

class MyButton : public MyBaseButton {
public:
	bool increase;

	MyButton(MyScale *_scale, bool _increase) : MyBaseButton(_scale), increase(_increase) {}

	virtual void do_action() {
		double value = scale->get_value();
		if(increase) {
			value += scale->step;
		} else {
			value -= scale->step;
		}
		scale->set_value(value);
	}
};

class MySampleButton : public MyBaseButton {
public:
	MySampleButton(MyScale *_scale) : MyBaseButton(_scale) {}

	virtual void do_action() {
		static KammoGUI::UserEvent *ue = NULL;

		KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showSamplesDirectories");
		if(ue != NULL) {
			int *value = new int[1];
			value[0] = (int)(scale->get_value());

			std::map<std::string, void *> args;
			args["id"] = (void *)value;

			args["followup"] = new std::string("showControlsContainer");

			trigger_user_event(ue, args);
		}
	}
};

void process_value_changed(KammoGUI::Widget *wid) {
	MyScale *scl = (MyScale *)wid;
	auto ctr = scl->ctr;

	double value = scl->get_value();

	switch(ctr->get_type()) {

	case RemoteInterface::RIMachine::RIController::ric_sigid:
	case RemoteInterface::RIMachine::RIController::ric_enum:
	case RemoteInterface::RIMachine::RIController::ric_int:
	{
		int val = value;
		ctr->set_value(val);
	}
	break;

	case RemoteInterface::RIMachine::RIController::ric_float:
	{
		float val = value;
		ctr->set_value(val);
	}
	break;

	case RemoteInterface::RIMachine::RIController::ric_bool:
	{
		bool val = value == 1.0 ? true : false;
		ctr->set_value(val);
	}
	break;

	case RemoteInterface::RIMachine::RIController::ric_string:
	default:
		return;
	}

	std::ostringstream vstr;

	switch(ctr->get_type()) {
	case RemoteInterface::RIMachine::RIController::ric_enum:
	case RemoteInterface::RIMachine::RIController::ric_sigid:
		vstr << " : " << ctr->get_value_name((int)value);
		break;
	case RemoteInterface::RIMachine::RIController::ric_int:
	case RemoteInterface::RIMachine::RIController::ric_float:
	case RemoteInterface::RIMachine::RIController::ric_bool:
	case RemoteInterface::RIMachine::RIController::ric_string:
	default:
		vstr << " : " << value;
		break;
	}

	scl->v_lbl->set_title(vstr.str());
}

virtual void on_click(KammoGUI::Widget *wid) {
	MyBaseButton *mbu = (MyBaseButton *)wid;

	mbu->do_action();
	process_value_changed(mbu->scale);
}

virtual void on_value_changed(KammoGUI::Widget *wid) {
	process_value_changed(wid);
}

void add_scale(KammoGUI::Container *cnt,
	       std::shared_ptr<RemoteInterface::RIMachine::RIController> ctr) {
	double min, max, step, value;
	switch(ctr->get_type()) {

	case RemoteInterface::RIMachine::RIController::ric_sigid:
	case RemoteInterface::RIMachine::RIController::ric_enum:
	case RemoteInterface::RIMachine::RIController::ric_int:
	{
		int _min; ctr->get_min(_min);
		int _max; ctr->get_max(_max);
		int _val; ctr->get_value(_val);
		step = 1;
		min = _min;
		max = _max;
		value = _val;
	}
	break;

	case RemoteInterface::RIMachine::RIController::ric_float:
	{
		float _min; ctr->get_min(_min);
		float _max; ctr->get_max(_max);
		float _step; ctr->get_step(_step);
		float _val; ctr->get_value(_val);
		min = _min;
		max = _max;
		step = _step;
		value = _val;
	}
	break;

	case RemoteInterface::RIMachine::RIController::ric_bool:
	{
		bool _val; ctr->get_value(_val);
		min = 0.0;
		max = 1.0;
		step = 1.0;
		value = (_val) ? 1.0 : 0.0;
	}
	break;

	case RemoteInterface::RIMachine::RIController::ric_string:
	default:
		return;
	}

	KammoGUI::Label *lbl = new KammoGUI::Label();
	lbl->set_title(ctr->get_title());

	KammoGUI::Label *v_lbl = new KammoGUI::Label();
	std::ostringstream vstr;

	switch(ctr->get_type()) {
	case RemoteInterface::RIMachine::RIController::ric_enum:
	case RemoteInterface::RIMachine::RIController::ric_sigid:
		vstr << " : " << ctr->get_value_name((int)value);
		break;
	case RemoteInterface::RIMachine::RIController::ric_int:
	case RemoteInterface::RIMachine::RIController::ric_float:
	case RemoteInterface::RIMachine::RIController::ric_bool:
	case RemoteInterface::RIMachine::RIController::ric_string:
	default:
		vstr << " : " << value;
		break;
	}
	v_lbl->set_title(vstr.str());

	SATAN_DEBUG("min: %f, max: %f, value: %s\n", min, max, vstr.str().c_str());

	MyScale *scl =
		new MyScale(min, max, step, ctr, v_lbl);
	scl->set_fill(true);
	scl->set_expand(true);
	scl->attach_event_handler(this);
	scl->set_value(value);

	MyButton *less = new MyButton(scl, false);
	MyButton *more = new MyButton(scl, true);

	less->set_title("-");
	more->set_title("+");

	less->attach_event_handler(this);
	more->attach_event_handler(this);

	KammoGUI::Container *lbl_cnt = new KammoGUI::Container(true);
	lbl_cnt->add(*lbl);
	lbl_cnt->add(*v_lbl);

	KammoGUI::Container *int_cnt = new KammoGUI::Container(true);
	int_cnt->add(*less);
	int_cnt->add(*scl);
	int_cnt->add(*more);

	if(sample_editor_shortcut_enabled) {
		if(ctr->get_type() == RemoteInterface::RIMachine::RIController::ric_sigid) {
			MySampleButton *smb = new MySampleButton(scl);
			smb->attach_event_handler(this);
			smb->set_title("LOAD");
			int_cnt->add(*smb);
		}
	}

	cnt->add(*lbl_cnt);
	cnt->add(*int_cnt);
}

void rebuild_controller_list(std::shared_ptr<RemoteInterface::RIMachine> ri_m, std::string group_name) {
	static KammoGUI::Container *cnt = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&cnt, "controllerScroller");

	cnt->clear();

	SATAN_DEBUG("SHOW CONTROLLER GROUP (using RIMachine, partial): %s\n", group_name.c_str());

	auto c_names = ri_m->get_controller_names(group_name);

	for(auto k : c_names) {
		SATAN_DEBUG("   CONTROLLER: %s\n", k.c_str());
		try {
			auto ctr = ri_m->get_controller(k);

			switch(ctr->get_type()) {
			case RemoteInterface::RIMachine::RIController::ric_sigid:
			case RemoteInterface::RIMachine::RIController::ric_enum:
			case RemoteInterface::RIMachine::RIController::ric_int:
			case RemoteInterface::RIMachine::RIController::ric_float:
			case RemoteInterface::RIMachine::RIController::ric_bool:
				add_scale(cnt,ctr);
				break;
			case RemoteInterface::RIMachine::RIController::ric_string:
				break;
			default:
				break;
			}
		} catch(...) { /* ignore */ }
	}
}

std::string refresh_groups(std::shared_ptr<RemoteInterface::RIMachine> m) {
	static KammoGUI::List *groups = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&groups, "cgroups");
	groups->clear();

	std::string first_group = "";

	for(auto k : m->get_controller_groups()) {
		std::vector<std::string> row_contents;

		if(first_group == "") first_group = k;

		row_contents.clear();
		row_contents.push_back(k);

		groups->add_row(row_contents);
	}

	return first_group;
}

void refresh_controllers(std::shared_ptr<RemoteInterface::RIMachine> ri_m, std::string first_group) {
	static KammoGUI::List *groups = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&groups, "cgroups");

	try {
		KammoGUI::List::iterator li = groups->get_selected();
		first_group = groups->get_value(li, 0);
	} catch(...) {
		// ignore
	}

	rebuild_controller_list(ri_m, first_group);

}

virtual void on_select_row(KammoGUI::Widget *widget, KammoGUI::List::iterator row) {
	if(widget->get_id() == "cgroups") {
		std::string group_name = ((KammoGUI::List *)widget)->get_value(row, 0);
		SATAN_DEBUG("--- SELECTED CONTROLLER GROUP: %s\n", group_name.c_str());
		rebuild_controller_list(current_ri_machine, group_name);
	}
}

virtual void on_user_event(KammoGUI::UserEvent *ue, std::map<std::string, void *> args) {
	if(args.find("machine_to_control") != args.end()) {
		char *m_name_c = (char *)args["machine_to_control"];
		string m_name = m_name_c;
		free(m_name_c);

		current_ri_machine = chndl->get_machine_by_name(m_name);
		if(current_ri_machine) {
			SATAN_DEBUG("ControllerHandler got the RI Machine: %s\n", current_ri_machine->get_name().c_str());
		}
	}

	std::string first_group = "";

	if(current_ri_machine) {
		if(ue->get_id() == "showControlsContainer") {
			first_group = refresh_groups(current_ri_machine);
		}

		refresh_controllers(current_ri_machine, first_group);
	}
}

virtual void on_init(KammoGUI::Widget *wid) {
	if(wid->get_id() == "cgroups") {
		RemoteInterface::Client::register_ri_machine_set_listener(chndl);
	}
}

KammoEventHandler_Instance(ControllerHandlerOld);

void ControllerHandler::ri_machine_registered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine) {
	KammoGUI::run_on_GUI_thread(
		[this, ri_machine]() {
			machines[ri_machine->get_name()] = ri_machine;
		}
		);
}

void ControllerHandler::ri_machine_unregistered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine) {
	KammoGUI::run_on_GUI_thread(
		[this, ri_machine]() {
			auto mch = machines.find(ri_machine->get_name());
			if(mch != machines.end()) machines.erase(mch);
		}
		);
}

std::shared_ptr<RemoteInterface::RIMachine> ControllerHandler::get_machine_by_name(const std::string &name) {
	std::shared_ptr<RemoteInterface::RIMachine> retval;

	auto mch = machines.find(name);
	if(mch != machines.end()) {
		retval = (*mch).second;
	}

	return retval;
}

void ControllerHandler::disable_sample_editor_shortcut() {
	sample_editor_shortcut_enabled = false;
}
