/*
 * VuKNOB
 * (C) 2014 by Anton Persson
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

#include <iostream>
using namespace std;

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <fstream>
#include <math.h>

#include "scale_slider.hh"
#include "svg_loader.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#include "common.hh"

/***************************
 *
 *  Class ScaleSlider::Transition
 *
 ***************************/

ScaleSlider::Transition::Transition(
	ScaleSlider *_sc,
	std::function<void(ScaleSlider *context, float progress)> _callback) :
	KammoGUI::Animation(TRANSITION_TIME), sc(_sc), callback(_callback) {}

void ScaleSlider::Transition::new_frame(float progress) {
	callback(sc, progress);
}

void ScaleSlider::Transition::on_touch_event() { /* ignored */ }

/***************************
 *
 *  Class ScaleSlider
 *
 ***************************/

void ScaleSlider::on_render() {
	{ // Translate ScaleSlider, and scale it properly to fit the defined viewbox
		KammoGUI::SVGCanvas::SVGMatrix menu_t;
		menu_t.scale(scale, scale);
		menu_t.translate(translate_x, translate_y);
		
		KammoGUI::SVGCanvas::ElementReference root(this);
		root.set_transform(menu_t);
	}

	{ // move the scale knob into position
		scale_knob->get_boundingbox(knob_size);
		knob_size.height /= scale;
		
		KammoGUI::SVGCanvas::SVGMatrix knob_t;
		knob_t.translate(0.0, (1.0 - value) * (document_size.height - knob_size.height));

		scale_knob->set_transform(knob_t);
	}
	if(last_value != value) {
		last_value = value;
		char bfr[32];
		snprintf(bfr, 32, "%3.1f%%", 100.0 * value);
		front_text.set_text_content(std::string(bfr));
		shade_text.set_text_content(std::string(bfr));
	}
}

void ScaleSlider::on_resize() {
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.get_viewport(document_size);
}

void ScaleSlider::on_event(KammoGUI::SVGCanvas::SVGDocument *source,
			   KammoGUI::SVGCanvas::ElementReference *e_ref,
			   const KammoGUI::SVGCanvas::MotionEvent &event) {
	ScaleSlider *ctx = (ScaleSlider *)source;

	double now_y = event.get_y();

	switch(event.get_action()) {
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
	{
		double diff_y = ctx->last_y - now_y;
		diff_y /= ctx->scale;
		diff_y /= (ctx->document_size.height - ctx->knob_size.height);

		ctx->value += diff_y;
		if(ctx->value < 0.0) ctx->value = 0.0;
		if(ctx->value > 1.0) ctx->value = 1.0;

		ctx->last_y = now_y;
	}
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
		ctx->last_y = now_y;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
		if(ctx->listener) {
			ctx->listener->on_scale_slider_changed(ctx, ctx->value);
		}
		break;
	}
}

void ScaleSlider::interpolate(double value) {
	if(!transition_to_active_state) {
		if(value >= 1.0) {
			KammoGUI::SVGCanvas::ElementReference root(this);
			root.set_display("none"); // disable the view for this, until show is called
		}
		value = 1.0 - value;
	}
	
	double ix, iy, iw, ih;

	ix = (x - initial_x) * value + initial_x;
	iy = (y - initial_y) * value + initial_y;
	iw = (width - initial_width) * value + initial_width;
	ih = (height - initial_height) * value + initial_height;
	
	double scale_w = iw / document_size.width;
	double scale_h = ih / document_size.height;

	scale = scale_w < scale_h ? scale_w : scale_h;

	translate_x = ix;
	translate_y = iy;
}

void ScaleSlider::transition_progressed(ScaleSlider *sc, float progress) {
	sc->interpolate((double)progress);
}

void ScaleSlider::show(
	double _initial_x, double _initial_y,
	double _initial_width, double _initial_height,
	double _x, double _y, double _width, double _height) {
	
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.set_display("inline"); // time to show this

	initial_x = _initial_x;
	initial_y = _initial_y;
	initial_width = _initial_width;
	initial_height = _initial_height;

	x = _x;
	y = _y;
	width = _width;
	height = _height;

	transition_to_active_state = true;
	Transition *transition = new Transition(this, transition_progressed);
	if(transition) {
		start_animation(transition);
	} else {
		interpolate(1.0); // skip to viewable directly
	}
}

void ScaleSlider::hide(double final_x, double final_y, double final_width, double final_height) {
	initial_x = final_x;
	initial_y = final_y;
	initial_width = final_width;
	initial_height = final_height;
	
	transition_to_active_state = false;
	Transition *transition = new Transition(this, transition_progressed);
	if(transition) {
		start_animation(transition);
	} else {
		interpolate(0.0); // skip to disabled directly
		KammoGUI::SVGCanvas::ElementReference root(this);
		root.set_display("none"); // disable the view for this, until show is called
	}
	listener = NULL;
}

void ScaleSlider::set_label(const std::string &new_label) {
	front_label.set_text_content(new_label);
	shade_label.set_text_content(new_label);
}

void ScaleSlider::set_value(double val) {
	value = val;
}

double ScaleSlider::get_value() {
	return value;
}

void ScaleSlider::set_listener(ScaleSlider::ScaleSliderChangedListener *new_listener) {
	listener = new_listener;
}

ScaleSlider::ScaleSlider(KammoGUI::SVGCanvas *cnvs) : SVGDocument(std::string(SVGLoader::get_svg_directory() + "/Scale.svg"), cnvs), listener(NULL), value(0.75), last_value(-2.0) {
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.set_display("none"); // disable the view for this, until show is called

	scale_knob = new KammoGUI::SVGCanvas::ElementReference(this, "scaleKnob");
	scale_knob->set_event_handler(on_event);

	KammoGUI::SVGCanvas::ElementReference textContent(this, "textContent");
	front_text = textContent.find_child_by_class("frontText");
	shade_text = textContent.find_child_by_class("shadedText");

	KammoGUI::SVGCanvas::ElementReference labelContent(this, "labelContent");
	front_label = labelContent.find_child_by_class("frontText");
	shade_label = labelContent.find_child_by_class("shadedText");
}

ScaleSlider::~ScaleSlider() {
	QUALIFIED_DELETE(scale_knob);
}

