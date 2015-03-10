/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
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

#include <iostream>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <kamogui.hh>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string>
#include <sstream>

#include "license_view.hh"

#include "satan_debug.hh"

KammoEventHandler_Declare(LicenseViewHandler,"aboutLICENSE");

void add_line(KammoGUI::Container *cnt,
	      std::string line) {

	KammoGUI::Label *lbl = new KammoGUI::Label();
	lbl->set_title(line);

	cnt->add(*lbl);	
}

virtual void on_init(KammoGUI::Widget *wid) {
	std::istringstream license_stream;

	license_stream.str(GNU_GPL_CONTENT);

	KammoGUI::Container *cnt = (KammoGUI::Container *)wid;
	
	std::string s;
	while( getline(license_stream,s) ) {
		add_line(cnt, s);
	}	
}

KammoEventHandler_Instance(LicenseViewHandler);

