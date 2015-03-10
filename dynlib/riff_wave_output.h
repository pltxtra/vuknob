/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
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

#ifndef __RIFF_WAVE_OUTPUT_H
#define __RIFF_WAVE_OUTPUT_H

typedef struct _AsyncRiffWriter {
	AsyncOp operation;

	struct _AsyncRiffWriter *prev;
	struct _AsyncRiffWriter *next;

	int fd;
	size_t length;
	void *data;
} AsyncRiffWriter;

typedef struct _AsyncRiffWriterContainer {
	AsyncRiffWriter *unused;
	AsyncRiffWriter *pending;

	size_t current_nr_of_operations;
	size_t default_length;

	MachineTable *mt;
	void *machine_instance;
} AsyncRiffWriterContainer;

struct RIFF_WAVE_header {
	
	uint8_t RIFF[4];
	uint8_t length[4];
	uint8_t WAVE[4];

	uint8_t fmt_[4];
	uint8_t fmt_size[4];
	uint8_t fmt_format[2];
	uint8_t fmt_channels[2];
	uint8_t fmt_srate[4];
	uint8_t fmt_brate[4];
	uint8_t fmt_align[2];
	uint8_t fmt_bps[2];

	uint8_t data[4];
	uint8_t data_size[4];
};

typedef struct RIFF_WAVE_FILE {
	MachineTable *mt;
	
	struct RIFF_WAVE_header riff_header;
	int fd;
	uint32_t written_to_riff;

	AsyncRiffWriterContainer arwc;
} RIFF_WAVE_FILE_t;

void RIFF_prepare(MachineTable *mt, void *machine_instance, size_t buffer_size, RIFF_WAVE_FILE_t *rwf, uint32_t rate);
void RIFF_create_file(RIFF_WAVE_FILE_t *rwf, char *__file_name);
void RIFF_close_file(RIFF_WAVE_FILE_t *rwf);
void RIFF_write_data(RIFF_WAVE_FILE_t *rwf, void *data, int size);

#endif
