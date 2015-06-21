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

#ifndef LOGOSCREEN_CLASS
#define LOGOSCREEN_CLASS

#include <kamogui.hh>

#include "listview.hh"

class LogoScreen : public KammoGUI::SVGCanvas::SVGDocument {
private:
	class ThumpAnimation : public KammoGUI::Animation {
	private:
		static ThumpAnimation *active;
		double *thump_offset;

		ThumpAnimation(double *_thump_offset, float duration);
	public:
		// animate the thump_offset double
		static ThumpAnimation *start_thump(double *_thump_offset, float duration);

		~ThumpAnimation();

		virtual void new_frame(float progress);
		virtual void on_touch_event();
	};

	bool thump_in_progress = false;
	double thump_offset = 0.0;

	bool logo_base_got = false;
	KammoGUI::SVGCanvas::SVGMatrix knob_base_t;

	KammoGUI::SVGCanvas::SVGMatrix transform_m;

	KammoGUI::SVGCanvas::ElementReference *knobBody_element;
	KammoGUI::SVGCanvas::ElementReference *google_element;
	KammoGUI::SVGCanvas::ElementReference *start_element;
	KammoGUI::SVGCanvas::ElementReference *network_element;

	std::string selected_server = "localhost";
	int selected_port = -1;

	ListView server_list;

	void start_vuknob(bool start_with_jam_view);
	void select_server(std::function<void()> on_select_callback);

	static void element_on_event(KammoGUI::SVGCanvas::SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
				     const KammoGUI::SVGCanvas::MotionEvent &event);
public:
	LogoScreen(bool hide_network_element, KammoGUI::SVGCanvas *cnv, std::string file_name);

	virtual void on_resize();
	virtual void on_render();
};

#endif
