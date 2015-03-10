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

#include "dynlib.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//#define __DO_DYNLIB_DEBUG
#include "dynlib_debug.h"

#include "riff_wave_output.h"

/******************
 *
 * AsyncWriter
 *
 ******************/

void async_riff_writer_callback(AsyncOp *a_op) {
	void *d = a_op->data;
	AsyncRiffWriter *arw = (AsyncRiffWriter *)d;

	write(arw->fd, arw->data, arw->length);
}

AsyncRiffWriter *allocate_async_riff_writers(AsyncRiffWriterContainer *arwc, size_t nr) {
	AsyncRiffWriter *prev = NULL;
	AsyncRiffWriter *arw = (AsyncRiffWriter *)calloc(nr, sizeof(AsyncRiffWriter) + arwc->default_length);

	if(!(arw)) return NULL;
	
	uint8_t *data_pointer = (uint8_t *)(&arw[nr]);

	int k;
	for(k = 0; k < nr; k++) {
		arw[k].operation.func = async_riff_writer_callback;
		arw[k].operation.data = &arw[k];
		arw[k].prev = prev;
		arw[k].next = &arw[k+1];
		arw[k].data = data_pointer;
		data_pointer = &data_pointer[arwc->default_length];
		prev = &arw[k];
	}
	arw[k - 1].next = NULL; // last must point to NULL

	return arw;
}

void prepare_async_riff_writer_container(AsyncRiffWriterContainer *arwc, MachineTable *mt, void *machine_instance, size_t def_length) {
	arwc->mt = mt;
	arwc->machine_instance = machine_instance;
	arwc->default_length = def_length;

	arwc->unused = allocate_async_riff_writers(arwc, 200);
}

void poll_pending_async_riff_writers(AsyncRiffWriterContainer *arwc) {
	AsyncRiffWriter *arw = arwc->pending;
	while(arw) {
		AsyncRiffWriter *next = arw->next;

		if(arw->operation.finished) {
			// operation done - remove from pending queue
			if(arw->prev) // if not first pending
				arw->prev->next = arw->next;
			else
				arwc->pending = arw->next;
			
			if(arw->next)
				arw->next->prev = arw->prev;
			
			// insert into unused queue
			arw->prev = NULL;
			arw->next = arwc->unused;
			if(arwc->unused) {
				arwc->unused->prev = arw;
			}
			arwc->unused = arw;

		}
		
		arw = next;
	}
}

void queue_async_writer(AsyncRiffWriterContainer *arwc, AsyncRiffWriter *rv) {
	rv->operation.finished = 0;

	rv->prev = NULL;
	rv->next = arwc->pending;
	if(arwc->pending)
		arwc->pending->prev = rv;
	arwc->pending = rv;

	arwc->mt->run_async_operation(&(rv->operation));
}

AsyncRiffWriter *get_async_riff_writer(AsyncRiffWriterContainer *arwc) {
	AsyncRiffWriter *rv = NULL;

	poll_pending_async_riff_writers(arwc);
	
	if(!(arwc->unused)) arwc->unused = allocate_async_riff_writers(arwc, 32);
	
	if(arwc->unused) {
		rv = arwc->unused;
		arwc->unused = arwc->unused->next;
		arwc->unused->prev = NULL;
	} else {
		arwc->mt->register_failure(arwc->machine_instance, "Out of memory in AsyncRiffWriter.\n");
	}

	return rv;
}

int async_write_to_riff(AsyncRiffWriterContainer *arwc, int fd, void *data, size_t len) {
	AsyncRiffWriter *arw = get_async_riff_writer(arwc);

	if(arw) {
		arw->fd = fd;
		arw->length = len;
		memcpy(arw->data, data, len);
		
		queue_async_writer(arwc, arw);

		return len;
	}

	return 0;
}

void wait_for_pending_async_riff_writers(AsyncRiffWriterContainer *arwc) {
	while(arwc->pending) {
		usleep(1000);
		poll_pending_async_riff_writers(arwc);
	}
}

/******************
 *
 * RIFF / WAVE output support
 *
 ******************/

void RIFF_prepare(MachineTable *mt, void *machine_instance, size_t buffer_size, RIFF_WAVE_FILE_t *rwf, uint32_t rate) {
	prepare_async_riff_writer_container(&(rwf->arwc), mt, machine_instance, buffer_size * sizeof(int16_t));
	
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
	rwf->riff_header.fmt_srate[0] = (rate & 0x000000ff) >>  0; // 0x44;
	rwf->riff_header.fmt_srate[1] = (rate & 0x0000ff00) >>  8; // 0xac;
	rwf->riff_header.fmt_srate[2] = (rate & 0x00ff0000) >> 16; // 0x00;
	rwf->riff_header.fmt_srate[3] = (rate & 0xff000000) >> 24; // 0x00;

	// encode byte rate
	// (44100 * 2 channels * 16 bits / 8 bits per byte = 176400 = 0x0002b110)
	// in little endian
	uint32_t byte_rate = rate * 2 * 2; // rate * channels * bits
	rwf->riff_header.fmt_brate[0] = (byte_rate & 0x000000ff) >>  0; // 0x10;
	rwf->riff_header.fmt_brate[1] = (byte_rate & 0x0000ff00) >>  8; // 0xb1;
	rwf->riff_header.fmt_brate[2] = (byte_rate & 0x00ff0000) >> 16; // 0x02;
	rwf->riff_header.fmt_brate[3] = (byte_rate & 0xff000000) >> 24; // 0x00;

	// encode align = 0x0004 in little endian (num channels * num bits per sample / 8)
	rwf->riff_header.fmt_align[0] = 0x04;
	rwf->riff_header.fmt_align[1] = 0x00;

	// encode bits per sample = 0x0010 in little endian
	rwf->riff_header.fmt_bps[0] = 0x10;
	rwf->riff_header.fmt_bps[1] = 0x00;

	rwf->riff_header.data[0] = 'd';
	rwf->riff_header.data[1] = 'a';
	rwf->riff_header.data[2] = 't';
	rwf->riff_header.data[3] = 'a';
}

void RIFF_create_file(RIFF_WAVE_FILE_t *rwf, char *__file_name) {
	// append .wav to filename....
	char bfr[1024];

	snprintf(bfr, 1024, "%s.wav", __file_name);

	DYNLIB_DEBUG(" RIFF_create_file: ]%s[.\n", bfr); fflush(0);
	rwf->fd =
		open(bfr,
		     O_CREAT | O_TRUNC | O_RDWR,
		     S_IRUSR | S_IWUSR);

	DYNLIB_DEBUG(" sinkRecord FILE: %d.\n", rwf->fd); fflush(0);
	if(rwf->fd != -1) {

		// cast to void - ignore status
		(void)write(rwf->fd,
		      &(rwf->riff_header),
		      sizeof(struct RIFF_WAVE_header));
		rwf->written_to_riff = 0;
	}
}

void RIFF_close_file(RIFF_WAVE_FILE_t *rwf) {
	wait_for_pending_async_riff_writers(&(rwf->arwc));
	
	if(rwf->fd != -1) {		
		rwf->riff_header.data_size[0] = (rwf->written_to_riff & 0x000000ff);
		rwf->riff_header.data_size[1] = (rwf->written_to_riff & 0x0000ff00) >> 8;
		rwf->riff_header.data_size[2] = (rwf->written_to_riff & 0x00ff0000) >> 16;
		rwf->riff_header.data_size[3] = (rwf->written_to_riff & 0xff000000) >> 24;
		
		rwf->written_to_riff += 36; // size of header...
		
		rwf->riff_header.length[0] = (rwf->written_to_riff & 0x000000ff);
		rwf->riff_header.length[1] = (rwf->written_to_riff & 0x0000ff00) >> 8;
		rwf->riff_header.length[2] = (rwf->written_to_riff & 0x00ff0000) >> 16;
		rwf->riff_header.length[3] = (rwf->written_to_riff & 0xff000000) >> 24;
		
		// rewrite header with correct size data
		lseek(rwf->fd, 0, SEEK_SET);
		// cast to void - ignore status
		(void) write(rwf->fd,
		      &(rwf->riff_header),
		      sizeof(struct RIFF_WAVE_header));
		
		DYNLIB_DEBUG(" sinkRecord CLOSE file.\n"); fflush(0);
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
		
		k = async_write_to_riff(&(rwf->arwc), rwf->fd, data, size);
		
		rwf->written_to_riff += k;
	}
}
