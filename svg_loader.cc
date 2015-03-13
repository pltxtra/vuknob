/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include "svg_loader.hh"
#include "common.hh"

#include <fstream>
#include <dirent.h>
#include <unistd.h>

std::string SVGLoader::svg_directory;

void SVGLoader::locate_SVG_directory() {
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

std::string SVGLoader::get_svg_directory() {
	static bool init_statics_done = false;
	
	if(init_statics_done) return svg_directory;
	
	/* locate SVG directory */
	locate_SVG_directory();

	init_statics_done = true;

	return svg_directory;
}

std::string SVGLoader::get_svg_path(const std::string &fname) {
	return get_svg_directory() + "/" + fname;
}
