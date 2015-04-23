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

#ifndef TOPMENU_CLASS
#define TOPMENU_CLASS

#include "canvas_widget.hh"
#include "remote_interface.hh"

class TopMenu : public CanvasWidget, public RemoteInterface::GlobalControlObject::PlaybackStateListener {
private:
	KammoGUI::Canvas::SVGDefinition *bt_rewind_d;
	KammoGUI::Canvas::SVGDefinition *bt_play_d, *bt_play_pulse_d;
	KammoGUI::Canvas::SVGDefinition *bt_record_on_d, *bt_record_off_d;

	KammoGUI::Canvas::SVGDefinition *bt_project_d, *bt_project_sel_d;
	KammoGUI::Canvas::SVGDefinition *bt_compose_d, *bt_compose_sel_d;
	KammoGUI::Canvas::SVGDefinition *bt_jam_d, *bt_jam_sel_d;

	KammoGUI::Canvas::SVGBob *bt_rewind;
	KammoGUI::Canvas::SVGBob *bt_play, *bt_play_pulse;
	KammoGUI::Canvas::SVGBob *bt_record_on, *bt_record_off;

	KammoGUI::Canvas::SVGBob *bt_project, *bt_project_sel;
	KammoGUI::Canvas::SVGBob *bt_compose, *bt_compose_sel;
	KammoGUI::Canvas::SVGBob *bt_jam, *bt_jam_sel;

	static std::shared_ptr<TopMenu> top_menu;

	enum MenuSelection {
		selected_none, selected_project, selected_compose, selected_jam
	};

	MenuSelection current_selection;

	// since the sequence_row_playing_changed() callback is running
	// on the playback thread, we have this set_row_playing_markers() function
	// that the first function will put in queue to execute on the UI thread, where it belongs.
	static void run_play_pulse_marker(void *rowp);

	// this callback is called by Machine on every sequence_step line
	static void sequence_row_playing_changed(int row);

	int w;
	bool show_pulse;
	bool is_playing = false, is_recording = false;
public:
	TopMenu(CanvasWidgetContext *cwc);

	virtual void resized();
	virtual void expose();
	virtual void on_event(KammoGUI::canvasEvent_t ce, int x, int y);

	static void new_view_enabled(KammoGUI::Widget *w);

	static void setup(KammoGUI::Canvas *cnv);

	// RemoteInterface::GlobalControlObject::PlaybackStateListener
	virtual void playback_state_changed(bool is_playing) override;
	virtual void recording_state_changed(bool is_recording) override;
};

#endif
