/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
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

#include <jngldrum/jexception.hh>
#include "wavloader.hh"
#include "signal.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>

#include <sys/mman.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include "fixedpointmath.h"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

// 30 seconds of 2 channel, 2 byte data at 44100 samples / second
// this we use as a limit of RAM size of statically loaded signals
#define __STATIC_SIGNAL_RAM_SIZE_LIMIT 44100 * 2 * 2 * 30

/*****************************
 *                           *
 * class WavLoader::Chunk    *
 *                           *
 *****************************/

void WavLoader::Chunk::process_header(uint8_t *header, uint8_t *bts) {
	std::ostringstream stream;
	stream << (char)(header[0])
	       << (char)(header[1])
	       << (char)(header[2])
	       << (char)(header[3]);
	header_str = stream.str();
	
	length =
		(((uint32_t)bts[0])      ) |
		(((uint32_t)bts[1]) <<  8) |
		(((uint32_t)bts[2]) << 16) |
		(((uint32_t)bts[3]) << 24);
}

WavLoader::Chunk::Chunk(uint8_t *mmapped, off_t *current_offset, off_t max_offset) throw (jException) : is_memory_mapped(true), data(NULL) {
	off_t ofs = *current_offset;

	if(ofs + 8 >= max_offset)
		throw jException("WAV file is incomplete - chunk descriptor broken.", jException::sanity_error);
	
	process_header(&mmapped[ofs], &mmapped[ofs + 4]);

	SATAN_DEBUG("ofs: %ld, length: %d, max: %ld\n",
		    ofs, length, max_offset);
	
	if((ofs + (off_t)(length & 0x7fffffff)) >= max_offset)
		throw jException("WAV file is incomplete on disk!", jException::sanity_error);
	
	data = &mmapped[ofs + 8];
	
	*current_offset = ofs + length + 8; // 8 is the chunk header, it must be added to the offset as well...
}

WavLoader::Chunk::Chunk(int f) throw (jException) : is_memory_mapped(false), data(NULL) {
	uint8_t header[4];
	if(read(f, header, 4) != 4)
		throw jException("RIFF/WAV File incomplete.",
				 jException::sanity_error);

	uint8_t bts[4];
	if(read(f, bts, 4) != 4)
		throw jException("RIFF/WAV File incomplete.",
				 jException::sanity_error);

	process_header(header, bts);
	
	data = (uint8_t *)malloc(length);
	if((uint32_t)read(f, data, length) != length) {
		free(data);
		throw jException("RIFF/WAV File incomplete.",
				 jException::sanity_error);
	}
}

WavLoader::Chunk::~Chunk() throw() {
	if((!is_memory_mapped) && (data != NULL)) {
		free(data);
	}
}


bool WavLoader::Chunk::name_is(const std::string &cmp) {
	return cmp == header_str ? true : false;
}

/*************************************
 *                                   *
 * class WavLoader::MemoryMappedWave *
 *                                   *
 *************************************/

WavLoader::MemoryMappedWave::~MemoryMappedWave() {
	if(format)
		delete format;
	if(data)
		delete data;
	
	if(mapped)
		munmap(mapped, file_size);
	close(f);
}

WavLoader::MemoryMappedWave::MemoryMappedWave(const std::string &fname) : format(NULL), data(NULL) {
	f = open(fname.c_str(), O_RDONLY);

	if(f == -1)
		throw jException("Failed to open file.",
				 jException::syscall_error);

	// get file size
	file_size = lseek(f, 0, SEEK_END);

	if(file_size < 12) {
		close(f);
		throw jException("File to short to be a WAV file.",
				 jException::sanity_error);
	}
	
	// mmap the file
	mapped = (uint8_t *)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, f, 0);

	if(mapped == NULL) {
		close(f);
		throw jException("Unable to memory map WAV file.",
				 jException::syscall_error);
	}
	
	// confirm header is RIFF/WAVE
	uint8_t *header = mapped;
	if(memcmp(header, "RIFF", 4) != 0) {
		munmap(mapped, file_size);
		close(f);
		throw jException("Not a RIFF file.",
				 jException::sanity_error);
	}

	if(memcmp(&header[8], "WAVE", 4) != 0) {
		munmap(mapped, file_size);
		close(f);
		throw jException("Not a RIFF/WAVE file.",
				 jException::sanity_error);
	}

	// read chunks
	off_t mmap_offset = 12; // first chunk starts at offset 12
	Chunk *tc = NULL;
	try {
		while(format == NULL || data == NULL) {
			tc = new Chunk(mapped, &mmap_offset, file_size);
			
			if(tc->name_is("fmt ")) format = tc;
			else if(tc->name_is("data")) data = tc;
			else delete tc;
			tc = NULL;
		}
	} catch(...) {
		if(tc != NULL)
			delete(tc);
		munmap(mapped, file_size);
		close(f);
		throw;
	}

	// OK, we got the chunks, read format parameters
	channels =
		(( (uint16_t)format->data[2]) & 0x00ff) |
		((((uint16_t)format->data[3]) & 0x00ff) << 8);
	sample_rate =
		(( (uint32_t)format->data[4]) & 0x000000ff) |
		((((uint32_t)format->data[5]) & 0x000000ff) <<  8) |
		((((uint32_t)format->data[6]) & 0x000000ff) << 16) |
		((((uint32_t)format->data[7]) & 0x000000ff) << 24);
	bits_per_sample =
		(( (uint32_t)format->data[14]) & 0x000000ff) |
		((((uint32_t)format->data[15]) & 0x000000ff) <<  8);

	std::cout << "channels["
		  << channels
		  << "] rate["
		  << sample_rate
		  << "] resolution ["
		  << bits_per_sample
		  << "] - length: "
		  << data->length
		  <<"\n";
}

uint16_t WavLoader::MemoryMappedWave::get_channels_per_sample() {
	return channels;
}

uint32_t WavLoader::MemoryMappedWave::get_sample_rate() {
	return sample_rate;
}

uint16_t WavLoader::MemoryMappedWave::get_bits_per_sample() {
	return bits_per_sample;
}

uint32_t WavLoader::MemoryMappedWave::get_length_in_samples() {
	if(data)
		return data->length / (channels * bits_per_sample / 8);
	return 0;
}
		
uint8_t *WavLoader::MemoryMappedWave::get_data_pointer() {
	if(data)
		return data->data;
	return NULL;
}

WavLoader::MemoryMappedWave *WavLoader::MemoryMappedWave::get(const std::string &fname) {
	return new MemoryMappedWave(fname);
}

/*****************************
 *                           *
 * class WavLoader           *
 *                           *
 *****************************/

WavLoader::WavLoader() : StaticSignalLoader("WavLoader") {}

WavLoader WavLoader::loader_instance;

bool WavLoader::is_signal_valid(const std::string &fname) {
	int f = open(fname.c_str(), O_RDONLY);

	if(f == -1)
		return false;

	uint8_t header[12];

	// read header
	if(read(f, header, 12) != 12) {
		close(f);
		return false;
	}
	close(f);

	if(memcmp(header, "RIFF", 4) != 0) {
		return false;
	}

	if(memcmp(&header[8], "WAVE", 4) != 0) {
		return false;
	}
	
	return true;
}

// this function converts a wav file into fixed point data (f8p24_t)
static int load_to_ram(fp8p24_t *ram_buffer, void *data, int channels, int samples, int bits_per_sample) {
	uint8_t *_byte = (uint8_t *)data;
	
	for(int k = 0; k < samples; k++) {
		for(int c = 0; c < channels; c++) {
			uint32_t tmp = 0;
			int32_t *tmp_signed = (int32_t *)&tmp;
			
			switch(bits_per_sample) {
			case 8:
				tmp = ((uint32_t)_byte[k * channels + c]) << 24;
 				break;

			case 16:
				tmp =
					(((uint32_t)_byte[2 * (k * channels + c) + 1]) << 24)
					|
					(((uint32_t)_byte[2 * (k * channels + c) + 0]) << 16)
					;
 				break;

			case 24:
				tmp =
					(((uint32_t)_byte[3 * (k * channels + c) + 2]) << 24)
					|
					(((uint32_t)_byte[3 * (k * channels + c) + 1]) << 16)
					|
					(((uint32_t)_byte[3 * (k * channels + c) + 0]) << 8)
					;
 				break;

			case 32:
				tmp =
					(((uint32_t)_byte[4 * (k * channels + c) + 3]) << 24)
					|
					(((uint32_t)_byte[4 * (k * channels + c) + 2]) << 16)
					|
					(((uint32_t)_byte[4 * (k * channels + c) + 1]) << 8)
					|
					(((uint32_t)_byte[4 * (k * channels + c) + 0]) << 0)
					;
 				break;

			default:
				return 0; // can't understand this resolution...
			}
			
			ram_buffer[k * channels + c] = (*tmp_signed) >> 8; // shift it down to a value between -1.0 to 1.0
		}
	}
	
	return -1; // success
}

void WavLoader::load_static_signal(const std::string &fname, bool only_preview, int static_index) {

	std::string basename = fname;

	try {
		basename =std::string(fname,
				      fname.rfind('/') + 1);
	} catch(...) {
	}
	
	MemoryMappedWave *mmap_wave = MemoryMappedWave::get(fname);
	if(!mmap_wave) {
		throw jException("Unable to allocate mmap wave.", jException::sanity_error);
	}
	
	uint32_t ram_size =
		mmap_wave->get_bits_per_sample() *
		mmap_wave->get_length_in_samples() / 8;
	
	if(ram_size > __STATIC_SIGNAL_RAM_SIZE_LIMIT) {
		// limit static signals to predefined ammount of RAM data...
		delete mmap_wave;

		throw jException("Static signal is larger than built-in threshold - refusing to load.", jException::sanity_error);
	}

	uint16_t channels = mmap_wave->get_channels_per_sample();
	uint32_t sample_rate = mmap_wave->get_sample_rate();
	uint16_t bits_per_sample = mmap_wave->get_bits_per_sample();
	uint32_t samples = mmap_wave->get_length_in_samples();

	// convert 8, 16, 24 or 32 into fp8p24 fixed point values
	fp8p24_t *ram_buffer = (fp8p24_t *)malloc(samples * channels * sizeof(fp8p24_t));
	if(ram_buffer) {
		if(load_to_ram(ram_buffer, mmap_wave->get_data_pointer(), channels, samples, bits_per_sample)) {
			if(only_preview) {
				preview_signal(_0D, _fx8p24bit, channels, samples, sample_rate, ram_buffer);
			} else {
				replace_signal(static_index, _0D, _fx8p24bit, channels, samples, sample_rate,
					       fname, basename, ram_buffer);
			}
			delete mmap_wave;
		} else {
			free(ram_buffer);
			delete mmap_wave;
			throw jException("File is not in expected format.",
					 jException::sanity_error);
		}
	}	
}
