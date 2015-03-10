/*
 * VuKNOB
 * Copyright (C) 2014 by Anton Persson
 *
 * VuKNOB is based on SATAN:
 *
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

#include "time_measure.hh"

#ifdef __DO_TIME_MEASURE

#include <jngldrum/jthread.hh>

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

jThread::Monitor mtr;

TimeMeasure **all_stats = NULL;

TimeMeasure::TimeMeasure(const std::string _name, int _offending_treshold) : name_str(_name), offending_treshold(_offending_treshold) {
	clear_stats();
	name = name_str.c_str();

	jThread::Monitor::MonitorGuard g(&mtr);
	if(all_stats == NULL) {
		all_stats = (TimeMeasure **)calloc(128, sizeof(TimeMeasure *));
	}
	int k;
	for(k = 0; k < 128; k++) {
		if(all_stats[k] == NULL) {
			all_stats[k] = this;
			return;
		}
	}
}

TimeMeasure::~TimeMeasure() {
	jThread::Monitor::MonitorGuard g(&mtr);
	int k;
	for(k = 0; k < 128; k++) {
		if(all_stats[k] == this) {
			all_stats[k] = NULL;
			return;
		}
	}
}

void TimeMeasure::start_measure() {
	clock_gettime(CLOCK_MONOTONIC_RAW, &before);
}

void TimeMeasure::stop_measure(const char *cause) {
	clock_gettime(CLOCK_MONOTONIC_RAW, &after);
	after.tv_sec -= before.tv_sec;
	after.tv_nsec -= before.tv_nsec;
	if(after.tv_nsec < 0) {
		after.tv_sec--;
		after.tv_nsec += 1000000000;
	}
	long micro_seconds = after.tv_sec * 1000000 + after.tv_nsec / 1000;
	average_time = (average_time + micro_seconds) / 2;
	if(worst_time < micro_seconds) {
		worst_time = micro_seconds;
		worst_cause = cause;
	}
	if(best_time > micro_seconds) best_time = micro_seconds;

	if(micro_seconds > offending_treshold)
		offending_frames++;
	total_frames++;
}

int TimeMeasure::print_stats() {
	int off = 0;
	if(worst_time == 0) return 0;
	
	if(total_frames != 0)
		off = (100 * offending_frames) / total_frames;
	SATAN_DEBUG(" ------------------------\n");
	SATAN_DEBUG(" ------- Stats for: %s\n", name);
	SATAN_DEBUG(" ------- offender: %s\n", worst_cause);
	SATAN_DEBUG(" ------- worst: %d us\n", worst_time);
	SATAN_DEBUG(" ------- averg: %d us\n", average_time);
	SATAN_DEBUG(" -------  best: %d us\n", best_time);
	SATAN_DEBUG("        offend: %d/100\n", off);
	return worst_time;
}

void TimeMeasure::clear_stats() {
	best_time = 100000000;
	average_time = 0;
	worst_time = 0;
	offending_frames = total_frames = 0;
	worst_cause = "not set";
}

void TimeMeasure::print_all_stats() {
	if(all_stats == NULL) return;

	int worst_accumulated = 0;
	for(int k = 0; k < 128; k++) {
		TimeMeasure *p = all_stats[k];
		if(p != NULL) {
			worst_accumulated += p->print_stats();
		}
	}
	SATAN_DEBUG("    --------- ACCUMULATED: %ld us\n", worst_accumulated);
}

void TimeMeasure::clear_all_stats() {
	if(all_stats == NULL) return;
	
	for(int k = 0; k < 128; k++) {
		TimeMeasure *p = all_stats[k];
		if(p != NULL) {
			p->clear_stats();
		}
	}	
}

#endif // __DO_TIME_MEASURE
