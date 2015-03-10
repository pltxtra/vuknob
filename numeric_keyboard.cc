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

#include "numeric_keyboard.hh"
#include "pncsequencer.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#define HORIZONTAL_KEYS 8
#define VERTICAL_KEYS 2

/***************************
 *
 *  Class NumericKeyboard
 *
 ***************************/

NumericKeyboard *NumericKeyboard::num = NULL;

NumericKeyboard::NumericKeyboard(CanvasWidgetContext *cwc) :
	CanvasWidget(cwc, 0.0, 0.0, 1.0, 1.0, 0xff), alternative_keyboard(false) {

	bg_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/2LineKeyboard.svg"));
	
	bg = new KammoGUI::Canvas::SVGBob(
		__cnv, bg_d);

	bg_alt_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/2LineKeyboard-alternative1.svg"));
	
	bg_alt = new KammoGUI::Canvas::SVGBob(
		__cnv, bg_alt_d);

	bg_no_sel_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/2LineKeyboard-noSelection.svg"));
	
	bg_no_sel = new KammoGUI::Canvas::SVGBob(
		__cnv, bg_no_sel_d);

	bg_spac_sel_d   = KammoGUI::Canvas::SVGDefinition::from_file(
		std::string(CanvasWidgetContext::svg_directory + "/2LineKeyboard-spacingSelected.svg"));
	
	bg_spac_sel = new KammoGUI::Canvas::SVGBob(
		__cnv, bg_spac_sel_d);
}

void NumericKeyboard::resized() {
	int w, h;

	w = x2 - x1;
	h = y2 - y1;
	w /= HORIZONTAL_KEYS;
	h /= VERTICAL_KEYS;
	s = w < h ? w : h;

	w = s * HORIZONTAL_KEYS;
	h = s * VERTICAL_KEYS;
	
	xp = (((x2 - w) - x1) / 2) + x1;
	yp = (((y2 - h) - y1) / 2) + y1;
	
	bg->set_blit_size(w, h);
	bg_alt->set_blit_size(w, h);
	bg_no_sel->set_blit_size(w, h);
	bg_spac_sel->set_blit_size(w, h);
}

void NumericKeyboard::expose() {
	switch(PncSequencer::get_sequencer_mode()) {
	case PncSequencer::no_selection:
		__cnv->blit_SVGBob(bg_no_sel, xp, yp);
		break;
	case PncSequencer::machine_selected:
		if(alternative_keyboard) {
			__cnv->blit_SVGBob(bg_alt, xp, yp);
		} else {
			__cnv->blit_SVGBob(bg, xp, yp);
		}
		break;
	case PncSequencer::spacing_selected:
		__cnv->blit_SVGBob(bg_spac_sel, xp, yp);		
		break;
	}
}

PncSequencer::KeySum alt_keymap1[] = {
	PncSequencer::key_1, PncSequencer::key_2, PncSequencer::key_3, PncSequencer::key_4, PncSequencer::key_5, PncSequencer::key_left, PncSequencer::key_connector_editor, PncSequencer::key_tracker, 
	PncSequencer::key_6, PncSequencer::key_7, PncSequencer::key_8, PncSequencer::key_9, PncSequencer::key_0, PncSequencer::key_right, PncSequencer::key_delete, PncSequencer::key_alternative
};

PncSequencer::KeySum alt_keymap2[] = {
	PncSequencer::NO_KEY, PncSequencer::NO_KEY, PncSequencer::NO_KEY, PncSequencer::NO_KEY, PncSequencer::NO_KEY, PncSequencer::NO_KEY, PncSequencer::key_control_editor, PncSequencer::key_envelope_editor,
	PncSequencer::NO_KEY, PncSequencer::NO_KEY, PncSequencer::NO_KEY, PncSequencer::NO_KEY, PncSequencer::NO_KEY, PncSequencer::NO_KEY, PncSequencer::NO_KEY, PncSequencer::key_alternative
};

PncSequencer::KeySum *alt_keymap = alt_keymap1;

void NumericKeyboard::on_event(KammoGUI::canvasEvent_t ce, int x, int y) {
	int r, c, k;

	if(!(ce == KammoGUI::cvButtonPress || ce == KammoGUI::cvButtonRelease)) return;

	// remove offset
	x -= xp; y -= yp;	

	if(x < 0 || x > s * HORIZONTAL_KEYS ||
	   y < 0 || y > s * VERTICAL_KEYS) {
		// outside of keypad, just return
		return;
	}	
	
	r = y / s;
	c = x / s;
	k = r * HORIZONTAL_KEYS + c;

	if(k >= 0 && k < 24) {
		if(alt_keymap[k] == PncSequencer::key_alternative) {
			alternative_keyboard = (ce == KammoGUI::cvButtonPress) ? true : false;
		} else {
			if(alt_keymap[k] != PncSequencer::NO_KEY) {
				PncSequencer::send_keysum(ce == KammoGUI::cvButtonPress ? true : false,
							  alt_keymap[k]);
			}
		}
	}
	if(ce == KammoGUI::cvButtonRelease)
		alternative_keyboard = false;
	
	alt_keymap = alternative_keyboard ? alt_keymap2 : alt_keymap1;
	SATAN_DEBUG("alternative_keyboard = %s\n", alternative_keyboard ? "true" : "false");
}

void NumericKeyboard::setup(KammoGUI::Canvas *cnvs) {
	CanvasWidgetContext *cwc =
		new CanvasWidgetContext(cnvs, 2.0f, 0.65f);
	cnvs->set_bg_color(1.0f, 1.0f, 1.0f);
	num = new NumericKeyboard(cwc);

	cwc->enable_events();
}

/***************************
 *
 *  Kamoflage Event Declaration
 *
 ***************************/

KammoEventHandler_Declare(NumericKeyboardHandler,"numericKeyboard");

virtual void on_init(KammoGUI::Widget *wid) {
	if(wid->get_id() == "numericKeyboard") {
		try {
			NumericKeyboard::setup((KammoGUI::Canvas *)wid);
		} catch(jException e) {
			SATAN_DEBUG("Caught an jException (%s) in on_init() for \"numericKeyboard\" \n", e.message.c_str());
			exit(0);
		} catch(...) {
			SATAN_DEBUG("Caught an exception in on_init() for \"numericKeyboard\" \n");
			exit(0);
		}
	}
}

KammoEventHandler_Instance(NumericKeyboardHandler);
