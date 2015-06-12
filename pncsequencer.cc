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

#include "machine.hh"
#include "pncsequencer.hh"
#include "tracker.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

static float FLING_DEACCELERATION = 5000.0f;
static int HORIZONTAL_ZONES = 7;
static int VERTICAL_ZONES = 9;

/***************************
 *
 *  PncHelpDisplay
 *
 ***************************/

KammoGUI::Canvas::SVGDefinition *PncHelpDisplay::btn_help_one_d = NULL;
KammoGUI::Canvas::SVGDefinition *PncHelpDisplay::btn_help_two_d = NULL;
KammoGUI::Canvas::SVGDefinition *PncHelpDisplay::btn_help_three_d = NULL;
KammoGUI::Canvas::SVGBob *PncHelpDisplay::btn_help_one = NULL;
KammoGUI::Canvas::SVGBob *PncHelpDisplay::btn_help_two = NULL;
KammoGUI::Canvas::SVGBob *PncHelpDisplay::btn_help_three = NULL;

PncHelpDisplay::PncHelpDisplay(
	CanvasWidgetContext *cwc,
	float x1, float y1, float x2, float y2,
	PncHelpDisplay::HelpType ht
	) : CanvasWidget(cwc, x1, y1, x2, y2, 0x1),
	    help_type(ht)
	    {
	if(btn_help_one_d == NULL) {
		btn_help_one_d   = KammoGUI::Canvas::SVGDefinition::from_file(
			std::string(CanvasWidgetContext::svg_directory + "/pncseq_help_1.svg"));
		btn_help_two_d   = KammoGUI::Canvas::SVGDefinition::from_file(
			std::string(CanvasWidgetContext::svg_directory + "/pncseq_help_2.svg"));
		btn_help_three_d   = KammoGUI::Canvas::SVGDefinition::from_file(
			std::string(CanvasWidgetContext::svg_directory + "/pncseq_help_3.svg"));

		btn_help_one = new KammoGUI::Canvas::SVGBob(
			__cnv, btn_help_one_d);
		btn_help_two = new KammoGUI::Canvas::SVGBob(
			__cnv, btn_help_two_d);
		btn_help_three = new KammoGUI::Canvas::SVGBob(
			__cnv, btn_help_three_d);
	}
}

void PncHelpDisplay::resized() {
	btn_help_one->set_blit_size(x2-x1, y2-y1);
	btn_help_two->set_blit_size(x2-x1, y2-y1);
	btn_help_three->set_blit_size(x2-x1, y2-y1);
}

void PncHelpDisplay::expose() {
	switch(help_type) {
	case _no_help:
		break;

	case _first_help:
		__cnv->blit_SVGBob(btn_help_one, x1, y1);
		break;

	case _second_help:
		__cnv->blit_SVGBob(btn_help_two, x1, y1);
		break;

	case _third_help:
		__cnv->blit_SVGBob(btn_help_three, x1, y1);
		break;
	}
}

void PncHelpDisplay::on_event(KammoGUI::canvasEvent_t ce, int x, int y) {
	// ignore
}

void PncHelpDisplay::set_help_type(PncHelpDisplay::HelpType ht) {
	help_type = ht;
}

/***************************
 *
 *  PncLoopIndicator
 *
 ***************************/

KammoGUI::Canvas::SVGDefinition *PncLoopIndicator::btn_loop_no_d = NULL;
KammoGUI::Canvas::SVGDefinition *PncLoopIndicator::btn_loop_on_d = NULL;
KammoGUI::Canvas::SVGDefinition *PncLoopIndicator::btn_loop_on_b_d = NULL;
KammoGUI::Canvas::SVGDefinition *PncLoopIndicator::btn_loop_on_c_d = NULL;
KammoGUI::Canvas::SVGDefinition *PncLoopIndicator::btn_loop_on_e_d = NULL;
KammoGUI::Canvas::SVGDefinition *PncLoopIndicator::btn_loop_off_d = NULL;
KammoGUI::Canvas::SVGDefinition *PncLoopIndicator::btn_loop_off_b_d = NULL;
KammoGUI::Canvas::SVGDefinition *PncLoopIndicator::btn_loop_off_c_d = NULL;
KammoGUI::Canvas::SVGDefinition *PncLoopIndicator::btn_loop_off_e_d = NULL;
KammoGUI::Canvas::SVGBob *PncLoopIndicator::btn_loop_no = NULL;
KammoGUI::Canvas::SVGBob *PncLoopIndicator::btn_loop_on = NULL;
KammoGUI::Canvas::SVGBob *PncLoopIndicator::btn_loop_on_b = NULL;
KammoGUI::Canvas::SVGBob *PncLoopIndicator::btn_loop_on_c = NULL;
KammoGUI::Canvas::SVGBob *PncLoopIndicator::btn_loop_on_e = NULL;
KammoGUI::Canvas::SVGBob *PncLoopIndicator::btn_loop_off = NULL;
KammoGUI::Canvas::SVGBob *PncLoopIndicator::btn_loop_off_b = NULL;
KammoGUI::Canvas::SVGBob *PncLoopIndicator::btn_loop_off_c = NULL;
KammoGUI::Canvas::SVGBob *PncLoopIndicator::btn_loop_off_e = NULL;

bool PncLoopIndicator::loop_is_on = true;

PncLoopIndicator::PncLoopIndicator(
	CanvasWidgetContext *cwc,
	float _x1, float _y1, float _x2, float _y2,
	const std::string &_value, int on_layers,
	RowNumberColumn *_parent,
	void (*_parent_callback)(void *cbd, PncLoopIndicator *pli, KammoGUI::canvasEvent_t ce, int x, int y)
	) : CanvasWidget(cwc, _x1, _y1, _x2, _y2, on_layers),
	    parent(_parent), parent_callback(_parent_callback),
	    state(_no_), value(_value) {
	SATAN_DEBUG("   LoopIndicator: %f, %f -> %f\n", _x1, _x2, _x2 - _x1);
	if(btn_loop_off_d == NULL) {
			btn_loop_no_d   = KammoGUI::Canvas::SVGDefinition::from_file(
				std::string(CanvasWidgetContext::svg_directory + "/loop_no.svg"));

			btn_loop_on_d   = KammoGUI::Canvas::SVGDefinition::from_file(
				std::string(CanvasWidgetContext::svg_directory + "/loop_on.svg"));
			btn_loop_on_b_d   = KammoGUI::Canvas::SVGDefinition::from_file(
				std::string(CanvasWidgetContext::svg_directory + "/loop_on_begin.svg"));
			btn_loop_on_c_d   = KammoGUI::Canvas::SVGDefinition::from_file(
				std::string(CanvasWidgetContext::svg_directory + "/loop_on_continue.svg"));
			btn_loop_on_e_d   = KammoGUI::Canvas::SVGDefinition::from_file(
				std::string(CanvasWidgetContext::svg_directory + "/loop_on_end.svg"));

			btn_loop_off_d   = KammoGUI::Canvas::SVGDefinition::from_file(
				std::string(CanvasWidgetContext::svg_directory + "/loop_off.svg"));
			btn_loop_off_b_d   = KammoGUI::Canvas::SVGDefinition::from_file(
				std::string(CanvasWidgetContext::svg_directory + "/loop_off_begin.svg"));
			btn_loop_off_c_d   = KammoGUI::Canvas::SVGDefinition::from_file(
				std::string(CanvasWidgetContext::svg_directory + "/loop_off_continue.svg"));
			btn_loop_off_e_d   = KammoGUI::Canvas::SVGDefinition::from_file(
				std::string(CanvasWidgetContext::svg_directory + "/loop_off_end.svg"));

			btn_loop_no = new KammoGUI::Canvas::SVGBob(
				__cnv, btn_loop_no_d);

			btn_loop_on = new KammoGUI::Canvas::SVGBob(
				__cnv, btn_loop_on_d);
			btn_loop_on_b = new KammoGUI::Canvas::SVGBob(
				__cnv, btn_loop_on_b_d);
			btn_loop_on_c = new KammoGUI::Canvas::SVGBob(
				__cnv, btn_loop_on_c_d);
			btn_loop_on_e = new KammoGUI::Canvas::SVGBob(
				__cnv, btn_loop_on_e_d);
			btn_loop_off = new KammoGUI::Canvas::SVGBob(
				__cnv, btn_loop_off_d);
			btn_loop_off_b = new KammoGUI::Canvas::SVGBob(
				__cnv, btn_loop_off_b_d);
			btn_loop_off_c = new KammoGUI::Canvas::SVGBob(
				__cnv, btn_loop_off_c_d);
			btn_loop_off_e = new KammoGUI::Canvas::SVGBob(
				__cnv, btn_loop_off_e_d);
	}
}

void PncLoopIndicator::resized() {
	SATAN_DEBUG("loop indicator (%d, %d - %d)\n", x1, x2, x2 - x1);
	btn_loop_no->set_blit_size(x2-x1, y2-y1);
	btn_loop_on->set_blit_size(x2-x1, y2-y1);
	btn_loop_on_b->set_blit_size(x2-x1, y2-y1);
	btn_loop_on_c->set_blit_size(x2-x1, y2-y1);
	btn_loop_on_e->set_blit_size(x2-x1, y2-y1);
	btn_loop_off->set_blit_size(x2-x1, y2-y1);
	btn_loop_off_b->set_blit_size(x2-x1, y2-y1);
	btn_loop_off_c->set_blit_size(x2-x1, y2-y1);
	btn_loop_off_e->set_blit_size(x2-x1, y2-y1);
}

void PncLoopIndicator::expose() {
	switch(state) {
	case _on_:
		if(loop_is_on)
			__cnv->blit_SVGBob(btn_loop_on, x1, y1);
		else
			__cnv->blit_SVGBob(btn_loop_off, x1, y1);
		break;
	case _on_b_:
		if(loop_is_on)
			__cnv->blit_SVGBob(btn_loop_on_b, x1, y1);
		else
			__cnv->blit_SVGBob(btn_loop_off_b, x1, y1);
		break;
	case _on_c_:
		if(loop_is_on)
			__cnv->blit_SVGBob(btn_loop_on_c, x1, y1);
		else
			__cnv->blit_SVGBob(btn_loop_off_c, x1, y1);
		break;
	case _on_e_:
		if(loop_is_on)
			__cnv->blit_SVGBob(btn_loop_on_e, x1, y1);
		else
			__cnv->blit_SVGBob(btn_loop_off_e, x1, y1);
		break;
	case _no_:
		__cnv->blit_SVGBob(btn_loop_no, x1, y1);
		break;
	}

	static char symbolz[32];
	snprintf(symbolz, 32, "      %2s", value.c_str());
	context->draw_symbols(x1, y1, x2, y2, 5, 3, symbolz);
}

void PncLoopIndicator::on_event(KammoGUI::canvasEvent_t ce, int x, int y) {
	if(parent != NULL && parent_callback != NULL)
		parent_callback(parent, this, ce, x, y);
}

PncLoopIndicator::LoopState PncLoopIndicator::get_state() {
	return state;
}

void PncLoopIndicator::set_state(LoopState nstate) {
	state = nstate;
}

void PncLoopIndicator::set_value(const std::string &_value) {
	value = _value;
}

void PncLoopIndicator::set_loop_on(bool _do_loop) {
	loop_is_on = _do_loop;
}

/***************************
 *
 *  Class PncSeqZone
 *
 ***************************/

KammoGUI::Canvas::SVGDefinition
	*PncSeqZone::btn_selected_l_d = NULL,
	*PncSeqZone::btn_selected_r_d = NULL,
	*PncSeqZone::btn_unselected_d = NULL,
	*PncSeqZone::btn_mute_on_d = NULL,
	*PncSeqZone::btn_mute_off_d = NULL,
	*PncSeqZone::btn_hidden_d = NULL,
	*PncSeqZone::btn_shade_d = NULL,
	*PncSeqZone::btn_plus_d = NULL;

KammoGUI::Canvas::SVGBob
	*PncSeqZone::btn_selected_l = NULL,
	*PncSeqZone::btn_selected_r = NULL,
	*PncSeqZone::btn_unselected = NULL,
	*PncSeqZone::btn_mute_on = NULL,
	*PncSeqZone::btn_mute_off = NULL,
	*PncSeqZone::btn_hidden = NULL,
	*PncSeqZone::btn_shade = NULL,
	*PncSeqZone::btn_plus = NULL;

PncSeqZone::PncSeqZone(
	CanvasWidgetContext *cwc,
	float _x1,
	float _y1,
	float _x2,
	float _y2,
	bool _show_mute, bool _show_value, bool _only_value,
	std::string _value, int on_layers) :
	CanvasWidget(cwc, _x1, _y1, _x2, _y2, on_layers),
	mode(normalZone), only_value(_only_value),
	show_mute(_show_mute),
	show_value(_show_value), do_shade(false), value(_value), selected(false), focus_left(false),
	on_event_cb(NULL), state_change(NULL), cbd(NULL) {

	if(btn_selected_l_d == NULL) {
		btn_selected_l_d   = KammoGUI::Canvas::SVGDefinition::from_file(
			std::string(CanvasWidgetContext::svg_directory + "/note_selected_left.svg"));
		btn_selected_r_d   = KammoGUI::Canvas::SVGDefinition::from_file(
			std::string(CanvasWidgetContext::svg_directory + "/note_selected_right.svg"));
		btn_unselected_d = KammoGUI::Canvas::SVGDefinition::from_file(
			std::string(CanvasWidgetContext::svg_directory + "/note_off.svg"));
		btn_mute_on_d = KammoGUI::Canvas::SVGDefinition::from_file(
			std::string(CanvasWidgetContext::svg_directory + "/note_hold_off.svg"));
		btn_mute_off_d = KammoGUI::Canvas::SVGDefinition::from_file(
			std::string(CanvasWidgetContext::svg_directory + "/note_hold_on.svg"));

		btn_hidden_d = KammoGUI::Canvas::SVGDefinition::from_file(
			std::string(CanvasWidgetContext::svg_directory + "/background.svg"));
		btn_shade_d = KammoGUI::Canvas::SVGDefinition::from_file(
			std::string(CanvasWidgetContext::svg_directory + "/note_shade.svg"));

		btn_plus_d = KammoGUI::Canvas::SVGDefinition::from_file(
			std::string(CanvasWidgetContext::svg_directory + "/plus.svg"));

		btn_selected_l = new KammoGUI::Canvas::SVGBob(
			__cnv, btn_selected_l_d);
		btn_selected_r = new KammoGUI::Canvas::SVGBob(
			__cnv, btn_selected_r_d);
		btn_unselected = new KammoGUI::Canvas::SVGBob(
			__cnv, btn_unselected_d);
		btn_mute_on = new KammoGUI::Canvas::SVGBob(
			__cnv, btn_mute_on_d);
		btn_mute_off = new KammoGUI::Canvas::SVGBob(
			__cnv, btn_mute_off_d);

		btn_hidden = new KammoGUI::Canvas::SVGBob(
			__cnv, btn_hidden_d);
		btn_shade = new KammoGUI::Canvas::SVGBob(
			__cnv, btn_shade_d);
		btn_plus = new KammoGUI::Canvas::SVGBob(
			__cnv, btn_plus_d);


	}
}

void PncSeqZone::set_callback_data(
	void *_cbd,
	void (*_state_change)(void *cbd, PncSeqZone *pnb, bool selected, bool focus_left),
	bool (*_on_event_cb)(void *cbd, PncSeqZone *pnb, KammoGUI::canvasEvent_t ce, int x, int y)
	) {
	cbd = _cbd;
	state_change = _state_change;
	on_event_cb = _on_event_cb;
}

void PncSeqZone::set_value(const std::string &_value) {
	value = _value;
}

void PncSeqZone::set_selected(bool _selected, bool _focus_left) {
	selected = _selected;
	focus_left = _focus_left;
	if(!selected) {
		fflush(0);
	}
}

bool PncSeqZone::get_selected() {
	return selected;
}

bool PncSeqZone::get_focus_left() {
	return focus_left;
}

void PncSeqZone::set_mode(zoneMode _mode) {
	mode = _mode;
}

void PncSeqZone::set_shade(bool shade) {
	if(shade != do_shade) {
		do_shade = shade;
	}
}

void PncSeqZone::resized() {
	btn_hidden->set_blit_size(x2-x1, y2-y1);
	btn_shade->set_blit_size(x2-x1, y2-y1);
	btn_plus->set_blit_size(x2-x1, y2-y1);
	if(show_mute) {
		btn_mute_on->set_blit_size(x2-x1, y2-y1);
		btn_mute_off->set_blit_size(x2-x1, y2-y1);
	} else {
		btn_selected_l->set_blit_size(x2-x1, y2-y1);
		btn_selected_r->set_blit_size(x2-x1, y2-y1);
		btn_unselected->set_blit_size(x2-x1, y2-y1);
	}
}

void PncSeqZone::expose() {
	SATAN_DEBUG("PncSeqZone Expose: %d, %d -> %d, %d\n", x1, y1, x2, y2);
	if(mode != normalZone) {
		if(mode == hiddenZone) {
			__cnv->blit_SVGBob(btn_hidden,
					   x1, y1);
		} else {
			__cnv->blit_SVGBob(btn_plus,
					   x1, y1);
		}
		return;
	}

	if(show_mute)
		__cnv->blit_SVGBob(selected ? btn_mute_on : btn_mute_off,
			x1, y1);
	else if(!only_value) {
		__cnv->blit_SVGBob(btn_unselected, x1, y1);
		if(selected)
			__cnv->blit_SVGBob((focus_left ? btn_selected_l : btn_selected_r),
					   x1, y1);
	}

	if(do_shade) {
		__cnv->blit_SVGBob(btn_shade,x1, y1);
	}

	if(show_value || selected) {
		static char symbolz[32];
		snprintf(symbolz, 32, "      %2s", value.c_str());
		context->draw_symbols(x1, y1, x2, y2, 5, 3, symbolz);
	}
}

void PncSeqZone::on_event(KammoGUI::canvasEvent_t ce, int x, int y) {
	if(mode == hiddenZone) return;
	if(mode == addnewZone) {
		// special shortcut case...
		// show machine type selector
		static KammoGUI::UserEvent *ue = NULL;
		KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showMachineTypeListScroller");
		if(ue != NULL) {
			static KammoGUI::UserEvent *callback_ue = NULL;
			KammoGUI::get_widget((KammoGUI::Widget **)&callback_ue, "showComposeContainer");

			std::map<std::string, void *> args;
			args["hint_match"] = strdup("generator");
			args["callback_event"] = callback_ue;
			KammoGUI::EventHandler::trigger_user_event(ue, args);
		}
		return;
	}

	bool internal_events = true;


	if(on_event_cb != NULL) {
		internal_events = on_event_cb(cbd, this, ce, x, y);
	}
	if((!only_value) && internal_events && ce == KammoGUI::cvButtonRelease) {
		if(show_mute) {
			selected = !selected;
		} else {
			if((x - x1) < ((x2 - x1) / 2)) focus_left = true;
			else focus_left = false;

			selected = true;
		}

		if(state_change) state_change(cbd, this, selected, focus_left);
	}
}

/***************************
 *
 *  class PncSequence
 *
 ***************************/

PncSequence::PncSequence(
	CanvasWidgetContext *cwc,
	int column_nr, int visible_rows,
	void (*_horizontal_scroll_cb)(void *cbd, int valu),
	void (*_vertical_scroll_cb)(void *cbd, int valu),
	void (*_zone_selected_cb)(void *cbd, PncSequence *pncs, int row, bool foc_left)
	) :
	context(cwc),
	mseq(NULL),
	scroll_start_w(0), scroll_start_h(0), scrolling(false),
	horizontal_scroll_cb(_horizontal_scroll_cb),
	vertical_scroll_cb(_vertical_scroll_cb),
	zone_selected_cb(_zone_selected_cb) {

	int k;
	float w = 1.0 / (HORIZONTAL_ZONES);
	float h = 1.0 / (VERTICAL_ZONES);

	// seq_number_zone shows the current sequence number of this PncSequence column
	{
		seq_number_zone =
			new PncSeqZone(
				cwc,
				0.0, (float)column_nr * h,
				w, (float)column_nr * h + h,
				true, true, false, "", 0x01);
		seq_number_zone->set_callback_data(this, zone_state_change, zone_event);
		seq_number_zone->set_mode(hiddenZone);
	}
	// rest of the sequence zones
	for(k = 0; k < visible_rows; k++) {
		PncSeqZone *psz =
			new PncSeqZone(
				cwc,
				(k + 1) * w, (float)column_nr * h,
				(2 + k) * w, (float)column_nr * h + h,
				false, true, false, "", 0x01);
		psz->set_callback_data(this, zone_state_change, zone_event);
		psz->set_mode(hiddenZone);
		row[k] = psz;
	}

}

PncSequenceFlingAnimation::PncSequenceFlingAnimation(
	float speed, float _zone_size, float _duration, void (*_scrolled)(void *__cbd, int rel), void *_cbd) :
	KammoGUI::Animation(_duration) {

	scrolled = _scrolled;
	cbd = _cbd;
	last_time = 0.0f;
	total_pixels_changed = 0.0f;
	zone_size = _zone_size;
	start_speed = speed;
	duration = _duration;

	if(start_speed < 0)
		deacc = -FLING_DEACCELERATION;
	else
		deacc = FLING_DEACCELERATION;

	SATAN_DEBUG("start speed: %f, duration: %f, deacc: %f\n",
		    start_speed, duration, deacc);
}

void PncSequenceFlingAnimation::new_frame(float progress) {
	current_speed = start_speed - deacc * progress * duration;

	SATAN_DEBUG("new_frame, current speed: %f\n", current_speed);
	float time_diff = (progress * duration) - last_time;
	float pixels_change = time_diff * current_speed;

	total_pixels_changed += pixels_change;

	float diff_f = total_pixels_changed / zone_size;
	int diff = diff_f;
	if(diff != 0 && scrolled != NULL) {
		total_pixels_changed = 0.0f;
		scrolled(cbd, diff);
	}

	last_time = progress * duration;
}

void PncSequenceFlingAnimation::on_touch_event() {
	stop();
}


bool PncSequence::zone_event(void *cbd, PncSeqZone *psz, KammoGUI::canvasEvent_t ce, int x, int y) {
	PncSequence *pnbc = (PncSequence *)cbd;

	KammoGUI::SVGCanvas::MotionEvent newStyle_event;
	switch(ce) {
	case KammoGUI::cvButtonPress:
		newStyle_event.init(0, 0, KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN, 1, 0, x, y);
		newStyle_event.init_pointer(0, 0, x, y, 1.0);
		break;
	case KammoGUI::cvButtonRelease:
		newStyle_event.init(0, 0, KammoGUI::SVGCanvas::MotionEvent::ACTION_UP, 1, 0, x, y);
		newStyle_event.init_pointer(0, 0, x, y, 1.0);
		break;
	case KammoGUI::cvButtonHold:
	case KammoGUI::cvMotion:
		newStyle_event.init(0, 0, KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE, 1, 0, x, y);
		newStyle_event.init_pointer(0, 0, x, y, 1.0);
		break;
	}

	if(pnbc->fling_detector.on_touch_event(newStyle_event)) {
		float speed_x, speed_y;
		float abs_speed_x, abs_speed_y;

		pnbc->fling_detector.get_speed(speed_x, speed_y);
		pnbc->fling_detector.get_absolute_speed(abs_speed_x, abs_speed_y);

		bool do_horizontal_fling = abs_speed_x > abs_speed_y ? true : false;

		float speed = 0.0f, abs_speed;
		float zone_size = 10.0f;
		void (*scrolled)(void *__cbd, int rel) = NULL;

		if(do_horizontal_fling) {
			abs_speed = abs_speed_x;
			speed = -speed_x; // horizontal scroll speed is in the oposite direction of the fling motion..
			zone_size = pnbc->context->__width / (HORIZONTAL_ZONES);
			scrolled = pnbc->horizontal_scroll_cb;
		} else {
			abs_speed = abs_speed_y;
			speed = -speed_y; // vertical scroll speed is in the oposite direction of the fling motion..
			zone_size = pnbc->context->__height / (VERTICAL_ZONES);
			scrolled = pnbc->vertical_scroll_cb;
		}

		float fling_duration = abs_speed / FLING_DEACCELERATION;

		PncSequenceFlingAnimation *flinganim =
			new PncSequenceFlingAnimation(speed, zone_size, fling_duration, scrolled, cbd);
		pnbc->context->__cnv->start_animation(flinganim);
	}

	if(ce == KammoGUI::cvButtonPress) {
		pnbc->scroll_start_w = x;
		pnbc->scroll_start_h = y;
		pnbc->scrolling = true;
		pnbc->did_scroll = false;
	}
	if(ce == KammoGUI::cvButtonHold) {
		pnbc->scrolling = false;

		if(!(pnbc->did_scroll)) {
			int i = PncSequencer::sequence_row_offset;
			int step = PncSequencer::sequence_step;

			std::map<int, PncSeqZone *>::iterator k;
			for(k = pnbc->row.begin(); k != pnbc->row.end() && i < MAX_SEQUENCE_LENGTH; k++, i += step) {
				int val = pnbc->mseq->get_loop_id_at(i);
				if((*k).second == psz) {
					Tracker::show_tracker_for(pnbc->mseq, val);
				}
			}

			return false;
		}
	}
	if(ce == KammoGUI::cvButtonRelease) {
		pnbc->scrolling = false;
		if(!(pnbc->did_scroll)) return true;
	}
	if(ce == KammoGUI::cvMotion) {
		// first check horizontal scroll
		{
			float w = pnbc->context->__width / (HORIZONTAL_ZONES);
			float diff_f = ((float)pnbc->scroll_start_w - x) / w;
			int diff = diff_f;

			if(diff != 0) {
				pnbc->did_scroll = true;
				pnbc->scroll_start_w = x;

				if(pnbc->horizontal_scroll_cb != NULL)
					pnbc->horizontal_scroll_cb(cbd, diff);
			}
		}
		// then check vertical scroll
		{
			float h = pnbc->context->__height / (VERTICAL_ZONES);
			float diff_f = ((float)pnbc->scroll_start_h - y) / h;
			int diff = diff_f;

			if(diff != 0) {
				pnbc->did_scroll = true;
				pnbc->scroll_start_h = y;

				if(pnbc->vertical_scroll_cb != NULL)
					pnbc->vertical_scroll_cb(cbd, diff);
			}
		}
	}
	return false;
}

void PncSequence::zone_state_change(
	void *cbd, PncSeqZone *szone, bool selected, bool focus_left) {
	PncSequence *pnbc = (PncSequence *)cbd;
	if(pnbc->zone_selected_cb != NULL) {
		int row = 0;
		if(szone == pnbc->seq_number_zone) {
			pnbc->mseq->set_mute(selected);
		} else {
			std::map<int, PncSeqZone *>::iterator k;
			for(k = pnbc->row.begin(); k != pnbc->row.end(); k++) {
				if((*k).second == szone) {
					row = (*k).first;
				}
			}
			pnbc->zone_selected_cb(cbd, pnbc, row, focus_left);
		}
	}
}

void PncSequence::set_current_playing_row(int r) {
	std::map<int, PncSeqZone *>::iterator i;

	r -= PncSequencer::sequence_row_offset;
	r /= PncSequencer::sequence_step;

	int k = 0;
	for(i = row.begin(); i != row.end(); i++, k++) {
		if(r == k)
			(*i).second->set_shade(true);
		else
			(*i).second->set_shade(false);
	}
}

void PncSequence::set_mode(zoneMode mode) {
	bool hide = true;

	if(mode == normalZone) hide = false;

	std::map<int, PncSeqZone *>::iterator i;

	seq_number_zone->set_mode(mode);

	for(i = row.begin(); i != row.end(); i++) {
		(*i).second->set_mode(hide ? hiddenZone : normalZone);
	}
}

void PncSequence::set_machine_sequencer(const std::string &title, MachineSequencer *_mseq) {
	mseq = _mseq;

	if(mseq == NULL) {
		set_mode(hiddenZone);
	} else {
		seq_number_zone->set_mode(normalZone);
		seq_number_zone->set_value(title.substr(2, 4));
		seq_number_zone->set_selected(mseq->is_mute(), false);

		int i, j;
		int step = PncSequencer::sequence_step;

		int position_vector[row.size()];
		int position_vector_length = row.size();
		std::map<int, PncSeqZone *>::iterator k;

		/******* STEP 1 - fill in the vector with proper indexes *****/
		for(k = row.begin(), i = PncSequencer::sequence_row_offset, j = 0;
		    k != row.end();
		    k++, i += step, j++) {
			position_vector[j] = i;
		}

		/******* STEP 2 - Get the loop ids using the position vector *******/
		mseq->get_loop_ids_at(position_vector, position_vector_length);

		/******* STEP 3 - update the PncSeqZone fields using the vector ****/
		for(k = row.begin(), j = 0; k != row.end(); k++, j++) {
			char bfr[5];
			int val = position_vector[j];

			if(val == NOTE_NOT_SET) {
				(*k).second->set_value("");
			} else {
				snprintf(bfr, 5, "%01d %01d", val / 10, val % 10);
				(*k).second->set_value(bfr);
			}
			(*k).second->set_mode(normalZone);
		}
	}
}

bool PncSequence::is_selected() {
	std::map<int, PncSeqZone *>::iterator k;
	for(k = row.begin(); k != row.end(); k++) {
		if((*k).second->get_selected()) return true;
	}
	return false;
}

void PncSequence::unselect_row(int row_id) {
	row[row_id]->set_selected(false, false);
}

void PncSequence::select_row(int row_id, bool sel_hi) {
	row[row_id]->set_selected(true, sel_hi);
}

void PncSequence::show_tracker() {
	if(mseq) {
		int i = PncSequencer::sequence_row_offset;
		int step = PncSequencer::sequence_step;
		int val = -1; // this is "no entry" - look in machine_sequencer.hh
		std::map<int, PncSeqZone *>::iterator k;

		for(k = row.begin(); k != row.end() && i < MAX_SEQUENCE_LENGTH; k++, i += step) {
			if((*k).second->get_selected())
				val = mseq->get_loop_id_at(i);
		}

		Tracker::show_tracker_for(mseq, val);
	}
}

void PncSequence::show_controls() {
	// show the ControlsContainer
	static KammoGUI::UserEvent *ue = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showControlsContainer");
	if(ue != NULL && mseq) {
		Machine *node_to_control = mseq->get_machine();

		if(node_to_control) {
			auto name = strdup(node_to_control->get_name().c_str());
			if(name) {
				// show the ControlsContainer
				static KammoGUI::UserEvent *ue = NULL;
				KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showControlsContainer");
				if(ue != NULL) {
					std::map<std::string, void *> args;
					args["machine_to_control"] = name;
					KammoGUI::EventHandler::trigger_user_event(ue, args);
				} else {
					free(name);
				}
			}
		}
	}
}

void PncSequence::show_envelopes() {
	// show the ControlsContainer
	static KammoGUI::UserEvent *ue = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showControllerEnvelope");
	if(ue != NULL && mseq) {
		std::map<std::string, void *> args;
		args["MachineSequencer"] = mseq; //node_to_control;
		KammoGUI::EventHandler::trigger_user_event(ue, args);
	}
}

/***************************
 *
 *  RowNumberColumn class
 *
 ***************************/

void RowNumberColumn::loop_indicator_event(
	void *cbd, PncLoopIndicator *pli, KammoGUI::canvasEvent_t ce, int x, int y) {
	RowNumberColumn *rnc = (RowNumberColumn *)cbd;

	float w = rnc->context->__width / (HORIZONTAL_ZONES);

	switch(ce) {

	case KammoGUI::cvButtonPress:
		SATAN_DEBUG("RNC button press. Just do tap = true\n");
		rnc->just_do_tap = true;
		rnc->motion_start_x = x;

		switch(pli->get_state()) {
		case PncLoopIndicator::_on_b_:
			rnc->loop_motion = _set_start_;
			break;

		case PncLoopIndicator::_on_c_:
			rnc->loop_motion = _move_loop_;
			break;

		case PncLoopIndicator::_on_e_:
		case PncLoopIndicator::_on_:
			rnc->loop_motion = _set_length_;
			break;

		case PncLoopIndicator::_no_:
		default:
			rnc->loop_motion = _ignore_motion_;
			break;

		}

		break;

	case KammoGUI::cvButtonHold:
	{
		float diff_f = rnc->motion_start_x / w;
		int diff = diff_f;
		SATAN_DEBUG("motion_x: %d, diff_f %f, diff %d\n",
			    rnc->motion_start_x, diff_f, diff);
		diff = PncSequencer::sequence_row_offset +
			(diff - 1) * PncSequencer::sequence_step;
		SATAN_DEBUG("will try and jump to %d\n", diff);
		Machine::jump_to(diff);
	}
		break;

	case KammoGUI::cvButtonRelease:
		SATAN_DEBUG("RNC button release. Just do tap? = %s\n", rnc->just_do_tap ? "true" : "false");
		if(rnc->just_do_tap) {
			if(pli->get_state() != PncLoopIndicator::_no_) {
				bool do_loop = Machine::get_loop_state();
				Machine::set_loop_state(do_loop ? false : true);
			} else {
				int loop_start = Machine::get_loop_start();
				int loop_length = Machine::get_loop_length();
				float diff_f = rnc->motion_start_x / w;
				int diff = diff_f;
				loop_start = PncSequencer::sequence_row_offset +
					(diff - 1) * PncSequencer::sequence_step;
				Machine::set_loop_start(loop_start);
				Machine::set_loop_length(loop_length);
			}
			rnc->refresh();
		} else {
		}
		break;

	case KammoGUI::cvMotion:
		SATAN_DEBUG("RNC motion. Just do tap = false\n");

		float diff_f = ((float)x - (float)rnc->motion_start_x) / w;
		int diff = diff_f;

		if(diff != 0) {
			rnc->just_do_tap = false;

			int loop_start = Machine::get_loop_start();
			int loop_length = Machine::get_loop_length();

			rnc->motion_start_x = x;

			switch(rnc->loop_motion) {
			case _move_loop_:
				loop_start += (diff * PncSequencer::sequence_step);
				if(loop_start >= 0) {
					// force loop into multiple of sequence_step
					loop_start /= PncSequencer::sequence_step;
					loop_start *= PncSequencer::sequence_step;

					loop_length /= PncSequencer::sequence_step;
					loop_length *= PncSequencer::sequence_step;

					Machine::set_loop_start(loop_start);
					Machine::set_loop_length(loop_length);
					rnc->refresh();
				}
				break;

			case _set_start_:
				loop_start += (diff * PncSequencer::sequence_step);
				loop_length -= (diff * PncSequencer::sequence_step);
				if(loop_start >= 0 && loop_length > 2) {
					// force loop into multiple of sequence_step
					loop_start /= PncSequencer::sequence_step;
					loop_start *= PncSequencer::sequence_step;

					loop_length /= PncSequencer::sequence_step;
					loop_length *= PncSequencer::sequence_step;

					Machine::set_loop_start(loop_start);
					Machine::set_loop_length(loop_length);
					rnc->refresh();
				}
				break;

			case _set_length_:
				loop_length += (diff * PncSequencer::sequence_step);
				if(loop_length > 2) {
					// force loop into multiple of sequence_step
					loop_start /= PncSequencer::sequence_step;
					loop_start *= PncSequencer::sequence_step;

					loop_length /= PncSequencer::sequence_step;
					loop_length *= PncSequencer::sequence_step;

					Machine::set_loop_start(loop_start);
					Machine::set_loop_length(loop_length);
					rnc->refresh();
				}
				break;

			case _ignore_motion_:
			default:
				break;
			}
		}
		break;

	}

}

RowNumberColumn::RowNumberColumn(
	CanvasWidgetContext *cwc,
	int _visible_rows
	)  :
	context(cwc),
	visible_rows(_visible_rows)
{

	int k;
	float w = 1.0 / (HORIZONTAL_ZONES);
	float h = 1.0 / (VERTICAL_ZONES);

	// rest of the sequence zones
	for(k = 0; k < visible_rows; k++) {
		PncLoopIndicator *pli =
			new PncLoopIndicator(
				cwc,
				(float)(k + 1) * w, (VERTICAL_ZONES - 1.0) * h,
				(float)(2 + k) * w, (VERTICAL_ZONES) * h,
				"-",
				0x01,
				this,
				loop_indicator_event);
		row[k] = pli;
	}

	refresh();
}

void RowNumberColumn::refresh() {
	std::map<int, PncLoopIndicator *>::iterator k;

	int i;

	int loop_start, loop_stop;
	bool do_loop;

	do_loop = Machine::get_loop_state();
	loop_start = Machine::get_loop_start();
	loop_stop = loop_start + Machine::get_loop_length();

	PncLoopIndicator::set_loop_on(do_loop);

	for(k = row.begin(), i = PncSequencer::sequence_row_offset;
	    k != row.end();
	    k++, i += PncSequencer::sequence_step) {
		char bfr[5];
		snprintf(bfr, 5, "%03d", i);

		if(loop_start >= i && loop_start < i + PncSequencer::sequence_step) {
			if(loop_stop <= i + PncSequencer::sequence_step) {
				(*k).second->set_state(PncLoopIndicator::_on_);
			} else {
				(*k).second->set_state(PncLoopIndicator::_on_b_);
			}
		} else if(loop_start < i) {
			if(loop_stop > i && loop_stop <= i + PncSequencer::sequence_step) {
				(*k).second->set_state(PncLoopIndicator::_on_e_);
			} else if(loop_stop > i + PncSequencer::sequence_step) {
				(*k).second->set_state(PncLoopIndicator::_on_c_);
			} else {
				(*k).second->set_state(PncLoopIndicator::_no_);
			}
		} else {
			(*k).second->set_state(PncLoopIndicator::_no_);
		}

		(*k).second->set_value(bfr);
	}
}

/***************************
 *
 *  PncSequencer main class stuff
 *
 ***************************/

PncSequencer::SequencerMode PncSequencer::sequencer_mode = PncSequencer::no_selection;
PncHelpDisplay *PncSequencer::phd = NULL;
PncSeqZone *PncSequencer::stz = NULL;
RowNumberColumn *PncSequencer::rnrcol = NULL;
std::vector<PncSequence *> PncSequencer::seq;
std::map<std::string, MachineSequencer *, ltstr> PncSequencer::mseqs;
int PncSequencer::sequence_row_offset, PncSequencer::sequence_step = 16;
int PncSequencer::sequence_offset;
bool PncSequencer::sel_seq_step = false;
int PncSequencer::sel_seq, PncSequencer::sel_row;
bool PncSequencer::sel_hi;
CanvasWidgetContext *PncSequencer::context;

void PncSequencer::vertical_scroll(void *cbd, int valu) {
	int old_of = sequence_offset;
	sequence_offset += valu;
	if(sequence_offset < 0) sequence_offset = 0;
	if(sequence_offset + 1 > (int)mseqs.size()) sequence_offset = (int)mseqs.size() - 1;

	int diff = sel_seq + (old_of - sequence_offset);
	if(diff < 0) diff = 0;
	if(diff >= (int)seq.size()) diff = seq.size() - 1;

	if(diff != sel_seq) {
		seq[diff]->select_row(sel_row, sel_hi);
		zone_selected(seq[diff], seq[diff], sel_row, sel_hi);
	}

	refresh_sequencers();
	rnrcol->refresh();
}

void PncSequencer::horizontal_scroll(void *cbd, int valu) {
	valu *= sequence_step;

	int old_of = sequence_row_offset;
	sequence_row_offset += valu;
	if(sequence_row_offset < 0) sequence_row_offset = 0;
	if(sequence_row_offset + 1 > MAX_SEQUENCE_LENGTH)
		sequence_row_offset = MAX_SEQUENCE_LENGTH - 1;

	int diff = sel_row + (old_of - sequence_row_offset) / sequence_step;
	if(diff < 0) diff = 0;
	if(diff >= HORIZONTAL_ZONES - 1) diff = HORIZONTAL_ZONES - 2;

	if(diff != sel_row) {
		seq[sel_seq]->unselect_row(sel_row);
		seq[sel_seq]->select_row(diff, sel_hi);
		sel_row = diff;
	}

	refresh_sequencers();
	rnrcol->refresh();
}

void PncSequencer::zone_selected(void *cbd, PncSequence *pncs, int row, bool focus_left) {
	std::vector<PncSequence *>::iterator k;
	int i, found = -1;
	for(i = 0, k = seq.begin(); k != seq.end(); k++, i++) {
		if((*k) == pncs) found = i;
	}

	if(!(found == sel_seq && row == sel_row)) {
		seq[sel_seq]->unselect_row(sel_row);
		sel_seq = found;
		sel_row = row;
	}

	sel_hi = focus_left;

	if(mseqs.size() == 1) {
		phd->set_help_type(PncHelpDisplay::_third_help);
	}

	// user does NOT want to update sequence_step, but sequence DATA
	sel_seq_step = false;
	stz->set_selected(false, false);
	sequencer_mode = machine_selected;
}

void PncSequencer::refresh_sequencers() {
	if(sequencer_mode != spacing_selected)
		sequencer_mode = no_selection;

	if(sequence_offset > (int)mseqs.size())
		sequence_offset = mseqs.size();

	int k;

	// first clear the columns (invisible)
	for(k = 0; k < (int)seq.size(); k++) {
		seq[k]->set_machine_sequencer("", NULL);
	}

	int help_degree = 1;

	// then enable the visible ones
	std::map<std::string, MachineSequencer *>::iterator i;
	for(i = mseqs.begin(), k = -sequence_offset;
	    i != mseqs.end();
	    i++) {
		if(k >= 0 && k < (int)seq.size()) {
			help_degree--;
			seq[k]->set_machine_sequencer((*i).first, (*i).second);
			if(seq[k]->is_selected())
				sequencer_mode = machine_selected;

		}
		k++;
	}
	SATAN_DEBUG("   addnewZone: k = %d\n", k);
	if(k >= 0 && k < (int)seq.size()) {
		seq[k]->set_mode(addnewZone);
	}

	if(help_degree == 1) {
		phd->set_help_type(PncHelpDisplay::_first_help);
	} else if(help_degree == 0) {
		if(sequencer_mode == machine_selected)
			phd->set_help_type(PncHelpDisplay::_third_help);
		else
			phd->set_help_type(PncHelpDisplay::_second_help);
	} else {
		phd->set_help_type(PncHelpDisplay::_no_help);
	}

	set_row_playing_markers(last_row_playing_marker);
}

void PncSequencer::machine_sequencer_set_changed(void *ignored) {

	mseqs = MachineSequencer::get_sequencers();

	refresh_sequencers();
	rnrcol->refresh();
}

void PncSequencer::call_machine_sequencer_set_changed(void *cbdata) {
	SATAN_DEBUG("call_machine_sequencer_set_changed().\n");
	KammoGUI::run_on_GUI_thread(machine_sequencer_set_changed, cbdata);
}

int PncSequencer::last_row_playing_marker = 0;
void PncSequencer::set_row_playing_markers(int row) {
	int k;
	for(k = 0; k < (int)seq.size(); k++) {
		seq[k]->set_current_playing_row(row);
	}

	// cache this entry
	last_row_playing_marker = row;
}

PncSequencer::SequencerMode PncSequencer::get_sequencer_mode() {
	return sequencer_mode;
}

void PncSequencer::send_keysum(bool button_down, KeySum ksum) {
	if(button_down) return;

	int ns = sel_seq, nr = sel_row;

	int kval = 0xF00; // 0xF00 == no value set!

	// Observe! 0xf00 means that kval was NOT set
	// Observe! 0x0f0 is a special code in this function only...(it means that we should unset the current value, to nothing)

	switch(ksum) {
	case key_alternative:
	case NO_KEY:
		return;
		break;

	case key_blank:
		kval = 0xf0;
		break;

	case key_return:
		return;
		break;
	case key_delete:
		kval = 0xf0;
		break;

	case key_left:
		if(sel_hi) {
			if(!(nr == 0 && sequence_row_offset == 0)) {
				nr--;
				sel_hi = false;
			}
		} else
			sel_hi = true;
		break;

	case key_right:
		if(!sel_hi) {
			nr++;
			sel_hi = true;
		} else
			sel_hi = false;
		break;

	case key_up:
		ns--;
		break;

	case key_down:
		ns++;
		if(ns >= (int)mseqs.size()) ns = mseqs.size() - 1;
		break;

	case key_0: kval = 0x00; break;
	case key_1: kval = 0x01; break;
	case key_2: kval = 0x02; break;
	case key_3: kval = 0x03; break;
	case key_4: kval = 0x04; break;
	case key_5: kval = 0x05; break;
	case key_6: kval = 0x06; break;
	case key_7: kval = 0x07; break;
	case key_8: kval = 0x08; break;
	case key_9: kval = 0x09; break;
	case key_a: kval = 0x0a; break;
	case key_b: kval = 0x0b; break;
	case key_c: kval = 0x0c; break;
	case key_d: kval = 0x0d; break;
	case key_e: kval = 0x0e; break;
	case key_f: kval = 0x0f; break;

		break;

	case key_tracker:
		{
			if(sequencer_mode == machine_selected) {
				std::vector<PncSequence *>::iterator k;
				for(k = seq.begin(); k != seq.end(); k++) {
					if((*k)->is_selected()) {
						(*k)->show_tracker();
					}
				}
			}
		}
		return;

	case key_control_editor:
		{
			if(sequencer_mode == machine_selected) {
				std::vector<PncSequence *>::iterator k;
				for(k = seq.begin(); k != seq.end(); k++) {
					if((*k)->is_selected()) {
						(*k)->show_controls();
					}
				}
			}
		}
		return;

	case key_envelope_editor:
		{
			if(sequencer_mode == machine_selected) {
				std::vector<PncSequence *>::iterator k;
				for(k = seq.begin(); k != seq.end(); k++) {
					if((*k)->is_selected()) {
						(*k)->show_envelopes();
					}
				}
			}
		}
		return;

	case key_connector_editor:
		{
			static KammoGUI::UserEvent *ue = NULL;
			KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showConnector_container");
			if(ue != NULL)
				KammoGUI::EventHandler::trigger_user_event(ue);
		}
		return;

	}

	if(sel_seq_step) {
		// OK at this stage we IGNORE the normal sequencer stuff
		// since the user has selected to modify the sequence_step value instead
		stz->set_selected(true, sel_hi);

		if(kval != 0xf00 && kval != 0xf0) {
			int value = sequence_step;

			if(sel_hi) { // set high nibble
				value %= 10;
				value = value + kval * 10;
			} else { // set low nibble
				value /= 10;
				value = (value * 10) + kval;
			}

			if(value < 1) value = 1;
			if(value > 99) value = 99;

			sequence_step = value;

			char bfr[9];
			snprintf(bfr, 8, "%01d %01d", value / 10, value % 10);
			stz->set_value(bfr);
		}
		refresh_sequencers();
		rnrcol->refresh();

		return;
	}

	// the key value was set
	if(kval != 0xf00) {
		nr++; // step forward

		MachineSequencer *msec = seq[sel_seq]->mseq;

		if(msec != NULL) {
			int position = sequence_row_offset + sel_row * sequence_step;

			int value = msec->get_loop_id_at(position);
			if(kval == 0xf0) {
				value = NOTE_NOT_SET;
			} else {
				if(value == NOTE_NOT_SET) value = 0x00;

				if(sel_hi) { // set high nibble
					value %= 10;
					value = value + kval * 10;
				} else { // set low nibble
					value /= 10;
					value = (value * 10) + kval;
				}
			}

			try {
				msec->set_loop_id_at(position, value);
			} catch(MachineSequencer::ParameterOutOfSpec poos) {
				SATAN_DEBUG("loop id ]%d[ out of spec\n", value); fflush(0);
			}
		}
		refresh_sequencers();
	}

	// ns means "new sequence"
	// values lower than zero requires a horizontal scroll first
	// as do values higher or equal to seq.size()
	// nr means "new row"
	// values lower than zero requires a vertical scroll first
	// likewise does values higher or equal to (VERTICAL_ZONES - 1)
	if(ns < 0) {
		vertical_scroll(seq[0], -1);
	} else if(ns >= (int)seq.size()) {
		vertical_scroll(seq[seq.size() - 1], 1);
	} else if(nr < 0) {
		horizontal_scroll(seq[0], -1);
	} else if(nr >= (HORIZONTAL_ZONES - 1)) {
		horizontal_scroll(seq[0], 1);
	}
	// before we do anything else here, do sanity check
	if(nr >= HORIZONTAL_ZONES - 1) nr = HORIZONTAL_ZONES - 2;
	if(nr < 0) nr = 0;
	if(ns >= VERTICAL_ZONES -1) ns = VERTICAL_ZONES - 2;
	if(ns < 0) ns = 0;

	// OK, at this stage we should move the cursor.
	// first we unselect the previous selection
	if(sel_seq != ns || sel_row != nr) {
		seq[sel_seq]->unselect_row(sel_row);
	}
	// then we select something new
	sel_seq = ns; sel_row = nr;
	seq[sel_seq]->select_row(sel_row, sel_hi);
}

void PncSequencer::step_increase(void *cbd, PncSeqZone *pnb, bool selected, bool focus_left) {
	if(selected) {
		sequencer_mode = spacing_selected;
		sel_seq_step = true;
		sel_hi = focus_left;
		seq[sel_seq]->unselect_row(sel_row);
	}
}

void PncSequencer::init(CanvasWidgetContext *cwc) {
	context = cwc;

	{
		float width_pixels;
		float width_inches;
		width_inches = KammoGUI::DisplayConfiguration::get_screen_width(KammoGUI::inches);
		width_pixels = KammoGUI::DisplayConfiguration::get_screen_width(KammoGUI::pixels);
		FLING_DEACCELERATION = 10.0f * width_pixels / width_inches;
	}
	{
		float w_inch, h_inch;

		w_inch = KammoGUI::DisplayConfiguration::get_screen_width(KammoGUI::inches);
		h_inch = KammoGUI::DisplayConfiguration::get_screen_height(KammoGUI::inches);

		SATAN_DEBUG("w/h_inch: %f, %f\n", w_inch, h_inch);

		if(w_inch >= 4.0f)
			HORIZONTAL_ZONES = 15;
		else if(w_inch >= 3.0f)
			HORIZONTAL_ZONES = 10;
		else if(w_inch >= 2.0f)
			HORIZONTAL_ZONES = 7;
		else
			HORIZONTAL_ZONES = 5;

		if(h_inch >= 6.0f)
			VERTICAL_ZONES = 18;
		else if(h_inch >= 5.0f)
			VERTICAL_ZONES = 12;
		else if(h_inch >= 3.0f)
			VERTICAL_ZONES = 8;
		else
			VERTICAL_ZONES = 7;
	}

	int k = 0;

	// basic calculus
	float w = 1.0 / (HORIZONTAL_ZONES);
	float h = 1.0 / (VERTICAL_ZONES);

	// step zone
	char bfr[9]; snprintf(bfr, 8, "%01d %01d", sequence_step / 10, sequence_step % 10);
	stz = new PncSeqZone(
		cwc, 0.0, (VERTICAL_ZONES - 1.0) * h , w, VERTICAL_ZONES * h, false, true, false, bfr, 0x01);
	stz->set_callback_data(NULL, step_increase, NULL);

	// Row Number column
	rnrcol = new RowNumberColumn(cwc, HORIZONTAL_ZONES - 1);

	// create sequence columns
	for(k = 0; k < VERTICAL_ZONES - 1; k++) {
		seq.push_back(
			new PncSequence(
				cwc,
				k, HORIZONTAL_ZONES - 1,
				horizontal_scroll, vertical_scroll, zone_selected
				)
			);
	}

	int help_w = 6 < HORIZONTAL_ZONES ? 6 : HORIZONTAL_ZONES;
	int help_h = 3;

	// show some helping phd
	phd = new PncHelpDisplay(cwc, w, 2 * h, help_w * w, help_h * h, PncHelpDisplay::_first_help);
	phd->ignore_events();

	// register us for change notification from MachineSequencer
	MachineSequencer::register_change_callback(call_machine_sequencer_set_changed);
	Machine::register_periodic(
		[](int row) {
			SATAN_DEBUG("sequence_row_playing_changed().\n");
			KammoGUI::run_on_GUI_thread(
				[row]() {
					if((row % PncSequencer::sequence_step) == 0) {
						set_row_playing_markers(row);
					}
				}
				);
		}
		);

	// initial playback marker
	set_row_playing_markers(0);

	// initial refresh
	refresh_sequencers();
}

/***************************
 *
 *  Event Declaration
 *
 ***************************/

KammoEventHandler_Declare(PncSequencerHandler,"pncSequencer:refreshProject");

virtual void on_init(KammoGUI::Widget *wid) {
	if(wid->get_id() == "pncSequencer") {
		((KammoGUI::Canvas *)wid)->set_bg_color(1.0f, 1.0f, 1.0f);

		CanvasWidgetContext *cwc =
			new CanvasWidgetContext((KammoGUI::Canvas *)wid, 2.0f, 2.0f);
		PncSequencer::init(cwc);
		cwc->enable_events();
	}
}

virtual void on_user_event(KammoGUI::UserEvent *ue, std::map<std::string, void *> args) {
	// this event is triggered for example when a project is loaded.
	if(ue->get_id() == "refreshProject") {
		SATAN_DEBUG("on refreshProject - calling PncSequencer::call_machine_sequencer_set_changed()\n");
		PncSequencer::call_machine_sequencer_set_changed(NULL);
		SATAN_DEBUG("on refreshProject - returned from PncSequencer::call_machine_sequencer_set_changed()\n");
	}
}

KammoEventHandler_Instance(PncSequencerHandler);
