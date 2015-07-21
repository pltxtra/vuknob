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

#include "serialize.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

namespace Serialize {

	std::string decode_string(const std::string &encoded) {
		char inp[encoded.size() + 1];
		(void) strncpy(inp, encoded.c_str(), sizeof(inp));
		int k = 0;
		char *tok = inp;

		std::string output = "";

		while(inp[k] != '\0') {
			if(inp[k] == '%') { // OK, encoded value - decode it
				inp[k] = '\0';
				output = output + tok;

				char v[] = {inp[k + 1], inp[k + 2]};
				for(int u = 0; u < 2; u++) {
					if(v[u] <= 'f' && v[u] >= 'a') v[u] = 0xA + v[u] - 'a';
					else if(v[u] <= 'F' && v[u] >= 'A') v[u] = 0xA + v[u] - 'A';
					else if(v[u] <= '9' && v[u] >= '0') v[u] = v[u] - '0';
				}
				v[0] = (v[0] << 4) | v[1];
				v[1] = '\0';

				output += v;

				k += 3;

				tok = &inp[k];
			} else {
				k++;
			}
		}
		if(*tok != '\0')
			output = output + tok;

		return output;
	}

	std::string encode_string(const std::string &uncoded) {
		char inp[uncoded.size() + 1];
		(void) strncpy(inp, uncoded.c_str(), sizeof(inp));

		char *tok = inp;
		std::string output = "";

		for(int k = 0; inp[k] != '\0'; k++) {
			switch(inp[k]) {
			case '%':
				inp[k] = '\0';
				output = output + tok;
				output = output + "%25";
				tok = &inp[k + 1];
				break;
			case '=':
				inp[k] = '\0';
				output = output + tok;
				output = output + "%3d";
				tok = &inp[k + 1];
				break;
			case ';':
				inp[k] = '\0';
				output = output + tok;
				output = output + "%3b";
				tok = &inp[k + 1];
				break;
				break;
			}
		}
		if(*tok != '\0')
			output = output + tok;

		return output;
	}

};
