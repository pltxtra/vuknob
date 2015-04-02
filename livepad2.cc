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

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <jngldrum/jinformer.hh>

#include "livepad2.hh"
#include "svg_loader.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#include "common.hh"

void LivePad2::on_resize() {
	KammoGUI::SVGCanvas::SVGRect document_size;
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.get_viewport(document_size);

	int pixel_w, pixel_h;
	get_canvas_size(pixel_w, pixel_h);
	
	float canvas_w_inches, canvas_h_inches;
	get_canvas_size_inches(canvas_w_inches, canvas_h_inches);

	float w_fingers = canvas_w_inches / INCHES_PER_FINGER;
	float h_fingers = canvas_h_inches / INCHES_PER_FINGER;

	// calculate pixel size of 12 by 15 fingers
	double finger_width = (12.0 * pixel_w ) / w_fingers;
	double finger_height = (15.0 * pixel_h) / h_fingers;

	// if larger than the canvas pixel size, then limit it
	finger_width = finger_width < pixel_w ? finger_width : pixel_w;
	finger_height = finger_height < pixel_h ? finger_height : pixel_h;
	
	// calculate scaling factor	
	double scaling_w = (finger_width) / (double)document_size.width;
	double scaling_h = (finger_height) / (double)document_size.height;

	doc_scale = scaling_w < scaling_h ? scaling_w : scaling_h;

	// calculate translation
	doc_x1 = (pixel_w - document_size.width * doc_scale) / 2.0;
	doc_y1 = (pixel_h - document_size.height * doc_scale) / 2.0;
	doc_x2 = doc_x1 + document_size.width * doc_scale;
	doc_y2 = doc_y1 + (8.0 / 10.0) * document_size.height * doc_scale; // we only use 8 out of 10 because the pad is 8 fingers high while the buttons below the pad are 2 fingers high.
		
	// initiate transform_m
	transform_m.init_identity();
	transform_m.scale(doc_scale, doc_scale);	
	transform_m.translate(doc_x1, doc_y1);
}

void LivePad2::on_render() {
	{ // Translate the document, and scale it properly to fit the defined viewbox	
		KammoGUI::SVGCanvas::ElementReference root(this);
		root.set_transform(transform_m);
	}

	{ /* get graphArea size */
		graphArea_element.get_viewport(graphArea_viewport);		
	}
}

void LivePad2::listview_callback(void *context, bool row_selected, int row_index, const std::string &text) {
	LivePad2 *ctx = (LivePad2 *)context;
		
	SATAN_DEBUG("---> Row selected: %s\n", text.c_str());

	if(row_selected) {
		switch(ctx->current_selector) {
		case selecting_machine:
		{
			ctx->machine_selected(text);
		}
		break;
		
		case selecting_mode:
		{
			ctx->mode_selected(text);
		}
		break;
		
		case selecting_chord:
		{
			ctx->chord_selected(text);
		}
		break;
		
		case selecting_scale:
		{
			ctx->scale_selected(text);
		}
		break;
		
		case selecting_controller:
		{
			ctx->controller_selected(text);
		}
		break;
		
		case selecting_menu:
		{
			ctx->menu_selected(row_index);
		}
		break;
		
		default:
		case not_selecting:
			/* ignore */
			break;
		}
	}

	ctx->current_selector = not_selecting;
	ctx->refresh_machine_settings();
}

void LivePad2::machine_selected(const std::string &machine_name) {
	for(auto k_weak : msequencers) {
		if(auto k = k_weak.lock()) {
			if(machine_name == k->get_sibling_name()) {
				SATAN_DEBUG("Machine selected: %s\n", k->get_sibling_name().c_str());
				LivePad2::use_new_MachineSequencer(k);
			}
		}
	}
}

void LivePad2::select_machine() {
	current_selector = selecting_machine;
	
	for(auto k_weak : msequencers) {
		if(auto k = k_weak.lock()) {
			SATAN_DEBUG("Adding machine %s to selection.\n", k->get_sibling_name().c_str());
			listView->add_row(k->get_sibling_name());
		}
	}
}

void LivePad2::mode_selected(const std::string &mode_name) {
	mode = mode_name;

	refresh_machine_settings();
}

void LivePad2::select_mode() {
	current_selector = selecting_mode;

	listView->add_row("No Arpeggio");

	for(auto arpid :  MachineSequencer::get_pad_arpeggio_patterns()) {
		listView->add_row(arpid);
	}
}

void LivePad2::chord_selected(const std::string &chord_name) {
	l_pad2->chord_mode = chord_name;
}

void LivePad2::select_chord() {
	current_selector = selecting_chord;

	listView->add_row("chord off");
	listView->add_row("chord triad");
}

static const char *key_text[] = {
	"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-",
	"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
};

void LivePad2::refresh_scale_key_names() {
	std::vector<int> keys = MachineSequencer::PadConfiguration::get_scale_key_names(scale_name);
	SATAN_DEBUG("keys[%s].size() = %d\n", scale_name.c_str(), keys.size());
	for(int n = 0; n < 8; n++) {
		std::stringstream key_name_id;
		key_name_id << "key_name_" << n;
		SATAN_DEBUG("Will try to get id: %s\n", key_name_id.str().c_str());
		KammoGUI::SVGCanvas::ElementReference key_name(this, key_name_id.str());

		int key_number = keys[n % keys.size()];
		
		std::stringstream key_name_text;
		key_name_text << key_text[key_number] << (octave + (key_number / 12) + (n / keys.size()));
		SATAN_DEBUG("   [%d] -> %s\n", key_number, key_name_text.str().c_str());
		key_name.set_text_content(key_name_text.str());
	}
}

void LivePad2::scale_selected(const std::string &_scale_name) {
	scale_name = _scale_name;
	
	std::vector<std::string> scale_names = MachineSequencer::PadConfiguration::get_scale_names();
	std::vector<std::string>::iterator k;
	int n = 0;
	
	
	for(auto scale : MachineSequencer::PadConfiguration::get_scale_names()) {
		if(scale_name == scale) {
			scale_index = n;
		}
		n++;
	}       	
}

void LivePad2::select_scale() {
	current_selector = selecting_scale;

	for(auto scale : MachineSequencer::PadConfiguration::get_scale_names()) {
		listView->add_row(scale);
	}
}

void LivePad2::controller_selected(const std::string &controller_name) {
	std::string new_ctrl = "Velocity"; // default to Velocity

	if(mseq) {
		for(auto ctrl : mseq->available_midi_controllers()) {
			if(ctrl == controller_name) {
				new_ctrl = ctrl; // we found a match, keep this name
			}
		}
	}

	controller = new_ctrl;
}

void LivePad2::select_controller() {
	current_selector = selecting_controller;

	listView->add_row("Velocity");

	if(mseq) {
		for(auto ctrl : mseq->available_midi_controllers()) {
			listView->add_row(ctrl);
		}
	}
}

void LivePad2::menu_selected(int row_index) {
	if(mseq) {
		if(row_index == 0) {
			mseq->pad_export_to_loop();
		} else {
			mseq->pad_export_to_loop(row_index - 1);
		}
	}
}

void LivePad2::select_menu() {
	current_selector = selecting_menu;

	if(mseq) { 
		listView->add_row("New loop");
		
		int max_loop = mseq->get_nr_of_loops();	
		for(int k = 0; k < max_loop; k++) {
			std::ostringstream loop_id;
			loop_id << "Loop #" << k;
			listView->add_row(loop_id.str());
		}
	}
}

void LivePad2::refresh_record_indicator() {
	KammoGUI::SVGCanvas::ElementReference recordIndicator_element(this, "recInd");

	recordIndicator_element.set_style(record ? "fill-opacity:1.0" : "fill-opacity:0.2");
}

void LivePad2::refresh_quantize_indicator() {
	KammoGUI::SVGCanvas::ElementReference quantizeOn_element(this, "quantizeOn");
	KammoGUI::SVGCanvas::ElementReference quantizeOff_element(this, "quantizeOff");

	quantizeOn_element.set_display(quantize ? "inline" : "none");
	quantizeOff_element.set_display(quantize ? "none" : "inline");
}

void LivePad2::refresh_scale_indicator() {
	int n = 0;
	
	for(auto scale : MachineSequencer::PadConfiguration::get_scale_names()) {
		if(scale_index == n) {
			selectScale_element.find_child_by_class("selectedText").set_text_content(scale);
		}
		n++;
	}
}

void LivePad2::refresh_controller_indicator() {
	std::string ctrl_name = "Velocity"; // default to Velocity

	if(mseq) {
		for(auto ctrl : mseq->available_midi_controllers()) {
			if(ctrl == controller) {
				ctrl_name = ctrl; // we found a match, keep this name
			}
		}
	}

	selectController_element.find_child_by_class("selectedText").set_text_content(ctrl_name);
}

void LivePad2::refresh_machine_settings() {
	refresh_scale_key_names();

	std::string name = "mchn";
	SATAN_DEBUG("Refresh machine settings, mseq\n");
	if(mseq) {
		mseq->pad_set_octave(octave);
		mseq->pad_set_scale(scale_index);
		mseq->pad_set_record(record);
		mseq->pad_set_quantize(quantize);

		mseq->pad_assign_midi_controller(controller);

		if(chord_mode == "chord triad") {
			mseq->pad_set_chord_mode(RemoteInterface::RIMachine::chord_triad);
		} else { // default
			mseq->pad_set_chord_mode(RemoteInterface::RIMachine::chord_off);
		}
				
		mseq->pad_set_arpeggio_pattern(mode);

		name = mseq->get_sibling_name();
	}

	refresh_record_indicator();
	refresh_quantize_indicator();
	refresh_scale_indicator();
	refresh_controller_indicator();
	
	{
		selectMode_element.find_child_by_class("selectedText").set_text_content(mode);
	}
	{
		selectChord_element.find_child_by_class("selectedText").set_text_content(chord_mode);
	}
	{
		selectMachine_element.find_child_by_class("selectedText").set_text_content(name);
	}
}

void LivePad2::octave_up() {
	octave = octave + 1;
	if(octave > 6) octave = 6;
	refresh_machine_settings();	
}

void LivePad2::octave_down() {
	octave = octave - 1;
	if(octave < 1) octave = 1;
	refresh_machine_settings();	
}

void LivePad2::toggle_record() {
	record = !record;
	refresh_machine_settings();	
}

void LivePad2::toggle_quantize() {
	quantize = !quantize;
	refresh_machine_settings();	
}

void LivePad2::yes(void *void_ctx) {
	LivePad2 *ctx = (LivePad2 *)void_ctx;
	
	if(ctx != NULL && ctx->mseq)
		ctx->mseq->pad_clear();
}

void LivePad2::no(void *ctx) {
	// do not do anything
}

void LivePad2::ask_clear_pad() {
	if(!mseq) {
		jInformer::inform("Select a machine in the drop down menu down to the left, otherwise there's nothing to clear.");
		return;
	}
	std::ostringstream question;

	question << "Do you want to clear: " << mseq->get_sibling_name();
	
	KammoGUI::ask_yes_no("Clear jammin?",
			     question.str(),
			     yes, this,
			     no, this);
}

void LivePad2::button_on_event(KammoGUI::SVGCanvas::SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
			       const KammoGUI::SVGCanvas::MotionEvent &event) {
	SATAN_DEBUG("event trigger for active element - %s\n", e_ref->get_id().c_str());

	LivePad2 *ctx = (LivePad2 *)source;
	
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
			if(x < 10 && y < 10) {
				std::string select_title = "";
				
				ctx->listView->clear();
				if(e_ref->get_id() == "recordGroup") {
					ctx->toggle_record();
				} else if(e_ref->get_id() == "quantize") {
					ctx->toggle_quantize();
				} else if(e_ref->get_id() == "clearPad") {
					ctx->ask_clear_pad();
				} else if(e_ref->get_id() == "octaveUp") {
					ctx->octave_up();
				} else if(e_ref->get_id() == "octaveDown") {
					ctx->octave_down();					
				} else if(e_ref->get_id() == "selectMachine") {
					ctx->select_machine();
					select_title = "Select machine";
				} else if(e_ref->get_id() == "selectMode") {
					ctx->select_mode();				 
					select_title = "Select mode";
				} else if(e_ref->get_id() == "selectChord") {
					ctx->select_chord();				 
					select_title = "Select chord";
				} else if(e_ref->get_id() == "selectScale") {
					ctx->select_scale();				 
					select_title = "Select scale";
				} else if(e_ref->get_id() == "selectController") {
					ctx->select_controller();				 
					select_title = "Select controller";
				} else if(e_ref->get_id() == "selectMenu") {
					ctx->select_menu();				 
					select_title = "Copy to loop";
				}
				
				if(ctx->current_selector != not_selecting) {
					ctx->listView->select_from_list(select_title, ctx, listview_callback);
				}
			}
			break;
		}
	}
}

void LivePad2::graphArea_on_event(KammoGUI::SVGCanvas::SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
				  const KammoGUI::SVGCanvas::MotionEvent &event) {
	LivePad2 *ctx = (LivePad2 *)source;

	if(!(ctx->mseq)) {
		jInformer::inform("Select a machine in the drop down menu down to the left, otherwise you will not get any sound.");
		return;
	}
	if(Machine::is_it_playing() == false) {
		jInformer::inform("Press the play button on the top left first, please. Otherwise you will not get any sound.");
		return;
	}
	
	float x, y;		
	/* scale by screen resolution, and complete svg offset */
	x = event.get_x(); y = event.get_y();
	if(x < ctx->doc_x1) x = ctx->doc_x1;
	if(x > ctx->doc_x2) x = ctx->doc_x2;
	if(y < ctx->doc_y1) y = ctx->doc_y1;
	if(y > ctx->doc_y2) y = ctx->doc_y2;
	
	x = (x - ctx->doc_x1);
	y = (y - ctx->doc_y1);
	
	int finger = event.get_pointer_id(event.get_action_index());
	RemoteInterface::RIMachine::PadEvent_t pevt = RemoteInterface::RIMachine::ms_pad_no_event;

	float width  = ctx->doc_x2 - ctx->doc_x1 + 1;
	float height = ctx->doc_y2 - ctx->doc_y1 + 1; // +1 to disable overflow
	
	switch(event.get_action()) {
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		pevt = RemoteInterface::RIMachine::ms_pad_press;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
		pevt = RemoteInterface::RIMachine::ms_pad_release;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
		pevt = RemoteInterface::RIMachine::ms_pad_press;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
		if(ctx->mseq) {
			for(int k = 0; k < event.get_pointer_count(); k++) {
				int f = event.get_pointer_id(k);
				/* scale by screen resolution, and complete svg offset */
				float ev_x = event.get_x(k); float ev_y = event.get_y(k);
				if(ev_x < ctx->doc_x1) ev_x = ctx->doc_x1;
				if(ev_x > ctx->doc_x2) ev_x = ctx->doc_x2;
				if(ev_y < ctx->doc_y1) ev_y = ctx->doc_y1;
				if(ev_y > ctx->doc_y2) ev_y = ctx->doc_y2;
				
				ev_x = (ev_x - ctx->doc_x1);
				ev_y = (ev_y - ctx->doc_y1);

				ev_x /= width;
				ev_y /= height;
				
				ctx->mseq->pad_enqueue_event(f, RemoteInterface::RIMachine::ms_pad_slide, ev_x, ev_y);
			}
		}
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
		pevt = RemoteInterface::RIMachine::ms_pad_release;
		break;
	}
	
	if(pevt != RemoteInterface::RIMachine::ms_pad_no_event && ctx->mseq) {
		ctx->mseq->pad_enqueue_event(finger, pevt, x / width, y / height);
	}
}

LivePad2::LivePad2(KammoGUI::SVGCanvas *cnv, std::string file_name) : SVGDocument(file_name, cnv), octave(3), scale_index(0), scale_name("C- "), record(false), quantize(false),
	chord_mode("chord off"), mode("No Arpeggio"), controller("velocity"), listView(NULL), current_selector(not_selecting)
{
	l_pad2 = this;
	
	{ // get the graph area and attach the event listener
		graphArea_element = KammoGUI::SVGCanvas::ElementReference(this, "graphArea");
		graphArea_element.set_event_handler(graphArea_on_event);
	}
	{ // get the controller selector and attach the event listener
		selectMachine_element = KammoGUI::SVGCanvas::ElementReference(this, "selectMachine");
		selectMachine_element.set_event_handler(button_on_event);
	}	
	{ // get the mode selector and attach the event listener
		selectMode_element = KammoGUI::SVGCanvas::ElementReference(this, "selectMode");
		selectMode_element.set_event_handler(button_on_event);
	}	
	{ // get the chord selector and attach the event listener
		selectChord_element = KammoGUI::SVGCanvas::ElementReference(this, "selectChord");
		selectChord_element.set_event_handler(button_on_event);
	}	
	{ // get the scale selector and attach the event listener
		selectScale_element = KammoGUI::SVGCanvas::ElementReference(this, "selectScale");
		selectScale_element.set_event_handler(button_on_event);
	}	
	{ // get the controller selector and attach the event listener
		selectController_element = KammoGUI::SVGCanvas::ElementReference(this, "selectController");
		selectController_element.set_event_handler(button_on_event);
	}	
	{ // get the quantize element and attach the event listener
		quantize_element = KammoGUI::SVGCanvas::ElementReference(this, "quantize");
		quantize_element.set_event_handler(button_on_event);
	}	
	{ // get the menu element and attach the event listener
		selectMenu_element = KammoGUI::SVGCanvas::ElementReference(this, "selectMenu");
		selectMenu_element.set_event_handler(button_on_event);
	}	

	{ // get the octave up button and attach the event listener
		octaveUp_element = KammoGUI::SVGCanvas::ElementReference(this, "octaveUp");
		octaveUp_element.set_event_handler(button_on_event);
	}	
	{ // get the octave down button and attach the event listener
		octaveDown_element = KammoGUI::SVGCanvas::ElementReference(this, "octaveDown");
		octaveDown_element.set_event_handler(button_on_event);
	}	
	{ // get the clear button and attach the event listener
		clear_element = KammoGUI::SVGCanvas::ElementReference(this, "clearPad");
		clear_element.set_event_handler(button_on_event);
	}	
	{ // get the record group and attach the event listener
		recordGroup_element = KammoGUI::SVGCanvas::ElementReference(this, "recordGroup");
		recordGroup_element.set_event_handler(button_on_event);
	}	

	listView = new ListView(cnv);		
	
	refresh_machine_settings();
}

LivePad2::~LivePad2() {
	QUALIFIED_DELETE(listView);
}

/***************************
 *
 * Public Statics
 *
 ***************************/

void LivePad2::use_new_MachineSequencer(std::shared_ptr<RemoteInterface::RIMachine> m) {
	l_pad2->mseq = m;
	l_pad2->refresh_machine_settings();
}

void LivePad2::ri_machine_registered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine) {
	SATAN_DEBUG("LivePad2::ri_machine_registered - type [%s] (%f, %f)\n",
		    ri_machine->get_machine_type().c_str(), ri_machine->get_x_position(), ri_machine->get_y_position());	

	if(ri_machine->get_machine_type() != "MachineSequencer") return; // we're not interested in anything but the MachineSequencers
	
	KammoGUI::run_on_GUI_thread(
		[this, ri_machine]() {
			msequencers.insert(ri_machine);
			mseq.reset();
			refresh_machine_settings();
		}
		);
}

void LivePad2::ri_machine_unregistered(std::shared_ptr<RemoteInterface::RIMachine> ri_machine) {
	if(ri_machine->get_machine_type() != "MachineSequencer") return; // we're not interested in anything but the MachineSequencers

	KammoGUI::run_on_GUI_thread(
		[this, ri_machine]() {
			for(auto found_weak = msequencers.begin();
			    found_weak != msequencers.end();
			    found_weak++) {
				auto found = found_weak->lock();
				if(found == ri_machine) {
					msequencers.erase(found_weak);
					break;
				}
			}
			mseq.reset();
			refresh_machine_settings();
		}
		);
}
/***************************
 *
 *  Kamoflage Event Declaration
 *
 ***************************/

LivePad2 *LivePad2::l_pad2 = NULL;

KammoEventHandler_Declare(LivePad2Handler,"livePad2:showLivePad2");

virtual void on_init(KammoGUI::Widget *wid) {
	if(wid->get_id() == "livePad2") {
		KammoGUI::SVGCanvas *cnvs = (KammoGUI::SVGCanvas *)wid;		
		cnvs->set_bg_color(1.0, 1.0, 1.0);
		
		static auto lpad = std::make_shared<LivePad2>(cnvs, SVGLoader::get_svg_path("/livePad2.svg"));
		RemoteInterface::Client::register_ri_machine_set_listener(lpad);		
	}
}

virtual void on_user_event(KammoGUI::UserEvent *ue, std::map<std::string, void *> args) {
}

KammoEventHandler_Instance(LivePad2Handler);
