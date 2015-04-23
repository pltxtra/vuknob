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

#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
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

#include "canvas_widget.hh"
#include "common.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#define MAX_SYMBOLS_IN_ARRAY 128

/***************************
 *
 *  Class CanvasWidgetContext
 *
 ***************************/

bool CanvasWidgetContext::init_statics_done = false;
std::string CanvasWidgetContext::svg_directory;
std::map<int, KammoGUI::Canvas::SVGDefinition *> CanvasWidgetContext::symbol;

CanvasWidgetContext::CanvasWidgetContext(KammoGUI::Canvas *canvas_object, float wi, float hi) :
	events_disabled(true), active_layers(0x1), __widget_in_focus(NULL),
	width_in_inches(wi), height_in_inches(hi), __cnv(canvas_object)
{
	init_statics();

	__cnv->set_callbacks(
		this,
		CanvasWidgetContext::canvas_widget_expose_cb,
		CanvasWidgetContext::canvas_widget_resize_cb,
		CanvasWidgetContext::canvas_widget_event_cb,
		CanvasWidgetContext::canvas_widget_measure_inches_cb
		);

}

void CanvasWidgetContext::add_widget(CanvasWidget *wid) {
	__widget.push_back(wid);
}

void CanvasWidgetContext::drop_widget(CanvasWidget *wid) {
	std::vector<CanvasWidget *>::iterator i;
	for(i  = __widget.begin();
	    i != __widget.end();
	    i++) {
		if((*i) == wid) {
			__widget.erase(i);
			i = __widget.end();
			return;
		}
	}
}

void CanvasWidgetContext::locate_SVG_directory() {
#ifdef ANDROID
	const char *dir_bfr;
	dir_bfr = KAMOFLAGE_ANDROID_ROOT_DIRECTORY;
#else
	char *dir_bfr, _dir_bf[2048];
	dir_bfr = getcwd(_dir_bf, sizeof(_dir_bf));
#endif
	if(dir_bfr == NULL)
		throw jException("Failed to get current working directory.",
				 jException::syscall_error);

	std::string candidate;
	std::vector<std::string> try_list;
	std::vector<std::string>::iterator try_list_entry;

	candidate = dir_bfr;

#ifdef ANDROID
	candidate += "/app_nativedata/SVG";
#else
	candidate += "/SVG";
#endif
	try_list.push_back(candidate);

	candidate = std::string(CONFIG_DIR) + "/SVG";
	try_list.push_back(candidate);

	for(try_list_entry  = try_list.begin();
	    try_list_entry != try_list.end();
	    try_list_entry++) {

		svg_directory = *try_list_entry;
		std::cout << "Trying to read SVG files in ]" << svg_directory << "[ ...\n";

		DIR *dir = opendir(svg_directory.c_str());
		if(dir != NULL) {
			closedir(dir);
			std::cout << "SVG files found in ]" << svg_directory << "[\n";
			return;
		}
	}
	if(try_list_entry == try_list.end())
		throw jException("Failed to read SVG directory.", jException::syscall_error);
}

void CanvasWidgetContext::enable_events() {
	events_disabled = false;
}

void CanvasWidgetContext::disable_events() {
	events_disabled = true;
}

void CanvasWidgetContext::draw_symbols(
	int x1, int y1,
	int x2, int y2,
	int fx, int fy,
	char *symbols) {
	/* draw symbols */
	int i = 0, j = 0, s = 0;
	int w = (x2 - x1) / fx;
	int h = (y2 - y1) / fy;

	std::map<int, KammoGUI::Canvas::SVGBob ** >::iterator symi;

	// Try and locate prerendered symbols for this symbol width (w).
	// The prerendered symbols can be reused to draw symbols
	// of the same width later...
	symi = prerendered_symbol.find(w);
	if(symi == prerendered_symbol.end()) {
		KammoGUI::Canvas::SVGBob **symarray =
			(KammoGUI::Canvas::SVGBob **)malloc(
				sizeof(KammoGUI::Canvas::SVGBob *) * MAX_SYMBOLS_IN_ARRAY);
		if(symarray == NULL) {
			SATAN_DEBUG_("   malloc failed... :-(\n");
			exit(-1);
		}
		memset(symarray, 0,
		       sizeof(KammoGUI::Canvas::SVGBob *) * MAX_SYMBOLS_IN_ARRAY);
		prerendered_symbol[w] = symarray;
		symi = prerendered_symbol.find(w);
	}

	// OK, time to actually do the drawing
	for(s = 0; symbols[s] != '\0' && j < fy; s++, i++) {
		if(i == fx) { i = 0; j++; }

		int psymb = symbols[s];
		if(psymb >= 0) {
			if((*symi).second[psymb] == NULL) {
				(*symi).second[psymb] =
					new KammoGUI::Canvas::SVGBob(
						__cnv, symbol[psymb]);
				if((*symi).second[psymb] == NULL) {
					SATAN_DEBUG_("   new failed... :-(\n");
					exit(-1);
				}
				(*symi).second[psymb]->set_blit_size(w, h);
			}

			if(symbols[s] != ' ')
				__cnv->blit_SVGBob((*symi).second[psymb],
						   x1 + i * w, y1 + j * h);
		}
	}
}

void CanvasWidgetContext::draw_string(std::string str,
				      int x1, int y1,
				      int x2, int y2,
				      std::string color
	) {
	float w, h;
	w = x2 - x1; h = y2 - y1;
	w = w / h;
	// create SVG for the string
	ostringstream ss;
	ss << "<svg "
	   << "   width=\"" << (w * 100.0f) << "\""
	   << "   height=\"" << (100.0f) << "\""
	   << "   version=\"1.0\">"

	   << "<g>\n"
	   << "    <text\n"
	   << "       x=\"30\"\n"
	   << "       y=\"70\"\n"
	   << "       style=\"font-size:50px;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;line-height:125%;letter-spacing:0px;word-spacing:0px;fill:" << color << ";fill-opacity:1;stroke:none;font-family:Roboto\"\n"
	   << "       xml:space=\"preserve\"\n"
	   << "       >"
	   << str
	   << "</text>\n"
	   << "</g>\n"

	   << "</svg>";

	SATAN_DEBUG("draw_string: width %d, height %d\n",
		    x2 - x1, y2 - y1);

	// Create SVG Definition object
	KammoGUI::Canvas::SVGDefinition *strdef = NULL;
	try {
		strdef = KammoGUI::Canvas::SVGDefinition::from_string(ss.str());
	} catch(...) {
		/* ignore */
		SATAN_DEBUG_(" %s\n", ss.str().c_str()); fflush(0);
	}

	if(strdef != NULL) {
		// Create SVG Bob
		KammoGUI::Canvas::SVGBob *strbob =
			new KammoGUI::Canvas::SVGBob(__cnv, strdef);

		// blit the bob
		strbob->set_blit_size(x2 - x1, y2 - y1);
		__cnv->blit_SVGBob(strbob, x1, y1);

		delete strbob;
		delete strdef;
	}
}

void CanvasWidgetContext::canvas_widget_expose() {
	std::vector<CanvasWidget *>::iterator i;

	for(i  = __widget.begin();
	    i != __widget.end();
	    i++) {
		if(
			((*i)->layers & active_layers)
		) {
			(*i)->expose();
		}
	}
}

void CanvasWidgetContext::canvas_widget_expose_cb(void *callback_data) {
	CanvasWidgetContext *cwc = (CanvasWidgetContext *)callback_data;
	cwc->canvas_widget_expose();
}

void CanvasWidgetContext::canvas_widget_resize(int width, int height) {
	std::vector<CanvasWidget *>::iterator i;
	__width = width;
	__height = height;
	for(i  = __widget.begin();
	    i != __widget.end();
	    i++) {
		(*i)->resize();
		(*i)->resized();
	}

}

void CanvasWidgetContext::canvas_widget_resize_cb(void *callback_data,
						  int width, int height) {
	CanvasWidgetContext *cwc = (CanvasWidgetContext *)callback_data;
	cwc->canvas_widget_resize(width, height);
}

void CanvasWidgetContext::canvas_widget_event(KammoGUI::canvasEvent_t ce,
					      int x, int y) {
	if(events_disabled) return;

	if(ce == KammoGUI::cvButtonHold) {
		SATAN_DEBUG_("  BUTTON HOLD EVENT!\n");
	}

	if(ce == KammoGUI::cvButtonPress) {
		std::vector<CanvasWidget *>::iterator i;
		for(i  = __widget.begin();
		    i != __widget.end();
		    i++) {
			if(
				( (*i)->layers & active_layers ) &&
				(x > (*i)->x1) &&
				(y > (*i)->y1) &&
				(x < (*i)->x2) &&
				(y < (*i)->y2) &&
				((*i)->do_ignore_events == false)
				) {
				printf("  we have a widget in focus...\n");
				__widget_in_focus = (*i);
			}
		}
	}
	if(__widget_in_focus) {
		printf("   passing event to widget in focus!\n");
		__widget_in_focus->on_event(
			ce,
			x,
			y);
	}
	if(ce == KammoGUI::cvButtonRelease)
		__widget_in_focus = NULL;

}

void CanvasWidgetContext::canvas_widget_event_cb(void *callback_data, KammoGUI::canvasEvent_t ce,
						 int x, int y) {
	CanvasWidgetContext *cwc = (CanvasWidgetContext *)callback_data;
	cwc->canvas_widget_event(ce, x, y);
}

float CanvasWidgetContext::canvas_widget_measure_inches_cb(void *callback_data, bool measureWidth) {
	CanvasWidgetContext *cwc = (CanvasWidgetContext *)callback_data;
	if(measureWidth)
		return cwc->width_in_inches;
	return cwc->height_in_inches;
}

void CanvasWidgetContext::set_active_layers(int _layers) {
	active_layers = _layers;
}

int CanvasWidgetContext::get_active_layers() {
	return active_layers;
}

void CanvasWidgetContext::init_statics() {
	if(init_statics_done) return;

	init_statics_done = true;
	/* locate SVG directory */
	locate_SVG_directory();

	/* init symbols */

//	char *symval = (char *)"0123456789abcdef";

	for(int k = 1; k < MAX_SYMBOLS_IN_ARRAY; k++) {
		ostringstream stream;
		stream <<

"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>"
"<!-- Created with Inkscape (http://www.inkscape.org/) -->"
"<svg"
"   xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
"   xmlns:cc=\"http://creativecommons.org/ns#\""
"   xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\""
"   xmlns:svg=\"http://www.w3.org/2000/svg\""
"   xmlns=\"http://www.w3.org/2000/svg\""
"   xmlns:sodipodi=\"http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd\""
"   xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\""
"   width=\"19.641434\""
"   height=\"25.389175\""
"   id=\"svg2\""
"   sodipodi:version=\"0.32\""
"   inkscape:version=\"0.46\""
"   sodipodi:docname=\"symbol_2.svg\""
"   inkscape:output_extension=\"org.inkscape.output.svg.inkscape\""
"   version=\"1.0\">"
"  <defs"
"     id=\"defs4\">"
"    <inkscape:perspective"
"       sodipodi:type=\"inkscape:persp3d\""
"       inkscape:vp_x=\"0 : 18.603565 : 1\""
"       inkscape:vp_y=\"0 : 1000 : 0\""
"       inkscape:vp_z=\"199.99998 : 18.603565 : 1\""
"       inkscape:persp3d-origin=\"99.999992 : 12.402377 : 1\""
"       id=\"perspective4034\" />"
"  </defs>"
"  <sodipodi:namedview"
"     id=\"base\""
"     pagecolor=\"#000000\""
"     bordercolor=\"#666666\""
"     borderopacity=\"1.0\""
"     gridtolerance=\"10000\""
"     guidetolerance=\"10\""
"     objecttolerance=\"10\""
"     inkscape:pageopacity=\"1\""
"     inkscape:pageshadow=\"2\""
"     inkscape:zoom=\"23.719913\""
"     inkscape:cx=\"15.000053\""
"     inkscape:cy=\"15.45555\""
"     inkscape:document-units=\"px\""
"     inkscape:current-layer=\"layer1\""
"     showgrid=\"false\""
"     showguides=\"true\""
"     inkscape:guide-bbox=\"true\""
"     inkscape:window-width=\"1440\""
"     inkscape:window-height=\"825\""
"     inkscape:window-x=\"0\""
"     inkscape:window-y=\"25\""
"     borderlayer=\"false\""
"     showborder=\"false\""
"     inkscape:showpageshadow=\"false\">"
"    <inkscape:grid"
"       type=\"xygrid\""
"       id=\"grid5692\""
"       visible=\"true\""
"       enabled=\"true\" />"
"  </sodipodi:namedview>"
"  <g"
"     inkscape:label=\"Main Layer\""
"     inkscape:groupmode=\"layer\""
"     id=\"layer1\""
"     style=\"display:inline\""
"     transform=\"translate(-5.0385751,-1012.0988)\">"
"    <text"
"       xml:space=\"preserve\""
"       style=\"font-size:39.34976196px;font-style:normal;font-variant:normal;font-weight:bold;font-stretch:normal;text-align:start;line-height:125%;writing-mode:lr-tb;text-anchor:start;fill:#ffffff;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Liberation Mono;-inkscape-font-specification:Liberation Mono Bold\""
"       x=\"2.4965672\""
"       y=\"1075.6406\""
"       id=\"text5688\""
"       transform=\"scale(1.036774,0.9645303)\""
"       sodipodi:linespacing=\"125%\"><tspan"
"         sodipodi:role=\"line\""
"         id=\"tspan5690\""
			"         x=\"2.4965672\"";

		stream << "         y=\"1075.6406\">&#" << ((int)k) << ";</tspan></text>";

		stream <<
"    <text"
"       xml:space=\"preserve\""
"       style=\"font-size:37.60708618px;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;text-align:start;line-height:125%;writing-mode:lr-tb;text-anchor:start;fill:#ffffff;fill-opacity:1;stroke:none;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1;font-family:Liberation Mono;-inkscape-font-specification:Liberation Mono\""
"       x=\"2.6889365\""
"       y=\"1091.6981\""
"       id=\"text5694\""
"       transform=\"scale(1.0530095,0.949659)\""
"       sodipodi:linespacing=\"125%\"><tspan"
"         sodipodi:role=\"line\""
"         id=\"tspan5696\""
			"         x=\"2.6889365\"";

		stream << "         y=\"1091.6981\">&#" << ((int)k) << ";</tspan></text>";

		stream << "  </g>"
			"</svg>";
		{
			ostringstream mstream;
			mstream << " Failed to creates symbol for &#" << ((int)k) << ";";
//			char buffer[256];
//			snprintf(buffer, 255, "%s\n", mstream.str().c_str();

			KammoGUI::Canvas::SVGDefinition *symdef;

			symdef = NULL;
			try {
				symdef = KammoGUI::Canvas::SVGDefinition::from_string(stream.str());
			} catch(...) {
				/* ignore */
				SATAN_DEBUG_(" %s\n", mstream.str().c_str()); fflush(0);
			}

			if(symdef != NULL)
				symbol[k] = symdef;
		}
	}
}

/***************************
 *
 *  Class CanvasWidget
 *
 ***************************/

CanvasWidget::CanvasWidget() {
	throw CanvasDontCallBaseConstructor();
}
CanvasWidget::CanvasWidget(CanvasWidgetContext *cwc,
			   float _vx1,
			   float _vy1,
			   float _vx2,
			   float _vy2,
			   int _layers) :
	context(cwc),
	vx1(_vx1), vy1(_vy1), vx2(_vx2), vy2(_vy2), layers(_layers), __cnv(cwc->__cnv),
	do_ignore_events(false)
{
	context->add_widget(this);
	resize();
}

void CanvasWidget::ignore_events() {
	do_ignore_events = true;
}

void CanvasWidget::unignore_events() {
	do_ignore_events = false;
}

CanvasWidget::~CanvasWidget() {
	context->drop_widget(this);
}

void CanvasWidget::resize() {
	x1 = vx1 * context->__width;
	y1 = vy1 * context->__height;
	x2 = vx2 * context->__width;
	y2 = vy2 * context->__height;
}

void CanvasWidget::redraw() {
	__cnv->redraw();
}
