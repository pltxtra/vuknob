/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
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

#ifndef CONTROLLER_ENVELOPE_CLASS
#define CONTROLLER_ENVELOPE_CLASS

#include <kamogui.hh>
#include <kamogui_scale_detector.hh>

#include "machine.hh"
#include "machine_sequencer.hh"
#include "listview.hh"

class ControllerEnvelope : public KammoGUI::SVGCanvas::SVGDocument, public KammoGUI::ScaleGestureDetector::OnScaleGestureListener {
private:
	class ControlPoint : public KammoGUI::SVGCanvas::ElementReference {
	public:
		int lineNtick;
		
		ControlPoint(KammoGUI::SVGCanvas::SVGDocument *context, const std::string &id, int lineNtick);
	};
	
	static void listview_callback(void *context, bool row_selected, int row_index, const std::string &content);
	static void graphArea_on_event(SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
				       const KammoGUI::SVGCanvas::MotionEvent &event);
	static void button_on_event(SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
				    const KammoGUI::SVGCanvas::MotionEvent &event);
	static void controlPoint_on_event(SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
					  const KammoGUI::SVGCanvas::MotionEvent &event);

	virtual bool on_scale(KammoGUI::ScaleGestureDetector *detector);
	virtual bool on_scale_begin(KammoGUI::ScaleGestureDetector *detector);
	virtual void on_scale_end(KammoGUI::ScaleGestureDetector *detector);

	/* calculate proper envelope coordinates from SVG coordinates
	   - i.e. screen coordinates must be multiplied by this->screen_resolution first */
	std::pair<int, int> get_envelope_coordinates(float x, float y);

	/* update the GUI - set the controller name field */
	void set_controller_name(const std::string &name);

	// clear the svg data for the current envelope
	void clear_envelope_svg();

	// update the position of the envelopeEnable toggle
	void refresh_enable_toggle();
	
	// generate proper SVG for the current envelope (clear the previously generated SVG)
	void generate_envelope_svg();
	
	// returns true if envelope is available, false if missing
	bool refresh_envelope();	

	// updates the selected controller envelope with what we have in this editor
	void update_envelope();
	
	KammoGUI::ScaleGestureDetector *sgd;

	std::string controller_name;
	MachineSequencer *m_seq;
	MachineSequencer::ControllerEnvelope current_envelope;
	const MachineSequencer::ControllerEnvelope *actual_envelope;

	std::vector<KammoGUI::SVGCanvas::ElementReference *> envelope_line;
	std::vector<KammoGUI::SVGCanvas::ElementReference *> envelope_node;
	
	enum graphMode_t { scroll_and_zoom, add_point, add_line, add_freehand, delete_point };
	graphMode_t current_mode;
	
	enum currentSelector_t { not_selecting, selecting_controller };
	currentSelector_t current_selector;
	
	double zoom_factor, offset, screen_resolution, pixels_per_tick; // pixels_per_tick is in local SVG "pixels", actual screen pixels is horizontal_resolution * pixels_per_tick
	double svg_hztl_offset, svg_vtcl_offset; // in the local canvas, how much is the svg file offset?
	std::vector<KammoGUI::SVGCanvas::ElementReference *> lineMarker;

	KammoGUI::SVGCanvas::SVGRect           graphArea_viewPort;
	KammoGUI::SVGCanvas::ElementReference *graphArea_element;
	
	KammoGUI::SVGCanvas::ElementReference *envelopeEnable_element;
	KammoGUI::SVGCanvas::ElementReference *selectController_element;

	KammoGUI::SVGCanvas::ElementReference *addPoint_element;
	KammoGUI::SVGCanvas::ElementReference *addLine_element;
	KammoGUI::SVGCanvas::ElementReference *addFreehand_element;
	KammoGUI::SVGCanvas::ElementReference *deletePoint_element;

	KammoGUI::SVGCanvas::ElementReference *oneSingleLineFirstPoint_element;
	KammoGUI::SVGCanvas::SVGMatrix oneSingleLineFirstPoint_matrix;
	KammoGUI::SVGCanvas::ElementReference *oneSingleLineBody_element;
	KammoGUI::SVGCanvas::ElementReference *oneSingleLine_element;

	KammoGUI::SVGCanvas::ElementReference *oneSinglePoint_element;
	KammoGUI::SVGCanvas::SVGMatrix oneSinglePoint_matrix;

	ListView *listView;
public:
	ControllerEnvelope(KammoGUI::SVGCanvas *cnv, std::string file_name);
	~ControllerEnvelope();
	
	virtual void on_render();

	void set_machine_sequencer(MachineSequencer *mseq);
};

#endif
