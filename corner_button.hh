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

#ifndef CORNER_CLASS
#define CORNER_CLASS

#include <kamogui.hh>
#include <functional>

class CornerButton : public KammoGUI::SVGCanvas::SVGDocument{
public:
	enum WhatCorner {top_left, top_right, bottom_left, bottom_right};

private:
	KammoGUI::SVGCanvas::SVGMatrix base_transform_t;
	KammoGUI::SVGCanvas::ElementReference button;
	WhatCorner my_corner;
	
	class Transition : public KammoGUI::Animation {
	private:
		CornerButton *ctx;
		std::function<void(CornerButton *context, float progress)> callback;

	public:
		Transition(CornerButton *context, std::function<void(CornerButton *context, float progress)> callback);
		virtual void new_frame(float progress);
		virtual void on_touch_event();
	};

	KammoGUI::SVGCanvas::ElementReference elref;
	
	double first_selection_x, first_selection_y;
	bool is_a_tap;

	bool hidden;
	double offset_factor, offset_target_x, offset_target_y;
	
	static void transition_progressed(CornerButton *ctx, float progress);	
	void run_transition();

	std::function<void()> callback_function;

	static void on_event(KammoGUI::SVGCanvas::SVGDocument *source,
			     KammoGUI::SVGCanvas::ElementReference *e_ref,
			     const KammoGUI::SVGCanvas::MotionEvent &event);
public:
	
	CornerButton(KammoGUI::SVGCanvas *cnv, const std::string &svg_file, WhatCorner corner);
	~CornerButton();

	virtual void on_render();
	virtual void on_resize();

	void show();
	void hide();

	void set_select_callback(std::function<void()> callback_function);
};

#endif

