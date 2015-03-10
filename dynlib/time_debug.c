typedef struct _TimeMeasure {	
	struct timespec before;
	struct timespec after;
	int worst_time;
	int average_time;
	int best_time;
	int last_time;
	int total_frames;
} TimeMeasure;

void clear_stats(TimeMeasure *tm) {
	tm->best_time = 100000000;
	tm->average_time = 0;
	tm->worst_time = 0;
	tm->total_frames = 0;
}

static inline void start_measure(TimeMeasure *tm) {
	clock_gettime(CLOCK_MONOTONIC_RAW, &(tm->before));
}

static inline void stop_measure(TimeMeasure *tm) {
	clock_gettime(CLOCK_MONOTONIC_RAW, &(tm->after));
	tm->after.tv_sec -= tm->before.tv_sec;
	tm->after.tv_nsec -= tm->before.tv_nsec;
	if(tm->after.tv_nsec < 0) {
		tm->after.tv_sec--;
		tm->after.tv_nsec += 1000000000;
	}
	long micro_seconds_l = tm->after.tv_sec * 1000000 + tm->after.tv_nsec / 1000;
	int micro_seconds = micro_seconds_l;
	tm->last_time = micro_seconds;
	tm->average_time = (tm->average_time + micro_seconds) >> 1;
	if(tm->worst_time < micro_seconds) {
		tm->worst_time = micro_seconds;
	}
	if(tm->best_time > micro_seconds) tm->best_time = micro_seconds;

	tm->total_frames++;
}

static void print_stats(TimeMeasure *tm) {
	DYNLIB_DEBUG(" ------------------------\n");
	DYNLIB_DEBUG(" ------- worst: %d us\n", tm->worst_time);
	DYNLIB_DEBUG(" ------- averg: %d us\n", tm->average_time);
	DYNLIB_DEBUG(" -------  best: %d us\n", tm->best_time);
	DYNLIB_DEBUG(" -------  last: %d us\n", tm->last_time);
	DYNLIB_DEBUG(" ------ frames: %d us\n", tm->total_frames);
}

