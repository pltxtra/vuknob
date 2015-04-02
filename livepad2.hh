/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2014 by Anton Persson
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

#ifndef LIVEPAD2_CLASS
#define LIVEPAD2_CLASS

#include <kamogui.hh>

#include "machine.hh"
#include "machine_sequencer.hh"
#include "listview.hh"
#include "remote_interface.hh"

#define HZONES PAD_HZONES
#define VZONES PAD_VZONES

class LivePad2 : public KammoGUI::SVGCanvas::SVGDocument,
		 public RemoteInterface::RIMachine::RIMachineSetListener {
private:
	static LivePad2 *l_pad2;

	KammoGUI::SVGCanvas::SVGMatrix transform_m;

	// canvas coordinates for the actual LivePad part
	double doc_x1, doc_y1, doc_x2, doc_y2, doc_scale;

	int octave, scale_index;
	std::string scale_name;
	
	bool record, quantize;
	std::string chord_mode, mode, controller;

	KammoGUI::SVGCanvas::ElementReference graphArea_element;
	KammoGUI::SVGCanvas::SVGRect graphArea_viewport;
	std::shared_ptr<RemoteInterface::RIMachine> mseq;

	std::set<std::weak_ptr<RemoteInterface::RIMachine>,
		 std::owner_less<std::weak_ptr<RemoteInterface::RIMachine> > >msequencers; // all known machine sequencers
	
	ListView *listView;

	KammoGUI::SVGCanvas::ElementReference octaveUp_element;
	KammoGUI::SVGCanvas::ElementReference octaveDown_element;
	KammoGUI::SVGCanvas::ElementReference clear_element;
	KammoGUI::SVGCanvas::ElementReference recordGroup_element;
	KammoGUI::SVGCanvas::ElementReference quantize_element;

	KammoGUI::SVGCanvas::ElementReference selectMachine_element;
	KammoGUI::SVGCanvas::ElementReference selectMode_element;
	KammoGUI::SVGCanvas::ElementReference selectChord_element;
	KammoGUI::SVGCanvas::ElementReference selectScale_element;
	KammoGUI::SVGCanvas::ElementReference selectController_element;
	KammoGUI::SVGCanvas::ElementReference selectMenu_element;

	enum currentSelector_t { not_selecting, selecting_machine, selecting_mode,
				 selecting_chord, selecting_scale, selecting_controller,
				 selecting_menu};
	currentSelector_t current_selector;
	
	void refresh_scale_key_names();

	void machine_selected(const std::string &machine_name);
	void select_machine();
	void mode_selected(const std::string &mode_name);
	void select_mode();
	void chord_selected(const std::string &chord_name);
	void select_chord();
	void scale_selected(const std::string &scale_name);
	void select_scale();
	void controller_selected(const std::string &controller_name);
	void select_controller();
	void menu_selected(int row_index);
	void select_menu();

	void refresh_record_indicator();
	void refresh_quantize_indicator();
	void refresh_scale_indicator();
	void refresh_controller_indicator();
	void refresh_machine_settings();

	void octave_up();
	void octave_down();
	void toggle_record();
	void toggle_quantize();

	static void yes(void *ctx);
	static void no(void *ctx);
	void ask_clear_pad();
	
	static void button_on_event(KammoGUI::SVGCanvas::SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
				    const KammoGUI::SVGCanvas::MotionEvent &event);
	static void listview_callback(void *context, bool row_selected, int row_index, const std::string &text);
	static void graphArea_on_event(SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
				       const KammoGUI::SVGCanvas::MotionEvent &event);
public:
	LivePad2(KammoGUI::SVGCanvas *cnv, std::string file_name);
	~LivePad2();
	
	virtual void on_resize();
	virtual void on_render();

	static void use_new_MachineSequencer(std::shared_ptr<RemoteInterface::RIMachine> mseq);

	virtual void ri_machine_registered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine);
	virtual void ri_machine_unregistered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine);
};

#endif
