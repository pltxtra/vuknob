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

#ifndef __TIME_MEASURE
#define __TIME_MEASURE

#ifdef __DO_TIME_MEASURE

#include <string>
#include <time.h>

class TimeMeasure {	
private:
	std::string name_str;
	
	struct timespec before;
	struct timespec after;

	int print_stats();
	void clear_stats();
	
public:
	TimeMeasure(const std::string name, int offending_treshold);
	~TimeMeasure();

	int offending_treshold;
	int offending_frames;
	int total_frames;
	int worst_time;
	int average_time;
	int best_time;
	const char *worst_cause;
	const char *name;
	
	void start_measure();
	void stop_measure(const char *cause);

	static void print_all_stats();
	static void clear_all_stats();
};

#define DECLARE_TIME_MEASURE_OBJECT_STATIC(a,b,c) static TimeMeasure a(b,c)
#define DECLARE_TIME_MEASURE_OBJECT(a) TimeMeasure *a
#define CREATE_TIME_MEASURE_OBJECT(a,b,c) a = new TimeMeasure(b,c)
#define DELETE_TIME_MEASURE_OBJECT(a) delete a

#define START_TIME_MEASURE(a) a.start_measure();
#define STOP_TIME_MEASURE(a,b) a.stop_measure(b);

#define TIME_MEASURE_PRINT_ALL_STATS() TimeMeasure::print_all_stats();
#define TIME_MEASURE_CLEAR_ALL_STATS() TimeMeasure::clear_all_stats();

#else

#define DECLARE_TIME_MEASURE_OBJECT_STATIC(a,b,c)
#define DECLARE_TIME_MEASURE_OBJECT(a)
#define CREATE_TIME_MEASURE_OBJECT(a,b,c)
#define DELETE_TIME_MEASURE_OBJECT(a)

#define START_TIME_MEASURE(a)
#define STOP_TIME_MEASURE(a,b)

#define TIME_MEASURE_PRINT_ALL_STATS()
#define TIME_MEASURE_CLEAR_ALL_STATS()

#endif

#endif // __TIME_MEASURE


