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

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <sstream>
#include <kamogui.hh>
#include <functional>

#include "listview.hh"
#include "svg_loader.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#include "common.hh"

// a static object needed to create a dummy pointer
static int listview_dummy_pointer = 0;

/***************************
 *
 *  Class ListView::Row
 *
 ***************************/

void ListView::Row::on_event(KammoGUI::SVGCanvas::SVGDocument *source,
			     KammoGUI::SVGCanvas::ElementReference *e_ref,
			     const KammoGUI::SVGCanvas::MotionEvent &event) {
	Row *row = (Row *)e_ref;
	double x = event.get_x();
	double y = event.get_y();

	static double scroll_start, start_x, start_y;

	SATAN_DEBUG("row_on_event: %d\n", event.get_action());

	{
		double scroll;
		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			start_x = x;
			scroll_start = start_y = y;
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			scroll = y - scroll_start;
			row->parent->offset += scroll;

			if(row->parent->offset > 0)
				row->parent->offset = 0;
			if(row->parent->offset < row->parent->min_offset)
				row->parent->offset = row->parent->min_offset;

			scroll_start = y;

			if(event.get_action() == KammoGUI::SVGCanvas::MotionEvent::ACTION_UP) {
				x = x - start_x;
				y = y - start_y;
				if(x < 0) x = -x; if(y < 0) y = -y;
				if(x < 10 && y < 10) {
					row->parent->row_selected(row->row_index, row->text);
				}
			}
			break;
		}
	}
}

ListView::Row::Row(ListView *_parent, const std::string &_text, int _row_index, const std::string &id) :
	ElementReference(_parent, id), row_index(_row_index), parent(_parent), text(_text) {

	// find the new row element's text element and change it
	find_child_by_class("listItemText").set_text_content(text);

	// attach the event handler
	set_event_handler(on_event);
}

ListView::Row *ListView::Row::create(ListView *parent, int row_index, const std::string &text) {
	// locate needed elements
	KammoGUI::SVGCanvas::ElementReference base_element = KammoGUI::SVGCanvas::ElementReference(parent, "listViewRow");
	KammoGUI::SVGCanvas::ElementReference container = KammoGUI::SVGCanvas::ElementReference(parent, "listContainer");

	// create a new id for the row
	std::stringstream new_id;
	new_id << "listViewRow__" << row_index;

	SATAN_DEBUG("ListView::Row::Create(%d, %s) -> %s\n", row_index, text.c_str(), new_id.str().c_str());

	// inject a clone of the base_element into container
	container.add_element_clone(new_id.str(), base_element);

	// return the new row
	return new Row(parent, text, row_index, new_id.str());
}

/***************************
 *
 *  Class ListView
 *
 ***************************/

void ListView::show() {
	KammoGUI::SVGCanvas::ElementReference root_element = KammoGUI::SVGCanvas::ElementReference(this);
	root_element.set_display("inline");
}

void ListView::hide() {
	KammoGUI::SVGCanvas::ElementReference root_element = KammoGUI::SVGCanvas::ElementReference(this);
	root_element.set_display("none");
}

void ListView::on_cancel_event(KammoGUI::SVGCanvas::SVGDocument *source,
			       KammoGUI::SVGCanvas::ElementReference *e_ref,
			       const KammoGUI::SVGCanvas::MotionEvent &event) {
	ListView *ctx = (ListView *)source;

	ctx->hide();

	if(ctx->callback_context != &listview_dummy_pointer)
		ctx->listview_callback(ctx->callback_context, false, -1, "");
	else
		ctx->listview_callback_new(false, -1, "");
}

void ListView::row_selected(int row_index, const std::string &selected_text) {
	hide();

	if(callback_context != &listview_dummy_pointer)
		listview_callback(callback_context, true, row_index, selected_text);
	else
		listview_callback_new(true, row_index, selected_text);
}

ListView::ListView(KammoGUI::SVGCanvas *cnv) : SVGDocument(std::string(SVGLoader::get_svg_directory() + "/listView.svg"), cnv), offset(0.0) {
	hide();
	title_text = KammoGUI::SVGCanvas::ElementReference(this, "titleText");
	cancel_button = KammoGUI::SVGCanvas::ElementReference(this, "titleText");
	shade_layer = KammoGUI::SVGCanvas::ElementReference(this, "shadeLayer");

	cancel_button.set_event_handler(on_cancel_event);
	shade_layer.set_event_handler(on_cancel_event);

	KammoGUI::SVGCanvas::ElementReference(this, "listViewRow").set_display("hidden");
}

void ListView::on_resize() {
	int canvas_w, canvas_h;
	float canvas_w_inches, canvas_h_inches;
	KammoGUI::SVGCanvas::SVGRect document_size;

	// get data
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.get_viewport(document_size);
	get_canvas_size(canvas_w, canvas_h);
	get_canvas_size_inches(canvas_w_inches, canvas_h_inches);

	{ // calculate transform for the shadeLayer
		// calculate scaling factor
		double scale_w = canvas_w / (double)document_size.width;
		double scale_h = canvas_h / (double)document_size.height;

		// initiate transform_t
		shade_transform_t.init_identity();
		shade_transform_t.scale(scale_w, scale_h);
	}

	{ // calculate transform for the main part of the document
		double tmp;

		// calculate the canvas size in "fingers"
		tmp = canvas_w_inches / INCHES_PER_FINGER;
		int canvas_width_fingers = (int)tmp;
		tmp = canvas_h_inches / INCHES_PER_FINGER;
		int canvas_height_fingers = (int)tmp;

		// calculate the size of a finger in pixels
		tmp = canvas_w / ((double)canvas_width_fingers);
		double finger_width = tmp;
		tmp = canvas_h / ((double)canvas_height_fingers);
		double finger_height = tmp;

		// force scaling to fit into 5 by 7 "fingers"
		if(canvas_width_fingers > 5) canvas_width_fingers = 5;
		if(canvas_height_fingers > 7) canvas_height_fingers = 7;

		// calculate scaling factor
		double target_w = (double)canvas_width_fingers  * finger_width;
		double target_h = (double)canvas_height_fingers  * finger_height;

		double scale_w = target_w / (double)document_size.width;
		double scale_h = target_h / (double)document_size.height;

		double scaling = scale_w < scale_h ? scale_w : scale_h;

		// calculate translation
		double translate_x = (canvas_w - document_size.width * scaling) / 2.0;
		double translate_y = (canvas_h - document_size.height * scaling) / 2.0;

		// initiate transform_t
		transform_t.init_identity();
		transform_t.scale(scaling, scaling);
		transform_t.translate(translate_x, translate_y);
	}
}

void ListView::on_render() {
	{
		KammoGUI::SVGCanvas::SVGMatrix mtrx;
		mtrx.init_identity();

		// translate initial offset
		mtrx.translate(0, offset);

		double height = 70;

		for(auto row : rows) {
			// transform the current row
			row->set_transform(mtrx);

			// then do an translation for the next one
			mtrx.translate(0, height);
		}

		// calculate the minimum offset (a negative number)
		min_offset = -height * (rows.size() - 4); // 4 is an arbitrary number at this point... not so pretty
	}

	{
		// Translate ListViewBody, and scale it properly to fit the defined viewbox
		shade_layer.set_transform(shade_transform_t);

		// Translate ListViewBody, and scale it properly to fit the defined viewbox
		KammoGUI::SVGCanvas::ElementReference main_layer(this, "mainLayer");
		main_layer.set_transform(transform_t);
	}
}

void ListView::clear() {
	for(auto row : rows) {
		SATAN_DEBUG("ListView::clear() - Dropping element.\n");
		row->drop_element();
		delete row;
	}
	rows.clear();
	SATAN_DEBUG("ListView::clear() -> dump id table.");
}

void ListView::add_row(const std::string &new_row_text) {
	rows.push_back(Row::create(this, rows.size(), new_row_text));
}

void ListView::select_from_list(const std::string &title,
				void *_callback_context,
				std::function<void(void *context, bool, int row_number, const std::string &)> _listview_callback) {
	offset = 0.0;
	callback_context = _callback_context;
	listview_callback = _listview_callback;

	title_text.set_text_content(title);

	show();
}

void ListView::select_from_list(const std::string &title,
				std::function<void(bool, int row_number, const std::string &)> _listview_callback) {
	offset = 0.0;
	callback_context = &listview_dummy_pointer;
	listview_callback_new = _listview_callback;

	title_text.set_text_content(title);

	show();
}
