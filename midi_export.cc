/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2011 by Anton Persson
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

#include "midi_export.hh"

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <jngldrum/jthread.hh>

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

MidiExport::Chunk::Chunk(const uint8_t *header) : finalized(false), was_dropped(false), allocated_length(0), pushed_data_length(0), data(NULL), current_offset(8) {
       	
	allocated_length = 128;
	data = (uint8_t *)malloc(allocated_length);

	if(data == NULL)
		throw jException("Unable to allocate new MIDI-export chunk data block.",
				 jException::syscall_error);

	memset(data, 0, allocated_length);
	memcpy(data, header, 4);
}

MidiExport::Chunk::~Chunk() {
	if(data != NULL) free(data);
}

void MidiExport::Chunk::finalize(int f) {
	finalized = true;

	uint32_t final_size = current_offset - 8;

	if(was_dropped || final_size == 0) return; // no dropped or empty blocks, please...
	
	data[4] = (final_size & 0xff000000) >> 24;
	data[5] = (final_size & 0x00ff0000) >> 16;
	data[6] = (final_size & 0x0000ff00) >>  8;
	data[7] = (final_size & 0x000000ff) >>  0;

	if((int)current_offset != write(f, data, current_offset))
		throw jException("Failed to write complete chunk to MIDI export target file.",
				 jException::syscall_error);
}

void MidiExport::Chunk::verify_fit(int length) {
	if((current_offset + length) >= allocated_length) {
		uint8_t *n = (uint8_t *)realloc(data, allocated_length * 2);
		if(n == NULL)
			throw jException("Failed to extend MIDI-export chunk memory segment!",
					 jException::syscall_error);

		data = n;
		allocated_length *= 2;
	}
}

void MidiExport::Chunk::drop() {
	was_dropped = true;
}

void MidiExport::Chunk::push_string(const std::string &str) {
	const char *tn = str.c_str();
	int tn_strlen = strlen(tn);
	push_u32b_word_var_len(tn_strlen);
	for(int k = 0; k < tn_strlen; k++) {
		push_u8b_word(tn[k]);
	}
}

void MidiExport::Chunk::push_u8b_word(uint8_t word) {
	verify_fit(1);
	data[current_offset++] = word;
}

void MidiExport::Chunk::push_u16b_word(uint16_t word) {
	verify_fit(2);
	data[current_offset++] = (word & 0xff00) >> 8;
	data[current_offset++] = (word & 0x00ff);
}

void MidiExport::Chunk::push_u32b_word(uint32_t word) {
	verify_fit(4);
	data[current_offset++] = (word & 0xff000000) >> 24;
	data[current_offset++] = (word & 0x00ff0000) >> 16;
	data[current_offset++] = (word & 0x0000ff00) >> 8;
	data[current_offset++] = (word & 0x000000ff);
}

void MidiExport::Chunk::push_u8b_word_var_len(uint8_t byte) {
	push_u32b_word_var_len((uint32_t)byte);
}

void MidiExport::Chunk::push_u16b_word_var_len(uint16_t word) {
	push_u32b_word_var_len((uint32_t)word);
}

void MidiExport::Chunk::push_u32b_word_var_len(uint32_t word) {
	verify_fit(4);

	uint32_t result = 0;
	bool just_continue = false;

	// truncate four highest bits, not supported
	word &= 0x0fffffff;
	
	if(word & 0x0fc00000) {
		//    0b 0000 1111 1110 0000 0000 0000 0000 0000
		// => 0b Y111 1111 Y000 0000 Y000 0000 N000 0000 where Y means byte follows, and N means no byte follows
		result |=
			(0x80000000) | // set the first Y bit
			((0x0fe00000 & word) << 3); // upshift the first seven bits three positions
		result = result >> 24;
		data[current_offset++] = result & 0xff;
		just_continue = true;
	}

	if(just_continue || (word & 0x001fc000)) {
		//    0b 0000 0000 0001 1111 1100 0000 0000 0000
		// => 0b Y111 1111 Y000 0000 N000 0000 where Y means byte follows, and N means no byte follows
		result |=
			(0x00800000) | // set the second Y bit
			((0x001fc0000 & word) << 2); // upshift the first seven bits two positions
		result = result >> 16;
		data[current_offset++] = result & 0xff;
		just_continue = true;
	}

	if(just_continue || (word & 0x00003f80)) {
		//    0b 0000 0000 0000 0000 0011 1111 1000 0000
		// => 0b Y111 1111 N000 0000 where Y means byte follows, and N means no byte follows
		result |=
			(0x00008000) | // set the third Y bit
			((0x3f80 & word) << 1); // upshift the first seven bits one position
		result = result >> 8;
		data[current_offset++] = result & 0xff;
	} 

	data[current_offset++] = word & 0x0000007f; // the last seven bits
}

MidiExport::MidiExport(const std::string &_fname) : fname(_fname), was_finalized(false) {}

MidiExport::~MidiExport() {
	std::vector<Chunk *>::iterator k;

	// delete all contained chunks
	for(k  = contained.begin();
	    k != contained.end();
	    k++) {
		delete (*k);
	}
}

MidiExport::Chunk *MidiExport::append_new_chunk(const std::string &header) {
	Chunk *c = new Chunk((uint8_t *)header.c_str());

	if(c == NULL) throw jException("Unable to allocate new MIDI export chunk.", jException::syscall_error);

	contained.push_back(c);

	return c;
}

void MidiExport::finalize_file() {
	if(was_finalized) throw jException("MidiExport file was already finalized once!",
					   jException::sanity_error);

	was_finalized = true;

	SATAN_DEBUG_("will open MIDI file: %s\n", fname.c_str());
	
	int f = open(fname.c_str(), O_CREAT | O_RDWR | O_TRUNC,
		     S_IRUSR | S_IWUSR |
		     S_IRGRP |
		     S_IROTH);

	SATAN_DEBUG_("MIDI file was opened: %d\n", f);

	if(f == -1)
		throw jException("Unable to open MIDI export target file.",
				 jException::syscall_error);

	SATAN_DEBUG_("MIDI file will be written\n");
	
	std::vector<Chunk *>::iterator k;

	for(k  = contained.begin();
	    k != contained.end();
	    k++) {
		try {
			(*k)->finalize(f);
		} catch(...) {
			// in case of an exception, close the file and rethrow
			close(f);
			throw; 
		}
	}

	SATAN_DEBUG_("Closing MIDI file...\n");
	close(f);
}
