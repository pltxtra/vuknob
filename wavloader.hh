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

#ifndef _WAVLOADER_HH
#define _WAVLOADER_HH

#include <jngldrum/jexception.hh>
#include <stdint.h>
#include <machine.hh>

class WavLoader : public Machine::StaticSignalLoader {
private:
	class Chunk {
	private:
		bool is_memory_mapped;

		void process_header(uint8_t *header, uint8_t *bts);
	public:
		std::string header_str;
		uint32_t length;
		uint8_t *data;

		Chunk(int f) throw(jException);
		Chunk(uint8_t *mmapped, off_t *current_offset, off_t max_offset) throw(jException);
		~Chunk() throw();

		bool name_is(const std::string &cmp);
	};

	WavLoader();

	static WavLoader loader_instance;

protected:
	virtual bool is_signal_valid(const std::string &fname);	
	virtual void load_static_signal(const std::string &fname, bool only_preview, int static_index);

public:
	class MemoryMappedWave {
	private:
		uint16_t channels;
		uint32_t sample_rate;
		uint16_t bits_per_sample;

		off_t file_size;
		uint8_t *mapped;
		int f;

		Chunk *format, *data;
		
		friend class WavLoader;

		MemoryMappedWave(const std::string &fname); // privatize constructor
		
	public:
		~MemoryMappedWave();
		
		uint16_t get_channels_per_sample();
		uint32_t get_sample_rate();
		uint16_t get_bits_per_sample();
		uint32_t get_length_in_samples();
		
		uint8_t *get_data_pointer();

		static MemoryMappedWave *get(const std::string &fname);		
	};

};

#endif
