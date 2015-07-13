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

#include <iostream>

#include "tracker.hh"
#include "svg_loader.hh"
#include "machine.hh"
#include "common.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/***************************
 *
 * Local globals
 *
 ***************************/

static Tracker *current_tracker = NULL;
static TrackerMenu *current_tracker_menu = NULL;
static SelectNoneButton *current_select_none_button = NULL;

static ScaleSlider *scale_slider = NULL;

// sizes in pixels
static double finger_width = 10.0, finger_height = 10.0;

// canvas size in "fingers"
static int canvas_width_fingers = 8, canvas_height_fingers = 8;
static int font_size = 55; // in pixels

static char *key_names[] = {
	(char *)"C-",
	(char *)"C#",
	(char *)"D-",
	(char *)"D#",

	(char *)"E-",
	(char *)"F-",
	(char *)"F#",
	(char *)"G-",

	(char *)"G#",
	(char *)"A-",
	(char *)"A#",
	(char *)"B-"};

/***************************
 *
 *  Class TrackerMenu::HelpTextAnimation
 *
 ***************************/

TrackerMenu::HelpTextAnimation::HelpTextAnimation(KammoGUI::SVGCanvas::ElementReference* _help_text, const std::string &display_text)
	: KammoGUI::Animation(1.0)
	, help_text(_help_text)
{

	help_text->set_display("inline");
	help_text->find_child_by_class("helpText").set_text_content(display_text);

	SATAN_DEBUG("Animation %p started (%s).\n", this, display_text.c_str());
}

void TrackerMenu::HelpTextAnimation::new_frame(float progress) {
	if(progress >= 1.0f) {
		SATAN_DEBUG("Animation %p finished\n", this);
		help_text->set_display("none");
	}
}

void TrackerMenu::HelpTextAnimation::on_touch_event() { /* ignored */ }

/***************************
 *
 *  Class TrackerMenu
 *
 ***************************/

void TrackerMenu::on_render() {
	{ // Translate the menu into place
		KammoGUI::SVGCanvas::SVGMatrix menu_t;
		menu_t.scale(scale, scale);
		menu_t.translate(translate_h, translate_v);

		KammoGUI::SVGCanvas::ElementReference root(this);
		root.set_transform(menu_t);
	}

	{ // visibility
		menu_rim.set_display(rim_visible ? "inline" : "none");
	}
	{ // disable undo button if undo stack is empty
		menu_disableUndo.set_display(tracker->undo_stack_disabled() ? "inline" : "none");
	}
	{ // snap selection
		if(tracker) {
			Tracker::SnapMode new_mode = tracker->get_snap_mode();
			if(new_mode != last_snap_mode) {
				last_snap_mode = new_mode;
				switch(new_mode) {
				case Tracker::snap_to_line:
					menu_disableSnap.set_display("inline");
					break;
				case Tracker::snap_to_tick:
				default:
					menu_disableSnap.set_display("none");
					break;
				}
			}
		}
	}
}

void TrackerMenu::on_resize() {
	get_canvas_size(pixel_w, pixel_h);
	float canvas_w_inches, canvas_h_inches;
	get_canvas_size_inches(canvas_w_inches, canvas_h_inches);

	float h_fingers = canvas_w_inches / INCHES_PER_FINGER;

	float five_fingers = 5.5f * (float)pixel_w / h_fingers;

	KammoGUI::SVGCanvas::SVGRect rect;
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.get_viewport(rect);

	scale = five_fingers / rect.width;

	translate_h = (float)pixel_w - rect.width * scale;
	translate_v = (float)pixel_h - rect.height * scale;
}

void TrackerMenu::menu_selection_on_event(KammoGUI::SVGCanvas::SVGDocument *source,
					  KammoGUI::SVGCanvas::ElementReference *e_ref,
					  const KammoGUI::SVGCanvas::MotionEvent &event) {
	TrackerMenu *ctx = (TrackerMenu *)source;

	double now_x = event.get_x();
	double now_y = event.get_y();

	switch(event.get_action()) {
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
		ctx->is_a_tap = false;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
		// check if the user is moving the finger too far, indicating abort action
		// if so - disable is_a_tap
		if(ctx->is_a_tap) {
			if(fabs(now_x - ctx->first_selection_x) > 20)
				ctx->is_a_tap = false;
			if(fabs(now_y - ctx->first_selection_y) > 20)
				ctx->is_a_tap = false;
		}
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
		ctx->is_a_tap = true;
		ctx->first_selection_x = now_x;
		ctx->first_selection_y = now_y;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
		if(ctx->is_a_tap && ctx->tracker) {
			HelpTextAnimation *helpanim = NULL;
			if(e_ref == &(ctx->menu_copy)) {
				ctx->tracker->copy_selected();
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Copy");

			} else if(e_ref == &(ctx->menu_paste)) {
				ctx->tracker->paste_selected();
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Paste");

			} else if(e_ref == &(ctx->menu_trash)) {
				ctx->tracker->trash_selected();
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Delete");

			} else if(e_ref == &(ctx->menu_quantize)) {
				ctx->tracker->quantize_selected();
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Quantize");

			} else if(e_ref == &(ctx->menu_shiftLeft)) {
				ctx->tracker->shift_selected(-1);
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Shift Left");

			} else if(e_ref == &(ctx->menu_shiftRight)) {
				ctx->tracker->shift_selected( 1);
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Shift Right");

			} else if(e_ref == &(ctx->menu_transposeUp)) {
				ctx->tracker->transpose_selected(  1);
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Transpose +1");

			} else if(e_ref == &(ctx->menu_transposeDown)) {
				ctx->tracker->transpose_selected( -1);
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Transpose -1");

			} else if(e_ref == &(ctx->menu_octaveUp)) {
				ctx->tracker->transpose_selected( 12);
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Transpose +12");

			} else if(e_ref == &(ctx->menu_octaveDown)) {
				ctx->tracker->transpose_selected(-12);
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Transpose -12");

			} else if(e_ref == &(ctx->menu_previousLoop)) {
				ctx->tracker->previous_selected();
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Previous");

			} else if(e_ref == &(ctx->menu_nextLoop)) {
				ctx->tracker->next_selected();
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Next");

			} else if(e_ref == &(ctx->menu_undo)) {
				ctx->tracker->restore_from_undo_buffer();
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Undo");

			} else if(e_ref == &(ctx->menu_snapTo)) {
				ctx->tracker->snap_toggled();
				helpanim = new HelpTextAnimation(&ctx->menu_help, "Snap toggled");
			}
			if(helpanim) {
				SATAN_DEBUG("help animation started.\n");
				ctx->tracker->start_animation(helpanim);
			}

		}
		break;
	}
}

void TrackerMenu::menu_center_on_event(KammoGUI::SVGCanvas::SVGDocument *source,
				       KammoGUI::SVGCanvas::ElementReference *e_ref,
				       const KammoGUI::SVGCanvas::MotionEvent &event) {
	TrackerMenu *ctx = (TrackerMenu *)source;

	switch(event.get_action()) {
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
		ctx->rim_visible = ctx->rim_visible ? false : true;
		break;
	}
}

TrackerMenu::TrackerMenu(KammoGUI::SVGCanvas *cnvs) :
	SVGDocument(std::string(SVGLoader::get_svg_directory() + "/CircleMenu.svg"), cnvs), rim_visible(false),
									 last_snap_mode(Tracker::snap_to_tick) {
	menu_center = KammoGUI::SVGCanvas::ElementReference(this, "menuCenter");
	menu_rim = KammoGUI::SVGCanvas::ElementReference(this, "menuRim");
	menu_loopNumberText = KammoGUI::SVGCanvas::ElementReference(this, "loopNumberText");
	menu_help = KammoGUI::SVGCanvas::ElementReference(this, "helpTextBlock");
	menu_help.set_display("none");

	menu_ignoreCircle = KammoGUI::SVGCanvas::ElementReference(this, "ignoreCircle");

	menu_copy = KammoGUI::SVGCanvas::ElementReference(this, "menuCopy");
	menu_paste = KammoGUI::SVGCanvas::ElementReference(this, "menuPaste");
	menu_trash = KammoGUI::SVGCanvas::ElementReference(this, "menuTrash");

	menu_quantize = KammoGUI::SVGCanvas::ElementReference(this, "menuQuantize");
	menu_shiftLeft = KammoGUI::SVGCanvas::ElementReference(this, "menuShiftLeft");
	menu_shiftRight = KammoGUI::SVGCanvas::ElementReference(this, "menuShiftRight");
	menu_transposeUp = KammoGUI::SVGCanvas::ElementReference(this, "menuTransposeUp");
	menu_transposeDown = KammoGUI::SVGCanvas::ElementReference(this, "menuTransposeDown");
	menu_octaveUp = KammoGUI::SVGCanvas::ElementReference(this, "menuOctaveUp");
	menu_octaveDown = KammoGUI::SVGCanvas::ElementReference(this, "menuOctaveDown");

	menu_undo = KammoGUI::SVGCanvas::ElementReference(this, "menuUndo");
	menu_disableUndo = KammoGUI::SVGCanvas::ElementReference(this, "disableUndo");

	menu_previousLoop = KammoGUI::SVGCanvas::ElementReference(this, "menuPrevious");
	menu_nextLoop = KammoGUI::SVGCanvas::ElementReference(this, "menuNext");

	menu_snapTo = KammoGUI::SVGCanvas::ElementReference(this, "menuSnap");
	menu_disableSnap = KammoGUI::SVGCanvas::ElementReference(this, "disableSnap");

	menu_center.set_event_handler(menu_center_on_event);

	menu_ignoreCircle.set_event_handler(menu_selection_on_event);

	menu_copy.set_event_handler(menu_selection_on_event);
	menu_paste.set_event_handler(menu_selection_on_event);
	menu_trash.set_event_handler(menu_selection_on_event);
	menu_quantize.set_event_handler(menu_selection_on_event);
	menu_shiftLeft.set_event_handler(menu_selection_on_event);
	menu_shiftRight.set_event_handler(menu_selection_on_event);
	menu_transposeUp.set_event_handler(menu_selection_on_event);
	menu_transposeDown.set_event_handler(menu_selection_on_event);
	menu_octaveUp.set_event_handler(menu_selection_on_event);
	menu_octaveDown.set_event_handler(menu_selection_on_event);
	menu_undo.set_event_handler(menu_selection_on_event);
	menu_disableUndo.set_event_handler(menu_selection_on_event); // we will ignore events from this one.
	menu_previousLoop.set_event_handler(menu_selection_on_event);
	menu_nextLoop.set_event_handler(menu_selection_on_event);
	menu_snapTo.set_event_handler(menu_selection_on_event);
}

void TrackerMenu::set_loop_number(int loop_nr) {
	char bfr[32];
	snprintf(bfr, 32, "%02d", loop_nr);
	menu_loopNumberText.set_text_content(bfr);
}

/***************************
 *
 *  Class SelectNoneButton
 *
 ***************************/

void SelectNoneButton::on_event(KammoGUI::SVGCanvas::SVGDocument *source,
	      KammoGUI::SVGCanvas::ElementReference *e_ref,
	      const KammoGUI::SVGCanvas::MotionEvent &event) {
	SelectNoneButton *ctx = (SelectNoneButton *)source;

	double now_x = event.get_x();
	double now_y = event.get_y();

	switch(event.get_action()) {
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
		// check if the user is moving the finger too far, indicating abort action
		// if so - disable is_a_tap
		if(ctx->is_a_tap) {
			if(fabs(now_x - ctx->first_selection_x) > 20)
				ctx->is_a_tap = false;
			if(fabs(now_y - ctx->first_selection_y) > 20)
				ctx->is_a_tap = false;
		}
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
		ctx->is_a_tap = true;
		ctx->first_selection_x = now_x;
		ctx->first_selection_y = now_y;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
		if(ctx->is_a_tap && ctx->tracker) {
			ctx->tracker->select_none();
		}
		break;
	}
}

void SelectNoneButton::on_render() {
	{ // only show if the Tracker has any NoteGraphic objects that are selected
		if(tracker) {
			select_none.set_display(tracker->anything_selected() ? "inline" : "none");
		}
	}

	{ // Translate the button into place
		KammoGUI::SVGCanvas::SVGMatrix button_t;
		button_t.scale(scale, scale);
		button_t.translate(translate_h, translate_v);

		KammoGUI::SVGCanvas::ElementReference root(this);
		root.set_transform(button_t);
	}
}

void SelectNoneButton::on_resize() {
	get_canvas_size(pixel_w, pixel_h);
	float canvas_w_inches, canvas_h_inches;
	get_canvas_size_inches(canvas_w_inches, canvas_h_inches);

	float h_fingers = canvas_w_inches / INCHES_PER_FINGER;

	float two_fingers = 2.0f * (float)pixel_w / h_fingers;

	KammoGUI::SVGCanvas::SVGRect rect;
	KammoGUI::SVGCanvas::ElementReference root(this);
	root.get_viewport(rect);

	scale = two_fingers / rect.width;

//	translate_h = (float)pixel_w - rect.width * scale * 1.4;
	translate_h = rect.width * scale * 0.4;
	translate_v = rect.height * scale * 0.4;
}

SelectNoneButton::SelectNoneButton(KammoGUI::SVGCanvas *cnvs) : SVGDocument(std::string(SVGLoader::get_svg_directory() + "/SelectNone.svg"), cnvs) {
	select_none = KammoGUI::SVGCanvas::ElementReference(this, "selectNone");
	select_none.set_event_handler(on_event);
}

/***************************
 *
 *  Class Tracker::Bar
 *
 ***************************/

Tracker::Bar::Bar(int _key, KammoGUI::SVGCanvas::SVGDocument *ctx, const std::string &id) : ElementReference(ctx, id), key(_key) {}

/***************************
 *
 *  Class Tracker::NoteGraphic
 *
 ***************************/

void Tracker::NoteGraphic::update_graphics() {
	KammoGUI::SVGCanvas::ElementReference rect = find_child_by_class("noteRect");
	KammoGUI::SVGCanvas::ElementReference line = find_child_by_class("noteLine");
	KammoGUI::SVGCanvas::ElementReference stretch = find_child_by_class("stretchLine");

	line.set_line_coords(on_at * tick_width,
			     (128.0f - note) * bar_height + bar_height / 2.0f,
			     (on_at + length) * tick_width,
			     (128.0f - note) * bar_height + bar_height / 2.0f);
	stretch.set_line_coords((on_at + length) * tick_width,
				(128.0f - note) * bar_height + bar_height / 2.0f,
				(on_at + length + MACHINE_TICKS_PER_LINE - 2.0f) * tick_width,
				(128.0f - note) * bar_height + bar_height / 2.0f);
	rect.set_rect_coords((on_at - 4) * tick_width,
			     (128 - note) * bar_height,
			     (length - 0.5 + 4) * tick_width,
			     bar_height);

	line.set_style(std::string("stroke:") + (selected ? "#f1a123" : "#33ccff") + ";");
	stretch.set_style(std::string("stroke:") + (selected ? "#e6920f" : "#22bbee") + ";");
}

void Tracker::NoteGraphic::update_data() {
	if(note_data) {
		// we have a note_data reference from an existing loop
		// we shall update that
		loop->note_update(note_data, this);
	} else if(loop) {
		// we have a loop, but no reference to an existing note
		// create one
		note_data = loop->note_insert(this);
	}

}

Tracker::NoteGraphic::NoteGraphic(uint8_t _key, unsigned int _start_tick, unsigned int _length,
				  KammoGUI::SVGCanvas::ElementReference *note_container, const std::string &id,
				  const MachineSequencer::NoteEntry *_note_data) :
	ElementReference(note_container, id), selected(false), note_data(_note_data), loop(NULL) {

	set_event_handler(on_event);

	note = _key;
	on_at = _start_tick;
	length = _length;

	if(note_data)
		set_to(note_data);

	update_graphics();
}

Tracker::NoteGraphic::~NoteGraphic() {
	SATAN_DEBUG("NoteGraphic will drop_element() for ptr %p\n", pointer());
	drop_element();
}

void Tracker::NoteGraphic::delete_note_and_graphic(NoteGraphic *ng) {
	SATAN_DEBUG("Will try and delete note data - ng: %p, ng->note_data: %p, ng->loop: %p\n",
		    ng, ng->note_data, ng->loop);

	if(ng->is_selected()) ng->set_selected(false);

	if(ng->note_data && ng->loop)  {
		ng->loop->note_delete(ng->note_data);
		ng->note_data = NULL;
		ng->loop = NULL;
	}
	delete ng;
}

void Tracker::NoteGraphic::add_graphic(KammoGUI::SVGCanvas::ElementReference *note_container, const std::string &id) {
	std::stringstream ss;
	ss << "<g id=\"" << id << "\">\n"
	   << "<rect "
	   << " class=\"noteRect\" "
	   << " style=\"fill:#000000;stroke:none;fill-opacity:0.01\" "
	   << " x=\"0\" \n"
	   << " y=\"0\" \n"
	   << " width=\"1\" \n"
	   << " height=\"1\" \n"
	   << "/>\n"
	   << "<line class=\"noteLine\" "
	   << "stroke=\"#33ccff\" "
//	   << "stroke-linecap=\"round\" "
	   << "stroke-width=\"" << (bar_height / 3)  << "\" "
	   << "x1=\"0\" " // initial coordinates don't matter
	   << "x2=\"0\" "
	   << "y1=\"0\" "
	   << "y2=\"0\" "
	   << "/>\n"
	   << "<line class=\"stretchLine\" "
	   << "stroke=\"#33ccff\" "
//	   << "stroke-linecap=\"round\" "
	   << "stroke-width=\"" << (bar_height / 4)  << "\" "
	   << "x1=\"0\" " // initial coordinates don't matter
	   << "x2=\"0\" "
	   << "y1=\"0\" "
	   << "y2=\"0\" "
	   << "/>\n"
	   << "</g>";

	SATAN_DEBUG(" WILL ADD GRAPHIC.\n");
	note_container->add_svg_child(ss.str());
	SATAN_DEBUG(" GRAPHIC ADDED. (%p)\n", KammoGUI::SVGCanvas::ElementReference(note_container, id).pointer());
}

void Tracker::NoteGraphic::on_event(KammoGUI::SVGCanvas::SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
				    const KammoGUI::SVGCanvas::MotionEvent &event) {
	NoteGraphic *ctx = (NoteGraphic *)e_ref;

	static double start_x;
	static double start_y;
	static bool just_tap = true;

	double now_x = event.get_x();
	double now_y = event.get_y();

	SATAN_DEBUG("  NoteGraphic::on_event() for %p\n", ctx);

	double dist_x = fabs(now_x - start_x);
	double dist_y = fabs(now_y - start_y);

	switch(event.get_action()) {
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
	{
		if(just_tap) {
			if(dist_y > Tracker::bar_height)
				just_tap = false;
			if(dist_x > (Tracker::tick_width * 2)) // 2 is just an arbitrary factor
				just_tap = false;
		} else {
			{ // stretch block
				int tick_diff = (now_x - start_x) / (Tracker::tick_width * skip_interval);
				if(abs(tick_diff) > 0) {
					int new_length = ctx->length + tick_diff * skip_interval;
					if(new_length < 1) new_length = 1;
					ctx->length = new_length;
					ctx->update_data();
					ctx->update_graphics();
					start_x = now_x;
				}
			}
			{ // transpose block

				int key_diff = (now_y - start_y) / Tracker::bar_height;

				SATAN_DEBUG("  key_diff: %d (note: %d)\n", key_diff, ctx->note);

				if(abs(key_diff) > 0) {
					SATAN_DEBUG("   TRANSPOSE ************ %d\n", key_diff);

					ctx->note -= key_diff;
					ctx->update_data();
					ctx->update_graphics();
					start_y = now_y;
				}
			}
		}
	}
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
		start_x = now_x;
		start_y = now_y;
		just_tap = true;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
		if(just_tap) {
			ctx->set_selected(ctx->selected ? false : true);
		}
		break;
	}
}

Tracker::NoteGraphic *Tracker::NoteGraphic::create(KammoGUI::SVGCanvas::ElementReference *note_container, const std::string &id,
						   int _key, int _start_tick, int _length) {
	SATAN_DEBUG("NoteGraphic::create()\n");
	add_graphic(note_container, id);
	return new NoteGraphic(_key, _start_tick, _length,
			       note_container, id, NULL);
}

Tracker::NoteGraphic *Tracker::NoteGraphic::reference_existing(KammoGUI::SVGCanvas::ElementReference *note_container, const std::string &id,
							       const MachineSequencer::NoteEntry *_note_data,
							       MachineSequencer::Loop *_loop) {
	SATAN_DEBUG("NoteGraphic::reference_existing()\n");
	add_graphic(note_container, id);
	NoteGraphic *ng = new NoteGraphic(_note_data->note, _note_data->on_at, _note_data->length,
			       note_container, id, _note_data);
	// reference the loop here
	ng->loop = _loop;

	return ng;
}

Tracker::NoteGraphic *Tracker::NoteGraphic::clone(KammoGUI::SVGCanvas::ElementReference *note_container, const std::string &id,
						  const MachineSequencer::NoteEntry *_note_data) {
	SATAN_DEBUG("NoteGraphic::clone()\n");
	add_graphic(note_container, id);
	NoteGraphic *ng = new NoteGraphic(_note_data->note, _note_data->on_at, _note_data->length,
					  note_container, id, NULL);

	// clone the _note_data content
	ng->set_to(_note_data);
	return ng;
}

void Tracker::NoteGraphic::set_data(int _start_tick, int _length) {
	on_at = _start_tick;
	length = _length;

	update_data();
	update_graphics();
}

void Tracker::NoteGraphic::quantize() {
	on_at = MachineSequencer::quantize_tick(on_at);

	update_data();
	update_graphics();
}

bool Tracker::NoteGraphic::try_shift(int offset) {
	offset *= MACHINE_TICKS_PER_LINE;

	int new_on_at = (int)on_at;

	new_on_at += offset;
	if(new_on_at < 0) return false;

	return true;
}

void Tracker::NoteGraphic::shift(int offset) {
	offset *= MACHINE_TICKS_PER_LINE;

	int new_on_at = (int)on_at;

	new_on_at += offset;
	if(new_on_at < 0) new_on_at = 0;

	on_at = new_on_at;

	update_data();
	update_graphics();
}

bool Tracker::NoteGraphic::try_transpose(int offset) {
	int new_note = (int)note;

	new_note += offset;
	if(new_note < 0) return false;
	if(new_note > 127) return false;

	return true;
}

void Tracker::NoteGraphic::transpose(int offset) {
	int new_note = (int)note;

	new_note += offset;
	if(new_note < 0) new_note = 0;
	if(new_note > 127) new_note = 127;

	note = (uint8_t)new_note;

	update_data();
	update_graphics();
}

void Tracker::NoteGraphic::add_note_to_loop(MachineSequencer::Loop *new_loop) {
	loop = new_loop;
	update_data();
	update_graphics();
}

void Tracker::NoteGraphic::set_selected(bool _selected) {
	selected = _selected;
	update_graphics();

	KammoGUI::SVGCanvas::SVGRect rect;
	get_boundingbox(rect);
	SATAN_DEBUG("    scale_slider: get_viewport: (%f, %f) (%f, %f)\n",
		    rect.x, rect.y, rect.width, rect.height);

	if(selected) {
		scale_slider->show(
			rect.x, rect.y,
			rect.width, rect.height,

			finger_width * (double)(canvas_width_fingers - 3),
			finger_height * (double)(1),
			finger_width * 3.0,
			finger_height * 6.0
			);
		scale_slider->set_listener(this);
		scale_slider->set_label("Velocity");
		scale_slider->set_value(((double)velocity) / 127.0);
	} else {
		scale_slider->hide(
			rect.x, rect.y,
			rect.width, rect.height
			);
	}
}

bool Tracker::NoteGraphic::is_selected() {
	return selected;
}

void Tracker::NoteGraphic::on_scale_slider_changed(ScaleSlider *scl, double new_value) {
	new_value *= 127;

	velocity = (uint8_t)new_value;
	update_data();
	// update_graphics(); // no graphic update needed since velocity is not yet visible graphically
}

/***************************
 *
 *  Class Tracker::BarFlingAnimation
 *
 ***************************/

Tracker::BarFlingAnimation::BarFlingAnimation(
	float speed, float _duration,
	std::function<void(Tracker *context, float pixels_changed)> _callback,
	Tracker *_context) :
	KammoGUI::Animation(_duration), start_speed(speed), duration(_duration),
	current_speed(0.0f), deacc((start_speed < 0) ? (-FLING_DEACCELERATION) : (FLING_DEACCELERATION)),
	last_time(0.0f), callback(_callback), context(_context) {}

void Tracker::BarFlingAnimation::new_frame(float progress) {
	current_speed = start_speed - deacc * progress * duration;

	float time_diff = (progress * duration) - last_time;
	float pixels_change = time_diff * current_speed;

	callback(context, pixels_change);

	last_time = progress * duration;
}

void Tracker::BarFlingAnimation::on_touch_event() {
	stop();
}

/***************************
 *
 *  Class Tracker::UndoStack
 *
 ***************************/

void Tracker::UndoStack::push(const std::vector<NoteGraphic *> &graphics) {
	std::vector<MachineSequencer::NoteEntry> *buffer = new std::vector<MachineSequencer::NoteEntry>();
	if(buffer == NULL) throw std::bad_alloc();

	for(auto graphic : graphics){
		buffer->push_back(MachineSequencer::NoteEntry(graphic));
	}

	buffers.push(buffer);
}

std::vector<MachineSequencer::NoteEntry> * Tracker::UndoStack::pop() {
	if(buffers.size() == 0) throw std::length_error("Undo stack is empty.");

	std::vector<MachineSequencer::NoteEntry> *buffer = buffers.top();
	buffers.pop();

	return buffer;
}

void Tracker::UndoStack::clear() {
	while(!buffers.empty()) {
		delete buffers.top();
		buffers.pop();
	}
}

bool Tracker::UndoStack::is_empty() {
	return buffers.empty();
}

/***************************
 *
 *  Class Tracker
 *
 ***************************/

void debug_array(KammoGUI::SVGCanvas::SVGDocument *doc, const char *dbg_head, std::vector<KammoGUI::SVGCanvas::ElementReference *> lnts) {
	for(unsigned int k = 0; k < lnts.size(); k++) {
		auto lnt = lnts.at(k);
		std::string id = "failed to get id";
		try  {
			id = lnt->get_id();
		} catch(...) {
			SATAN_DEBUG("%s - Error in array[%d], failed to get any id at all for pointer %p\n",
				    dbg_head, k, lnt->pointer());
		}
		if(id.find("tickntimel_") == std::string::npos) {
			SATAN_DEBUG("%s - Error in array[%d], id for ptr %p is %s\n", dbg_head, k, lnt->pointer(), id.c_str());
		}
		try {
			std::stringstream new_id;
			new_id << "tickntimel_" << k;
			auto ref = KammoGUI::SVGCanvas::ElementReference(doc, new_id.str());
		} catch(...) {
			SATAN_DEBUG("%s - Error in array[%d], referencing deleted element: %p\n", dbg_head, k, lnt->pointer());
		}
	}
}

int Tracker::tick_width = 0;
int Tracker::bar_height = 0;
unsigned int Tracker::skip_interval = 0;
std::vector<MachineSequencer::NoteEntry> Tracker::clipboard;
Tracker::UndoStack Tracker::undo_stack;

void Tracker::clear_and_erase_current() {
	// delete all NoteGraphics and also delete the actual loop data
	if(mseq != NULL) {
		for(auto graphic : graphics) {
			NoteGraphic::delete_note_and_graphic(graphic);
		}
		graphics.clear();
	}
	get_parent()->redraw();
}

void Tracker::replace_from_buffer(std::vector<MachineSequencer::NoteEntry> &notes) {
	clear_and_erase_current();

	int k = 0;
	for(auto note : notes) {
		std::stringstream ss;
		ss << "note_graphic_" << k++;
		SATAN_DEBUG("pasting graphic: %s\n", ss.str().c_str());

		NoteGraphic *g = NoteGraphic::clone(note_container, ss.str(), &note);
		g->add_note_to_loop(current_loop);

		graphics.push_back(g);
	}
}

void Tracker::save_to_undo_buffer() {
	if(current_loop == NULL) return;

	undo_stack.push(graphics);
}

void Tracker::clear_note_graphics() {
	// only clear the graphics, don't delete it from the loop
	for(auto graphic : graphics) {
		SATAN_DEBUG("Delete graphic: %p (%p)\n", graphic, graphic->pointer());
		delete graphic;
	}
	graphics.clear();
	SATAN_DEBUG("graphics array size is %d\n", graphics.size());
}

void Tracker::generate_note_graphics() {
	if(current_loop == NULL) return;

 	const MachineSequencer::NoteEntry *note = current_loop->notes_get();

	int k = 0;
	while(note != NULL) {
		std::stringstream ss;
		ss << "note_graphic_" << k;

		SATAN_DEBUG("Show note: %s (%d, %d, %d)\n", ss.str().c_str(), note->note, note->on_at, note->length);

		NoteGraphic *g = NoteGraphic::reference_existing(note_container, ss.str(), note, current_loop);
		graphics.push_back(g);

		note = note->next;
	}
}

void Tracker::refresh_note_graphics() {
	clear_note_graphics();
	generate_note_graphics();
}

void Tracker::show_tracker_for(MachineSequencer *ms, int loop_id) {
	if(loop_id == NOTE_NOT_SET) loop_id = 0;

	undo_stack.clear();

	current_tracker->current_loop_id = NOTE_NOT_SET; // change this further down if we successfully can fetch the loop
	current_tracker->current_loop = NULL;
	current_tracker->mseq = NULL;

	if(ms) {
		try {
			// current_loop and mseq is only set if current_loop can be set without exception
			// otherwise they will remain NULL
			current_tracker->current_loop = ms->get_loop(loop_id);
			current_tracker->current_loop_id = loop_id;
			current_tracker->mseq = ms;
		} catch(MachineSequencer::ParameterOutOfSpec poos) {
			/// ignore this error
		}
	}

	static KammoGUI::UserEvent *ue = NULL;
	KammoGUI::get_widget((KammoGUI::Widget **)&ue, "showTrackerContainer");
	KammoGUI::EventHandler::trigger_user_event(ue);

	current_tracker_menu->set_loop_number(current_tracker->current_loop_id);

	// finally, refresh note graphics
	if(current_tracker->note_container)  // only refresh the note graphics here if we have a note container (the note container will not exist until the first time on_resize() is called)
		current_tracker->refresh_note_graphics();
}

void Tracker::on_render() {
	{ // Translate bars vertically
		// this is a faux scrolling routine that simulates that we have
		// 128 keys and bar graphic objects
		double bar_offset = vertical_offset / ((double)bar_height);
		int bar_offset_i = (int)bar_offset;

		int key = 128 + bar_offset_i - bar_count;

		KammoGUI::SVGCanvas::SVGMatrix bar_container_t;
		bar_container_t.translate(0.0, bar_height * (bar_offset - (double)bar_offset_i - 2));

		bar_container->set_transform(bar_container_t);
		piano_roll_container->set_transform(bar_container_t);

		auto key_graphic = keys.begin();
		for(auto bar : bars) {
			bar->key = key;

			int half_note = key % 12;
			int octave = key / 12;
			bool black_not_white = false;

			if(half_note == 1 || half_note == 3 || half_note == 6 || half_note == 8 || half_note == 10) {
				// black key
				black_not_white = true;
			}

			std::stringstream bar_style;
			bar_style << "fill:" << (black_not_white ? "#b4b4b4" : "#d4d4d4") << ";stroke:none;"
				  << "display:" << ((key < 128) ? "inline" : "none") << ";";
			bar->set_style(bar_style.str());

			std::stringstream key_style;
			key_style << "fill:" << (black_not_white ? "#000000" : "#ffffff") << ";stroke:none;"
				  << "display:" << ((key < 128) ? "inline" : "none") << ";";
			std::stringstream text_style;
			text_style << "fill:" << (black_not_white ? "white" : "black") << ";stroke:none;"
				   << "display:" << ((key < 128) ? "inline" : "none") << ";";

			try {
				KammoGUI::SVGCanvas::ElementReference roll_key = (*key_graphic)->find_child_by_class("roll_key");
				KammoGUI::SVGCanvas::ElementReference roll_text = (*key_graphic)->find_child_by_class("roll_text");

				roll_key.set_style(key_style.str());
				roll_text.set_style(text_style.str());

				std::stringstream key_text;
				key_text << key_names[half_note] << octave;
				roll_text.set_text_content(key_text.str());

				bar->str = key_text.str();
			} catch(...) {
				SATAN_ERROR("Tracker::on_render() couldn't find roll_text or roll_key class in key graphic.\n");
			}

			key_graphic++;
			key++;
		}
	}

	// calcualte current space between ticks, given the current zoom factor
	double tick_spacing = horizontal_zoom_factor * tick_width;

	// calculate line offset
	double line_offset_d = line_offset / (tick_spacing * (double)MACHINE_TICKS_PER_LINE);
	int line_offset_i = line_offset_d; // we need the pure integer version too

	// calculate the pixel offset for the first line
	double graphics_offset = 0.0;

	{ // select visible ticks, and translate time line graphics
		graphics_offset = (line_offset_d - (double)line_offset_i) * tick_spacing * ((double)MACHINE_TICKS_PER_LINE);

		double zfactor = ((double)MACHINE_TICKS_PER_LINE) / (horizontal_zoom_factor * 4.0f);

		skip_interval = (unsigned int)zfactor;

		// clamp skip_interval to [1, MAX_LEGAL]
		static int legal_intervals[] = {
			0, 1, 2, 2, 4, 4, 4, 4, 8, 8, 8,  8,  8,  8,  8,  8,  16
		};
#define MAX_LEGAL (sizeof(legal_intervals) / sizeof(int))
		if(skip_interval > MAX_LEGAL) skip_interval = MAX_LEGAL;
		if(skip_interval <= 0) skip_interval = 1;
		skip_interval = legal_intervals[skip_interval];

//		SATAN_DEBUG("skip_interval: %d\n", skip_interval);

		KammoGUI::SVGCanvas::SVGMatrix mtrx;

		// translate by graphics_offset
		mtrx.translate(graphics_offset, 0);

		for(unsigned int k = 0; k < lines_n_ticks.size(); k++) {
			// transform the line marker
			(lines_n_ticks[k])->set_transform(mtrx);

			// then do an additional translation for the next one
			mtrx.translate(tick_spacing, 0);

			// calculate prospective line number
			int line_number = (-line_offset_i + (k / MACHINE_TICKS_PER_LINE));

//			SATAN_DEBUG("       lines_n_ticks[%d] -> %p (%p) [%s]\n", k, lines_n_ticks[k], lines_n_ticks[k]->pointer(), lines_n_ticks[k]->get_id().c_str());
			if((line_number >= 0) && (k % skip_interval == 0)) {
				lines_n_ticks[k]->set_display("inline");

				try { // for lines (not ticks) set timetext to the current line number
					std::stringstream ss;
					ss << line_number;

					lines_n_ticks[k]->find_child_by_class("timetext").set_text_content(ss.str());
				} catch(KammoGUI::SVGCanvas::NoSuchElementException e) { /* timetext only available on main lines, not ticks */ }
			} else {
				lines_n_ticks[k]->set_display("none");
			}
		}
	}

	{ // Translate and scale note container
		graphics_offset = (line_offset_d) * tick_spacing * ((double)MACHINE_TICKS_PER_LINE);

		KammoGUI::SVGCanvas::SVGMatrix note_container_t;

		note_container_t.translate(0.0, (double)vertical_offset);
		note_container_t.scale((double)horizontal_zoom_factor, 1.0);
		note_container_t.translate(graphics_offset, 0.0);

		note_container->set_transform(note_container_t);
	}
}

void Tracker::clear_timelines() {
	for(auto line_or_tick : lines_n_ticks) {
		line_or_tick->drop_element();
		delete line_or_tick;
	}
	lines_n_ticks.clear();

	if(timeline_container) {
		timeline_container->drop_element();
		delete timeline_container; timeline_container = NULL;
	}
}

void Tracker::create_timelines() {
	KammoGUI::SVGCanvas::ElementReference root_element(this);

	{ // timeline group creation block
		std::stringstream ss;
		ss << "<svg id=\"timeline_container\" "
		   << " x=\"0\" \n"
		   << " y=\"0\" \n"
		   << " width=\"" << canvas_w << "\" \n"
		   << " height=\"" << canvas_h << "\" \n"
		   << "/>\n";
		root_element.add_svg_child(ss.str());

		timeline_container = new KammoGUI::SVGCanvas::ElementReference(this, "timeline_container");
	}

	{ // timeline creation block

		// "optimal" horizontal finger count
		double line_count_d = canvas_w_inches / INCHES_PER_FINGER;

		int line_count = (int)line_count_d; // line_count is now the number of VISIBLE lines
		tick_width = canvas_w / (line_count * MACHINE_TICKS_PER_LINE);

		// generate line_count * 2 + 2 line nodes, because of scrolling + zooming
		for(int k = 0; k < (line_count * 2 + 2) * MACHINE_TICKS_PER_LINE ; k++) {
			std::stringstream new_id;
			new_id << "tickntimel_" << k;
			if(k % MACHINE_TICKS_PER_LINE == 0) {
				// create main timeline

				std::stringstream ss;
				ss << "<svg id=\"" << new_id.str() << "\" "
				   << " x=\"0\" \n"
				   << " y=\"0\" \n"
				   << " width=\"" << bar_height << "\" \n" // use bar_height as width...
				   << " height=\"" << canvas_h << "\" \n"
				   << ">\n"
				   << "<line class=\"timeline\" "
				   << "stroke=\"black\" "
				   << "stroke-width=\"1.0\" "
				   << "x1=\"0\" "
				   << "x2=\"0\" "
				   << "y1=\"0\" "
				   << "y2=\"" << canvas_h << "\" "
				   << "/>\n"
				   << "<text x=\"5\" y=\"45\" "
				   << "class=\"timetext\" "
				   << "font-family=\"Roboto\" font-size=\"" << font_size << "\" fill=\"black\"> "
				   << "0"
				   << "</text>\n"
				   << "</svg>\n"
					;
				timeline_container->add_svg_child(ss.str());

				lines_n_ticks.push_back(new KammoGUI::SVGCanvas::ElementReference(this, new_id.str()));
			} else {
				// create MACHINE_TICKS_PER_LINE - 1 dashed lines marking the "in between" ticks per timeline

				std::stringstream ss;
				ss << "<line id=\"" << new_id.str() << "\" "
				   << "stroke=\"black\" "
				   << "stroke-opacity=\"0.2\" "
				   << "stroke-width=\"1.0\" "
				   << "x1=\"0\" "
				   << "x2=\"0\" "
				   << "y1=\"0\" "
				   << "y2=\"" << canvas_h << "\" "
				   << "/>\n";
				timeline_container->add_svg_child(ss.str());

				lines_n_ticks.push_back(new KammoGUI::SVGCanvas::ElementReference(this, new_id.str()));
			}
		}
	}
}

void Tracker::clear_bars() {
	for(auto bar : bars) {
		bar->drop_element();
		delete bar;
	}
	bars.clear();

	if(bar_container) {
		bar_container->drop_element();
		delete bar_container; bar_container = NULL;
	}
}

void Tracker::create_bars() {
	KammoGUI::SVGCanvas::ElementReference root_element(this);

	{ // bar group creation block
		std::stringstream ss;
		ss << "<svg id=\"bar_container\" "
		   << " x=\"0\" \n"
		   << " y=\"0\" \n"
		   << " width=\"" << canvas_w << "\" \n"
		   << " height=\"" << canvas_h << "\" \n"
		   << "/>\n";
		root_element.add_svg_child(ss.str());

		bar_container = new KammoGUI::SVGCanvas::ElementReference(this, "bar_container");
	}

	{ // bar creation block

		float bar_count_f = canvas_h_inches / INCHES_PER_FINGER;

		bar_count = (int)bar_count_f; // bar_count is now the number of VISIBLE bars
		bar_height = canvas_h / bar_count; // using the amount of VISIBLE bars we calculate the bar_height, in pixels

		// + 1 is because we need two additional bars to support our faux scrolling routine in on_render
		max_vertical_offset = (float)((128 - bar_count + 1) * bar_height);

		// - 2 for faux scrolling
		for(int k = (128 - bar_count - 2); k < 128; k++) {
			std::stringstream new_id;
			new_id << "bar_" << k;

			int half_note = k % 12;
			bool black_not_white = false;

			if(half_note == 1 || half_note == 3 || half_note == 6 || half_note == 8 || half_note == 10) {
				// black key
				black_not_white = true;
			}

			std::stringstream ss;
			ss << "<g>"
			   << "<rect id=\"" << new_id.str() << "\" "
			   << " style=\"fill:" << (black_not_white ? "#b4b4b4" : "#d4d4d4") << ";stroke:none;\" "
			   << " x=\"0\" \n"
			   << " y=\"" << (128 * bar_height - k * bar_height) << "\" \n"
			   << " width=\"" << canvas_w << "\" \n"
			   << " height=\"" << (bar_height - 5) << "\" \n"
			   << "/>\n"
			   << "</g>";
			bar_container->add_svg_child(ss.str());

			Bar *new_bar =
				new Bar(k, this, new_id.str());
			new_bar->set_event_handler(bar_on_event);

			bars.push_back(new_bar);
		}
	}
}

void Tracker::clear_piano_roll() {
	for(auto key : keys) {
		key->drop_element();
		delete key;
	}
	keys.clear();

	if(piano_roll_container) {
		piano_roll_container->drop_element();
		delete piano_roll_container; piano_roll_container = NULL;
	}
}

void Tracker::create_piano_roll() {
	KammoGUI::SVGCanvas::ElementReference root_element(this);

	{ // piano roll group creation block
		std::stringstream ss;
		ss << "<svg id=\"piano_roll_container\" "
		   << " x=\"0\" \n"
		   << " y=\"0\" \n"
		   << " width=\"" << canvas_w << "\" \n"
		   << " height=\"" << canvas_h << "\" \n"
		   << "/>\n";
		root_element.add_svg_child(ss.str());

		piano_roll_container = new KammoGUI::SVGCanvas::ElementReference(this, "piano_roll_container");
	}

	{ // piano roll creation block
		// - 2 for supporting faux scrolling in on_render()
		for(int k = (128 - bar_count - 2); k < 128; k++) {
			std::stringstream new_id;
			new_id << "key_" << k;

			int half_note = k % 12;
			int octave = k / 12;
			bool black_not_white = false;

			if(half_note == 1 || half_note == 3 || half_note == 6 || half_note == 8 || half_note == 10) {
				// black key
				black_not_white = true;
			}

			std::stringstream ss;
			ss << "<g id=\"" << new_id.str() << "\">\n"
			   << "<rect class=\"roll_key\" "
			   << " style=\"fill:" << (black_not_white ? "#000000" : "#ffffff") << ";fill-opacity:0.7;stroke:none;\" "
			   << " x=\"0\" \n"
			   << " y=\"" << (128 * bar_height - k * bar_height) << "\" \n"
			   << " width=\"" << bar_height << "\" \n"
			   << " height=\"" << (bar_height) << "\" \n"
			   << "/>\n"
			   << "<text class=\"roll_text\" x=\"5\" y=\"" << (128 * bar_height - k * bar_height + font_size) << "\" "
			   << "font-family=\"Roboto\" font-size=\"" << font_size << "\" fill=\"" << (black_not_white ? "white" : "black") << "\" >"
			   << key_names[half_note] << octave
			   << "</text>"
			   << "</g>";
			piano_roll_container->add_svg_child(ss.str());

			KammoGUI::SVGCanvas::ElementReference *new_key =
				new KammoGUI::SVGCanvas::ElementReference(this, new_id.str());
			new_key->set_event_handler(scroll_on_event);

			keys.push_back(new_key);
		}
	}
}

void Tracker::clear_time_index() {
	if(time_index_container) {
		time_index_container->drop_element();
		delete time_index_container; time_index_container = NULL;
	}

}

void Tracker::create_time_index() {
	KammoGUI::SVGCanvas::ElementReference root_element(this);

	{ // time index container
		std::stringstream ss;
		ss << "<svg id=\"time_index_container\" "
		   << " x=\"0\" \n"
		   << " y=\"0\" \n"
		   << " width=\"" << canvas_w << "\" \n"
		   << " height=\"" << bar_height << "\" \n"
		   << ">\n"
		   << "<rect id=\"time_index_rectangle\" "
		   << " style=\"fill:#33ccff;fill-opacity:0.5;stroke:none\" "
		   << " x=\"0\" \n"
		   << " y=\"0\" \n"
		   << " width=\"" << canvas_w << "\" \n"
		   << " height=\"" << bar_height << "\" \n"
		   << "/>\n"
		   << "</svg>\n";
		root_element.add_svg_child(ss.str());

		time_index_container = new KammoGUI::SVGCanvas::ElementReference(this, "time_index_container");
		time_index_container->set_event_handler(scroll_on_event);
	}
}

void Tracker::clear_note_container() {
	if(note_container) {
		note_container->drop_element();
		delete note_container; note_container = NULL;
	}

}

void Tracker::create_note_container() {
	KammoGUI::SVGCanvas::ElementReference root_element(this);

	{ // note container
		std::stringstream ss;
		ss << "<svg id=\"note_container\" "
		   << " x=\"0\" \n"
		   << " y=\"0\" \n"
		   << " width=\"" << canvas_w << "\" \n"
		   << " height=\"" << bar_height << "\" \n"
		   << ">\n"
		   << "</svg>\n";
		root_element.add_svg_child(ss.str());

		note_container = new KammoGUI::SVGCanvas::ElementReference(this, "note_container");
	}
}

void Tracker::clear_everything() {
	// clear stuff in reverse order of creation
	clear_time_index();
	clear_piano_roll();
	clear_note_container();
	clear_timelines();
	clear_bars();
}

void Tracker::refresh_svg() {
	SATAN_DEBUG("   refresh_svg()\n");
	clear_everything();

	// create stuff
	create_bars();
	create_timelines();
	create_note_container();
	create_piano_roll();
	create_time_index();

	// set default offsets
	line_offset = bar_height; // so first line is not stuck under the piano roll

	// finally, refresh note graphics
	if(note_container)  // only refresh the note graphics here if we have a note container (the note container will not exist until the first time on_resize() is called)
		refresh_note_graphics();

}

void Tracker::on_resize() {
	get_canvas_size(canvas_w, canvas_h);
	get_canvas_size_inches(canvas_w_inches, canvas_h_inches);

	double tmp;

	tmp = canvas_w_inches / INCHES_PER_FINGER;
	canvas_width_fingers = (int)tmp;
	tmp = canvas_h_inches / INCHES_PER_FINGER;
	canvas_height_fingers = (int)tmp;

	tmp = canvas_w / ((double)canvas_width_fingers);
	finger_width = tmp;
	tmp = canvas_h / ((double)canvas_height_fingers);
	finger_height = tmp;

	// font_size = finger_height / 3 - but in integer math
	font_size = 10 * finger_height;
	font_size /= 30;

	SATAN_DEBUG("Font size set to : %d\n", font_size);

	refresh_svg();

	// when the Tracker is created, set the vertical_offset to something sensible
	if(default_offset_not_set) {
		default_offset_not_set = false;
		vertical_offset = -(6 * 12 + 3) * bar_height;
		SATAN_DEBUG("default offset set to: %f\n", vertical_offset);
	}
}

void Tracker::scrolled_vertical(Tracker *ctx, float pixels_changed) {
	ctx->vertical_offset += pixels_changed;

	if(ctx->vertical_offset > 0) ctx->vertical_offset = 0;

	if(ctx->vertical_offset < (-ctx->max_vertical_offset) ) ctx->vertical_offset = -ctx->max_vertical_offset;
}

void Tracker::scrolled_horizontal(Tracker *ctx, float pixels_changed) {
	ctx->line_offset += pixels_changed;
}

bool Tracker::on_scale(KammoGUI::ScaleGestureDetector *detector) {
	horizontal_zoom_factor *= detector->get_scale_factor();
	if(horizontal_zoom_factor < 1.0) horizontal_zoom_factor = 1.0;

	line_offset *= detector->get_scale_factor();

	SATAN_DEBUG("  new zoom factor: %f\n", horizontal_zoom_factor);

	return true;
}

bool Tracker::on_scale_begin(KammoGUI::ScaleGestureDetector *detector) { return true; }
void Tracker::on_scale_end(KammoGUI::ScaleGestureDetector *detector) { }

void Tracker::start_add(int key, double x, double y) {
	if(!add_graphic) {
		bar_mode = bar_add_mode;
		bar_first_x = bar_current_x = x;
		bar_first_y = bar_current_y = y;

		std::stringstream ss;
		ss << "note_graphic_" << graphics.size();

		add_graphic = NoteGraphic::create(note_container, ss.str(), key, 0, 1);

		update_add_graphic();
	}
}

void Tracker::stop_add(double x, double y) {
	if(add_graphic) {
		save_to_undo_buffer();

		if(snap_mode == Tracker::snap_to_line) {
			add_graphic->length = add_graphic->length & MACHINE_LINE_BITMASK;
		}

		if(add_graphic->length == 0) add_graphic->length = skip_interval;

		add_graphic->add_note_to_loop(current_loop);
		graphics.push_back(add_graphic);
		add_graphic = NULL;
	}

	bar_mode = bar_default_mode;
}

void Tracker::update_add_graphic() {
	// if bar_mode = add, update the add_graphic
	if(add_graphic) {
		double x1, x2;

		if(bar_first_x < bar_current_x) {
			x1 = bar_first_x;
			x2 = bar_current_x;
		} else {
			x1 = bar_current_x;
			x2 = bar_first_x;
		}

		// calcualte current space between ticks, given the current zoom factor
		double tick_spacing = horizontal_zoom_factor * tick_width;

		double nx1 = (x1 - line_offset) / tick_spacing;
		double nx2 = (x2 - line_offset) / tick_spacing;


		int start = (int)nx1;
		int stop = (int)nx2;

		if(start < 0) start = 0;
		if(stop < 0) stop = 0;

		switch(snap_mode) {
		case Tracker::snap_to_tick:
			break;
		case Tracker::snap_to_line:
			start = start & MACHINE_LINE_BITMASK;
			stop = stop & MACHINE_LINE_BITMASK;
			break;
		}

		add_graphic->set_data(start, stop - start);
	}
}

void Tracker::bar_on_event(KammoGUI::SVGCanvas::SVGDocument *source,
			   KammoGUI::SVGCanvas::ElementReference *e_ref,
			   const KammoGUI::SVGCanvas::MotionEvent &event) {
	Tracker *ctx = (Tracker *)source;
	Bar *b = (Bar *)e_ref;

	if(ctx->bar_mode == bar_default_mode) {
		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			SATAN_DEBUG("START ADD key: %d (%s)\n", b->key, b->str.c_str());
			ctx->start_add(b->key, event.get_x(), event.get_y());
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			break;
		}
	} else if(ctx->bar_mode == bar_add_mode) {
		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			ctx->stop_add(event.get_x(), event.get_y());
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			ctx->bar_current_x = event.get_x();
			ctx->bar_current_y = event.get_y();

			ctx->update_add_graphic();
			break;
		}
	}
}

void Tracker::scroll_on_event(KammoGUI::SVGCanvas::SVGDocument *source,
			      KammoGUI::SVGCanvas::ElementReference *e_ref,
			      const KammoGUI::SVGCanvas::MotionEvent &event) {
	static double scroll_start_x = 0, scroll_start_y = 0;
	double scroll_x, scroll_y;

	Tracker *ctx = (Tracker *)source;

	static bool ignore_scroll = false;

	bool scroll_event = ctx->sgd->on_touch_event(event);

	if((!(scroll_event)) && (!ignore_scroll)) {
		if(ctx->fling_detector.on_touch_event(event)) {
			float speed_x, speed_y;
			float abs_speed_x, abs_speed_y;

			ctx->fling_detector.get_speed(speed_x, speed_y);
			ctx->fling_detector.get_absolute_speed(abs_speed_x, abs_speed_y);

			bool do_horizontal_fling = abs_speed_x > abs_speed_y ? true : false;

			float speed = 0.0f, abs_speed;
			std::function<void(Tracker *context, float pixels_changed)> scrolled;

			if(do_horizontal_fling) {
				abs_speed = abs_speed_x;
				speed = speed_x;
				scrolled = scrolled_horizontal;
			} else {
				abs_speed = abs_speed_y;
				speed = speed_y;
				scrolled = scrolled_vertical;
			}

			float fling_duration = abs_speed / FLING_DEACCELERATION;

			BarFlingAnimation *flinganim =
				new BarFlingAnimation(speed, fling_duration, scrolled, ctx);
			ctx->start_animation(flinganim);
		}

		switch(event.get_action()) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
			scroll_start_x = event.get_x();
			scroll_start_y = event.get_y();
			break;
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
			scroll_x = event.get_x() - scroll_start_x;
			scroll_y = event.get_y() - scroll_start_y;

			scrolled_horizontal(ctx, scroll_x);
			scrolled_vertical(ctx, scroll_y);

			scroll_start_x = event.get_x();
			scroll_start_y = event.get_y();
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

void Tracker::copy_selected() {
	clipboard.clear();

	// if nothing is selected, copy all NoteGraphic entries
	bool copy_all = !anything_selected();

	for(auto graphic : graphics){
		if(copy_all || graphic->is_selected()) {
			clipboard.push_back(MachineSequencer::NoteEntry(graphic));
		}
	}
	SATAN_DEBUG("Copy selected\n");
}

void Tracker::yes_overwrite(void *void_ctx) {
	Tracker *ctx = (Tracker *)void_ctx;

	SATAN_DEBUG("yes_overwrite() : %p, %p, %p\n", ctx, ctx->mseq, ctx->current_loop);

	// then paste
	if(ctx != NULL && ctx->mseq != NULL) {
		ctx->save_to_undo_buffer();
		ctx->replace_from_buffer(clipboard);
	}
}

void Tracker::yes_erase_all(void *void_ctx) {
	Tracker *ctx = (Tracker *)void_ctx;
	if(ctx) {
		ctx->save_to_undo_buffer();
		ctx->clear_and_erase_current();
	}
}

void Tracker::do_nothing(void *ctx) {
	// do not do anything
}

void Tracker::paste_selected() {
	SATAN_DEBUG("Paste selected\n");

	if(graphics.size() > 0) {
		std::ostringstream question;

		question << "Do you want overwrite the previous content?";

		KammoGUI::ask_yes_no("Overwrite?",
				     question.str(),
				     yes_overwrite, this,
				     do_nothing, this);

	} else {
		yes_overwrite(this);
	}
}

void Tracker::trash_selected() {
	if(graphics.size() > 0 && !(anything_selected())) {
		// we have graphics, but none is selected
		// if the user wants to erase the entire loop - do so - otherwise do nothing
		std::ostringstream question;

		question << "Do you want to erase the entire loop?";

		KammoGUI::ask_yes_no("Erase?",
				     question.str(),
				     yes_erase_all, this,
				     do_nothing, this);

	} else {
		// user has selected one or more notes, erase them directly

		save_to_undo_buffer();

		std::vector<NoteGraphic *>::iterator i = graphics.begin();
		while(i != graphics.end()) {
			if((*i)->is_selected()) {
				NoteGraphic::delete_note_and_graphic((*i));
				graphics.erase(i);
			} else {
				i++;
			}
		}
	}
	SATAN_DEBUG("Trash selected\n");
}

void Tracker::quantize_selected() {
	// if nothing is selected, transpose all NoteGraphic entries
	bool transpose_all = !anything_selected();

	save_to_undo_buffer();

	for(auto graphic : graphics) {
		if(transpose_all || graphic->is_selected()) graphic->quantize();
	}
}

void Tracker::shift_selected(int offset) {
	// if nothing is selected, transpose all NoteGraphic entries
	bool shift_all = !anything_selected();
	bool all_will_succeed = true;
	for(auto graphic : graphics) {
		if(shift_all || graphic->is_selected()) {
			all_will_succeed = all_will_succeed && graphic->try_shift(offset);
		}
	}
	if(all_will_succeed) {
		save_to_undo_buffer();

		for(auto graphic : graphics) {
			if(shift_all || graphic->is_selected()) graphic->shift(offset);
		}
	}
}

void Tracker::transpose_selected(int offset) {
	// if nothing is selected, transpose all NoteGraphic entries
	bool transpose_all = !anything_selected();
	bool all_will_succeed = true;
	for(auto graphic : graphics) {
		if(transpose_all || graphic->is_selected()) {
			all_will_succeed = all_will_succeed && graphic->try_transpose(offset);
		}
	}
	if(all_will_succeed) {
		save_to_undo_buffer();

		for(auto graphic : graphics) {
			if(transpose_all || graphic->is_selected()) graphic->transpose(offset);
		}
	}
}

void Tracker::previous_selected() {
	SATAN_DEBUG("Previous selected\n");
	int new_loop_id = current_loop_id - 1;
	if(new_loop_id >= 0) {
		show_tracker_for(mseq, new_loop_id);
	}
}

void Tracker::next_selected() {
	int new_loop_id = current_loop_id + 1;
	if(new_loop_id < 100) {
		SATAN_DEBUG("Next selected (%d) (max; %d)\n", new_loop_id, mseq->get_nr_of_loops());
		if(mseq->get_nr_of_loops() <= new_loop_id) {
			// add new loop first
			new_loop_id = mseq->add_new_loop();
			SATAN_DEBUG(" added new loop: %d\n", new_loop_id);
		}
		show_tracker_for(mseq, new_loop_id);
	}
}

void Tracker::restore_from_undo_buffer() {
	if(current_loop == NULL) return;

	std::vector<MachineSequencer::NoteEntry> *buffer = undo_stack.pop();
	replace_from_buffer(*buffer);
	delete buffer;
}

void Tracker::snap_toggled() {
	SATAN_DEBUG("Snap toggled.\n");
	switch(snap_mode) {
	case Tracker::snap_to_tick:
		snap_mode = Tracker::snap_to_line;
		break;
	case Tracker::snap_to_line:
		snap_mode = Tracker::snap_to_tick;
		break;
	}
}

Tracker::SnapMode Tracker::get_snap_mode() {
	return snap_mode;
}

bool Tracker::undo_stack_disabled() {
	return undo_stack.is_empty();
}

bool Tracker::anything_selected() {
	for(auto graphic : graphics) {
		if(graphic->is_selected()) return true;
	}
	return false;
}

void Tracker::select_none() {
	for(auto graphic : graphics) {
		if(graphic->is_selected()) graphic->set_selected(false);
	}
}

Tracker::Tracker(KammoGUI::SVGCanvas *cnvs, std::string fname) : SVGDocument(fname, cnvs), bar_mode(bar_default_mode), snap_mode(Tracker::snap_to_line), add_graphic(NULL), mseq(NULL), current_loop(NULL), default_offset_not_set(true), vertical_offset(0.0), horizontal_zoom_factor(1.0), line_offset(0.0), bar_container(NULL), piano_roll_container(NULL), timeline_container(NULL), time_index_container(NULL), note_container(NULL) {
	sgd = new KammoGUI::ScaleGestureDetector(this);
}

Tracker::~Tracker() {
	clear_everything();

	if(sgd) delete sgd;
}

/***************************
 *
 *  Kamoflage Event Declaration
 *
 ***************************/

KammoEventHandler_Declare(TrackerHandler,"tracker");

virtual void on_init(KammoGUI::Widget *wid) {
	if(wid->get_id() == "tracker") {
		KammoGUI::SVGCanvas *cnvs = (KammoGUI::SVGCanvas *)wid;
		cnvs->set_bg_color(1.0, 1.0, 1.0);

		current_tracker = new Tracker(cnvs, std::string(SVGLoader::get_svg_directory() + "/tracker.svg"));
		current_tracker_menu = new TrackerMenu(cnvs);
		current_select_none_button = new SelectNoneButton(cnvs);

		scale_slider = new ScaleSlider(cnvs);

		current_tracker_menu->tracker = current_tracker;
		current_select_none_button->tracker = current_tracker;
	}
}

KammoEventHandler_Instance(TrackerHandler);
