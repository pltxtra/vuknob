/*
 * VuKNOB
 * (C) 2015 by Anton Persson
 *
 * http://www.vuknob.com/
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

#include <iostream>
#include <math.h>

#include "corner_button.hh"
#include "svg_loader.hh"
#include "common.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/***************************
 *
 *  Class ConerButton::Transition
 *
 ***************************/

CornerButton::Transition::Transition(
	CornerButton *_context,
	std::function<void(CornerButton *context, float progress)> _callback) :
	KammoGUI::Animation(TRANSITION_TIME), ctx(_context), callback(_callback) {
	SATAN_DEBUG(" created new Transition for CornerButton %p\n", ctx);
}

void CornerButton::Transition::new_frame(float progress) {
	callback(ctx, progress);
}

void CornerButton::Transition::on_touch_event() { /* ignored */ }

/***************************
 *
 *  Class CornerButton
 *
 ***************************/

void CornerButton::transition_progressed(CornerButton *ctx, float progress) {
	SATAN_DEBUG("  CornerButton transition progressed for ctx %p\n", ctx);
	if(ctx->hidden)
		ctx->offset_factor = (double)progress;
	else
		ctx->offset_factor = 1.0 - ((double)progress);
}

void CornerButton::run_transition() {
	Transition *transition = new Transition(this, transition_progressed);
	SATAN_DEBUG(" Created new CornerButton::Transition - %p\n", transition);
	if(transition) {
		start_animation(transition);
		SATAN_DEBUG("   CornerButton animation started.\n");
	} else {
		transition_progressed(this, 1.0); // skip to final state directly
	}
}

void CornerButton::on_event(KammoGUI::SVGCanvas::SVGDocument *source,
			  KammoGUI::SVGCanvas::ElementReference *e_ref,
			  const KammoGUI::SVGCanvas::MotionEvent &event) {
	SATAN_DEBUG(" CornerButton::on_event. (doc %p - CornerButton %p)\n", source, e_ref);
	CornerButton *ctx = (CornerButton *)source;

	double now_x = event.get_x();
	double now_y = event.get_y();

	switch(event.get_action()) {
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
		ctx->is_a_tap = false;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
		// check if the user is moving the finger too far, indicating abort action
		// if so - disable is_a_tap
		if(ctx->is_a_tap) {
			if(fabs(now_x - ctx->first_selection_x) > 20)
				ctx->is_a_tap = false;
			if(fabs(now_y - ctx->first_selection_y) > 20)
				ctx->is_a_tap = false;
		}
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
		ctx->is_a_tap = true;
		ctx->first_selection_x = now_x;
		ctx->first_selection_y = now_y;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
		if(ctx->is_a_tap) {
			ctx->callback_function();
		}
		break;
	}
}

CornerButton::CornerButton(KammoGUI::SVGCanvas *cnvs, const std::string &svg_file, WhatCorner _corner) : SVGDocument(svg_file, cnvs), my_corner(_corner), hidden(false), offset_factor(0.0), callback_function([](){}) {
	elref = KammoGUI::SVGCanvas::ElementReference(this);
	elref.set_event_handler(on_event);
}

CornerButton::~CornerButton() {}

void CornerButton::on_render() {
	// Translate the document, and scale it properly to fit the defined viewbox
	KammoGUI::SVGCanvas::ElementReference root(this);

	KammoGUI::SVGCanvas::SVGMatrix transform_t = base_transform_t;
	transform_t.translate(offset_factor * offset_target_x, offset_factor * offset_target_y);
	root.set_transform(transform_t);
}

void CornerButton::on_resize() {
	int canvas_w, canvas_h;
	float canvas_w_inches, canvas_h_inches;
	KammoGUI::SVGCanvas::SVGRect document_size;

	// get data
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.get_viewport(document_size);
	get_canvas_size(canvas_w, canvas_h);
	get_canvas_size_inches(canvas_w_inches, canvas_h_inches);

	{ // calculate transform for the main part of the document
		double tmp;

		// calculate the width of the canvas in "fingers"
		tmp = canvas_w_inches / INCHES_PER_FINGER;
		double canvas_width_fingers = tmp;

		// calculate the size of a finger in pixels
		tmp = canvas_w / (canvas_width_fingers);
		double finger_width = tmp;

		// calculate scaling factor
		double scaling = (1.5 * finger_width) / (double)document_size.width;

		// calculate translation
		double translate_x = 0.0, translate_y = 0.0;

		switch(my_corner) {
		case top_left:
		{
			translate_x = 0.25 * document_size.width * scaling;
			translate_y = 0.25 * document_size.height * scaling;
			offset_target_x = -3.0 * document_size.width * scaling;
			offset_target_y = -2.0 * document_size.height * scaling;
		}
		break;

		case top_right:
		{
			translate_x = canvas_w - 1.25 * document_size.width * scaling;
			translate_y = 0.25 * document_size.height * scaling;
			offset_target_x = 3.0 * document_size.width * scaling;
			offset_target_y = -2.0 * document_size.height * scaling;
		}
		break;

		case bottom_left:
		case bottom_right:
		{
			translate_x = canvas_w - 1.25 * document_size.width * scaling;
			translate_y = canvas_h - 1.25 * document_size.height * scaling;
			offset_target_x = 3.0 * document_size.width * scaling;
			offset_target_y = 3.0 * document_size.height * scaling;
		}
		break;
		}

		// initiate transform_m
		base_transform_t.init_identity();
		base_transform_t.scale(scaling, scaling);
		base_transform_t.translate(translate_x, translate_y);
	}
}

void CornerButton::show() {
	hidden = false;
	run_transition();
}

void CornerButton::hide() {
	hidden = true;
	run_transition();
}

void CornerButton::set_select_callback(std::function<void()> _callback_function) {
	callback_function = _callback_function;
}
