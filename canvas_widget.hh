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

#ifndef CANVAS_WIDGET
#define CANVAS_WIDGET

class CanvasDontCallBaseConstructor {
};

class CanvasWidget;

class CanvasWidgetContext {
private:
	friend class CanvasWidget;

	// static variable set when the init_statics() function is called
	// the first time. When this is set, init_statics() won't do anything
	// the following times it's called
	static bool init_statics_done;

	// disable input events
	bool events_disabled;

	// current layer (this is used as a bit-pattern, so
	// active_layer will be logically ANDed with the layer
	// of a widget) (a widget can therefore be on multiple layers..)
	int active_layers;

	std::vector<CanvasWidget *> __widget;
	CanvasWidget *__widget_in_focus;

	static std::map<int, KammoGUI::Canvas::SVGDefinition *> symbol;
	std::map<int, KammoGUI::Canvas::SVGBob ** > prerendered_symbol;

	static void locate_SVG_directory();
	static void init_statics();

	float width_in_inches;
	float height_in_inches;

public:
	static std::string svg_directory;

	CanvasWidgetContext(KammoGUI::Canvas *canvas_object, float width_in_inches, float height_in_inches);

	void add_widget(CanvasWidget *wid);
	void drop_widget(CanvasWidget *wid);

	KammoGUI::Canvas *__cnv;
	int __width;
	int __height;

	void enable_events();
	void disable_events();

	void draw_symbols(
		int x1, int y1,
		int x2, int y2,
		int fx, int fy,
		char *symbols);
	void draw_string(std::string str,
			 int x1, int y1,
			 int x2, int y2,
			 std::string color);

	void set_active_layers(int _layers);
	int get_active_layers();

	/* static callback entries, called by KammoGUI::Canvas */
	static void canvas_widget_expose_cb(void *callback_data);
	static void canvas_widget_resize_cb(void *callback_data,
					    int width, int height);
	static void canvas_widget_event_cb(void *callback_data, KammoGUI::canvasEvent_t ce,
					   int x, int y);
	static float canvas_widget_measure_inches_cb(void *callback_data, bool measureWidth);

	/* per-object callback entries, called by the static equivalents */
	void canvas_widget_expose();
	void canvas_widget_resize(int width, int height);
	void canvas_widget_event(KammoGUI::canvasEvent_t ce,
				 int x, int y);

};

class CanvasWidget {
private:
	friend class CanvasWidgetContext;

	CanvasWidget();
protected:
	CanvasWidgetContext *context;

	// percent of parent canvas, defined at construction
	float vx1, vy1, vx2, vy2;

	int layers; // layer = -1, always on, otherwise only show when the specified layer is active
	KammoGUI::Canvas *__cnv;

	bool do_ignore_events;

	// in pixels, recalculated
	int x1, y1, x2, y2;

	CanvasWidget(CanvasWidgetContext *cntxt,
		     float _vx1,
		     float _vy1,
		     float _vx2,
		     float _vy2,
		     int layer);

public:
	void ignore_events();
	void unignore_events();

	virtual ~CanvasWidget();

	void resize();

	virtual void resized() = 0;
	virtual void expose() = 0;
	virtual void on_event(KammoGUI::canvasEvent_t ce, int x, int y) = 0;

	void redraw();
};

#endif
