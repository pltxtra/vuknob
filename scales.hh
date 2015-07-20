/*
 * VuKNOB
 * Copyright (C) 2015 by Anton Persson
 *
 * http://www.vuknob.com/
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

#ifndef SCALES_HH
#define SCALES_HH

namespace Scales {

	struct ScaleEntry {
		int offset;
		const char *name;
		int notes[21];
	};

	const int get_number_of_scales();
	const ScaleEntry* get_scale(int index);
	const char* get_key_text(int key);
	int get_custom_scale_note(int offset);
	void set_custom_scale_note(int offset, int note);

};

#endif
