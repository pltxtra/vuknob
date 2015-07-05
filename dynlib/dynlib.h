/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
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

/*********************************************
 *
 *    ----N---O---T---E---!----
 *
 * When implementing a new dynamic machine, please
 * consider how your code will execute on different
 * architectures. For example, the sin() (and other math
 * functions) on an ARM-based CPU might not be implemented
 * in hardware and may therefore be very slow. In this
 * case there might be alternative solutions, like using
 * a lookup-table instead of doing calculations over and
 * over again.
 *
 *********************************************/

#ifndef DYNLIB_H
#define DYNLIB_H

#include<stdint.h>
#include<string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#ifndef __SATAN_USES_FXP
#ifndef __SATAN_USES_FLOATS
#error "You must define either __SATAN_USES_FXP or __SATAN_USES_FLOATS"
#endif
#endif

#ifdef __SATAN_USES_FXP
#include <fixedpointmath.h>
#else
#include <math.h>
#endif

#include "../kiss_fftr.h"

#define VALUE_NOT_SET 0xffffffff

#define STRING_CONTROLLER_SIZE 2048

// low latency treshold, in seconds - this is where it is useful to enable low latency related features
#define LOW_LATENCY_THRESHOLD 0.09f
#define MINIMUM_LATENCY 0.08f

#ifdef __SATAN_USES_FXP

#define FTYPE fp8p24_t
#define itoFTYPE(x) itofp8p24(x)
#define ftoFTYPE(x) ftofp8p24(x)
#define FTYPEtof(x) fp8p24tof(x)
#define FTYPEtod(x) fp8p24tod(x)
#define FTYPEtoi(x) fp8p24toi(x)
#define mulFTYPE(x,y) mulfp8p24(x,y)
#define divFTYPE(x,y) divfp8p24(x,y)
#define FTYPE_RESOLUTION _fx8p24bit

#define FTYPE_IS_FP8P24

#else

#define FTYPE float
#define itoFTYPE(x) ((float)(x))
#define ftoFTYPE(x) ((float)(x))
#define FTYPEtof(x) ((float)(x))
#define FTYPEtod(x) ((double)(x))
#define FTYPEtoi(x) ((int)(x))
#define mulFTYPE(x,y) ((x) * (y))
#define divFTYPE(x,y) ((x) / (y))
#define FTYPE_RESOLUTION _fl32bit

#define FTYPE_IS_FLOAT

#endif

	enum Dimension {
		_0D = 0,
		_1D = 1,
		_2D = 2,
		_MIDI = 3,
		_MAX_D = 4
	};
	enum Resolution {
		_8bit = 1,
		_16bit = 2,
		_32bit = 3,
		_fl32bit = 4,
		_fx8p24bit = 5,
		_PTR = 6,
		_MAX_R = 7
	};

	enum SinkStatus {
		_sinkJustPlay = 0,
		_sinkRecord = 1,

		_sinkResumed = -4,
		_notSink = -3,
		_sinkPaused = -2,
		_sinkException = -1,

		// use these values when returning from your fill_sink_callback
		_sinkCallbackOK = 2,
		_sinkCallbackFAIL = 3

	};

	typedef void MachinePointer;
	typedef void SignalPointer;
	typedef uint32_t Parameter;

	typedef struct _AsyncOp {
		void (*func)(struct _AsyncOp *a_op);
		void *data; // generic data pointer
		int i_data1; // generic integer data storage
		int finished; // indicate if operation is finished
	} AsyncOp;

	typedef struct _MachineTable {
		MachinePointer *mp;

		/* call this if you are the sink and want to generate a frame. This pointer
		 * is always valid and won't change
		 *
		 * OBSERVE! Do not call fill_sink() from a thread originating in Satan, it may deadlock!
		 */
		int (*fill_sink)(struct _MachineTable *mt,
				 int (*fill_sink_callback)(int status, void *cbd),
				 void *callback_data);

		void (*enable_low_latency_mode)();
		void (*disable_low_latency_mode)();

		void (*set_signal_defaults)(struct _MachineTable *,
					    int dim,
					    int len, int res,
					    int frequency);

		SignalPointer *(*get_input_signal)(struct _MachineTable *, const char *);
#ifdef THIS_IS_A_MOCKERY
		SignalPointer *(*get_mocking_signal)(struct _MachineTable *, const char *);
#endif
		SignalPointer *(*get_output_signal)(struct _MachineTable *, const char *);
		SignalPointer *(*get_next_signal)(struct _MachineTable *, SignalPointer *);
		SignalPointer *(*get_static_signal)(int index);

		int (*get_signal_dimension)(SignalPointer *);
		int (*get_signal_channels)(SignalPointer *);
		int (*get_signal_resolution)(SignalPointer *);
		int (*get_signal_frequency)(SignalPointer *);
		int (*get_signal_samples)(SignalPointer *);
		void *(*get_signal_buffer)(SignalPointer *);

		void (*run_simple_thread)(void (*thread_function)(void *), void *);

		// copies the filename to dst, maximum len chars, returns 0 on OK
		int (*get_recording_filename)(struct _MachineTable *, char *dst, unsigned int len);

		void (*register_failure)(void *machine_instance, const char *);

		// Satan's "portable" math library
		void (*get_math_tables)(
			float **sine_table,
			FTYPE (**__pow_fun)(FTYPE x, FTYPE y),
			FTYPE **sine_table_FTYPE
			);

		// Fast Fourier Transform - FFT
		kiss_fftr_cfg (*prepare_fft)(int samples, int inverse_fft);
		void (*do_fft)(kiss_fftr_cfg cfg, FTYPE *timedata, kiss_fft_cpx *freqdata);
		void (*inverse_fft)(kiss_fftr_cfg cfg, kiss_fft_cpx *freqdata, FTYPE *timedata);

		// Async Operations
		void (*run_async_operation)(AsyncOp *op);

#ifdef ANDROID
		// I don't like this hack either...
		void (*VuknobAndroidAudio__CLEANUP_STUFF)();
		void (*VuknobAndroidAudio__SETUP_STUFF)(
			int *period_size, int *rate,
			int (*__entry)(void *data),
			void *__data,
			int (**android_audio_callback)(FTYPE vol, FTYPE *in, int il, int ic),
			void (**android_audio_stop_f)(void)
			);

/*** THESE VALUES MUST MATCH THE VALUES IN SatanAudio.java ***/
#define __PLAYBACK_OPENSL_DIRECT 14
#define __PLAYBACK_OPENSL_BUFFERED 16
#define __PLAYBACK_SAMSUNG 17

		// return one of the __OPENSL_*_MODE values
		int (*VuknobAndroidAudio__get_native_audio_configuration_data)(int *frequency, int *buffersize);
#endif


	} MachineTable;

/*******************************************
 *
 *     misc stuff
 *
 *******************************************/

#define FILTER_RECALC_PART int __filter_recalc_step

// we use the pointer to get an offset, we don't want all machines to recalculate the filter simultaneously..
#define DO_FILTER_RECALC(a) (a->__filter_recalc_step == ((unsigned int)a % FILTER_RECALC_PERIOD))
#define STEP_FILTER_RECALC(a) a->__filter_recalc_step = (a->__filter_recalc_step + 1) % FILTER_RECALC_PERIOD


/*******************************************
 *
 *     MIDI stuff
 *
 *******************************************/

#define MIDI_MESSAGE_TYPE_MASK	0xf0
#define MIDI_NOTE_OFF		0x80
#define MIDI_NOTE_ON		0x90
#define MIDI_POLY_AFTERTOUCH	0xa0
#define MIDI_CONTROL_CHANGE	0xb0
#define MIDI_MODE_CHANGE       	0xb0
#define MIDI_PROGRAM_CHANGE    	0xc0
#define MIDI_CHANNEL_AFTERTOUCH	0xd0
#define MIDI_PITCH_BEND		0xe0

	typedef struct _MidiEvent {
		size_t length;
		uint8_t data[1];
	} MidiEvent;

#define SET_MIDI_DATA_2(mev,a,b) \
			mev->data[0] = a; \
			mev->data[1] = b;
#define SET_MIDI_DATA_3(mev,a,b,c) \
			mev->data[0] = a; \
			mev->data[1] = b; \
			mev->data[2] = c;

/********************************************
 *
 *     Satan's "portable" math library
 *
 * macros with FTYPE ending might either be normal float or fp8p24_t
 * depending on if we use fixed point math or not.
 *
 ********************************************/

#ifdef __SATAN_USES_FXP
	// USING fixed point math

#define SAT_SIN_TABLE_LEN 8192
#define SAT_SIN_FTYPE_TABLE_LEN 0x1001

/** This stuff is usable if we have a CPU that lacks a proper FPU **/
#define USE_SATANS_MATH \
	float *__satan_sine_table; \
	FTYPE *__satan_sine_table_FTYPE; \
	FTYPE (*__satan_pow_function)(FTYPE x, FTYPE y);

#define SETUP_SATANS_MATH(m)						\
	m->get_math_tables(&__satan_sine_table, &__satan_pow_function, &__satan_sine_table_FTYPE);

#define SAT_SIN(x)				\
 __satan_sine_table[\
  ((int)((x/(2*M_PI))*SAT_SIN_TABLE_LEN))&(SAT_SIN_TABLE_LEN-1)]
#define SAT_COS(x) \
 __satan_sine_table[\
  (((int)((x/(2*M_PI))*SAT_SIN_TABLE_LEN))+(SAT_SIN_TABLE_LEN>>2))&(SAT_SIN_TABLE_LEN-1)]
#define SAT_SIN_SCALAR(x) \
 __satan_sine_table[\
  ((int)(x*SAT_SIN_TABLE_LEN))&(SAT_SIN_TABLE_LEN-1)]
#define SAT_COS_SCALAR(x) \
 __satan_sine_table[\
  (((int)(x*SAT_SIN_TABLE_LEN))+(SAT_SIN_TABLE_LEN>>2))&(SAT_SIN_TABLE_LEN-1)]

#define SAT_SIN_SCALAR_FTYPE(x) \
	__satan_sine_table_FTYPE[((x & 0x01fff000) >> 12)]
#define SAT_COS_SCALAR_FTYPE(x) \
	__satan_sine_table_FTYPE[(SAT_SIN_FTYPE_TABLE_LEN>>2) + ((x & 0x01fff000) >> 12)]
#define SAT_TAN_SCALAR_FTYPE(x) divFTYPE(SAT_SIN_SCALAR_FTYPE(x), SAT_COS_SCALAR_FTYPE(x))

#define SAT_SIN_FTYPE(x) \
	SAT_SIN_SCALAR_FTYPE(divFTYPE(x, ftoFTYPE(2.0f * M_PI)))
#define SAT_COS_FTYPE(x) \
	SAT_COS_SCALAR_FTYPE(divFTYPE(x, ftoFTYPE(2.0f * M_PI)))
#define SAT_TAN_FTYPE(x) \
	SAT_TAN_SCALAR_FTYPE(divFTYPE(x, ftoFTYPE(2.0f * M_PI)))

#define SAT_POW_FTYPE(x,y) __satan_pow_function(x,y)

#define ABS_FTYPE(x) ((((x & 0x80000000) >> 31) * (-x) + ((x ^ 0x80000000) >> 31)  * (x)))

#define SAT_FLOOR(x) (x & 0xff000000)

#else // not using fixed point math

/** If we have a FPU, there is not much reason not to use the standard C math library ****/
#define USE_SATANS_MATH
#define SETUP_SATANS_MATH(m)
#define SAT_SIN(x) sinf(x)
#define SAT_COS(x) cosf(x)
#define SAT_SIN_SCALAR(x) sinf(2.0f*M_PI*x)
#define SAT_COS_SCALAR(x) cosf(2.0f*M_PI*x)
#define SAT_SIN_SCALAR_FTYPE(x) SAT_SIN_SCALAR(x)
#define SAT_COS_SCALAR_FTYPE(x) SAT_COS_SCALAR(x)
#define SAT_TAN_SCALAR_FTYPE(x) divFTYPE(SAT_SIN_SCALAR_FTYPE(x), SAT_COS_SCALAR_FTYPE(x))

#define SAT_SIN_FTYPE(x) sin(x)
#define SAT_COS_FTYPE(x) cos(x)
#define SAT_TAN_FTYPE(x) tan(x)

#define SAT_POW_FTYPE(x,y) pow(x,y)

#define ABS_FTYPE(x) fabs(x)

#define SAT_FLOOR(x) floorf(x)

#endif

#ifdef __cplusplus
};
#endif

#endif /* !DYNLIB_H */
