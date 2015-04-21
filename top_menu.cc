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

TopMenu *TopMenu::top_menu = NULL;

TopMenu::TopMenu(CanvasWidgetContext *cwc) :
	CanvasWidget(cwc, 0.0, 0.0, 1.0, 1.0, 0xff), current_selection(selected_none), show_pulse(false), do_record(false) {

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
		std::string(CanvasWidgetContext::svg_directory + "/TopMenu-Compose.svg"));
	bt_compose_sel_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/TopMenu-Compose-Selected.svg"));
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
	__cnv->blit_SVGBob(Machine::get_record_state() ? bt_record_on : bt_record_off, x1 + 2 * w, y1);

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
	KammoGUI::get_widget((KammoGUI::Widget **)&compose_widget, "sequence_container");
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

	if(x < w) {
		// rewind
		SATAN_DEBUG("TopMenu::on_event()    - rewind\n");
		Machine::rewind();
	} else if(x < 2 * w) {
		// play
		SATAN_DEBUG("TopMenu::on_event()    - play\n");
		if(Machine::is_it_playing()) {
			Machine::stop();
		} else {
			Machine::play();
		}
	} else if(x < 3 * w) {
		// record
		SATAN_DEBUG("TopMenu::on_event()    - record\n");
		if(Machine::get_record_file_name() == "") {
			do_record = false;
			KammoGUI::display_notification(
				"Information",
				"Please save the project one time first...");
			return;
		}

		do_record = !do_record;

		if(do_record)
			KammoGUI::display_notification(
				"During playback..",
				std::string("The sound is written to: ") +
				Machine::get_record_file_name() + ".wav");
		
		Machine::set_record_state(do_record);
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
		KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showComposeContainer");
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

void TopMenu::run_play_pulse_marker(void *rowp) {
	int row = (int)rowp;

	if(!(row % Machine::get_lpb()))
		top_menu->show_pulse = true;
	else
		top_menu->show_pulse = false;
}

void TopMenu::sequence_row_playing_changed(int row) {
	KammoGUI::run_on_GUI_thread(run_play_pulse_marker, (void *)row);
}

void TopMenu::setup(KammoGUI::Canvas *cnvs) {
	float w_inch = KammoGUI::DisplayConfiguration::get_screen_width(KammoGUI::inches);

	float min_h = w_inch / 6.0f;
	min_h = min_h < 0.3f ? min_h : 0.3f;
	
	CanvasWidgetContext *cwc =
		new CanvasWidgetContext(cnvs, w_inch, min_h);
	cnvs->set_bg_color(0.9f, 0.9f, 0.9f);
	top_menu = new TopMenu(cwc);

	cwc->enable_events();

	Machine::register_periodic(sequence_row_playing_changed, 1);
}

/***************************
 *
 *  Kamoflage Event Declaration
 *
 ***************************/

KammoEventHandler_Declare(TopMenuHandler,"topMenu:modeTabs");

virtual void on_init(KammoGUI::Widget *wid) {
	if(wid->get_id() == "topMenu") {
		try {
			TopMenu::setup((KammoGUI::Canvas *)wid);
		} catch(...) {
			SATAN_DEBUG("Caught an exception in on_init() for \"TopMenu\" \n"); fflush(0);
		}
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
