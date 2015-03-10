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
 * the GNU General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include <iostream>

#include "general_tools.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

bool remove_directory(std::string path) {
	static std::string offset = "";
	std::string this_offset = offset;
	
	// puh... we have to scan the whole shebang and delete ALL files...
	DIR *d = opendir(path.c_str());
	SATAN_DEBUG_("%sRemoving DIR %s\n", this_offset.c_str(), path.c_str());
	
	struct dirent *ent = NULL;
	
	if(d == NULL) {
		return false;
	}

	while((ent = readdir(d)) != NULL) {
		if(ent->d_type == DT_DIR) {
			if(!(
				(strlen(ent->d_name) == 1 && ent->d_name[0] == '.')
				||
				(strlen(ent->d_name) == 2 && ent->d_name[0] == '.' && ent->d_name[1] == '.')
				   )
				) {
				SATAN_DEBUG_("%s entering dir %s\n",
					     this_offset.c_str(), ent->d_name);
				offset = offset + "   ";
				if(!remove_directory(path + "/" + ent->d_name)) goto cleanup_n_fail;
				offset = this_offset;
			}
		}
		if(ent->d_type == DT_REG) {
			std::string fp = path + "/" + (ent->d_name);
			SATAN_DEBUG_("%s unlink file %s\n",
			       this_offset.c_str(), ent->d_name);
			if(unlink(fp.c_str())) goto cleanup_n_fail;
		}
	}

	closedir(d);
	if(rmdir(path.c_str())) goto fail;
	return true;

cleanup_n_fail:
	closedir(d);
fail:
	SATAN_DEBUG_("%sFAILED\n", this_offset.c_str());
	return false;
}

