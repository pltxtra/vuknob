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

#ifndef CLASS_MIDI_EXPORT
#define CLASS_MIDI_EXPORT

#include <iostream>
#include <stdint.h>
#include <vector>

class MidiExport {
public:
	class Chunk {
	private:
		bool finalized;
		bool was_dropped;
		
		uint32_t allocated_length;
		int pushed_data_length;
		uint8_t *data;
		uint32_t current_offset;

		friend class MidiExport;
		// Expects 4 bytes, always!
		Chunk(const uint8_t *header);
		~Chunk();

		void finalize(int fd);
		void verify_fit(int length);
	public:
		// this will tell the MidiExport system to ignore this particular chunk
		// when creating the output file. This is useful if you can not determine
		// early if a chunk will be needed or not.
		void drop();

		// push a string to the chunk
		void push_string(const std::string &str);
		
		// push bytes, local endian converted automatically to big endian.
		void push_u8b_word(uint8_t byte);
		void push_u16b_word(uint16_t word);
		void push_u32b_word(uint32_t word);

		// variable length fields (see MIDI file spec.) Automatic endian conversion
		void push_u8b_word_var_len(uint8_t word);
		void push_u16b_word_var_len(uint16_t word);		
		void push_u32b_word_var_len(uint32_t word); // 28 bit maximum, highest bits truncated
		
	};

private:
	std::vector<Chunk *> contained;
	std::string fname;

	bool was_finalized;
	
public:
	// create a new midi file
	MidiExport(const std::string &fname);
	~MidiExport();	

	// the header will be truncated to four chars, it expects ANSI encoding.
	Chunk *append_new_chunk(const std::string &header);

	// when done, finalize and write the file to disk
	void finalize_file();
};

#endif
