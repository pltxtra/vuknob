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
#include <math.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "top_menu.hh"

#include "machine.hh"
#include "machine_sequencer.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/***************************
 *
 *  Class TopMenu
 *
 ***************************/

std::shared_ptr<TopMenu> TopMenu::top_menu;

TopMenu::TopMenu(bool _no_compose_mode, CanvasWidgetContext *cwc) :
	CanvasWidget(cwc, 0.0, 0.0, 1.0, 1.0, 0xff), current_selection(selected_none), no_compose_mode(_no_compose_mode),
	show_pulse(false) {

	bt_rewind_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/TopMenu-Rewind.svg"));
	bt_play_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/TopMenu-Play.svg"));
	bt_play_pulse_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/TopMenu-PlayPulse.svg"));
	bt_record_on_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/TopMenu-RecordOn.svg"));
	bt_record_off_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/TopMenu-RecordOff.svg"));

	bt_project_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/TopMenu-Project.svg"));
	bt_project_sel_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/TopMenu-Project-Selected.svg"));
	bt_compose_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + (no_compose_mode ? "/TopMenu-Connector.svg" : "/TopMenu-Compose.svg")
			));
	bt_compose_sel_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + (no_compose_mode ? "/TopMenu-Connector-Selected.svg" : "/TopMenu-Compose-Selected.svg")
			));
	bt_jam_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/TopMenu-Jam.svg"));
	bt_jam_sel_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/TopMenu-Jam-Selected.svg"));

	bt_rewind = new KammoGUI::Canvas::SVGBob(__cnv, bt_rewind_d);
	bt_play = new KammoGUI::Canvas::SVGBob(__cnv, bt_play_d);
	bt_play_pulse = new KammoGUI::Canvas::SVGBob(__cnv, bt_play_pulse_d);
	bt_record_on = new KammoGUI::Canvas::SVGBob(__cnv, bt_record_on_d);
	bt_record_off = new KammoGUI::Canvas::SVGBob(__cnv, bt_record_off_d);

	bt_project = new KammoGUI::Canvas::SVGBob(__cnv, bt_project_d);
	bt_project_sel = new KammoGUI::Canvas::SVGBob(__cnv, bt_project_sel_d);
	bt_compose = new KammoGUI::Canvas::SVGBob(__cnv, bt_compose_d);
	bt_compose_sel = new KammoGUI::Canvas::SVGBob(__cnv, bt_compose_sel_d);
	bt_jam = new KammoGUI::Canvas::SVGBob(__cnv, bt_jam_d);
	bt_jam_sel = new KammoGUI::Canvas::SVGBob(__cnv, bt_jam_sel_d);

}

void TopMenu::resized() {
	w = y2 - y1;

	bt_rewind->set_blit_size(w, w);
	bt_play->set_blit_size(w, w);
	bt_play_pulse->set_blit_size(w, w);
	bt_record_on->set_blit_size(w, w);
	bt_record_off->set_blit_size(w, w);

	bt_project->set_blit_size(w, w);
	bt_project_sel->set_blit_size(w, w);
	bt_compose->set_blit_size(w, w);
	bt_compose_sel->set_blit_size(w, w);
	bt_jam->set_blit_size(w, w);
	bt_jam_sel->set_blit_size(w, w);
}

void TopMenu::expose() {
	__cnv->blit_SVGBob(bt_rewind,				                     x1,         y1);
	__cnv->blit_SVGBob(show_pulse ? bt_play_pulse : bt_play,                     x1 + w,     y1);
	__cnv->blit_SVGBob(is_recording ? bt_record_on : bt_record_off, x1 + 2 * w, y1);

	__cnv->blit_SVGBob(current_selection == selected_project ? bt_project_sel : bt_project,
			   x2 - 3 * w, y1);
	__cnv->blit_SVGBob(current_selection == selected_compose ? bt_compose_sel : bt_compose,
			   x2 - 2 * w, y1);
	__cnv->blit_SVGBob(current_selection == selected_jam ? bt_jam_sel : bt_jam,
			   x2 - 1 * w, y1);
}

void TopMenu::new_view_enabled(KammoGUI::Widget *new_view) {
	static KammoGUI::Widget *project_widget = NULL;
	static KammoGUI::Widget *compose_widget = NULL;
	static KammoGUI::Widget *jam_widget = NULL;

	KammoGUI::get_widget((KammoGUI::Widget **)&project_widget, "projectContainer");
	if(top_menu->no_compose_mode) {
		KammoGUI::get_widget((KammoGUI::Widget **)&compose_widget, "connector_container");
	} else {
		KammoGUI::get_widget((KammoGUI::Widget **)&compose_widget, "sequence_container");
	}
	KammoGUI::get_widget((KammoGUI::Widget **)&jam_widget, "livePad2");

	if(new_view == project_widget) {
		top_menu->current_selection = selected_project;
	} else if(new_view == compose_widget) {
		top_menu->current_selection = selected_compose;
	} else if(new_view == jam_widget) {
		top_menu->current_selection = selected_jam;
	} else {
		top_menu->current_selection = selected_none;
	}
}

void TopMenu::on_event(KammoGUI::canvasEvent_t ce, int x, int y) {
	if(!(ce == KammoGUI::cvButtonRelease)) return;

	SATAN_DEBUG("TopMenu::on_event()\n");

	auto gco = RemoteInterface::GlobalControlObject::get_global_control_object();

	if(x < w) {
		// rewind
		SATAN_DEBUG("TopMenu::on_event()    - rewind\n");
		if(gco) gco->rewind();
	} else if(x < 2 * w) {
		// play
		SATAN_DEBUG("TopMenu::on_event()    - play\n");

		if(gco) {
			if(gco->is_it_playing()) {
				gco->stop();
			} else {
				gco->play();
			}
		}
	} else if(x < 3 * w) {
		// record
		std::string fname = "";
		if(gco) fname = gco->get_record_file_name();

		SATAN_DEBUG("TopMenu::on_event()    - record\n");
		if(fname == "") {
			is_recording = false;
			KammoGUI::display_notification(
				"Information",
				"Please save the project one time first...");
			return;
		}

		is_recording = !is_recording;

		if(is_recording)
			KammoGUI::display_notification(
				"During playback..",
				std::string("The sound is written to: ") +
				fname + ".wav");

		if(gco) gco->set_record_state(is_recording);
	} else if(x > (x2 - w)) {
		// jam
		static KammoGUI::UserEvent *ue = NULL;
		KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showLivePad2");
		if(ue != NULL) {
			std::map<std::string, void *> args;
			KammoGUI::EventHandler::trigger_user_event(ue, args);
		}
	} else if(x > (x2 - 2 * w)) {
		// compose
		static KammoGUI::UserEvent *ue = NULL;
		if(no_compose_mode) {
			KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showConnector_container");
		} else {
			KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showComposeContainer");
		}
		if(ue != NULL) {
			std::map<std::string, void *> args;
			KammoGUI::EventHandler::trigger_user_event(ue, args);
		}
	} else if(x > (x2 - 3 * w)) {
		// project
		static KammoGUI::UserEvent *ue = NULL;
		KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showProjectContainer");
		if(ue != NULL) {
			std::map<std::string, void *> args;
			KammoGUI::EventHandler::trigger_user_event(ue, args);
		}
	}
}

void TopMenu::setup(bool _no_compose_mode, KammoGUI::Canvas *cnvs) {
	float w_inch = KammoGUI::DisplayConfiguration::get_screen_width(KammoGUI::inches);

	float min_h = w_inch / 6.0f;
	min_h = min_h < 0.3f ? min_h : 0.3f;

	CanvasWidgetContext *cwc =
		new CanvasWidgetContext(cnvs, w_inch, min_h);
	cnvs->set_bg_color(0.9f, 0.9f, 0.9f);
	top_menu = std::make_shared<TopMenu>(_no_compose_mode, cwc);

	cwc->enable_events();

	RemoteInterface::GlobalControlObject::register_playback_state_listener(top_menu);
}

void TopMenu::playback_state_changed(bool _is_playing) {
	KammoGUI::run_on_GUI_thread(
		[this, _is_playing]() {
			is_playing = _is_playing;
			redraw();
		}
		);
}

void TopMenu::recording_state_changed(bool _is_recording) {
	KammoGUI::run_on_GUI_thread(
		[this, _is_recording]() {
			is_recording = _is_recording;
			redraw();
		}
		);
}

void TopMenu::periodic_playback_update(int row) {
	KammoGUI::run_on_GUI_thread(
		[this, row]() {
			if(auto gco = RemoteInterface::GlobalControlObject::get_global_control_object()) {
				auto lpb = gco->get_lpb();
				if(!(row % lpb))
					show_pulse = true;
				else
					show_pulse = false;
			}
		},
		false
		);
}

/***************************
 *
 *  Kamoflage Event Declaration
 *
 ***************************/

KammoEventHandler_Declare(TopMenuHandler,"topMenuNoCompose:topMenu:modeTabs");

virtual void on_init(KammoGUI::Widget *wid) {
	if(wid->get_id() == "topMenu") {
		TopMenu::setup(false, (KammoGUI::Canvas *)wid);
	} else if(wid->get_id() == "topMenuNoCompose") {
		TopMenu::setup(true, (KammoGUI::Canvas *)wid);
	}
}

virtual void on_value_changed(KammoGUI::Widget *wid) {
	if(wid->get_id() == "modeTabs") {
		KammoGUI::Tabs *t = (KammoGUI::Tabs *)wid;

		SATAN_DEBUG("Tab changed... %s\n", t->get_current_view()->get_id().c_str());
		TopMenu::new_view_enabled(t->get_current_view());
	}
}

KammoEventHandler_Instance(TopMenuHandler);
