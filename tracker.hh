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

#ifndef TRACKER_CLASS
#define TRACKER_CLASS

#include <kamogui.hh>
#include <functional>
#include <kamogui_scale_detector.hh>
#include <kamogui_fling_detector.hh>

#include "machine_sequencer.hh"
#include "scale_slider.hh"

class Tracker : public KammoGUI::SVGCanvas::SVGDocument, public KammoGUI::ScaleGestureDetector::OnScaleGestureListener {
public:
	enum SnapMode {
		snap_to_tick, snap_to_line
	};

	static int tick_width, bar_height;
private:
	class Bar : public KammoGUI::SVGCanvas::ElementReference {
	public:
		int key;
		std::string str;

		Bar(int key, KammoGUI::SVGCanvas::SVGDocument *ctx, const std::string &id);

	};

	class NoteGraphic : public MachineSequencer::NoteEntry, public KammoGUI::SVGCanvas::ElementReference,
			    public ScaleSlider::ScaleSliderChangedListener {
	private:
		bool selected;

		const MachineSequencer::NoteEntry *note_data;
		MachineSequencer::Loop *loop;

		NoteGraphic(uint8_t key, unsigned int start_tick, unsigned int length, KammoGUI::SVGCanvas::ElementReference *note_container,
			    const std::string &id,
			    const MachineSequencer::NoteEntry *_node_data);
		void update_graphics();
		void update_data();

		static void add_graphic(KammoGUI::SVGCanvas::ElementReference *note_container, const std::string &id);

		static void on_event(KammoGUI::SVGCanvas::SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
				     const KammoGUI::SVGCanvas::MotionEvent &event);
	public:

		// this will delete the graphic, but not the note in the actual loop
		virtual ~NoteGraphic();
		// this will delete the graphic AND the note in the actual loop as well, if it exists
		static void delete_note_and_graphic(NoteGraphic *);

		static NoteGraphic *create(KammoGUI::SVGCanvas::ElementReference *note_container, const std::string &id,
					   int key, int start_tick, int length);
		// reference an existing NoteEntry from a loop
		static NoteGraphic *reference_existing(KammoGUI::SVGCanvas::ElementReference *note_container, const std::string &id,
						       const MachineSequencer::NoteEntry *note_data, MachineSequencer::Loop *loop);
		// clone the note_data into a new NoteGraphic entry, without attaching to an existing loop
		static NoteGraphic *clone(KammoGUI::SVGCanvas::ElementReference *note_container, const std::string &id,
					  const MachineSequencer::NoteEntry *note_data);

		void set_data(int start_tick, int length);
		void quantize();
		bool try_shift(int offset);
		void shift(int offset);
		bool try_transpose(int offset);
		void transpose(int offset);
		void add_note_to_loop(MachineSequencer::Loop *loop);

		void set_selected(bool selected);
		bool is_selected();

		virtual void on_scale_slider_changed(ScaleSlider *scl, double new_value);
	};

	class BarFlingAnimation : public KammoGUI::Animation {
	private:
		float start_speed, duration, current_speed, deacc;
		float last_time;

		std::function<void(Tracker *context, float pixels_changed)> callback;
		Tracker *context;

	public:
		BarFlingAnimation(float speed, float duration,
				  std::function<void(Tracker *context, float pixels_changed)> callback,
				  Tracker *context);
		virtual void new_frame(float progress);
		virtual void on_touch_event();
	};

	class UndoStack {
	private:
		std::stack<std::vector<MachineSequencer::NoteEntry> *> buffers;

	public:
		void push(const std::vector<NoteGraphic *> &old_data);
		std::vector<MachineSequencer::NoteEntry> *pop();
		void clear();
		bool is_empty();
	};

	static UndoStack undo_stack;

	static std::vector<MachineSequencer::NoteEntry> clipboard;

	// bar touch data
	enum BarMode {
		bar_default_mode, bar_add_mode
	};

	BarMode bar_mode;
	SnapMode snap_mode;
	double bar_first_x, bar_first_y;
	double bar_current_x, bar_current_y;
	NoteGraphic *add_graphic;

	void clear_and_erase_current();
	void replace_from_buffer(std::vector<MachineSequencer::NoteEntry> &buffer);
	void save_to_undo_buffer();

	void start_add(int key, double x, double y);
	void stop_add(double x, double y);
	void update_add_graphic();

	// yes/no functions (used when asking the user yes or no questions
	static void yes_overwrite(void *void_ctx);
	static void yes_erase_all(void *void_ctx);
	static void do_nothing(void *ctx);

	// Tracker data
	MachineSequencer *mseq;
	MachineSequencer::Loop *current_loop; // which loop we are currently editing
	int current_loop_id;
	std::vector<NoteGraphic *> graphics;

	void clear_note_graphics();
	void generate_note_graphics();
	void refresh_note_graphics();

	// fling detector
	KammoGUI::FlingGestureDetector fling_detector;

	// BEGIN scale detector
	KammoGUI::ScaleGestureDetector *sgd;
	virtual bool on_scale(KammoGUI::ScaleGestureDetector *detector);
	virtual bool on_scale_begin(KammoGUI::ScaleGestureDetector *detector);
	virtual void on_scale_end(KammoGUI::ScaleGestureDetector *detector);
	// END scale detector

	bool default_offset_not_set; // on first creation, force the vertical offset to a sensible default.
	int bar_count; // number of _visible_ bars (total number of bars is 128)
	int canvas_w, canvas_h;
	static unsigned int skip_interval;
	float canvas_w_inches, canvas_h_inches;
	double vertical_offset, max_vertical_offset;
	double horizontal_zoom_factor, line_offset;

	KammoGUI::SVGCanvas::ElementReference *bar_container;
	KammoGUI::SVGCanvas::ElementReference *piano_roll_container;
	KammoGUI::SVGCanvas::ElementReference *timeline_container;
	KammoGUI::SVGCanvas::ElementReference *time_index_container;
	KammoGUI::SVGCanvas::ElementReference *note_container;

	std::vector<Bar *> bars;
	std::vector<KammoGUI::SVGCanvas::ElementReference *> keys;
	std::vector<KammoGUI::SVGCanvas::ElementReference *> lines_n_ticks;

	void clear_bars();
	void create_bars();

	void clear_timelines();
	void create_timelines();

	void clear_piano_roll();
	void create_piano_roll();

	void clear_time_index();
	void create_time_index();

	void clear_note_container();
	void create_note_container();

	void clear_everything();
	void refresh_svg();

	static void scrolled_vertical(Tracker *ctx, float pixels_changed);
	static void scrolled_horizontal(Tracker *ctx, float pixels_changed);

	static void bar_on_event(KammoGUI::SVGCanvas::SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
				     const KammoGUI::SVGCanvas::MotionEvent &event);
	static void scroll_on_event(KammoGUI::SVGCanvas::SVGDocument *source, KammoGUI::SVGCanvas::ElementReference *e_ref,
				    const KammoGUI::SVGCanvas::MotionEvent &event);
public:
	bool anything_selected();

	// these are called by the TrackerMenu
	void copy_selected();
	void paste_selected();
	void trash_selected();
	void quantize_selected();
	void shift_selected(int line_offset); // shifts notes X lines left or right
	void transpose_selected(int offset);
	void previous_selected();
	void next_selected();
	void restore_from_undo_buffer();
	void snap_toggled();
	SnapMode get_snap_mode();
	bool undo_stack_disabled();

	// these are called by SelectNoneButton
	void select_none(); // deselect all NoteGraphics

	//
	Tracker(KammoGUI::SVGCanvas *cnv, std::string file_name);
	~Tracker();

	virtual void on_resize();
	virtual void on_render();

	static void show_tracker_for(MachineSequencer *ms, int loop_id);
};

class TrackerMenu : public KammoGUI::SVGCanvas::SVGDocument {
private:
	class HelpTextAnimation : public KammoGUI::Animation {
	private:
		KammoGUI::SVGCanvas::ElementReference *help_text;
	public:
		HelpTextAnimation(KammoGUI::SVGCanvas::ElementReference* _help_text, const std::string &display_text);
		virtual void new_frame(float progress);
		virtual void on_touch_event();
	};

	bool rim_visible;

	Tracker::SnapMode last_snap_mode;

	int pixel_w, pixel_h;
	float scale, translate_h, translate_v;

	KammoGUI::SVGCanvas::ElementReference menu_center;
	KammoGUI::SVGCanvas::ElementReference menu_rim;
	KammoGUI::SVGCanvas::ElementReference menu_loopNumberText;
	KammoGUI::SVGCanvas::ElementReference menu_help;

	KammoGUI::SVGCanvas::ElementReference menu_ignoreCircle;

	KammoGUI::SVGCanvas::ElementReference menu_copy;
	KammoGUI::SVGCanvas::ElementReference menu_paste;
	KammoGUI::SVGCanvas::ElementReference menu_trash;

	KammoGUI::SVGCanvas::ElementReference menu_quantize;
	KammoGUI::SVGCanvas::ElementReference menu_shiftLeft;
	KammoGUI::SVGCanvas::ElementReference menu_shiftRight;
	KammoGUI::SVGCanvas::ElementReference menu_transposeUp;
	KammoGUI::SVGCanvas::ElementReference menu_transposeDown;
	KammoGUI::SVGCanvas::ElementReference menu_octaveUp;
	KammoGUI::SVGCanvas::ElementReference menu_octaveDown;

	KammoGUI::SVGCanvas::ElementReference menu_undo;
	KammoGUI::SVGCanvas::ElementReference menu_disableUndo;

	KammoGUI::SVGCanvas::ElementReference menu_previousLoop;
	KammoGUI::SVGCanvas::ElementReference menu_nextLoop;

	KammoGUI::SVGCanvas::ElementReference menu_snapTo;
	KammoGUI::SVGCanvas::ElementReference menu_disableSnap;

	double first_selection_x, first_selection_y;
	bool is_a_tap;
	static void menu_selection_on_event(KammoGUI::SVGCanvas::SVGDocument *source,
					    KammoGUI::SVGCanvas::ElementReference *e_ref,
					    const KammoGUI::SVGCanvas::MotionEvent &event);
	static void menu_center_on_event(KammoGUI::SVGCanvas::SVGDocument *source,
					 KammoGUI::SVGCanvas::ElementReference *e_ref,
					 const KammoGUI::SVGCanvas::MotionEvent &event);

public:
	Tracker *tracker;

	TrackerMenu(KammoGUI::SVGCanvas *cnv);

	void set_loop_number(int loop_nr);

	virtual void on_resize();
	virtual void on_render();
};

class SelectNoneButton : public KammoGUI::SVGCanvas::SVGDocument {
private:
	KammoGUI::SVGCanvas::ElementReference select_none;

	double first_selection_x, first_selection_y;
	bool is_a_tap;

	int pixel_w, pixel_h;
	float scale, translate_h, translate_v;

	static void on_event(KammoGUI::SVGCanvas::SVGDocument *source,
			     KammoGUI::SVGCanvas::ElementReference *e_ref,
			     const KammoGUI::SVGCanvas::MotionEvent &event);
public:
	Tracker *tracker;

	SelectNoneButton(KammoGUI::SVGCanvas *cnv);

	virtual void on_resize();
	virtual void on_render();
};

#endif
