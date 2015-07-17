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

#include "scales.hh"

namespace Scales {

	static bool scales_library_initialized = false;

	static struct ScaleEntry scales_library[] = {
		{ 0x0, "C- ", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x0, "C-m", {0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
			       0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x0, "C#m", {0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
			       0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x1, "D- ", {0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
			       0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x1, "D-m", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x1, "Db ", {0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
			       0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x1, "D#m", {0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
			       0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x2, "E- ", {0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
			       0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x2, "E-m", {0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x2, "Eb ", {0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
			       0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x3, "F- ", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x3, "F-m", {0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
			       0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x3, "F# ", {0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
			       0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x3, "F#m", {0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
			       0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x4, "G- ", {0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x4, "G-m", {0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
			       0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x4, "G#m", {0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
			       0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x5, "A- ", {0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
			       0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x5, "A-m", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x5, "Ab ", {0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
			       0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x6, "B- ", {0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
			       0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x6, "B-m", {0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
			       0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x6, "Bb ", {0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
			       0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
		{ 0x6, "Bbm", {0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
			       0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
			       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
			} },
	};

	static void initialize_scale(ScaleEntry *s) {
		for(int x = 0; x < 7; x++) {
			s->notes[x +  7] = s->notes[x % 8] + 12;
			s->notes[x + 14] = s->notes[x % 8] + 24;
		}
	}

	static int max_scales = -1;

	static inline void initialize_scales_library() {
		if(scales_library_initialized) return;

		scales_library_initialized = true;

		int k;

		max_scales = sizeof(scales_library) / sizeof(ScaleEntry);

		for(k = 0; k < max_scales; k++) {
			initialize_scale(&(scales_library[k]));
		}
	}

	static const char *key_text[] = {
		"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-",
		"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
	};

	const int get_number_of_scales() {
		initialize_scales_library();

		return max_scales;
	}

	const ScaleEntry* get_scale(int index) {
		index = (index % max_scales);
		initialize_scales_library();

		return &(scales_library[index]);
	}

	const char* get_key_text(int key) {
		return key_text[key % 12];
	}

};
