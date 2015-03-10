/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
 * Copyright (C) 2012 by Anton Persson
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

#ifndef __WHISTLE_UI_COUNTDOWN_HH
#define __WHISTLE_UI_COUNTDOWN_HH

#include <jngldrum/jthread.hh>
#include "canvas_widget.hh"

class CounterDowner : public CanvasWidget {
public:
	CounterDowner(CanvasWidgetContext *cwc);
	
	virtual void resized();
	virtual void expose();
	virtual void on_event(KammoGUI::canvasEvent_t ce, int x, int y);

	void set_state(const std::string &msg, int phase); // phase = [0, 1, 2, 3]	
};

class WhistleUICountdown : public jThread {
private:

	static CounterDowner *downcounter;
	
public:
	virtual void thread_body();
};

#endif
