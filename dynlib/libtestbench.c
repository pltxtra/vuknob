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

#include "libtestbench.h"

#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../fixedpointmathlib.cc"

#include "../kiss_fftr.h"

#ifndef TIMER_TESTBENCH_DECLARED
#error "You must #include libtestbench_timer.c ahead of the file you want to bench."
#endif

struct mockery *mockery_table[] = {
	NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL
};

struct mockery *find_mockery(MachineTable *mt) {
	int k;
	for(k = 0; k < 10; k++) {
		if(&(mockery_table[k]->mt) == mt) {
			return mockery_table[k];
		}
	}

	return NULL;
}

struct mockery *register_mockery(struct mockery *m) {
	int k;

	for(k = 0; k < 10; k++) {
		if(mockery_table[k] == NULL) {
			return mockery_table[k] = m; // return the pointer we got as input...
		}
	}

	return NULL;
}

int fill_sink(struct _MachineTable *mt,
	      int (*fill_sink_callback)(int status, void *cbd),
	      void *callback_data) {
}

void set_signal_defaults(struct _MachineTable *mt, 
			 int dim,
			 int len, int res,
			 int frequency) {
}

SignalPointer *get_input_signal(struct _MachineTable *mt, const char *name) {
	struct mockery *m = find_mockery(mt);
	if(m == NULL) return NULL;

	struct signus *s = m->inputs;

	while(s != NULL) {
		if(strcmp(s->name, name) == 0) {
			return s;
		}
		
		s = s->next;
	}

	return NULL;
}

SignalPointer *get_output_signal(struct _MachineTable *mt, const char *name) {
	struct mockery *m = find_mockery(mt);
	if(m == NULL) return NULL;

	struct signus *s = m->outputs;

	while(s != NULL) {
		if(strcmp(s->name, name) == 0) {
			return s;
		}
		
		s = s->next;
	}

	return NULL;
}

SignalPointer *get_mocking_signal(struct _MachineTable *mt, const char *name) {
	struct mockery *m = find_mockery(mt);
	if(m == NULL) return NULL;

	struct signus *s = m->intermediates;

	while(s != NULL) {
		if(strcmp(s->name, name) == 0) {
			return s;
		}
		
		s = s->next;
	}

	return NULL;
}

SignalPointer *get_next_signal(struct _MachineTable *mt, SignalPointer *sp) {
	return NULL;
}

#define __TESTBENCH_STATIC_SIGNAL_LENGTH (44100 / 2)
static int16_t static_signal_data[__TESTBENCH_STATIC_SIGNAL_LENGTH];
struct signus static_signal = {
	.name = "staticus",
	.is_static = -1, // yes, it's true, it is_static
	.dim = _0D,
	.chan = 1,
	.res = _16bit,
	.freq = 44100,
	.sam = __TESTBENCH_STATIC_SIGNAL_LENGTH,
	.data = (void *)static_signal_data,
	.next = NULL
};
	
SignalPointer *get_static_signal(int index) {
	int k;
	float value, t;
	for(k = 0; k < __TESTBENCH_STATIC_SIGNAL_LENGTH; k++) {

		t = k;
		t = t / ((float) __TESTBENCH_STATIC_SIGNAL_LENGTH);
		t *= 10.0f;
		t *= 2.0f * M_PI;
		value = 32767.0f * sinf(t);
		
		static_signal_data[k] = value;
	}
	
	return &static_signal;
}

int get_signal_dimension(SignalPointer *sp) {
	struct signus *s = (struct signus *)sp;

	return s->dim;
}

int get_signal_channels(SignalPointer *sp) {
	struct signus *s = (struct signus *)sp;

	return s->chan;
}

int get_signal_resolution(SignalPointer *sp) {
	struct signus *s = (struct signus *)sp;

	return s->res;
}

int get_signal_frequency(SignalPointer *sp) {
	struct signus *s = (struct signus *)sp;

	return s->freq;
}

int get_signal_samples(SignalPointer *sp) {
	struct signus *s = (struct signus *)sp;

	if(s->is_static) {
		return s->sam;
	}
	struct mockery *m = (struct mockery *)(s->mockery);
	return m->test_step_length;
}

void copy_output_buffer(struct signus *s) {
	if(s->is_static) {
		return;
	}

	struct mockery *m = (struct mockery *)s->mockery;
	int offset = (m->current_test_step) * (m->test_step_length);

	if(s->dim == 0 || s->dim == _MIDI) {
		switch(s->res) {
		case _8bit:
		{
			int8_t *p = (int8_t *)(s->u_data);
			p = &(p[offset]);
			memcpy(p, s->data, sizeof(int8_t) * m->test_step_length);
		}
			break;
		case _16bit:
		{
			int16_t *p = (int16_t *)(s->u_data);
			p = &(p[offset]);
			memcpy(p, s->data, sizeof(int16_t) * m->test_step_length);
		}
			break;
		case _32bit:
		{
			int32_t *p = (int32_t *)(s->u_data);
			p = &(p[offset]);
			memcpy(p, s->data, sizeof(int32_t) * m->test_step_length);
		}
			break;
		case _fl32bit:
		{
			float *p = (float *)(s->u_data);
			p = &(p[offset]);
			memcpy(p, s->data, sizeof(float) * m->test_step_length);
		}
			break;
		case _fx8p24bit:
		{
			fp8p24_t *p = (fp8p24_t *)(s->u_data);
			p = &(p[offset]);
			memcpy(p, s->data, sizeof(int32_t) * m->test_step_length);
		}
			break;
		case _PTR:
		{
			void **p = (void **)(s->u_data);
			p = &(p[offset]);
			memcpy(p, s->data, sizeof(void *) * m->test_step_length);
		}
			break;
		}
	}

}

void *get_signal_buffer(SignalPointer *sp) {
	struct signus *s = (struct signus *)sp;

	if(s->is_static) {
		return s->data;
	}

	struct mockery *m = (struct mockery *)s->mockery;
	int offset = (m->current_test_step) * (m->test_step_length);

	if(s->dim == 0 || s->dim == _MIDI) {
		switch(s->res) {
		case _8bit:
		{
			int8_t *p = (int8_t *)(s->u_data);
			p = &(p[offset]);
			memcpy(s->data, p, sizeof(int8_t) * m->test_step_length);
		}
			break;
		case _16bit:
		{
			int16_t *p = (int16_t *)(s->u_data);
			p = &(p[offset]);
			memcpy(s->data, p, sizeof(int16_t) * m->test_step_length);
		}
			break;
		case _32bit:
		{
			int32_t *p = (int32_t *)(s->u_data);
			p = &(p[offset]);
			memcpy(s->data, p, sizeof(int32_t) * m->test_step_length);
		}
			break;
		case _fl32bit:
		{
			float *p = (float *)(s->u_data);
			p = &(p[offset]);
			memcpy(s->data, p, sizeof(float) * m->test_step_length);
		}
			break;
		case _fx8p24bit:
		{
			fp8p24_t *p = (fp8p24_t *)(s->u_data);
			p = &(p[offset]);
			memcpy(s->data, p, sizeof(fp8p24_t) * m->test_step_length);
		}
			break;
		case _PTR:
		{
			void **p = (void **)(s->u_data);
			p = &(p[offset]);
			memcpy(s->data, p, sizeof(void *) * m->test_step_length);
			printf("midi_in at %p\n", p);
	
		}
			break;
		}
	}

	return s->data;
}

int get_recording_filename(struct _MachineTable *mt, char *dst, unsigned int len) {
	dst[0] = '\0';
	return 0;
}

void register_failure(void *machine_instance, const char *msg) {
	printf("error: %s\n", msg);
}

// Satan's "portable" math library
#ifdef __SATAN_USES_FXP
#include <math.h>

#include "fixedpointmath.h"
fp8p24_t satan_powfp8p24(fp8p24_t x, fp8p24_t y);

typedef enum {
	true = -1,
	false = 0
} bool;

bool __internal_satan_sine_table_initiated = false;
float __internal_satan_sine_table[SAT_SIN_TABLE_LEN];
FTYPE __internal_satan_sine_table_FTYPE[SAT_SIN_FTYPE_TABLE_LEN << 1]; 
#endif

void get_math_tables(
	float **sine_table,
	FTYPE (**__pow_fun)(FTYPE x, FTYPE y),
	FTYPE **sine_table_FTYPE
	) {
#ifdef __SATAN_USES_FXP
	if(!__internal_satan_sine_table_initiated) {
		int x;
		float x_f;
		for(x = 0; x < SAT_SIN_TABLE_LEN; x++) {
			x_f = x;
			x_f /= (float)SAT_SIN_TABLE_LEN;
			__internal_satan_sine_table[x] =
				sin(x_f * 2 * M_PI);
		}
		for(x = 0; x < SAT_SIN_FTYPE_TABLE_LEN; x++) {
			FTYPE v = x;
			v = v << 12;
			float t = fp8p24tof(v);
			float y = sinf(t * 2.0f * M_PI);
			__internal_satan_sine_table_FTYPE[SAT_SIN_FTYPE_TABLE_LEN + x] =
				__internal_satan_sine_table_FTYPE[x] = ftoFTYPE(y);
		}
		__internal_satan_sine_table_initiated = true;
	}
	
	 *sine_table = __internal_satan_sine_table;
	 *__pow_fun = satan_powfp8p24;
	 *sine_table_FTYPE = __internal_satan_sine_table_FTYPE;
#else
	 *sine_table = NULL;
	 *__pow_fun = NULL;
	 *sine_table_FTYPE = NULL;
#endif
}

kiss_fftr_cfg prepare_fft(int samples, int do_inverse) {
	return kiss_fftr_alloc(samples, do_inverse, NULL, NULL);
}

void do_fft(kiss_fftr_cfg cfg, FTYPE *timedata, kiss_fft_cpx *freqdata) {
#ifndef __SATAN_USES_FXP
	kiss_fftr(cfg, timedata, freqdata);
#else
	printf(" Warning, do_fft not implemented in fixed point mode\n");
#endif
}

void inverse_fft(kiss_fftr_cfg cfg, kiss_fft_cpx *freqdata, FTYPE *timedata) {
#ifndef __SATAN_USES_FXP
	kiss_fftri(cfg, freqdata, timedata);
#else
	printf(" Warning, do_fft not implemented in fixed point mode\n");
#endif
}

void init_mock_machine_table(MachineTable *mt) {
	mt->fill_sink = fill_sink;

	mt->set_signal_defaults = set_signal_defaults;

	mt->get_input_signal = get_input_signal;
	mt->get_mocking_signal = get_mocking_signal;
	mt->get_output_signal = get_output_signal;
	mt->get_next_signal = get_next_signal;
	mt->get_static_signal = get_static_signal;

	mt->get_signal_dimension = get_signal_dimension;
	mt->get_signal_channels = get_signal_channels;
	mt->get_signal_resolution = get_signal_resolution;
	mt->get_signal_frequency = get_signal_frequency;
	mt->get_signal_samples = get_signal_samples;
	mt->get_signal_buffer = get_signal_buffer;

	mt->get_recording_filename = get_recording_filename;
	mt->register_failure = register_failure;

	mt->get_math_tables = get_math_tables;

	mt->prepare_fft = prepare_fft;
	mt->do_fft = do_fft;
	mt->inverse_fft = inverse_fft;

}

void validate_mock_signal(struct signus *s) {
	int k;

	for(k = 0; k < 16; k++) {
		if((s->pad_a[k] != 0) || (s->pad_b[k] != 0)) {
			printf("validation failed.\n");
			if(s == NULL) {
				printf("     s pointer is NULL.\n");
			} else {
				if(s->name == NULL) {
					printf("   name pointer is NULL\n");
				} else {
					printf("   name: %s\n", s->name);
				}
			}
			exit(0);
		}
	}
}

struct signus *create_mock_signal(
	const char *name,
	int dimension,
	int channels,
	int resolution,
	int frequency,
	int samples,
	void *data_pointer,
	struct mockery *m) {
	struct signus *s = (struct signus *)malloc(sizeof(struct signus));

	if(s == NULL) return NULL;

	s->name = strdup(name);
	s->is_static = 0; // set to "false"
	s->dim = dimension;
	s->chan = channels;
	s->res = resolution;
	s->freq = frequency;
	s->sam = samples;

	s->u_data = data_pointer;

	if(s->dim == _0D || s->dim == _MIDI) {
		size_t len = 0;
		switch(s->res) {
		case _8bit:
			len = sizeof(int8_t);
			break;
		case _16bit:
			len = sizeof(int16_t);
			break;
		case _32bit:
			len = sizeof(int32_t);
			break;
		case _fl32bit:
			len = sizeof(float);
			break;
		case _fx8p24bit:
			len = sizeof(int32_t);
 			break;
		case _PTR:
			len = sizeof(void *);
			break;
		}
		
		s->pad_data = (uint8_t *)calloc(len * (m->test_step_length) + 32, sizeof(uint8_t));
		uint8_t *t = &(s->pad_data[16]);
		s->pad_a = s->pad_data;
		s->pad_b = &(s->pad_data[16 + len * (m->test_step_length)]);
		s->data = (void *)t;
	}

	s->mockery = m;

	return s;
}

/*********************************************
 *
 *     PUBLIC FUNCTIONS
 *
 *********************************************/

int prepare_mockery(struct mockery *m, int test_step_length, const char *machine_name) {
	memset(m, 0, sizeof(struct mockery));
	init_mock_machine_table(&(m->mt));
	m->inputs = NULL;
	m->outputs = NULL;

	if(register_mockery(m) != m) return -1;

	m->machine_data = init(&(m->mt), machine_name);
	if(m->machine_data == NULL) {
		printf("Mockery failed.\n");
		return -1;
	}

	m->current_test_step;
	m->test_step_length = test_step_length;
	
	return 0;
}

void *get_controller(
	struct mockery *m, const char *name, const char *group
	) {
	return get_controller_ptr(&(m->mt), m->machine_data, name, group);
}

int create_mock_input_signal(
	struct mockery *m, const char *name,
	int dimension,	
	int channels,
	int resolution,
	int frequency,
	int samples,
	void *data_pointer) {

	if(m == NULL) return -1;
	
	struct signus *s = create_mock_signal(name, dimension, channels, FTYPE_RESOLUTION, frequency, samples, data_pointer, m);
	if(s == NULL)
		return -1;

	s->next = m->inputs;
	m->inputs = s;
	
	printf("Set first input to %p (%s, %d, %d, %d, %d, %d, %p)\n", s,
	       name, dimension, channels, FTYPE_RESOLUTION, frequency, samples, data_pointer);
	
	return 0;
}

int create_mock_output_signal(
	struct mockery *m, const char *name,
	int dimension,
	int channels,
	int resolution,
	int frequency,
	int samples,
	void *data_pointer) {

	if(m == NULL) return -1;
	
	struct signus *s = create_mock_signal(name, dimension, channels, FTYPE_RESOLUTION, frequency, samples, data_pointer, m);
	if(s == NULL)
		return -1;

	s->next = m->outputs;
	m->outputs = s;

	printf("Set first output to %p (%s, %d, %d, %d, %d, %d, %p)\n", s,
	       name, dimension, channels, FTYPE_RESOLUTION, frequency, samples, data_pointer);
	
	return 0;
}

int create_mock_intermediate_signal(
	struct mockery *m, const char *name,
	int dimension,
	int channels,
	int resolution,
	int frequency,
	int samples,
	void *data_pointer) {

	if(m == NULL) return -1;
	
	struct signus *s = create_mock_signal(name, dimension, channels, resolution, frequency, samples, data_pointer, m);
	if(s == NULL)
		return -1;

	s->next = m->intermediates;
	m->intermediates = s;

	printf("Set first intermediate to %p\n", s);
	
	return 0;
}

void make_a_mockery(struct mockery *m, int steps) {

	{
		int check_length = steps * m->test_step_length;
		if((m->outputs != NULL && (check_length > m->outputs->sam)) ||
		   (m->inputs != NULL && (check_length > m->inputs->sam))) {
			printf("Error: step length times steps is longer than the input or output signal lengths. This will cause a crash.\n");
			exit(0);
		}
	}
	
	int k;	
	
	for(k = 0; k < steps; k++) {
		m->current_test_step = k;
		execute(&(m->mt), m->machine_data);

		{
			struct signus *out = m->outputs;
			while(out) {
				validate_mock_signal(out);
				out = out->next;
			}
		}
		{
			struct signus *in = m->outputs;
			while(in) {
				validate_mock_signal(in);
				in = in->next;
			}
		}
		{
			struct signus *inte = m->intermediates;
			while(inte) {
				validate_mock_signal(inte);
				inte = inte->next;
			}
		}
		{
			struct signus *out = m->outputs;
			while(out) {
				copy_output_buffer(out);
				out = out->next;
			}
		}
	}

	print_all_stats();
	
	delete(m->machine_data);

	printf("We made a mocking bird!\n");
}

/******************
 *
 * RIFF / WAVE output support
 *
 ******************/

void RIFF_prepare(RIFF_WAVE_FILE_t *rwf) {
	rwf->read_only = 0;
	rwf->fd = -1;
	rwf->written_to_riff = 0;
	
	rwf->riff_header.RIFF[0] = 'R';
	rwf->riff_header.RIFF[1] = 'I';
	rwf->riff_header.RIFF[2] = 'F';
	rwf->riff_header.RIFF[3] = 'F';
	rwf->riff_header.WAVE[0] = 'W';
	rwf->riff_header.WAVE[1] = 'A';
	rwf->riff_header.WAVE[2] = 'V';
	rwf->riff_header.WAVE[3] = 'E';

	rwf->riff_header.fmt_[0] = 'f';
	rwf->riff_header.fmt_[1] = 'm';
	rwf->riff_header.fmt_[2] = 't';
	rwf->riff_header.fmt_[3] = ' ';

	// the "fmt " chunk size is always 16, 0x00000010, for a standard file
	rwf->riff_header.fmt_size[0] = 0x10;
	rwf->riff_header.fmt_size[1] = 0x00;
	rwf->riff_header.fmt_size[2] = 0x00;
	rwf->riff_header.fmt_size[3] = 0x00;
		
	// encode PCM = 0x0001 in little endian
	rwf->riff_header.fmt_format[0] = 0x01;
	rwf->riff_header.fmt_format[1] = 0x00;

	// encode channels = 0x0002 in little endian
	rwf->riff_header.fmt_channels[0] = 0x02;
	rwf->riff_header.fmt_channels[1] = 0x00;

	// encode sample rate (44100 = 0x0000ac44) in little endian
	rwf->riff_header.fmt_srate[0] = 0x44;
	rwf->riff_header.fmt_srate[1] = 0xac;
	rwf->riff_header.fmt_srate[2] = 0x00;
	rwf->riff_header.fmt_srate[3] = 0x00;

	// encode byte rate
	// (44100 * 2 channels * 16 bits / 8 bits per byte = 176400 = 0x0002b110)
	// in little endian
	rwf->riff_header.fmt_brate[0] = 0x10;
	rwf->riff_header.fmt_brate[1] = 0xb1;
	rwf->riff_header.fmt_brate[2] = 0x02;
	rwf->riff_header.fmt_brate[3] = 0x00;

	// encode align = 0x0004 in little endian (num channels * num bits per sample / 8)
	rwf->riff_header.fmt_align[0] = 0x04;
	rwf->riff_header.fmt_align[1] = 0x00;

	// encode bits per sample = 0x0010 in little endian
	rwf->riff_header.fmt_bps[0] = 0x10;
	rwf->riff_header.fmt_bps[1] = 0x00;

	rwf->data_chunk.iden[0] = 'd';
	rwf->data_chunk.iden[1] = 'a';
	rwf->data_chunk.iden[2] = 't';
	rwf->data_chunk.iden[3] = 'a';
}

void RIFF_create_file(RIFF_WAVE_FILE_t *rwf, const char *bfr) {
	DYNLIB_DEBUG(" RIFF_create_file: ]%s[.\n", bfr); fflush(0);
	rwf->fd =
		open(bfr,
		     O_CREAT | O_TRUNC | O_RDWR,
		     S_IRUSR | S_IWUSR);

	printf(" sinkRecord FILE: %d.\n", rwf->fd); fflush(0);
	if(rwf->fd != -1) {
		int ignore_status = write(rwf->fd,
		      &(rwf->riff_header),
		      sizeof(struct RIFF_WAVE_header));
		ignore_status = write(rwf->fd,
		      &(rwf->data_chunk),
		      sizeof(struct RIFF_WAVE_chunk));
		rwf->written_to_riff = 0;
	}
}

int RIFF_open_file(RIFF_WAVE_FILE_t *rwf, const char *name) {
	rwf->fd =
		open(name,
		     O_RDONLY);

	if(rwf->fd == -1) {
		printf("Failed to open file...\n");
		return -1;
	}
	
	int status = read(rwf->fd,
			  &(rwf->riff_header),
			  sizeof(struct RIFF_WAVE_header));
	if(status != sizeof(struct RIFF_WAVE_header)) {
		printf("   Failed to read RIFF/WAV header...\n");
		close(rwf->fd);
		return -2;
	}

	{
		int r;
		struct RIFF_WAVE_chunk *chunk = &(rwf->data_chunk);
		memset(chunk, 0, sizeof(struct RIFF_WAVE_chunk));
		
		r = read(rwf->fd, chunk, sizeof(struct RIFF_WAVE_chunk));
		if(r != sizeof(struct RIFF_WAVE_chunk))
			return 0;
		while(!(rwf->data_chunk.iden[0] == 'd' &&
			rwf->data_chunk.iden[1] == 'a' &&
			rwf->data_chunk.iden[2] == 't' &&
			rwf->data_chunk.iden[3] == 'a')) {
			int skiplen =
				((rwf->data_chunk.leng[3] << 24) & 0xff000000) |
				((rwf->data_chunk.leng[2] << 16) & 0x00ff0000) |
				((rwf->data_chunk.leng[1] <<  8) & 0x0000ff00) |
				((rwf->data_chunk.leng[0] <<  0) & 0x000000ff);
			r = lseek(rwf->fd, skiplen, SEEK_CUR);
			if(r == -1) return 0;
			
			r = read(rwf->fd, chunk, sizeof(struct RIFF_WAVE_chunk));
			if(r != sizeof(struct RIFF_WAVE_chunk))
				return 0;
		}
	}	

	rwf->channels =
		((rwf->riff_header.fmt_channels[1]) << 8) |
		((rwf->riff_header.fmt_channels[0]));

	rwf->samples =
		((rwf->data_chunk.leng[3]) << 24) |
		((rwf->data_chunk.leng[2]) << 16) |
		((rwf->data_chunk.leng[1]) << 8)  |
		((rwf->data_chunk.leng[0]));

	printf("WAV Channels: %d\n", rwf->channels);
	printf("WAV Samples: %d\n", rwf->samples);
	
	return 0;
}

int RIFF_read_data(RIFF_WAVE_FILE_t *rwf, int16_t *data, int samples) {
	return read(rwf->fd,
		    data,
		    samples * rwf->riff_header.fmt_channels[0]);
}

void RIFF_close_file(RIFF_WAVE_FILE_t *rwf) {
	if(rwf->fd != -1) {
		if(!rwf->read_only) {
			rwf->data_chunk.leng[0] = (rwf->written_to_riff & 0x000000ff);
			rwf->data_chunk.leng[1] = (rwf->written_to_riff & 0x0000ff00) >> 8;
			rwf->data_chunk.leng[2] = (rwf->written_to_riff & 0x00ff0000) >> 16;
			rwf->data_chunk.leng[3] = (rwf->written_to_riff & 0xff000000) >> 24;
			
			rwf->written_to_riff += sizeof(struct RIFF_WAVE_header) + sizeof(struct RIFF_WAVE_chunk); // size of headers...
			
			rwf->riff_header.length[0] = (rwf->written_to_riff & 0x000000ff);
			rwf->riff_header.length[1] = (rwf->written_to_riff & 0x0000ff00) >> 8;
			rwf->riff_header.length[2] = (rwf->written_to_riff & 0x00ff0000) >> 16;
			rwf->riff_header.length[3] = (rwf->written_to_riff & 0xff000000) >> 24;
			
			// rewrite header with correct size data
			lseek(rwf->fd, 0, SEEK_SET);
			int ignore_status = write(rwf->fd,
						  &(rwf->riff_header),
						  sizeof(struct RIFF_WAVE_header));
			ignore_status = write(rwf->fd,
					      &(rwf->data_chunk),
					      sizeof(struct RIFF_WAVE_chunk));
			
			printf(" sinkRecord CLOSE file.\n"); fflush(0);
		}
		
		close(rwf->fd);
		rwf->fd = -1;
	}
}

void RIFF_write_data(
	RIFF_WAVE_FILE_t *rwf,
	void *data,
	int size) {

	if(rwf->fd != -1) {
		int k;
		
		k = write(rwf->fd, data, size);
		
		rwf->written_to_riff += k;
	}
}

void interleave(int16_t *destination, int offset, const FTYPE *in, int samples) {
	int i;
	for(i = 0; i < samples; i++) {		
		FTYPE tmp = in[i];
		int32_t out;
		
		if(tmp < itoFTYPE(-1)) tmp = itoFTYPE(-1);
		if(tmp > itoFTYPE(1)) tmp = itoFTYPE(1);

#ifdef __SATAN_USES_FXP
		out = (tmp << 7);
		out = (out & 0xffff0000) >> 16;
#else
		tmp *= 32767.0f;
		out = tmp;
#endif
		
		destination[i * 2 + offset] = (int16_t)out;
	}
}

void write_wav(const FTYPE *left, const FTYPE *right, int samples, const char *fname) {
//	int16_t mixed_signal[samples * 2];
	int16_t *mixed_signal = (int16_t *)malloc(sizeof(int16_t) * samples * 2);

	// prepare, create and open the RIFF/WAV file
	RIFF_WAVE_FILE_t rwf;
	RIFF_prepare(&rwf);
	RIFF_create_file(&rwf, fname);

	// interleave the two signals and convert to int16_t
	interleave(mixed_signal, 0, left, samples);
	interleave(mixed_signal, 1, right, samples);

	// Write to file
	RIFF_write_data(&rwf, mixed_signal, samples * 2);
	
	// Close file
	RIFF_close_file(&rwf);
}

void convert_pcm_to_mock_signal(
	FTYPE *destination,
	int d_ch, int d_sm,

	int16_t *pcm_d,
	int p_ch, int p_sm) {

	// get smallest
	int ms = d_sm < p_sm ? d_sm : p_sm;

	int k;

	if(d_ch == 1) {
		for(k = 0; k < ms; k++) {
			float x;

			x = pcm_d[k * p_ch];

			x /= 32768.0f;

			destination[k] = ftoFTYPE(x);
		}
	} else {
		printf("MULTICHANNEL NOT SUPPORTED YET.\n");
		exit(0);
	}
}

void convert_pcm_to_static_signal(int16_t *pcm_d, int p_ch, int p_sm) {
	static_signal.data = pcm_d;
	static_signal.chan = p_ch;
	static_signal.sam = p_sm;	
}

int plot_wave(const char *fname, int first_index, int length) {
	int bfr_length = strlen(fname) + 100;
	char bfr[bfr_length];
	if(bfr_length == snprintf(bfr, strlen(fname) + 100, "./wave_plotter.m %s %d %d", fname, first_index, length)) {
		return -1;
	}
	return system(bfr);
}
