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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TIMER_TESTBENCH_DECLARED

typedef struct TimeMeasureStruct {	
	struct timespec before;
	struct timespec after;

	long offending_treshold;
	long offending_frames;
	long total_frames;
	long worst_time;
	long average_time;
	long best_time;
	const char *worst_cause;
	const char *name;
} TimeMeasure_t;


TimeMeasure_t **all_stats = NULL;

void clear_stats(TimeMeasure_t *tmt) {
	tmt->best_time = 100000000;
	tmt->average_time = 0;
	tmt->worst_time = 0;
	tmt->offending_frames = tmt->total_frames = 0;
	tmt->worst_cause = "not set";
}

TimeMeasure_t *create_TimeMeasure(const char *_name, long _offending_treshold) {
	TimeMeasure_t *tmt = (TimeMeasure_t *)calloc(1, sizeof(TimeMeasure_t));
	
	clear_stats(tmt);

	tmt->offending_treshold = _offending_treshold;
	tmt->name = _name;

	if(all_stats == NULL) {
		all_stats = (TimeMeasure_t **)calloc(128, sizeof(TimeMeasure_t *));
	}
	int k;
	for(k = 0; k < 128; k++) {
		if(all_stats[k] == NULL) {
			all_stats[k] = tmt;
			return;
		}
	}
	return tmt;
}

void start_measure(TimeMeasure_t *tmt) {
	clock_gettime(CLOCK_MONOTONIC_RAW, &(tmt->before));
}

void stop_measure(TimeMeasure_t *tmt, const char *cause) {
	clock_gettime(CLOCK_MONOTONIC_RAW, &(tmt->after));
	tmt->after.tv_sec -= tmt->before.tv_sec;
	tmt->after.tv_nsec -= tmt->before.tv_nsec;
	if(tmt->after.tv_nsec < 0) {
		tmt->after.tv_sec--;
		tmt->after.tv_nsec += 1000000000;
	}
	long micro_seconds = tmt->after.tv_sec * 1000000 + tmt->after.tv_nsec / 1000;
	tmt->average_time = (tmt->average_time + micro_seconds) / 2;
	if(tmt->worst_time < micro_seconds) {
		tmt->worst_time = micro_seconds;
		tmt->worst_cause = cause;
	}
	if(tmt->best_time > micro_seconds) tmt->best_time = micro_seconds;

	if(micro_seconds > tmt->offending_treshold)
		tmt->offending_frames++;
	tmt->total_frames++;
}

long print_stats(TimeMeasure_t *tmt) {
	long off = 0;
	if(tmt->total_frames != 0)
		off = (100 * tmt->offending_frames) / tmt->total_frames;
	printf(" ------------------------\n");
	printf(" ------- Stats for: %s\n", tmt->name);
	printf(" ------- offender: %s\n", tmt->worst_cause);
	printf(" ------- worst: %ld us\n", tmt->worst_time);
	printf(" ------- averg: %ld us\n", tmt->average_time);
	printf(" -------  best: %ld us\n", tmt->best_time);
	printf("        offend: %ld/100\n", off);
	return tmt->worst_time;
}

void print_all_stats() {
	if(all_stats == NULL) return;
	long worst_accumulated = 0;
	int k;
	for(k = 0; k < 128; k++) {
		TimeMeasure_t *p = all_stats[k];
		if(p != NULL) {
			worst_accumulated += print_stats(p);
		}
	}
	printf("    --------- ACCUMULATED: %ld us\n", worst_accumulated);
}

void clear_all_stats() {
	int k;
	for(k = 0; k < 128; k++) {
		TimeMeasure_t *p = all_stats[k];
		if(p != NULL) {
			clear_stats(p);
		}
	}	
}

