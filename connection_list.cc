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
#include <unistd.h>

#include "connection_list.hh"
#include "svg_loader.hh"
#include "common.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/***************************
 *
 *  Class ConnectionList::ConnectionGraphic
 *
 ***************************/

ConnectionList::ConnectionGraphic::ConnectionGraphic(ConnectionList *context, const std::string &id,
						     std::function<void()> on_delete,
						     const std::string &src, const std::string &dst) : ElementReference(context, id) {
	set_display("inline");

	find_child_by_class("outputName").set_text_content(src);
	find_child_by_class("inputName").set_text_content(dst);

	delete_button = find_child_by_class("deleteButton");
	delete_button.set_event_handler(
		[this, context, on_delete](KammoGUI::SVGCanvas::SVGDocument *,
		       KammoGUI::SVGCanvas::ElementReference *,
		       const KammoGUI::SVGCanvas::MotionEvent &event) {
			if(event.get_action() == KammoGUI::SVGCanvas::MotionEvent::ACTION_UP) {
				on_delete();
				context->hide();
			}
		}
		);
}

ConnectionList::ConnectionGraphic::~ConnectionGraphic() {
	drop_element();
}

auto ConnectionList::ConnectionGraphic::create(ConnectionList *context, int id,
					       std::function<void()> on_delete,
					       const std::string &src, const std::string &dst) -> std::shared_ptr<ConnectionGraphic> {
	std::stringstream id_stream;
	id_stream << "graphic_" << id;

	SATAN_DEBUG("Create ConnectionList::ConnectionGraphic - context: %p\n", context);
	
	KammoGUI::SVGCanvas::ElementReference graphic_template = KammoGUI::SVGCanvas::ElementReference(context, "connectionTemplate");
	KammoGUI::SVGCanvas::ElementReference connection_layer = KammoGUI::SVGCanvas::ElementReference(context, "connectionLayer");

	(void) connection_layer.add_element_clone(id_stream.str(), graphic_template);
	
	auto retval = std::make_shared<ConnectionGraphic>(context, id_stream.str(), on_delete, src, dst);
		
	return retval;
}

/***************************
 *
 *  Class ConnectionList::ZoomTransition
 *
 ***************************/

ConnectionList::ZoomTransition::ZoomTransition(
	std::function<void(double zoom)> _callback,
	double _zoom_start, double _zoom_stop) :

	KammoGUI::Animation(TRANSITION_TIME), callback(_callback),
	zoom_start(_zoom_start), zoom_stop(_zoom_stop)

{}

void ConnectionList::ZoomTransition::new_frame(float progress) {
	double T = (double)progress;
	double zoom = (zoom_stop - zoom_start) * T + zoom_start;
	callback(zoom);
}

void ConnectionList::ZoomTransition::on_touch_event() { /* ignored */ }

/***************************
 *
 *  Class ConnectionList
 *
 ***************************/

ConnectionList::ConnectionList(KammoGUI::SVGCanvas *cnvs) : SVGDocument(SVGLoader::get_svg_path("connection.svg"), cnvs) {
	KammoGUI::SVGCanvas::ElementReference(this, "connectionTemplate").set_display("none");
	KammoGUI::SVGCanvas::ElementReference(this).set_display("none");

	backdrop = KammoGUI::SVGCanvas::ElementReference(this, "backDrop");
	backdrop.set_event_handler(
		[this](KammoGUI::SVGCanvas::SVGDocument *,
		       KammoGUI::SVGCanvas::ElementReference *,
		       const KammoGUI::SVGCanvas::MotionEvent &event) {
			if(event.get_action() == KammoGUI::SVGCanvas::MotionEvent::ACTION_UP) {
				hide();
			}
		}
		);
}

ConnectionList::~ConnectionList() {}

void ConnectionList::on_render() {
	// Translate the document, and scale it properly to fit the defined viewbox
	{
		KammoGUI::SVGCanvas::ElementReference root(this);
		
		KammoGUI::SVGCanvas::SVGMatrix transform_t = base_transform_t;
		root.set_transform(transform_t);
	}

	// Zoom the backdrop
	{
		KammoGUI::SVGCanvas::SVGMatrix transform_t;

		transform_t.translate(-(document_size.width / 2.0), -(document_size.height / 2.0));
		transform_t.scale(zoom_factor * 50.0, zoom_factor * 50.0);
		transform_t.translate((document_size.width / 2.0), (document_size.height / 2.0));

		backdrop.set_transform(transform_t);
	}

	// translate each list element
	{
		KammoGUI::SVGCanvas::SVGMatrix element_transform_t;
		
		for(auto gfx : graphics) {
			gfx->set_transform(element_transform_t);
			element_transform_t.translate(0.0, element_vertical_offset);
		}
	}

	// zoom the connectionLayer
	{
		KammoGUI::SVGCanvas::ElementReference layer(this, "connectionLayer");

		KammoGUI::SVGCanvas::SVGMatrix zoom_transform_t;
		zoom_transform_t.translate(-(document_size.width / 2.0), -(document_size.height / 2.0));
		zoom_transform_t.scale(zoom_factor, zoom_factor);
		zoom_transform_t.translate((document_size.width / 2.0), (document_size.height / 2.0));
		zoom_transform_t.translate(0.0, -(element_vertical_offset * ((double)graphics.size()) / 2.0) + 0.5 * element_vertical_offset);

		layer.set_transform(zoom_transform_t);
	}
}

void ConnectionList::on_resize() {
	int canvas_w, canvas_h;
	float canvas_w_inches, canvas_h_inches;
	
	// get data
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.get_viewport(document_size);
	get_canvas_size(canvas_w, canvas_h);
	get_canvas_size_inches(canvas_w_inches, canvas_h_inches);

	element_vertical_offset = document_size.height;
	
	{ // calculate transform for the main part of the document
		double tmp;

		// calculate the width of the canvas in "fingers" 
		tmp = canvas_w_inches / INCHES_PER_FINGER;
		double canvas_width_fingers = tmp;

		// calculate the size of a finger in pixels
		tmp = canvas_w / (canvas_width_fingers);
		double finger_width = tmp;
				
		// calculate scaling factor
		double scaling = (4.0 * finger_width) / (double)document_size.width;
				
		// calculate translation
		double translate_x, translate_y;

		translate_x = (canvas_w - document_size.width * scaling) / 2.0;
		translate_y = (canvas_h - document_size.height * scaling) / 2.0;

		// initiate transform_m
		base_transform_t.init_identity();
		base_transform_t.scale(scaling, scaling);
		base_transform_t.translate(translate_x, translate_y);
	}
}

void ConnectionList::clear() {
	graphics.clear();
}

void ConnectionList::add(const std::string &src, const std::string &dst, std::function<void()> on_delete) {
	graphics.push_back(ConnectionGraphic::create(this, graphics.size(), on_delete, src, dst));
}

void ConnectionList::show() {
	KammoGUI::SVGCanvas::ElementReference(this).set_display("inline");

	ZoomTransition *transition = new ZoomTransition([this](double factor){
								zoom_factor = factor;
							}
							, 0.0001, 1.0);
	start_animation(transition);
}

void ConnectionList::hide() {
	KammoGUI::SVGCanvas::ElementReference(this).set_display("inline");

	ZoomTransition *transition = new ZoomTransition([this](double factor){
								zoom_factor = factor;
								if(factor <= 0.0004) {
									KammoGUI::SVGCanvas::ElementReference(this).set_display("none");
								}
							}
							, 1.0, 0.0001);
	start_animation(transition);
}

