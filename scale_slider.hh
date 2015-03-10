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

#ifndef SCALE_SLIDER_CLASS
#define SCALE_SLIDER_CLASS

#include <kamogui.hh>
#include <functional>

class ScaleSlider : public KammoGUI::SVGCanvas::SVGDocument {
public:
	class ScaleSliderChangedListener {
	public:
		virtual void on_scale_slider_changed(ScaleSlider *scl, double new_value) = 0;
	};
	

private:
	class Transition : public KammoGUI::Animation {
	private:
		ScaleSlider *sc;
		std::function<void(ScaleSlider *context, float progress)> callback;

	public:
		Transition(ScaleSlider *sc, std::function<void(ScaleSlider *context, float progress)> callback);
		virtual void new_frame(float progress);
		virtual void on_touch_event();
	};

	ScaleSliderChangedListener *listener;
	double value, last_value;

	// graphical data
	KammoGUI::SVGCanvas::SVGRect document_size; // the size of the document as loaded
	KammoGUI::SVGCanvas::SVGRect knob_size; // visual size of the knob (in document coordinates, not screen)
	
	double x, y, width, height; // the viewport which we should squeeze the document into (animate TO)
	double initial_x, initial_y, initial_width, initial_height; // the viewport which we animate FROM
	double translate_x, translate_y, scale; // current translation and scale

	double last_y;
	
	bool transition_to_active_state;
	
	KammoGUI::SVGCanvas::ElementReference *scale_knob;

	KammoGUI::SVGCanvas::ElementReference front_text;
	KammoGUI::SVGCanvas::ElementReference shade_text;
	KammoGUI::SVGCanvas::ElementReference front_label;
	KammoGUI::SVGCanvas::ElementReference shade_label;

	void interpolate(double value);
	static void transition_progressed(ScaleSlider *sc, float progress);
	
	static void on_event(KammoGUI::SVGCanvas::SVGDocument *source,
			     KammoGUI::SVGCanvas::ElementReference *e_ref,
			     const KammoGUI::SVGCanvas::MotionEvent &event);
	
public:
	ScaleSlider(KammoGUI::SVGCanvas *cnv);
	~ScaleSlider();

	void show(
		double initial_x, double initial_y, double initial_width, double initial_height,
		double x, double y, double width, double height);
	void hide(double final_x, double final_y, double final_width, double final_height);

	void set_label(const std::string &label);
	
	void set_value(double val);
	double get_value();

	void set_listener(ScaleSliderChangedListener *scl);
	
	virtual void on_resize();
	virtual void on_render();
};

#endif
