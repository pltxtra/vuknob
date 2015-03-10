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

#ifndef __LIBTESTBENCH_H
#define __LIBTESTBENCH_H

#include "dynlib_debug.h"

struct signus {
	const char *name;

	int is_static;
	struct signus *next;
	void *mockery;
	
	int dim, chan, res, freq, sam;

	void *u_data;

	uint8_t *pad_a, *pad_b;
	void *data;
	uint8_t *pad_data;


};

struct mockery {
	MachineTable mt;

	void *machine_data;

	int current_test_step;
	int test_step_length;
	
	struct signus *inputs;
	struct signus *intermediates;
	struct signus *outputs;
};

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

};

struct RIFF_WAVE_chunk {
	uint8_t iden[4];
	uint8_t leng[4];
};

typedef struct RIFF_WAVE_FILE {
	int read_only;
	
	struct RIFF_WAVE_header riff_header;
	struct RIFF_WAVE_chunk data_chunk;
	int fd;
	uint32_t written_to_riff;

	int samples, channels;
} RIFF_WAVE_FILE_t;

void RIFF_prepare(RIFF_WAVE_FILE_t *rwf);
void RIFF_create_file(RIFF_WAVE_FILE_t *rwf, const char *__file_name);
int RIFF_open_file(RIFF_WAVE_FILE_t *rwf, const char *name);
int RIFF_read_data(RIFF_WAVE_FILE_t *rwf, int16_t *data, int samples);
void RIFF_close_file(RIFF_WAVE_FILE_t *rwf);
void RIFF_write_data(
	RIFF_WAVE_FILE_t *rwf,
	void *data,
	int size);
void interleave(int16_t *destination, int offset, const FTYPE *in, int samples);
void write_wav(const FTYPE *left, const FTYPE *right, int samples, const char *fname);

int prepare_mockery(struct mockery *m, int test_step_length, const char *machine_name);
int create_mock_input_signal(
	struct mockery *m, const char *name,
	int dimension,
	int channels,
	int resolution,
	int frequency,
	int samples,
	void *data_pointer);
int create_mock_intermediate_signal(
	struct mockery *m, const char *name,
	int dimension,
	int channels,
	int resolution,
	int frequency,
	int samples,
	void *data_pointer);
int create_mock_output_signal(
	struct mockery *m, const char *name,
	int dimension,
	int channels,
	int resolution,
	int frequency,
	int samples,
	void *data_pointer);

void *get_controller(struct mockery *m, const char *name, const char *group);

void convert_pcm_to_mock_signal(
	FTYPE *destination,
	int dest_channels, int dest_samples,
	
	int16_t *pcm_data,
	int pcm_channels, int pcm_samples);

void convert_pcm_to_static_signal(int16_t *pcm_d, int p_ch, int p_sm);

void make_a_mockery(struct mockery *m, int steps);

#endif
