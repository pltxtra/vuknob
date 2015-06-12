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

#ifndef PNCSEQUENCER_CLASS
#define PNCSEQUENCER_CLASS

#include "machine_sequencer.hh"
#include "canvas_widget.hh"

#include <kamogui_fling_detector.hh>

class RowNumberColumn;

class PncHelpDisplay : public CanvasWidget {
public:
	enum HelpType { _no_help, _first_help, _second_help, _third_help};

private:
	static KammoGUI::Canvas::SVGDefinition
		*btn_help_one_d,
		*btn_help_two_d,
		*btn_help_three_d
		;

	static KammoGUI::Canvas::SVGBob
		*btn_help_one,
		*btn_help_two,
		*btn_help_three
		;

	HelpType help_type;

public:

	PncHelpDisplay(
		CanvasWidgetContext *cwc,
		float x1,
		float y1,
		float x2,
		float y2,
		HelpType ht
		);

	virtual void resized();
	virtual void expose();
	virtual void on_event(KammoGUI::canvasEvent_t ce, int x, int y);

	void set_help_type(HelpType ht);
};

class PncLoopIndicator : public CanvasWidget {
public:
	enum LoopState {
		_on_, _on_b_, _on_c_, _on_e_, _no_
	};


private:
	friend class RowNumberColumn;

	class RowNumberColumn *parent;
	void (*parent_callback)(void *cbd, PncLoopIndicator *pli, KammoGUI::canvasEvent_t ce, int x, int y);

	static KammoGUI::Canvas::SVGDefinition
	*btn_loop_no_d,
		*btn_loop_on_d,
		*btn_loop_on_b_d,
		*btn_loop_on_c_d,
		*btn_loop_on_e_d,
		*btn_loop_off_d,
		*btn_loop_off_b_d,
		*btn_loop_off_c_d,
		*btn_loop_off_e_d
		;

	static KammoGUI::Canvas::SVGBob
	*btn_loop_no,
		*btn_loop_on,
		*btn_loop_on_b,
		*btn_loop_on_c,
		*btn_loop_on_e,
		*btn_loop_off,
		*btn_loop_off_b,
		*btn_loop_off_c,
		*btn_loop_off_e
		;

	LoopState state;
	std::string value;

	static bool loop_is_on;

public:
	PncLoopIndicator(
		CanvasWidgetContext *cwc,
		float x1,
		float y1,
		float x2,
		float y2,
		const std::string &value, int on_layers,
		RowNumberColumn *parent,
		void (*parent_callback)(void *cbd, PncLoopIndicator *pli, KammoGUI::canvasEvent_t ce, int x, int y)
		);

	virtual void resized();
	virtual void expose();
	virtual void on_event(KammoGUI::canvasEvent_t ce, int x, int y);

	LoopState get_state();
	void set_state(LoopState state);
	void set_value(const std::string &value);

	static void set_loop_on(bool is_on);
};


enum zoneMode {normalZone, hiddenZone, addnewZone};

class PncSeqZone : public CanvasWidget {
private:
	zoneMode mode;

	bool only_value; // true if this PncSeqZone only shows value (no other gfx..)

	// this is true if the zone is to use the mute selector graphics instead
	// of cursor graphics (used for the top row only)
	bool show_mute;

	bool show_value;

	bool do_shade;

	std::string value;

	bool selected;
	bool focus_left; // true if focus on left digit, false if focus on right digit..

	static KammoGUI::Canvas::SVGDefinition
	*btn_selected_l_d,
		*btn_selected_r_d,
		*btn_unselected_d,
		*btn_mute_on_d, *btn_mute_off_d,
		*btn_hidden_d,
		*btn_shade_d,
		*btn_plus_d;

	static KammoGUI::Canvas::SVGBob
	*btn_selected_l,
	*btn_selected_r,
		*btn_unselected,
		*btn_mute_on, *btn_mute_off,
		*btn_hidden,
		*btn_shade,
		*btn_plus;

	bool (*on_event_cb)(void *cbd, PncSeqZone *pnb, KammoGUI::canvasEvent_t ce, int x, int y);
	void (*state_change)(void *cbd, PncSeqZone *pnb, bool selected, bool focus_left);
	void *cbd;

public:

	PncSeqZone(
		CanvasWidgetContext *cwc,
		float x1,
		float y1,
		float x2,
		float y2,
		bool show_mute, bool show_value, bool only_value,
		std::string value, int on_layers);

	void set_callback_data(
		void *cbd,
		void (*state_change)(void *cbd, PncSeqZone *pnb, bool selected, bool focus_left),
		bool (*on_event_cb)(void *cbd, PncSeqZone *pnb, KammoGUI::canvasEvent_t ce, int x, int y)
		);

	void set_value(const std::string &value);
	void set_selected(bool selected, bool focus_left);
	bool get_selected();
	bool get_focus_left();

	void set_mode(zoneMode mode);

	void set_shade(bool shade);

	virtual void resized();
	virtual void expose();
	virtual void on_event(KammoGUI::canvasEvent_t ce, int x, int y);

};

class PncSequenceFlingAnimation : public KammoGUI::Animation {
private:
	float start_speed, duration, current_speed, deacc, zone_size;
	float last_time;
	float total_pixels_changed;
	void (*scrolled)(void *cbd, int rel);
	void *cbd; // call-back-data

public:
	PncSequenceFlingAnimation(float speed, float zone_size, float duration, void (*scrolled)(void *cbd, int rel), void *cbd);
	virtual void new_frame(float progress);
	virtual void on_touch_event();
};

class PncSequence {
private:
	// fling detector
	KammoGUI::FlingGestureDetector fling_detector;

	friend class PncSequencer;

	CanvasWidgetContext *context;

	MachineSequencer *mseq;

	PncSeqZone *seq_number_zone; // this is the top zone of the column, shows mute and sequence number
	std::map<int, PncSeqZone *> row; // this contain the rest of the column, each zone displays a row of the sequence data

	int scroll_start_w, scroll_start_h;
	bool scrolling, did_scroll;

	// call back, called from PncSeqZone on motion/click events
	static bool zone_event(void *cbd, PncSeqZone *szone, KammoGUI::canvasEvent_t ce, int x, int y);
	// call back, called from PncSeqZone when a zone's state has changed
	static void zone_state_change(
		void *cbd, PncSeqZone *szone, bool enabled, bool focus_left);

	// this function will be called when the user has done a horizontal scroll.
	void (*horizontal_scroll_cb)(void *cbd, int valu);
	// this function will be called when the user has done a vertical scroll.
	void (*vertical_scroll_cb)(void *cbd, int valu);
	// this function will be called when the user has selected a zone.
	void (*zone_selected_cb)(void *cbd, PncSequence *pncs, int row, bool focus_left);
public:

	PncSequence(
		CanvasWidgetContext *cwc,
		int column_number, int visible_rows,
		void (*_horizontal_scroll_cb)(void *cbd, int valu),
		void (*_vertical_scroll_cb)(void *cbd, int valu),
		void (*_zone_selected_cb)(void *cbd, PncSequence *pncs, int row, bool foc_left)
		);

	void set_current_playing_row(int row);

	void set_mode(zoneMode mode);

	void set_machine_sequencer(const std::string &title, MachineSequencer *_mseq);

	bool is_selected();
	void unselect_row(int row);
	void select_row(int row, bool sel_hi);

	void show_tracker();
	void show_controls();
	void show_envelopes();
};

// this class will just display current row numbers
class RowNumberColumn {
private:
	typedef enum {_set_start_, _move_loop_, _set_length_, _ignore_motion_} loopMotion_t;

	CanvasWidgetContext *context;

	int visible_rows; // this value says how many sequence rows that are currently visible

	bool just_do_tap;
	loopMotion_t loop_motion;

	int motion_start_x;

	std::map<int, PncLoopIndicator *> row;

	// call back, called from PncLoopIndicator on motion/click events
	static void loop_indicator_event(
		void *cbd, PncLoopIndicator *pli, KammoGUI::canvasEvent_t ce, int x, int y);

public:
	RowNumberColumn(
		CanvasWidgetContext *cwc,
		int visible_rows);

	void refresh();
};

class PncSequencer {
public:
	enum SequencerMode {
		no_selection,
		machine_selected,
		spacing_selected
	};

	typedef enum _KeySum {
		key_0 =      0x00,
		key_1 =      0x01,
		key_2 =      0x02,
		key_3 =      0x03,
		key_4 =      0x04,
		key_5 =      0x05,
		key_6 =      0x06,
		key_7 =      0x07,
		key_8 =      0x08,
		key_9 =      0x09,
		key_a =      0x0a,
		key_b =      0x0b,
		key_c =      0x0c,
		key_d =      0x0d,
		key_e =      0x0e,
		key_f =      0x0f,

		key_up =     0x10,
		key_down =   0x11,
		key_left =   0x12,
		key_right =  0x13,
		key_return = 0x14,
		key_delete = 0x15,
		key_blank =  0x16,

		key_connector_editor = 0x20,
		key_tracker =    0x21,
		key_control_editor = 0x22,
		key_envelope_editor = 0x23,

		key_alternative = 0x50,

		NO_KEY = 0xff
	} KeySum;

private:
	friend class PncSequence;
	friend class RowNumberColumn;

	static SequencerMode sequencer_mode;

	static PncHelpDisplay *phd; // we need a phd to explain this shit!
	static PncSeqZone *stz; // step zone, shows the sequence step
	static RowNumberColumn *rnrcol;
	static int sequence_row_offset, sequence_step;
	static int sequence_offset; // of the visible colums, which sequence is shown in the left most.
	static std::map<std::string, MachineSequencer *, ltstr> mseqs;

	static bool sel_seq_step; // this is true if input should go to the sequence_step parameter
	static int sel_seq, sel_row;
	static bool sel_hi; // select high byte or low byte

	// Canvas context
	static CanvasWidgetContext *context;

	static std::vector<PncSequence *> seq;

	//
	static void refresh_sequencers();

	// this is called when the user wants to increase step length/sequence_step
	static void step_increase(void *dbd, PncSeqZone *pnb, bool selected, bool focus_left);

	// these three are called (callbacks) from PncSequence
	static void horizontal_scroll(void *cbd, int valu);
	static void vertical_scroll(void *cbd, int valu);
	static void zone_selected(void *cbd, PncSequence *pncs, int row, bool focus_left);

	// since the sequence_row_playing_changed() callback is running
	// on the playback thread, we have this set_row_playing_markers() function
	// that the first function will put in queue to execute on the UI thread, where it belongs.
	static int last_row_playing_marker;
	static void set_row_playing_markers(int row);

	// this callback is called by Machine on every sequence_step line
	static void sequence_row_playing_changed(int row);

	// this is must be run when the machine sequencer set is changed to refresh the UI
	// must be called on UI thread
	static void machine_sequencer_set_changed(void *);

public:
	// this will make sure the machine_sequencer_set_changed() is called on the UI thread
	static void call_machine_sequencer_set_changed(void *);

	static SequencerMode get_sequencer_mode();
	static void send_keysum(bool button_down, KeySum ksum);
	static void init(CanvasWidgetContext *cwc);

};

#endif
