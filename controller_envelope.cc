/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
 * Copyright (C) 2013 by Anton Persson
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <iostream>
#include <sstream>
#include <math.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "controller_envelope.hh"
#include "svg_loader.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/***************************
 *
 *  Class ControllerEnvelope::ControlPoint
 *
 ***************************/

ControllerEnvelope::ControlPoint::ControlPoint(KammoGUI::SVGCanvas::SVGDocument *context, const std::string &id, int _lineNtick) :
	KammoGUI::SVGCanvas::ElementReference(context, id), lineNtick(_lineNtick) {}

/***************************
 *
 *  Class ControllerEnvelope
 *
 ***************************/

std::pair<int, int> ControllerEnvelope::get_envelope_coordinates(float x, float y) {
	x -= graphArea_viewPort.x;
	x -= offset;
	x /= (pixels_per_tick);
	
	y -= graphArea_viewPort.y;	
	y  = ((765.0 - y) / 765.0); // currently an ugly hard coded hack

	// correct the x value, must be > than 0.0
	if(x < 0.0) x = 0.0;
	
	// correct the y values, must be between 0 and 0x7fff
	if(y > 1.0) y = 1.0;
	if(y < 0.0) y = 0.0;
	y *= ((float)0x7fff);	

	// convert to integers, and put in the return pair
	std::pair<int, int> return_pair;

	return_pair.first = (int)x;
	return_pair.second = (int)y;

	return return_pair;
}

void ControllerEnvelope::on_render() {
	{ /* rescale the view to fit the canvas properly */
		int canvas_w, canvas_h;
		
		get_canvas_size(canvas_w, canvas_h);
		
		KammoGUI::SVGCanvas::ElementReference rootElement(this, "rootElement");
		KammoGUI::SVGCanvas::SVGMatrix t_root;
		
		KammoGUI::SVGCanvas::SVGRect viewport;
		rootElement.get_viewport(viewport);
		
		double
			element_w = viewport.width,
			element_h = viewport.height;	
		
		double scale_w, scale_h;
		scale_w = ((double)canvas_w) / element_w;
		scale_h = ((double)canvas_h) / element_h;
		
		double scale = scale_w < scale_h ? scale_w : scale_h;

		screen_resolution = 1 / scale;

		element_w *= scale; element_h *= scale;
		
		svg_hztl_offset = element_w = (((double)canvas_w) - element_w) / 2.0;
		svg_vtcl_offset = element_h = (((double)canvas_h) - element_h) / 2.0;
		
		t_root.scale(scale, scale);
		t_root.translate(element_w, element_h);
		
		rootElement.set_transform(t_root);		
	}

	{ /* move the grid in place */
		graphArea_element->get_viewport(graphArea_viewPort);

		// pixels_per_tick is the number of horizontal pixels for each line/tick marker
		pixels_per_tick = zoom_factor * (graphArea_viewPort.width) / (double)(lineMarker.size() - 9 * MACHINE_TICKS_PER_LINE);

		// initiate the transformation matrix
		KammoGUI::SVGCanvas::SVGMatrix mtrx;
		mtrx.init_identity();

		// calculate the line number offset
		double line_offset_d = offset / (pixels_per_tick * (double)MACHINE_TICKS_PER_LINE);
		int line_offset_i = line_offset_d; // we need the pure integer version too

		// calculate the pixel offset for the first line, and translate the matrix accordingly
		double graphics_offset = (line_offset_d - (double)line_offset_i - 1) * pixels_per_tick * ((double)MACHINE_TICKS_PER_LINE);
		mtrx.translate(graphics_offset, 0);

		// number to display for each line
		int line_number = - line_offset_i - 1;
		
		std::vector<KammoGUI::SVGCanvas::ElementReference *>::iterator k;
		for(k = lineMarker.begin(); k != lineMarker.end(); k++) {

			// transform the line marker
			(*k)->set_transform(mtrx);

			// then do an additional translation for the next one
			mtrx.translate(pixels_per_tick, 0);

			// set the text marker (the class "markText" is not available if this is a tick marker only..)
			KammoGUI::SVGCanvas::ElementReference text;
			try {
				std::stringstream line_number_s;
				line_number_s << line_number;
				line_number++;
				
				(*k)->find_child_by_class("markText").set_text_content(line_number_s.str());
				
			} catch(KammoGUI::SVGCanvas::NoSuchElementException e) {
				/* ignore since this is expected for a tick marker, as that has no markText element */
			}
		}
	}

	{ /* move the envelope nodes */
		int crnt_tick;
		
		// initiate the transformation matrix
		KammoGUI::SVGCanvas::SVGMatrix mtrx;
		mtrx.init_identity();

		std::vector<KammoGUI::SVGCanvas::ElementReference *>::iterator k;
		std::vector<KammoGUI::SVGCanvas::ElementReference *>::iterator l;
		
		const std::map<int, int> &envelope = current_envelope.get_control_points();
		std::map<int, int>::const_iterator i;

		float lx, ly, old_lx = 0.0f, old_ly = 0.0f;

		for(k  = envelope_node.begin(), i = envelope.begin(), l = envelope_line.begin();
		    k != envelope_node.end();
		    k++, i++) {
			// calculate the current tick for this node
			int t = (*i).first;
			float y = (float)((*i).second);
			y /= ((float)0x7fff); // divide it by maximum 14 bit value
			
			crnt_tick =
				PAD_TIME_LINE(t) * MACHINE_TICKS_PER_LINE +
				PAD_TIME_TICK(t);

			// move the node into position
			lx = mtrx.e = offset + pixels_per_tick * crnt_tick; 
			ly = mtrx.f = 765.0 - y * 765.0; // 765.0 is hardcoded, boo hoo.. :(

			// change line
			if(k != envelope_node.begin()) {
				(*l)->set_line_coords(old_lx, old_ly, lx, ly);
				l++;
			}
			old_lx = lx; old_ly = ly;
			
			// set the transformation matrix for the element
			(*k)->set_transform(mtrx);
		}
	}
}

void ControllerEnvelope::graphArea_on_event(KammoGUI::SVGCanvas::SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
					    const KammoGUI::SVGCanvas::MotionEvent &event) {
	ControllerEnvelope *ctx = (ControllerEnvelope *)source;
	
	SATAN_DEBUG("--------------------\n");
	SATAN_DEBUG("graphArea_on_event: %d\n", event.get_action());
	SATAN_DEBUG("pointer count: %d\n", event.get_pointer_count());
	SATAN_DEBUG("    action id: %d\n", event.get_pointer_id(event.get_action_index()));


	float x, y;		
	/* scale by screen resolution, and complete svg offset */
	x = (event.get_x() - ctx->svg_hztl_offset) * ctx->screen_resolution;
	y = (event.get_y() - ctx->svg_vtcl_offset) * ctx->screen_resolution;

	bool scroll_event = false;
	
	switch(ctx->current_mode) {
	case scroll_and_zoom: { 
		static bool ignore_scroll = false;

		scroll_event = ctx->sgd->on_touch_event(event);
		SATAN_DEBUG("   scroll_event: %s\n", scroll_event ? "true" : "false");

		if((!(scroll_event)) && (!ignore_scroll)) {
			static int scroll_start;
			double scroll;
			switch(event.get_action()) {
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
				break;
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
				scroll_start = event.get_x();
				break;
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
				scroll = event.get_x() - scroll_start;
				ctx->offset += scroll * ctx->screen_resolution;
				if(ctx->offset > 0.0) ctx->offset = 0.0;
				scroll_start = event.get_x();
				break;
			}
		} else {
			ignore_scroll = true;
			switch(event.get_action()) {
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
				break;
			case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
				// when the last finger is lifted, break the ignore_scroll lock
				ignore_scroll = false;
				SATAN_DEBUG(" reset ignore_scroll!\n");
				break;
			}
		}
	}
		break;

	case add_point: {
		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			/* ignore */
			break;
			
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			/* show the "oneSinglePoint" graphic, then fall through */
			ctx->oneSinglePoint_element->set_display("inline");
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			/* move the "oneSinglePoint" graphic */
		{
			KammoGUI::SVGCanvas::SVGMatrix mtrx = ctx->oneSinglePoint_matrix; // copy the original matrix
			// translate initial offset
			mtrx.translate(x, y);
			// transform the current row
			ctx->oneSinglePoint_element->set_transform(mtrx);
		}
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			/* hide the "oneSinglePoint" graphic */
			ctx->oneSinglePoint_element->set_display("none");

			/* switch back to scroll and zoom mode */
			ctx->current_mode = scroll_and_zoom;

			/* multiply by resolution */
			x -= ctx->graphArea_viewPort.x;
			x -= ctx->offset;

			y -= ctx->graphArea_viewPort.y;

			x /= (ctx->pixels_per_tick);
			y  = ((765.0 - y) / 765.0);
			if(y > 1.0) y = 1.0;
			if(y < 0.0) y = 0.0;
			y *= ((float)0x7fff);

			int x_i = (int)x;
			int y_i = (int)y;

			SATAN_DEBUG("     set (%d, %d) to %d.\n", PAD_TIME_LINE(x_i), PAD_TIME_TICK(x_i), y_i);

			try {
				ctx->current_envelope.set_control_point(PAD_TIME_LINE(x_i), PAD_TIME_TICK(x_i), y_i);
				
				ctx->update_envelope();
				ctx->generate_envelope_svg();
			} catch(jException e) {
				SATAN_DEBUG("Received exception : %s\n", e.message.c_str());
				exit(0);
			}
			break;
		}
	}
		break;		
	case add_freehand: {
		static std::pair<int, int>first_coord;

		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			/* ignore */
			break;
			
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			/* show the "oneSinglePoint" graphic, then fall through */
			ctx->oneSinglePoint_element->set_display("inline");

			/* remember the first coordinates */
			first_coord = ctx->get_envelope_coordinates(x, y);
			
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			/* move the "oneSinglePoint" graphic */
		{
			KammoGUI::SVGCanvas::SVGMatrix mtrx = ctx->oneSinglePoint_matrix; // copy the original matrix
			// translate initial offset
			mtrx.translate(x, y);
			// transform the current row
			ctx->oneSinglePoint_element->set_transform(mtrx);

			std::pair<int, int>second_coord;

			second_coord = ctx->get_envelope_coordinates(x, y);

			/* update the envelope, and graphics */
			try {
				ctx->current_envelope.set_control_point_line(
					PAD_TIME_LINE(first_coord.first), PAD_TIME_TICK(first_coord.first), first_coord.second,
					PAD_TIME_LINE(second_coord.first), PAD_TIME_TICK(second_coord.first), second_coord.second
					);
				
				ctx->update_envelope();
				ctx->generate_envelope_svg();
			} catch(jException e) {
				SATAN_DEBUG("Received exception : %s\n", e.message.c_str());
				exit(0);
			}

			/* change first coord to this one */
			first_coord = second_coord;
			
		}
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			/* hide the "oneSinglePoint" graphic */
			ctx->oneSinglePoint_element->set_display("none");

			/* switch back to scroll and zoom mode */
			ctx->current_mode = scroll_and_zoom;

			break;
		}
	}
		break;		
	case add_line: {
		static float first_x, first_y;
		
		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			/* ignore */
			break;
			
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			/* show the needed graphics */
			ctx->oneSinglePoint_element->set_display("inline");
			ctx->oneSingleLineFirstPoint_element->set_display("inline");
			ctx->oneSingleLineBody_element->set_display("inline");

			/* move the first point in place */
			{
				KammoGUI::SVGCanvas::SVGMatrix mtrx = ctx->oneSingleLineFirstPoint_matrix; // copy the original matrix
				// translate initial offset
				mtrx.translate(x, y);
				// transform the current row
				ctx->oneSingleLineFirstPoint_element->set_transform(mtrx);
			}

			/* remember coords */
			first_x = x; first_y = y;
			
			/* then fall through to next case */
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			/* move the second point */
		{			
			KammoGUI::SVGCanvas::SVGMatrix mtrx = ctx->oneSinglePoint_matrix; // copy the original matrix
			// translate initial offset
			mtrx.translate(x, y);
			// transform the current row
			ctx->oneSinglePoint_element->set_transform(mtrx);
		}
		/* move the line */
		{
			ctx->oneSingleLine_element->set_line_coords(first_x, first_y, x, y);
		}
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			/* hide the graphics */
			ctx->oneSinglePoint_element->set_display("none");
			ctx->oneSingleLineFirstPoint_element->set_display("none");
			ctx->oneSingleLineBody_element->set_display("none");

			/* switch back to scroll and zoom mode */
			ctx->current_mode = scroll_and_zoom;

			/* calculate envelope coordinates */
			std::pair<int, int> first_coord, second_coord;

			first_coord = ctx->get_envelope_coordinates(first_x, first_y);
			second_coord = ctx->get_envelope_coordinates(x, y);
			
			// make sure we do left to right...
			if(first_coord.first > second_coord.first) {
				std::pair<int, int> temporary = first_coord;
				first_coord = second_coord;
				second_coord = temporary;
			}
			
			/* update the envelope, and graphics */
			try {
				ctx->current_envelope.set_control_point_line(
					PAD_TIME_LINE(first_coord.first), PAD_TIME_TICK(first_coord.first), first_coord.second,
					PAD_TIME_LINE(second_coord.first), PAD_TIME_TICK(second_coord.first), second_coord.second
					);
				
				ctx->update_envelope();
				ctx->generate_envelope_svg();
			} catch(jException e) {
				SATAN_DEBUG("Received exception : %s\n", e.message.c_str());
				exit(0);
			}
			
			break;
		}
	}
		break;		
	case delete_point: {
		static float first_x, first_y;
		
		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			/* ignore */
			break;
			
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			/* show the needed graphics */
			ctx->oneSinglePoint_element->set_display("inline");
			ctx->oneSingleLineFirstPoint_element->set_display("inline");

			/* move the first point in place */
			{
				KammoGUI::SVGCanvas::SVGMatrix mtrx = ctx->oneSingleLineFirstPoint_matrix; // copy the original matrix
				// translate initial offset
				mtrx.translate(x, y);
				// transform the current row
				ctx->oneSingleLineFirstPoint_element->set_transform(mtrx);
			}

			/* remember coords */
			first_x = x; first_y = y;
			
			/* then fall through to next case */
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			/* move the second point */
		{			
			KammoGUI::SVGCanvas::SVGMatrix mtrx = ctx->oneSinglePoint_matrix; // copy the original matrix
			// translate initial offset
			mtrx.translate(x, y);
			// transform the current row
			ctx->oneSinglePoint_element->set_transform(mtrx);
		}
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			/* hide the graphics */
			ctx->oneSinglePoint_element->set_display("none");
			ctx->oneSingleLineFirstPoint_element->set_display("none");

			/* switch back to scroll and zoom mode */
			ctx->current_mode = scroll_and_zoom;

			/* calculate envelope coordinates */
			std::pair<int, int> first_coord, second_coord;

			first_coord = ctx->get_envelope_coordinates(first_x, first_y);
			second_coord = ctx->get_envelope_coordinates(x, y);
			
			// make sure we do left to right...
			if(first_coord.first > second_coord.first) {
				std::pair<int, int> temporary = first_coord;
				first_coord = second_coord;
				second_coord = temporary;
			}
			
			/* update the envelope, and graphics */
			try {
				ctx->current_envelope.delete_control_point_range(
					PAD_TIME_LINE(first_coord.first), PAD_TIME_TICK(first_coord.first),
					PAD_TIME_LINE(second_coord.first), PAD_TIME_TICK(second_coord.first)
					);
				
				ctx->update_envelope();
				ctx->generate_envelope_svg();
			} catch(jException e) {
				SATAN_DEBUG("Received exception : %s\n", e.message.c_str());
				exit(0);
			}
			
			break;
		}
	}
		break;		
	} 
}

void ControllerEnvelope::controlPoint_on_event(KammoGUI::SVGCanvas::SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
					       const KammoGUI::SVGCanvas::MotionEvent &event) {
	ControllerEnvelope *ctx = (ControllerEnvelope *)source;
	ControlPoint *cont_p = (ControlPoint *)e_ref;
	
	SATAN_DEBUG("controlPoint_on_event: %d\n", event.get_action());

	float x, y;

	/* scale by screen resolution, and complete svg offset */
	x = (event.get_x() - ctx->svg_hztl_offset) * ctx->screen_resolution;
	y = (event.get_y() - ctx->svg_vtcl_offset) * ctx->screen_resolution;
	
	switch(ctx->current_mode) {
	case scroll_and_zoom: {
		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			/* ignore */
			break;
			
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
		{
			/* hide the real controlPoint element */
			e_ref->set_display("none");		
			/* show the "oneSinglePoint" graphic, then fall through */
			ctx->oneSinglePoint_element->set_display("inline");
		}
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			/* move the "oneSinglePoint" graphic */
		{
			KammoGUI::SVGCanvas::SVGMatrix mtrx = ctx->oneSinglePoint_matrix; // copy the original matrix
			// translate initial offset
			mtrx.translate(x, y);
			// transform the current row
			ctx->oneSinglePoint_element->set_transform(mtrx);
		}
		break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			/* switch back to scroll and zoom mode */
			ctx->current_mode = scroll_and_zoom;
			
			/* hide the "oneSinglePoint" graphic */
			ctx->oneSinglePoint_element->set_display("none");
			
			/* delete the original point */
			try {
				ctx->current_envelope.delete_control_point(PAD_TIME_LINE(cont_p->lineNtick),
									   PAD_TIME_TICK(cont_p->lineNtick));
			} catch(jException e) {
				SATAN_DEBUG("Received exception : %s\n", e.message.c_str());
				exit(0);
			}
			
			/* calculate the coords for the new point */
			x -= ctx->graphArea_viewPort.x;
			x -= ctx->offset;
			
			y -= ctx->graphArea_viewPort.y;
			
			x /= (ctx->pixels_per_tick);
			SATAN_DEBUG(" *** add point at pixel %f, %f (%f, %f)\n", x, y, ctx->graphArea_viewPort.x, ctx->graphArea_viewPort.y);
			y  = ((765.0 - y) / 765.0);
			if(y > 1.0) y = 1.0;
			if(y < 0.0) y = 0.0;
			y *= ((float)0x7fff);
			
			SATAN_DEBUG(" add point at pixel %f, %f (%f, %f)\n", x, y, ctx->graphArea_viewPort.x, ctx->graphArea_viewPort.y);
			SATAN_DEBUG("    height: %f\n", ctx->graphArea_viewPort.height);
			
			int x_i = (int)x;
			int y_i = (int)y;
			
			/* add the new point */
			SATAN_DEBUG("     set (%d, %d) to %d.\n", PAD_TIME_LINE(x_i), PAD_TIME_TICK(x_i), y_i);
			
			try {
				ctx->current_envelope.set_control_point(PAD_TIME_LINE(x_i), PAD_TIME_TICK(x_i), y_i);
				
				ctx->update_envelope();
				ctx->generate_envelope_svg();
			} catch(jException e) {
				SATAN_DEBUG("Received exception : %s\n", e.message.c_str());
				exit(0);
			}
			break;
		}
	}
		break;

	case delete_point: {
		static float start_x, start_y;
		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			/* ignore */
			break;
			
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			/* get start x and y */
			start_x = x;
			start_y = y;
		break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			/* switch back to scroll and zoom mode */
			ctx->current_mode = scroll_and_zoom;

			x = start_x - x;
			y = start_y - y;
			x = x < 0 ? -x : x;
			y = y < 0 ? -y : y;

			if(x < 10 && y < 10) {
				/* delete the original point */
				try {
					ctx->current_envelope.delete_control_point(PAD_TIME_LINE(cont_p->lineNtick),
										   PAD_TIME_TICK(cont_p->lineNtick));
				} catch(jException e) {
					SATAN_DEBUG("Received exception : %s\n", e.message.c_str());
					exit(0);
				}

				/* refresh graphics */
				try {
					ctx->update_envelope();
					ctx->generate_envelope_svg();
				} catch(jException e) {
					SATAN_DEBUG("Received exception : %s\n", e.message.c_str());
					exit(0);
				}
			}
			break;
		}
	}
		break;
		
	case add_point:
	case add_line:
	case add_freehand:
	default:
		/* ignore */
		break;
	}
}

bool ControllerEnvelope::on_scale(KammoGUI::ScaleGestureDetector *detector) {
	zoom_factor *= detector->get_scale_factor();
	if(zoom_factor < 0.6) zoom_factor = 0.6;

	offset *= detector->get_scale_factor();
	
	return true;
}

bool ControllerEnvelope::on_scale_begin(KammoGUI::ScaleGestureDetector *detector) { return true; }
void ControllerEnvelope::on_scale_end(KammoGUI::ScaleGestureDetector *detector) { }

void ControllerEnvelope::clear_envelope_svg() {
	for(auto k : envelope_line) {
		k->drop_element();
		delete k;
	}
	envelope_line.clear();

	for(auto k : envelope_node) {
		k->drop_element();
		delete k;
	}
	envelope_node.clear();
	
}

void ControllerEnvelope::generate_envelope_svg() {
	clear_envelope_svg();

	int k;

	const std::map<int, int> &envelope = current_envelope.get_control_points();
	std::map<int, int>::const_iterator l;
	
	for(l  = envelope.begin(), k = 0;
	    l != envelope.end();
	    l++, k++) {
		{
			std::stringstream new_id;
			new_id << "envelopeLine_" << k;
			
			std::stringstream ss;			
			ss << "<line id=\"" << new_id.str() << "\" "
			   << " style=\"fill:none;stroke:#b4b4b4;stroke-width:2;stroke-miterlimit:4;stroke-opacity:1;stroke-dasharray:none\" "
			   << " x1=\"0\" \n"
			   << " y1=\"0\" \n"
			   << " x2=\"0\" \n"
			   << " y2=\"0\" \n"
			   << "/>\n";			
			graphArea_element->add_svg_child(ss.str());

			KammoGUI::SVGCanvas::ElementReference *new_line =
				new KammoGUI::SVGCanvas::ElementReference(this, new_id.str());

			envelope_line.push_back(new_line);

		}
		{
			std::stringstream new_id;					
			new_id << "envNodeClone_" << k;

			std::stringstream ss;			
			ss << "<use id=\"" << new_id.str() << "\" "
			   << " xlink:href=\"#envelopeNode\" \n"
			   << "/>\n";			
			graphArea_element->add_svg_child(ss.str());
			
			ControlPoint *new_node = new ControlPoint(this, new_id.str(), (*l).first);

			envelope_node.push_back(new_node);

			new_node->set_event_handler(controlPoint_on_event);
		}		
	}
}

void ControllerEnvelope::update_envelope() {
	SATAN_DEBUG("Update envelope.\n");
	if(!m_seq || controller_name == "") return;
	SATAN_DEBUG("    yes, we got it.\n");
	if(actual_envelope) {
		SATAN_DEBUG("  time to update_controller_envelope\n");
		m_seq->update_controller_envelope(actual_envelope, &current_envelope);
	}
}

void ControllerEnvelope::refresh_enable_toggle() {
	KammoGUI::SVGCanvas::SVGMatrix mtrx;
	mtrx.init_identity();

	// translate initial offset - 80.0 is hardcoded.. :(
	mtrx.translate(current_envelope.enabled ? 80.0 : 0.0, 0.0);
	
	envelopeEnable_element->set_transform(mtrx);		
}

bool ControllerEnvelope::refresh_envelope() {
	SATAN_DEBUG(" refresh_envelope() %p, %s\n", m_seq, controller_name.c_str());
	if(!m_seq || controller_name == "") return false;

	actual_envelope = NULL;
	try {
		actual_envelope = m_seq->get_controller_envelope(controller_name);
		current_envelope.set_to(actual_envelope);
	} catch(MachineSequencer::NoSuchController e) {
		return false;
	}

	SATAN_DEBUG(" calling generate_envelope_svg\n");
	generate_envelope_svg();
	refresh_enable_toggle();

	return true;
}

void ControllerEnvelope::set_controller_name(const std::string &name) {
	KammoGUI::SVGCanvas::ElementReference controller_ref(this, "controllerId");	
	controller_ref.set_text_content(name);	
	controller_name = name;
}

void ControllerEnvelope::listview_callback(void *context, bool row_selected, int row_index, const std::string &text) {
	ControllerEnvelope *ctx = (ControllerEnvelope *)context;
		
	if(row_selected) {
		switch(ctx->current_selector) {
		case selecting_controller:
		{
			ctx->set_controller_name(text);
		}
		break;
		
		default:
		case not_selecting:
			/* ignore */
			break;
		}
	}

	ctx->current_selector = not_selecting;
	(void) ctx->refresh_envelope(); // ignore return value here
}

void ControllerEnvelope::button_on_event(KammoGUI::SVGCanvas::SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
					   const KammoGUI::SVGCanvas::MotionEvent &event) {
	SATAN_DEBUG("event trigger for active element - %s\n", e_ref->get_id().c_str());

	ControllerEnvelope *ctx = (ControllerEnvelope *)source;
	
	{
		float x = event.get_x();
		float y = event.get_y();

		static float start_x, start_y;
		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			start_x = x;
			start_y = y;
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			x = start_x - x;
			y = start_y - y;
			x = x < 0 ? -x : x;
			y = y < 0 ? -y : y;
			SATAN_DEBUG("diff: %f, %f (%s)\n", x, y, e_ref->get_id().c_str());
			if(e_ref->get_id() == "envelopeEnable") {
				// toggle envelope enable
				ctx->current_envelope.enabled = ctx->current_envelope.enabled ? false : true;
				// update actual envelope
				ctx->update_envelope();
				// move the toggle, graphically
				ctx->refresh_enable_toggle();
			} else if(x < 10 && y < 10) {
				if(e_ref->get_id() == "selectController") {
					if(ctx->m_seq) {
						ctx->listView->clear();

						ctx->current_selector = selecting_controller;

						std::set<std::string>::iterator k;
						std::set<std::string> ctrs = ctx->m_seq->available_midi_controllers();
						for(k = ctrs.begin(); k != ctrs.end(); k++) {
							SATAN_DEBUG("Adding controller %s\n", (*k).c_str());
							ctx->listView->add_row((*k));
						}
						ctx->listView->select_from_list("Select controller", ctx, listview_callback);
					}
				} else if(e_ref->get_id() == "addPoint") {
					ctx->current_mode = add_point;
					
				} else if(e_ref->get_id() == "addLine") {
					ctx->current_mode = add_line;
					
				} else if(e_ref->get_id() == "addFreehand") {
					ctx->current_mode = add_freehand;
					
				} else if(e_ref->get_id() == "deletePoint") {
					ctx->current_mode = delete_point;
					
				} 
			}
			break;
		}
	}
}

#define QUALIFIED_DELETE(a) {if(a) { delete a; a = NULL; }}

ControllerEnvelope::~ControllerEnvelope() {
	std::vector<KammoGUI::SVGCanvas::ElementReference *>::iterator k;
	for(k = lineMarker.begin(); k != lineMarker.end(); k++) {
		delete (*k);
	}

	clear_envelope_svg();
	
	QUALIFIED_DELETE(sgd);

	QUALIFIED_DELETE(graphArea_element);
	
	QUALIFIED_DELETE(envelopeEnable_element);
	QUALIFIED_DELETE(selectController_element);

	QUALIFIED_DELETE(addPoint_element);
	QUALIFIED_DELETE(addLine_element);
	QUALIFIED_DELETE(addFreehand_element);
	QUALIFIED_DELETE(deletePoint_element);

	QUALIFIED_DELETE(oneSinglePoint_element);

	QUALIFIED_DELETE(oneSingleLineFirstPoint_element);
	QUALIFIED_DELETE(oneSingleLineBody_element);
	QUALIFIED_DELETE(oneSingleLine_element);

	QUALIFIED_DELETE(listView);
}

ControllerEnvelope::ControllerEnvelope(KammoGUI::SVGCanvas *cnvs, std::string fname) : SVGDocument(fname, cnvs), actual_envelope(NULL), current_mode(scroll_and_zoom), zoom_factor(1.0), offset(0.0) {

	{ // get the envelopeEnable toggle and attach the event listener
		envelopeEnable_element = new KammoGUI::SVGCanvas::ElementReference(this, "envelopeEnable");
		envelopeEnable_element->set_event_handler(button_on_event);
	}
	
	{ // get the controller selector and attach the event listener
		selectController_element = new KammoGUI::SVGCanvas::ElementReference(this, "selectController");
		selectController_element->set_event_handler(button_on_event);
	}
	
	{ // get the oneSinglePoint element, and copy it's matrix
		oneSinglePoint_element = new KammoGUI::SVGCanvas::ElementReference(this, "oneSinglePoint");
		oneSinglePoint_element->get_transform(oneSinglePoint_matrix);
	}

	{ // get the oneSingleLineFirstPoint element, and copy it's matrix
		oneSingleLineFirstPoint_element = new KammoGUI::SVGCanvas::ElementReference(this, "oneSingleLineFirstPoint");
		oneSingleLineFirstPoint_element->get_transform(oneSingleLineFirstPoint_matrix);
	}

	{ // get the oneSingleLineBody element
		oneSingleLineBody_element = new KammoGUI::SVGCanvas::ElementReference(this, "oneSingleLineBody");
	}

	{ // get the oneSingleLine element
		oneSingleLine_element = new KammoGUI::SVGCanvas::ElementReference(this, "oneSingleLine");
	}

	{ // get the addPoint button and attach the event listener
		addPoint_element = new KammoGUI::SVGCanvas::ElementReference(this, "addPoint");
		addPoint_element->set_event_handler(button_on_event);
	}
	
	{ // get the addLine button and attach the event listener
		addLine_element = new KammoGUI::SVGCanvas::ElementReference(this, "addLine");
		addLine_element->set_event_handler(button_on_event);
	}
	
	{ // get the addFreehand button and attach the event listener
		addFreehand_element = new KammoGUI::SVGCanvas::ElementReference(this, "addFreehand");
		addFreehand_element->set_event_handler(button_on_event);
	}
	
	{ // get the deletePoint button and attach the event listener
		deletePoint_element = new KammoGUI::SVGCanvas::ElementReference(this, "deletePoint");
		deletePoint_element->set_event_handler(button_on_event);
	}
	
	{ // create a SVGListView object
		listView = new ListView(cnvs);
	}
	
	graphArea_element = new KammoGUI::SVGCanvas::ElementReference(this, "graphArea");
	graphArea_element->set_event_handler(graphArea_on_event);

	sgd = new KammoGUI::ScaleGestureDetector(this);
	
	int k;
	for(k = 0; k < (14 * MACHINE_TICKS_PER_LINE); k++) {
		std::stringstream ss;		

		if(!(k % MACHINE_TICKS_PER_LINE)) {
			ss << "<g id=\"lineMarker_" << k << "\" >\n"
			   << "    <use xlink:href=\"#lineMark\" x=\"0\" y=\"0\" />\n"
			   << "    <text\n"
			   << "       id=\"lineMarkerText_" << k << "\"\n"
			   << "       x=\"0\"\n"
			   << "       y=\"800\"\n"
			   << "       style=\"font-size:40px;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;line-height:125%;letter-spacing:0px;word-spacing:0px;fill:#e9e9e9;fill-opacity:1;stroke:none;font-family:Roboto;-inkscape-font-specification:Roboto\"\n"
			   << "       xml:space=\"preserve\"\n"
			   << "       class=\"markText\">" << k << "</text>\n"
			   << "</g>\n";
		} else {
			ss << "<g id=\"lineMarker_" << k << "\" >\n"
			   << "     <use xlink:href=\"#tickMark\" x=\"0\" y=\"0\" />\n"
			   << "</g>\n";
		}
		graphArea_element->add_svg_child(ss.str());

		SATAN_DEBUG("Added the following element: %s\n", ss.str().c_str());

		std::stringstream name;
		name << "lineMarker_" << k;
		KammoGUI::SVGCanvas::ElementReference *e_ref = new KammoGUI::SVGCanvas::ElementReference(this, name.str());

		lineMarker.push_back(e_ref);
	}
}

void ControllerEnvelope::set_machine_sequencer(MachineSequencer *_m_seq) {
	m_seq = _m_seq;

	if(!refresh_envelope()) {
		/* the selected envelope is not available for this machine, set to empty */
		set_controller_name("");
	}
}

/***************************
 *
 *  Kamoflage Event Declaration
 *
 ***************************/

static ControllerEnvelope *c_env = NULL;

KammoEventHandler_Declare(ControllerEnvelopeHandler,"controllerEnvelope:showControllerEnvelope");

virtual void on_init(KammoGUI::Widget *wid) {
	if(wid->get_id() == "controllerEnvelope") {
		KammoGUI::SVGCanvas *cnvs = (KammoGUI::SVGCanvas *)wid;		
		cnvs->set_bg_color(1.0, 1.0, 1.0);
		c_env = new ControllerEnvelope(cnvs, std::string(SVGLoader::get_svg_directory() + "/controllerEnvelope.svg"));
	}
}

virtual void on_user_event(KammoGUI::UserEvent *ue, std::map<std::string, void *> args) {
	MachineSequencer *mseq = NULL;

	if(args.find("MachineSequencer") != args.end())
		mseq = (MachineSequencer *)args["MachineSequencer"];
	
	if(mseq != NULL) {
		c_env->set_machine_sequencer(mseq);
	}	
}

KammoEventHandler_Instance(ControllerEnvelopeHandler);
