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

#include "machine.hh"

#include <kamogui.hh>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>

#include "machine.hh"

#include "controller_handler.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

KammoEventHandler_Declare(ControllerHandler,"showControlsContainer:cgroups");

std::vector<KammoGUI::Widget *> erasable_widgets;
class MyScale : public KammoGUI::Scale {
public:
	KammoGUI::Label *v_lbl;
	Machine::Controller *ctr;
	double min, max,step;

	MyScale(double _min,
		double _max,
		double _step,
		Machine::Controller *_ctr,
		KammoGUI::Label *_v_lbl) :
		KammoGUI::Scale(true, _min, _max, _step), v_lbl(_v_lbl),
		ctr(_ctr), min(_min), max(_max), step(_step) {
	}

	~MyScale() {
		delete ctr;
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
	Machine::Controller *ctr = scl->ctr;

	double value = scl->get_value();

	switch(ctr->get_type()) {

	case Machine::Controller::c_sigid:
	case Machine::Controller::c_enum:
	case Machine::Controller::c_int:
	{
		int val = value;
		ctr->set_value(val);
	}
	break;

	case Machine::Controller::c_float:
	{
		float val = value;
		ctr->set_value(val);
	}
	break;

	case Machine::Controller::c_bool:
	{
		bool val = value == 1.0 ? true : false;
		ctr->set_value(val);
	}
	break;

	case Machine::Controller::c_string:
	default:
		return;
	}

	std::ostringstream vstr;

	switch(ctr->get_type()) {
	case Machine::Controller::c_enum:
	case Machine::Controller::c_sigid:
		vstr << " : " << ctr->get_value_name((int)value);
		break;
	case Machine::Controller::c_int:
	case Machine::Controller::c_float:
	case Machine::Controller::c_bool:
	case Machine::Controller::c_string:
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
	       Machine::Controller *ctr) {
	double min, max, step, value;
	switch(ctr->get_type()) {

	case Machine::Controller::c_sigid:
	case Machine::Controller::c_enum:
	case Machine::Controller::c_int:
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

	case Machine::Controller::c_float:
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

	case Machine::Controller::c_bool:
	{
		bool _val; ctr->get_value(_val);
		min = 0.0;
		max = 1.0;
		step = 1.0;
		value = (_val) ? 1.0 : 0.0;
	}
	break;

	case Machine::Controller::c_string:
	default:
		return;
	}

	KammoGUI::Label *lbl = new KammoGUI::Label();
	lbl->set_title(ctr->get_title());

	KammoGUI::Label *v_lbl = new KammoGUI::Label();
	std::ostringstream vstr;

	switch(ctr->get_type()) {
	case Machine::Controller::c_enum:
	case Machine::Controller::c_sigid:
		vstr << " : " << ctr->get_value_name((int)value);
		break;
	case Machine::Controller::c_int:
	case Machine::Controller::c_float:
	case Machine::Controller::c_bool:
	case Machine::Controller::c_string:
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

	if(ctr->get_type() == Machine::Controller::c_sigid) {
		MySampleButton *smb = new MySampleButton(scl);
		smb->attach_event_handler(this);
		smb->set_title("LOAD");
		int_cnt->add(*smb);
	}
	cnt->add(*lbl_cnt);
	cnt->add(*int_cnt);
}

void rebuild_controller_list(Machine *m, std::string group_name) {
	static KammoGUI::Container *cnt = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&cnt, "controllerScroller");
	cnt->clear();

	std::vector<std::string> c_names;

	SATAN_DEBUG("SHOW CONTROLLER GROUP: %s\n", group_name.c_str());

	if(group_name != "")
		c_names = m->get_controller_names(group_name);
	else
		c_names = m->get_controller_names();

	std::set<std::string>::iterator k;

	for(auto k : c_names) {
		SATAN_DEBUG("   CONTROLLER: %s\n", k.c_str());
		try {
			Machine::Controller *ctr =
				m->get_controller(k);

			switch(ctr->get_type()) {
			case Machine::Controller::c_sigid:
			case Machine::Controller::c_enum:
			case Machine::Controller::c_int:
			case Machine::Controller::c_float:
			case Machine::Controller::c_bool:
				add_scale(cnt,ctr);
				break;
			case Machine::Controller::c_string:
				break;
			default:
				break;
			}
		} catch(...) { /* ignore */ }
	}
}

std::string refresh_groups(Machine *m) {
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

void refresh_controllers(Machine *m, std::string first_group) {
	static KammoGUI::List *groups = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&groups, "cgroups");

	try {
		KammoGUI::List::iterator li = groups->get_selected();
		first_group = groups->get_value(li, 0);
	} catch(...) {
		// ignore
	}

	rebuild_controller_list(m , first_group);

}

Machine *current_machine = NULL;

virtual void on_select_row(KammoGUI::Widget *widget, KammoGUI::List::iterator row) {
	if(widget->get_id() == "cgroups") {
		std::string group_name = ((KammoGUI::List *)widget)->get_value(row, 0);
		SATAN_DEBUG("--- SELECTED CONTROLLER GROUP: %s\n", group_name.c_str());
		rebuild_controller_list(current_machine, group_name);
	}
}

virtual void on_user_event(KammoGUI::UserEvent *ue, std::map<std::string, void *> args) {
	void *data = NULL;

	if(args.find("machine_to_control") != args.end()) {
		char *m_name_c = (char *)args["machine_to_control"];
		string m_name = m_name_c;
		free(m_name_c);

		for(auto m : Machine::get_machine_set()) {
			if(m->get_name() == m_name) {
				data = m;
			}
		}
	}

	std::string first_group = "";

	if(ue->get_id() == "showControlsContainer") {
		if(data != NULL) {
			current_machine = (Machine *)data;
			first_group = refresh_groups(current_machine);
		}
	}

	if(current_machine != NULL) {
		refresh_controllers(current_machine, first_group);
	}
}

KammoEventHandler_Instance(ControllerHandler);
