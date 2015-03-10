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

/*****************
 *
 * Generic stuff
 *
 *****************/

/* MachineType value:
 * -1	= sink
 * >=0	= midi input
 */
typedef int MachineType;

/*********************
 * Define audio subsystem
 *********************/
#ifdef ANDROID

#ifndef USE_OPEN_SL_ES
#define USE_ANDROID_AUDIO
#endif

#else
#define USE_ALSA_AUDIO
//#define USE_PULSE_AUDIO
#endif

/*********************
 *
 * OK, jack or pulse
 * specific stuff below
 *
 *********************/

#ifdef USE_JACK_AUDIO
/* OK, we are NOT using
 * Pulse Audio - This
 * means that we use JACK!
 * So, everything is jack.. ;)
 */

#include <jack/jack.h>
#include <jack/midiport.h>

#define MAX_MIDI_IN 32
/* four megabytes! */
#define MIDI_BUFFER_SIZE 4 * 1024 * 1024
typedef struct _JackInstanceData {
	jack_client_t *client;

	int midi_in_counter;
	jack_port_t *midi_in[MAX_MIDI_IN];

	jack_port_t *output_left;
	jack_port_t *output_right;

	int frequency, new_frequency, len;

	int jack_open; // 0 = false, !0 = true

	uint8_t *midi_event_buffer;
	uint8_t *first_free_slot;
} JackInstanceData;

JackInstanceData *instance = NULL;

/************************************
 *
 *     Sink type functions (for JACK)
 *
 ************************************/

int srate(jack_nframes_t nframes, void *unused_mtable_ptr) {
	instance->new_frequency = nframes;
	return 0;
}

int fillerup(jack_nframes_t len, void *mtable_ptr) {
	static int ctr = 150;
	MachineTable *mt = (MachineTable *)mtable_ptr;
	
	if((len != instance->len) || (instance->new_frequency != instance->frequency)) {
		instance->len = len;
		instance->frequency = instance->new_frequency;

		/* set audio signal defaults */
		mt->set_signal_defaults(mt, _0D, len, _fl32bit, instance->frequency);

		/* set midi signal defaults */
		mt->set_signal_defaults(mt, _MIDI, len, _PTR, instance->frequency);
	}

	/* this will eventually call our execute function, otherwise fill
	 * buffers with zeroes.
	 */

	if((ctr--) == 0) { 
		DYNLIB_DEBUG("mtable-ptr in fillerup: %p\n", mt);
		ctr = 150;
	}

	if(mt->fill_sink(mt) != 0) {
		/* something went wrong - zero output please */
		jack_default_audio_sample_t *outl =
			(jack_default_audio_sample_t *) jack_port_get_buffer (instance->output_left, len);
		jack_default_audio_sample_t *outr =
			(jack_default_audio_sample_t *) jack_port_get_buffer (instance->output_right, len);
		memset(outl, 0, sizeof(jack_default_audio_sample_t) * len);
		memset(outr, 0, sizeof(jack_default_audio_sample_t) * len);
	}

	/* free all allocated midi events after each run */
	instance->first_free_slot = instance->midi_event_buffer;
	
	return 0;
}

void execute_sink(MachineTable *mt) {
	SignalPointer *s = mt->get_input_signal(mt, "stereo");
	
	float *in = NULL;
	int il = 0;
	int ic = 0;

	if(s == NULL)
		return;
	
	in = mt->get_signal_buffer(s);
	il = mt->get_signal_samples(s);
	ic = mt->get_signal_channels(s);
	
	jack_default_audio_sample_t *out_l =
		(jack_default_audio_sample_t *)
		jack_port_get_buffer (instance->output_left, il);
	jack_default_audio_sample_t *out_r =
		(jack_default_audio_sample_t *)
		jack_port_get_buffer (instance->output_right, il);

	int i;

	float im;
	
	if(ic != 2) {
		mt->register_failure(mt, "Unexpected number of input channels in signal connected to LiveOut, expected 2.");
		return;
	}
	
	for(i = 0; i < il; i++) {
		/* channel 0 (left) */
		out_l[i] = (jack_default_audio_sample_t)in[i * ic + 0];
		/* channel 1 (right) */
		out_r[i] = (jack_default_audio_sample_t)in[i * ic + 1];
	}

}

/************************************
 *
 *     MIDI type functions (For JACK)
 *
 ************************************/

void delete_midi(MachineType mtype) {
	/* ignore errors */
	(void) jack_port_unregister(instance->client, instance->midi_in[mtype]);
	instance->midi_in_counter--;
}

/* return -1 on failure */
MachineType init_midi(MachineTable *mt) {
	int i;

	for(i = 0; i < MAX_MIDI_IN; i++) {
		if(instance->midi_in[i] == NULL) {
			char input_name[256];
			
			snprintf(input_name, 255, "midi_in%02d", i);

			instance->midi_in[i] = jack_port_register
				(instance->client,
				 input_name,
				 JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);

			/* Check if the call succeeded. */
			if(instance->midi_in[i] != NULL) {
				/* it did.. oh joy! */
				instance->midi_in_counter++;
				return (MachineType)i;
			}
			return -1; /* something went wrong...*/
		}
	}
	return -1; /* no free slots. */
}

void execute_midi(MachineTable *mt, MachineType mtype) {
	SignalPointer *s = mt->get_output_signal(mt, "midi");

	void **out = NULL;
	int ol = 0;
	int oc = 0;
	MidiEvent *mev = NULL;

	if(s == NULL)
		return;
	
	out = (void **)mt->get_signal_buffer(s);
	ol = mt->get_signal_samples(s);
	oc = mt->get_signal_channels(s);	

	static int ctr = 15;
		
	if(ctr > 0) { 
		DYNLIB_DEBUG("In execute_midi for jackif!\n");
		ctr--;
	}

	if(oc != 1) {
		mt->register_failure(mt, "Unexpected number of output channels in signal connected to LiveOut_Midi, expected 2.");
		return;
	}
	
	void* port_buf = jack_port_get_buffer(instance->midi_in[mtype], instance->len);
	jack_midi_event_t in_event;
	jack_nframes_t event_count = jack_midi_get_event_count(port_buf);
	
	int i;	

	if(event_count > ol) {
		mt->register_failure(mt, "Midi event count larger than output buffer size - this is insane.");
		return;
	}
	
	if(instance->first_free_slot - instance->midi_event_buffer >= MIDI_BUFFER_SIZE) {
		DYNLIB_DEBUG("Error in LiveOut:Midi - Midi buffer was too small, events are missed.");
		return;
	}

	/* clear output first */
	memset(out, 0, sizeof(void *) * ol);

	for(i = 0; i < event_count; i++) {
		jack_midi_event_get(&in_event, port_buf, i);

		mev = out[in_event.time] = (MidiEvent *)instance->first_free_slot;

		mev->length = in_event.size;
		memcpy(mev->data, in_event.buffer, in_event.size);

		instance->first_free_slot += sizeof(mev->length) + in_event.size;
	}
}

/********************************
 *
 *    Generic jack stuff
 *
 ********************************/

void delete_jack() {
	if(instance == NULL) return;

	/* free instance data here */
	jack_client_close(instance->client);	
	
	free(instance);
	instance = NULL;
}

void *init_jack(MachineTable *mt) {
	/* allocate and clear the JackInstanceData structure */
	instance = (JackInstanceData *)malloc(sizeof(JackInstanceData));
	if(instance == NULL) return NULL;
	memset(instance, 0, sizeof(JackInstanceData));
	
	/* prepare midi event buffer */
	instance->midi_event_buffer =
		(uint8_t *)malloc(sizeof(uint8_t) * MIDI_BUFFER_SIZE);
	instance->first_free_slot = instance->midi_event_buffer;
	
	/* if we fail to allocate buffer - toss everything */
	if(instance->midi_event_buffer == NULL) {
		free(instance);
		return NULL;
	}
	
	/* init jack */
	jack_status_t status = 0x00;
	if ((instance->client = jack_client_new("satan")) == 0)
	{
		DYNLIB_DEBUG("jack server not running [0x%08x]?\n", status);
		goto fail;
	}
	
	DYNLIB_DEBUG("FLuffofluffo!\n"); fflush(0);
	instance->frequency = jack_get_sample_rate (instance->client);

	jack_set_process_callback (instance->client, fillerup, mt);

	jack_set_sample_rate_callback (instance->client, srate, mt);

/*	jack_on_shutdown (client, jack_shutdown, 0);*/

	instance->output_left = jack_port_register (instance->client, "out_left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	instance->output_right = jack_port_register (instance->client, "out_right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	if (jack_activate (instance->client))
	{
		DYNLIB_DEBUG("cannot activate jack client.");
		goto fail;
	}

	DYNLIB_DEBUG("Returning new jack instance: %p\n", instance); fflush(0);
	/* return pointer to instance data */
	return (void *)instance;

 fail:
	delete_jack(NULL);

	return NULL;
}

/************************************
 *
 *     interface functions (for JACK version)
 *
 ************************************/

void *init(MachineTable *mt, const char *name) {
	MachineType *retval;

	DYNLIB_DEBUG("\n\n\n***************CREATING: %s****************\n\n\n", name);
	
	retval = (MachineType *)malloc(sizeof(MachineType));
	if(retval == NULL) return NULL;
	
	if(instance == NULL) {
		/* Allocate and initiate instance data here */
		DYNLIB_DEBUG("Creating new jack instance: %p\n", retval); fflush(0);
		if(init_jack(mt) == NULL) {
			free(retval);
			return NULL;
		}
		DYNLIB_DEBUG("jack instance created: %p\n", instance); fflush(0);
	}
	
	if(strcmp("liveoutsink", name) == 0) {
		instance->jack_open = 1;
		*retval = -1;
		return retval;
	} else if(strcmp("liveoutmidi_in", name) == 0) {
		*retval = init_midi(mt);
		if(*retval == -1) {
			free(retval);
			retval = NULL;
		}
		return retval;
	}
	
	return NULL;
}

void delete(void *data) {
	MachineType mtype = *(MachineType *)data;

	if(mtype == -1)
		instance->jack_open = 0;
	else 
		delete_midi(mtype);

	/* free MachineType */
	free(data);

	if(instance->jack_open == 0 && instance->midi_in_counter == 0)
		delete_jack();
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	MachineType mtype = *(MachineType *)data;
	if(mtype == -1)
		execute_sink(mt);
	else
		execute_midi(mt, mtype);
}

// OK End of JACK version
#elif defined(USE_ALSA_AUDIO)
// Begin ALSA / asound

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <fixedpointmath.h>

typedef struct AlsaInstance_ {
	int type; // 0 = liveoutsink, 1 = liveoutmidi_in

	/* BEGIN MIDI IN TYPE DATA */
	// not implemented
	/* END MIDI IN TYPE DATA */

	/* BEGIN SINK TYPE DATA */
	pthread_t thread;
	pthread_attr_t pta;

	pthread_mutexattr_t attr;
	pthread_mutex_t mutex;
	
	snd_pcm_t *handle;

	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	
	int16_t *samples;
	size_t length;

	MachineTable *mt;
	
	struct RIFF_WAVE_FILE riff_file;

	int alsa_open;

	int doRecord, recordFile;

	float volume;

	char *device;                     /* playback device */
	snd_pcm_format_t format;    /* sample format */
	unsigned int rate;                       /* stream rate */
	unsigned int channels;                       /* count of channels */
	unsigned int buffer_time;               /* ring buffer length in us */
	unsigned int period_time;               /* period time in us */
	int resample;                                /* enable alsa-lib resampling */
	
	snd_pcm_sframes_t buffer_size;
	snd_pcm_sframes_t period_size;
	snd_output_t *output;

	/* END SINK TYPE DATA */
} AlsaInstance;

// time in micro seconds
#define BUFFER_TIME_DEF 40000
#define PERIOD_TIME_DEF BUFFER_TIME_DEF / 2


static int set_hwparams(AlsaInstance *instance)
{
	snd_pcm_t *handle = instance->handle;
	snd_pcm_hw_params_t *params = instance->hwparams;
	snd_pcm_access_t access = SND_PCM_ACCESS_RW_INTERLEAVED;
	unsigned int rperiod_time;
        unsigned int rrate;
        snd_pcm_uframes_t size;
        int err, dir;

        /* choose all parameters */
        err = snd_pcm_hw_params_any(handle, params);
        if (err < 0) {
                DYNLIB_DEBUG("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
                return err;
        }
        /* set hardware resampling */
        err = snd_pcm_hw_params_set_rate_resample(handle, params, instance->resample);
        if (err < 0) {
                DYNLIB_DEBUG("Resampling setup failed for playback: %s\n", snd_strerror(err));
                return err;
        }
        /* set the interleaved read/write format */
        err = snd_pcm_hw_params_set_access(handle, params, access);
        if (err < 0) {
                DYNLIB_DEBUG("Access type not available for playback: %s\n", snd_strerror(err));
                return err;
        }
        /* set the sample format */
        err = snd_pcm_hw_params_set_format(handle, params, instance->format);
        if (err < 0) {
                DYNLIB_DEBUG("Sample format not available for playback: %s\n", snd_strerror(err));
                return err;
        }
        /* set the count of channels */
        err = snd_pcm_hw_params_set_channels(handle, params, instance->channels);
        if (err < 0) {
                DYNLIB_DEBUG("Channels count (%i) not available for playbacks: %s\n", instance->channels, snd_strerror(err));
                return err;
        }
        /* set the stream rate */
        rrate = instance->rate;
        err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
        if (err < 0) {
                DYNLIB_DEBUG("Rate %iHz not available for playback: %s\n", instance->rate, snd_strerror(err));
                return err;
        }
        if (rrate < 40000 || rrate > 60000) {
                DYNLIB_DEBUG("Rate doesn't match close enough (requested %iHz, get %iHz)\n", instance->rate, rrate);
                return -EINVAL;
        }
        /* set the buffer time */
        err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &(instance->buffer_time), &dir);
        if (err < 0) {
                DYNLIB_DEBUG("Unable to set buffer time %i for playback: %s\n", instance->buffer_time, snd_strerror(err));
                return err;
        }
        err = snd_pcm_hw_params_get_buffer_size(params, &size);
        if (err < 0) {
                DYNLIB_DEBUG("Unable to get buffer size for playback: %s\n", snd_strerror(err));
                return err;
        }
        instance->buffer_size = size;
        /* set the period time */
	rperiod_time = instance->period_time;
        err = snd_pcm_hw_params_set_period_time_near(handle, params, &(instance->period_time), &dir);
        if (err < 0) {
                DYNLIB_DEBUG("Unable to set period time %i for playback: %s\n", instance->period_time, snd_strerror(err));
                return err;
        }
        err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
        if (err < 0) {
                DYNLIB_DEBUG("Unable to get period size for playback: %s\n", snd_strerror(err));
                return err;
        }
	DYNLIB_DEBUG("Requested %d - got %d -> %d (%f).\n",
	       (int)rperiod_time, (int)instance->period_time, (int)size, (float)size / (float)(instance->rate));
        instance->period_size = size;
        /* write the parameters to device */
        err = snd_pcm_hw_params(handle, params);
        if (err < 0) {
                DYNLIB_DEBUG("Unable to set hw params for playback: %s\n", snd_strerror(err));
                return err;
        }
        return 0;
}

static int set_swparams(AlsaInstance *instance)
{
	snd_pcm_t *handle = instance->handle;
	snd_pcm_sw_params_t *swparams = instance->swparams;
        int err;

        /* get the current swparams */
        err = snd_pcm_sw_params_current(handle, swparams);
        if (err < 0) {
                DYNLIB_DEBUG("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
                return err;
        }
        /* start the transfer when the buffer is almost full: */
        /* (buffer_size / avail_min) * avail_min  --- trick that uses integer math */
        err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (instance->buffer_size / instance->period_size) * instance->period_size);
        if (err < 0) {
                DYNLIB_DEBUG("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
                return err;
        }
        /* allow the transfer when at least period_size samples can be processed */
        err = snd_pcm_sw_params_set_avail_min(handle, swparams, instance->period_size);
        if (err < 0) {
                DYNLIB_DEBUG("Unable to set avail min for playback: %s\n", snd_strerror(err));
                return err;
        }
        /* write the parameters to the playback device */
        err = snd_pcm_sw_params(handle, swparams);
        if (err < 0) {
                DYNLIB_DEBUG("Unable to set sw params for playback: %s\n", snd_strerror(err));
                return err;
        }
        return 0;
}

/*
 *   Underrun and suspend recovery
 */
 
static int xrun_recovery(snd_pcm_t *handle, int err)
{
        if (err == -EPIPE) {    /* under-run */
                err = snd_pcm_prepare(handle);
                if (err < 0)
                        DYNLIB_DEBUG("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
                return 0;
        } else if (err == -ESTRPIPE) {
                while ((err = snd_pcm_resume(handle)) == -EAGAIN)
                        sleep(1);       /* wait until the suspend flag is released */
                if (err < 0) {
                        err = snd_pcm_prepare(handle);
                        if (err < 0)
                                DYNLIB_DEBUG("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
                }
                return 0;
        }
        return err;
}

int alsa_fill_sink_callback(int callback_status, void *data) {
	AlsaInstance *inst = (AlsaInstance *)data;

	MachineTable *mt = NULL;
	mt = inst->mt;

	switch(callback_status) {
	case _sinkJustPlay:
		if(inst->recordFile != -1) {
			close(inst->recordFile);
			inst->recordFile = -1;
		}
		inst->doRecord = 0;
		break;
	case _sinkRecord:
		if(!inst->doRecord) {
			inst->doRecord = 1;
			char file_name[1024];
			if(mt->get_recording_filename(
				   mt, file_name, sizeof(file_name)) == 0) {
				if(file_name[0] == '\0') {
					strncpy(file_name, "DEFAULT.WAV", sizeof(file_name));
				}

				RIFF_create_file(&(inst->riff_file), file_name);				
			}
		}
		break;
			
	default:
	case _notSink:
	case _sinkPaused:
	case _sinkException:
		RIFF_close_file(&(inst->riff_file));

		inst->doRecord = 0;
		memset(inst->samples,
		       0,
		       sizeof(int16_t) * inst->channels * inst->length);
		break;
	}		

	RIFF_write_data(&(inst->riff_file),
			inst->samples,
			inst->period_size * inst->channels * sizeof(int16_t));

        signed short *ptr;
        int err, cptr;

	ptr = inst->samples;
	cptr = inst->period_size;

	while (cptr > 0) {
		err = snd_pcm_writei(inst->handle,
				     ptr, cptr);
		if (err == -EAGAIN)
			continue;
		if (err < 0) {
			if (xrun_recovery(inst->handle, err) < 0) {
				DYNLIB_DEBUG("Write error: %s\n", snd_strerror(err));
				exit(EXIT_FAILURE);
			} 
			break;  /* skip one period */
		}
		ptr += err * inst->channels;
		cptr -= err;
	}

	return _sinkCallbackOK;
}

void cleanup_alsa(AlsaInstance *instance) {
	free(instance->samples);
	snd_pcm_close(instance->handle);
}

/*
 *   Transfer method - write only
 */

void *alsa_thread(void *data)
{
	AlsaInstance *instance = (AlsaInstance *)data;
	MachineTable *mt = NULL;

	instance->doRecord = 0;
	instance->recordFile = -1;
	
	mt = instance->mt;
	
	memset(instance->samples,
	       0,
	       sizeof(int16_t) * instance->channels * instance->length);

	// there is a _theoretic_ possibility that
	// the master thread has already locked this and is
	// waiting for us to terminate. This situation
	// would cause a deadlock. However, it is my (antons)
	// opinion that this situation will not occur
	// since the liveout instance would only be destroyed
	// on user action only. The time interval between creation
	// and destruction would therefore be long enough for us to
	// acquire the lock here.
	int r;
	r = pthread_mutex_lock(&(instance->mutex));
	if(r != 0) {
		DYNLIB_DEBUG("  primary mutex lock failed.\n");
		exit(1);
	}
        while (instance->alsa_open) {
		r = pthread_mutex_unlock(&(instance->mutex));
		if(r != 0) {
			DYNLIB_DEBUG("  mutex unlock failed.\n");
			exit(1);
		}

		(void)mt->fill_sink(mt, alsa_fill_sink_callback, instance);

		r = pthread_mutex_lock(&(instance->mutex));
		if(r != 0) {
			DYNLIB_DEBUG("  secondary mutex lock failed.\n");
			exit(1);
		}
        }
	pthread_mutex_unlock(&(instance->mutex));

	DYNLIB_DEBUG("   TIME TO CLEANUP ALSA\n");
		
	cleanup_alsa(instance);
	free(instance);
	
	DYNLIB_DEBUG(" ALSA thread terminating.\n");
	
	return NULL;
}

void execute_sink(AlsaInstance *instance, MachineTable *mt) {
	SignalPointer *s = mt->get_input_signal(mt, "stereo");

	if(s == NULL) return;
	
	fp8p24_t *in = mt->get_signal_buffer(s);

	int il = mt->get_signal_samples(s);
	int ic = mt->get_signal_channels(s);	

	int i;

	fp8p24_t vol = ftofp8p24(instance->volume);
	for(i = 0; i < il * ic; i++) {
		int32_t out;

		in[i] = mulfp8p24(in[i], vol);
		
		if(in[i] < itofp8p24(-1)) in[i] = itofp8p24(-1);
		if(in[i] > itofp8p24(1)) in[i] = itofp8p24(1);
		
		out = (in[i] << 7);
		out = (out & 0xffff0000) >> 16;
		
		instance->samples[i] = (int16_t)out;
	}
}

 
/*
 *
 */

static AlsaInstance *init_alsa(MachineTable *mt) {
        int err;
		
	DYNLIB_DEBUG("asounder8.3\n");

	AlsaInstance *instance = (AlsaInstance *)malloc(sizeof(AlsaInstance));
	if(!instance) return NULL;

	memset(instance, 0, sizeof(AlsaInstance));
	instance->device = "default";                     /* playback device */
	//instance->device = "hw:1,0";                     /* playback device */
	instance->format = SND_PCM_FORMAT_S16;    /* sample format */
	instance->rate = 44100;                       /* stream rate */
	instance->channels = 2;                       /* count of channels */
	instance->buffer_time = BUFFER_TIME_DEF;               /* ring buffer length in us */
	instance->period_time = PERIOD_TIME_DEF;               /* period time in us */
	instance->resample = 1;                                /* enable alsa-lib resampling */

	instance->volume = 0.5f;
	
	pthread_mutexattr_init(&(instance->attr));
	pthread_mutex_init(&(instance->mutex), &(instance->attr));
	
	snd_pcm_hw_params_alloca(&(instance->hwparams));
        snd_pcm_sw_params_alloca(&(instance->swparams));

	DYNLIB_DEBUG(" hwparams: %p\n", instance->hwparams);
	DYNLIB_DEBUG(" swparams: %p\n", instance->swparams);
        err = snd_output_stdio_attach(&(instance->output), stdout, 0);
        if (err < 0) {
                DYNLIB_DEBUG("Output failed: %s\n", snd_strerror(err));
		goto failure;
        }

        if ((err = snd_pcm_open(&(instance->handle), instance->device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		DYNLIB_DEBUG("Failed to open ALSA device '%s', trying default.\n", instance->device);
		if ((err = snd_pcm_open(&(instance->handle),
					"default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
			DYNLIB_DEBUG("Playback open error: %s\n", snd_strerror(err));
			goto failure;
		}
        }
        
        if ((err = set_hwparams(instance)) < 0) {
                DYNLIB_DEBUG("Setting of hwparams failed: %s\n", snd_strerror(err));
                goto failure;
        }
        if ((err = set_swparams(instance)) < 0) {
                DYNLIB_DEBUG("Setting of swparams failed: %s\n", snd_strerror(err));
                goto failure;
        }

        instance->samples = malloc((instance->period_size * instance->channels * snd_pcm_format_width(instance->format)) / 8);
	DYNLIB_DEBUG("\n\n\n\n   period_size: %ld, channels: %d, format width: %d\n",
	       instance->period_size, instance->channels, snd_pcm_format_width(instance->format));
	DYNLIB_DEBUG("\n\n\n\n   ALLOCATED %ld bytes (%ld samples, %ld per channel)\n\n\n\n",
	       (instance->period_size * instance->channels * snd_pcm_format_width(instance->format)) / 8,
	       ((instance->period_size * instance->channels * snd_pcm_format_width(instance->format)) / 8) / 2,
	       ((instance->period_size * instance->channels * snd_pcm_format_width(instance->format)) / 8) / 4);
        if (instance->samples == NULL) {
                DYNLIB_DEBUG("No enough memory\n");
                goto failure;;
        }

	instance->length = instance->period_size;
	instance->alsa_open = 1;

	/* set audio signal defaults */
	mt->set_signal_defaults(mt, _0D, instance->period_size, FTYPE_RESOLUTION, instance->rate);	
	/* set midi signal defaults */
	mt->set_signal_defaults(mt, _MIDI, instance->period_size, _PTR, instance->rate);

	/* OK, create pthread for this */
	pthread_attr_init(&(instance->pta));
	pthread_attr_setschedpolicy(&(instance->pta), SCHED_RR);

	instance->mt = mt;

	RIFF_prepare(&(instance->riff_file), instance->rate);
	
	if(pthread_create(
		   &(instance->thread),
		   &(instance->pta),
		   alsa_thread,
		   instance) != 0)
		goto failure;
	

        return instance;

 failure:
	if(instance->handle) {
		snd_pcm_close(instance->handle);
		instance->handle = NULL;
	}

	if(instance->samples) {
		free(instance->samples);
		instance->samples = NULL;
	}
	
	if(instance) {
		free(instance);
		instance = NULL;
	}

	return NULL;
}

void *init(MachineTable *mt, const char *name) {
	AlsaInstance *retval = NULL;

	DYNLIB_DEBUG("\n\n\n***************CREATING: %s****************\n\n\n", name);

	if(strcmp("liveoutsink", name) == 0) {
		DYNLIB_DEBUG("Creating new alsa audio instance: %p\n", retval); fflush(0);

		retval = init_alsa(mt);
		if(retval == NULL) {
			DYNLIB_DEBUG("NO alsa audio instance created: FAILURE!\n"); fflush(0);
			return NULL;
		}
		retval->type = 0;
		DYNLIB_DEBUG("alsa audio instance created: %p\n", retval); fflush(0);

		return retval;
	} else if(strcmp("liveoutmidi_in", name) == 0) {
		retval = NULL; /* NO MIDI SUPPORT YET */
		return retval;
	}
	
	return NULL;
}

void delete(void *data) {
	AlsaInstance *instance = (AlsaInstance *)data;
	DYNLIB_DEBUG("  DELETING LIVEOUT!\n"); fflush(0);
	if((instance->type) == 0) {
		int r;
		DYNLIB_DEBUG("Killing thread.\n");  fflush(0);
		r = pthread_mutex_lock(&(instance->mutex));
		if(r != 0) {
			DYNLIB_DEBUG("  triary mutex lock failed.\n");
			exit(1);
		}
		instance->alsa_open = 0;
		r = pthread_mutex_unlock(&(instance->mutex));
		if(r != 0) {
			DYNLIB_DEBUG("  secondary mutex ulock failed.\n");
			exit(1);
		}

		// OK, forget it from now on - it should clean up it self, the alsa thread...
	} else {
		// No MIDI support!
	}

}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	AlsaInstance *instance = (AlsaInstance *)void_info;
	DYNLIB_DEBUG("Trying to get controller %s, in group %s\n",
	       name, group);
	if(instance->type == 0 && strcmp("volume", name) == 0)
		return &(instance->volume);
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	AlsaInstance *instance = (AlsaInstance *)data;
	if(instance->type == 0)
		execute_sink(instance, mt);
	else
		return; /* NO MIDI SUPPORT YET */
}

// End ALSA version

#elif defined(USE_PULSE_AUDIO)
// Begin Pulse Audio version

#include <pulse/simple.h>
#include <pulse/error.h>
//#include <pulse/gccmacro.h>

#define PULSE_BUFFER_SIZE 2048
#define PULSE_CHANNELS 2

#define MAX_MIDI_IN 32
/* four megabytes! */
#define MIDI_BUFFER_SIZE 4 * 1024 * 1024
typedef struct _PulseInstanceData {
	pthread_t thread;
	pthread_attr_t pta;
	
	pa_simple *s;

	int pulse_open;
	
	int16_t buffer[PULSE_BUFFER_SIZE * PULSE_CHANNELS];
		
} PulseInstanceData;

PulseInstanceData *instance = NULL;

void *pulse_thread(void *mtable_ptr) {
	int error;
	MachineTable *mt = (MachineTable *)mtable_ptr;       

	for (;;) {
		if(mt->fill_sink(mt) != 0) {
			DYNLIB_DEBUG("fill sink failed.\n");
			sleep(1);
		}
		
		if (pa_simple_write(instance->s,
				    instance->buffer,
				    PULSE_BUFFER_SIZE * PULSE_CHANNELS,
				    &error) < 0) {
			fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
			goto fail;
		}
	}

 fail:
	/* OK, this may win the ugly contest... */
	for (;;) {
		DYNLIB_DEBUG("Pulse has failed, no output.\n");
		sleep(1);					
	}

	return NULL;
}

void execute_sink(MachineTable *mt) {
	SignalPointer *s = mt->get_input_signal(mt, "stereo");

	float *in = mt->get_signal_buffer(s);
	int il = mt->get_signal_samples(s);
	int ic = mt->get_signal_channels(s);	
	int i;
	float im;

	for(i = 0; i < il; i++) {
		im = 32767 * in[i * ic + 0];
		instance->buffer[i * ic + 0] = (int)im;
		im = 32767 * in[i * ic + 1];
		instance->buffer[i * ic + 1] = (int)im;
	}
}

PulseInstanceData *init_pulse(MachineTable *mt) {
	int error;
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = 44100,
		.channels = 2
	};

	/* alloc and clear */
	instance = (PulseInstanceData *)malloc(sizeof(PulseInstanceData));
	if(instance == NULL) return NULL;
	memset(instance, 0, sizeof(PulseInstanceData));

	/* init pulse! */
	if (!(instance->s = pa_simple_new(NULL, "satan", PA_STREAM_PLAYBACK, NULL, "playback",
					  &ss, NULL, NULL, &error))) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		goto failure;
	}

	/* set audio signal defaults */
	mt->set_signal_defaults(mt, _0D, PULSE_BUFFER_SIZE,
				_fl32bit, ss.rate);

	/* OK, create pthread for this */
	pthread_attr_init(&(instance->pta));
		
	if(pthread_create(
		   &(instance->thread),
		   &(instance->pta),
		   pulse_thread,
		   mt) != 0)
		goto failure;
	
	return instance;

 failure:
	if(instance != NULL) free(instance);
	instance = NULL;
	
	return NULL;
}

void *init(MachineTable *mt, const char *name) {
	MachineType *retval;

	DYNLIB_DEBUG("\n\n\n***************CREATING: %s****************\n\n\n", name);
	
	retval = (MachineType *)malloc(sizeof(MachineType));
	if(retval == NULL) return NULL;
	
	if(instance == NULL) {
		/* Allocate and initiate instance data here */
		DYNLIB_DEBUG("Creating new pulse audio instance: %p\n", retval); fflush(0);
		if(init_pulse(mt) == NULL) {
			free(retval);
			return NULL;
		}
		DYNLIB_DEBUG("pulse audio instance created: %p\n", instance); fflush(0);
	}
	
	if(strcmp("liveoutsink", name) == 0) {
		instance->pulse_open = 1;
		*retval = -1;
		return retval;
	} else if(strcmp("liveoutmidi_in", name) == 0) {
		free(retval);
		retval = NULL; /* NO MIDI SUPPORT YET */
		return retval;
	}
	
	return NULL;
}

void delete(void *data) {
	MachineType mtype = *(MachineType *)data;

	if(mtype == -1)
		instance->pulse_open = 0;
	else {
		/* no MIDI support yet */
	}

	/* free MachineType */
	free(data);

	if(instance->pulse_open == 0) {
		/* remove pulse.. */
	}
}

void *get_controller_ptr(MachineTable *mt, void *void_info,
			 const char *name,
			 const char *group) {
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	MachineType mtype = *(MachineType *)data;
	if(mtype == -1)
		execute_sink(mt);
	else
		return; /* NO MIDI SUPPORT YET */
}

// End Pulse Audio version
#elif defined(USE_ANDROID_AUDIO)
// Begin ANDROID

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <sys/time.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <fixedpointmath.h>

typedef struct AndroidInstance_ {
	int (*android_audio_callback)(FTYPE vol, FTYPE *in, int il, int ic);
	void (*android_audio_stop_f)(void);

	MachineTable *mt;

	struct RIFF_WAVE_FILE riff_file;
	
	int doRecord, recordFile;

	int android_open;

	int instance_is_invalid;

	// used for writing audio output to a file
	int16_t *file_output_data;
	int file_output_channels, file_output_samples;
} AndroidInstance;

AndroidInstance *valid_instance = NULL;

int __a_mutex_created = 0;
pthread_mutexattr_t __a_attr;
pthread_mutex_t __a_mutex;

int fill_sink_callback(int callback_status, void *data) {
	AndroidInstance *inst = (AndroidInstance *)data;

	(void) pthread_mutex_lock(&(__a_mutex));

	if(inst->instance_is_invalid) {
		free(inst);
		inst = NULL;
		goto fail_unlock_return;
	}

	MachineTable *mt = NULL;
	mt = inst->mt;

	(void) pthread_mutex_unlock(&(__a_mutex));
	
	switch(callback_status) {
	case _sinkJustPlay:
		if(inst->recordFile != -1) {
			close(inst->recordFile);
			inst->recordFile = -1;
		}
		inst->doRecord = 0;
		break;
	case _sinkRecord:
		if(!inst->doRecord) {
			inst->doRecord = 1;
			char file_name[1024];
			if(mt->get_recording_filename(
				   mt, file_name, sizeof(file_name)) == 0) {
				DYNLIB_DEBUG(" sinkRecord FNAME: ]%s[.\n", file_name); fflush(0);
				if(file_name[0] == '\0') {
					strncpy(file_name, "/mnt/sdcard/SATAN_OUTPUT.WAV", sizeof(file_name));
				}

				RIFF_create_file(&(inst->riff_file), file_name);				
			}
		}
		break;
		
	default:
	case _sinkPaused:
		(void) inst->android_audio_stop_f();
	case _notSink:
	case _sinkException:
		RIFF_close_file(&(inst->riff_file));

		inst->doRecord = 0;
		
		memset(inst->file_output_data,
		       0,
		       sizeof(int16_t) * inst->file_output_channels * inst->file_output_samples);
		
		break;
	}		
	
	RIFF_write_data(
		&(inst->riff_file), inst->file_output_data,
		inst->file_output_samples * inst->file_output_channels * sizeof(int16_t));

	return _sinkCallbackOK;

fail_unlock_return:
	(void) pthread_mutex_unlock(&(__a_mutex));
	return _sinkCallbackFAIL;
}

// this functions is called by the Android thread AndroidAudio::java_thread_loop()
// this function will basically tell the Machine tree to generate all signals
// that will in the end trigger the execute_sink() function, which then calls BACK
// into the AndroidAudio class...
int android_dynamic_machine_entry(void *data)
{
	AndroidInstance *inst = (AndroidInstance *)data;

	if(inst == NULL) goto fail_return;

	(void) pthread_mutex_lock(&(__a_mutex));

	if(inst->instance_is_invalid) {
		free(inst);
		inst = NULL;
		goto fail_unlock_return;
	}
		
	MachineTable *mt = NULL;
	mt = inst->mt;

	(void) pthread_mutex_unlock(&(__a_mutex));

	int status = mt->fill_sink(mt, fill_sink_callback, inst);
	
	return status;
	
fail_unlock_return:
	(void) pthread_mutex_unlock(&(__a_mutex));
fail_return:
	return -1;
	
}

float volume = 0.50;
void execute_sink(MachineTable *mt, AndroidInstance *inst) {
	SignalPointer *s = mt->get_input_signal(mt, "stereo");

	if(s == NULL) return;
	
	FTYPE *in = mt->get_signal_buffer(s);

	int il = mt->get_signal_samples(s);
	int ic = mt->get_signal_channels(s);	

	if(inst->file_output_data == NULL ||
	   il != inst->file_output_samples ||
	   ic != inst->file_output_channels) {
		if(inst->file_output_data != NULL) free(inst->file_output_data);

		inst->file_output_samples = il;
		inst->file_output_channels = ic;
		
		inst->file_output_data = (int16_t *)malloc(
			inst->file_output_channels * inst->file_output_samples * sizeof(int16_t));

		if(inst->file_output_data == NULL) {
			DYNLIB_DEBUG(" WARNING - SATAN FAILED TO ALLOCATE BUFFER FOR DISK OUTPUT.\n"); fflush(0);
			inst->file_output_samples = inst->file_output_channels = 0;
		}
	}
	
	FTYPE vol = ftoFTYPE(volume);

	// XXX check for !0 return (== error!) and do something
	(void) inst->android_audio_callback(vol, in, il, ic);

	// at this point, the android_audio_callback
	// is assumed to have mutliplied each sample with
	// volume factor...
	
	// mix input into disk output buffer
	int i;

	if(inst->file_output_data != NULL) {
		for(i = 0; i < il * ic; i++) {
			int32_t out;

#ifdef FTYPE_IS_FP8P24
			if(in[i] < ftofp8p24(-1.0)) {
				in[i] = ftofp8p24(-1.0);
			}
			if(in[i] >= (fp8p24_t)0x01000000) {
				in[i] = 0x00ffffff;
			}
			
			out = (in[i] << 7);
			out = (out & 0xffff0000) >> 16;
#else
			out = (int32_t)((float)in[i] * 32767.0f);
#endif
			
			inst->file_output_data[i] = (int16_t)out;
		}
	}
}

 
/*
 *
 */

static AndroidInstance *init_android(MachineTable *mt) {
	DYNLIB_DEBUG("AndroidAudio\n");

	AndroidInstance *inst = NULL;
	
	if(!__a_mutex_created) {
		pthread_mutexattr_init(&(__a_attr));
		pthread_mutex_init(&(__a_mutex), &(__a_attr));
		__a_mutex_created = 1;
	}

	(void) pthread_mutex_lock(&(__a_mutex));

	if(!valid_instance) {
		inst = (AndroidInstance *)malloc(sizeof(AndroidInstance));
		if(!inst) goto return_unlock;	

		memset(inst, 0, sizeof(AndroidInstance));
		
		inst->instance_is_invalid = 0;
		inst->doRecord = 0;
		inst->recordFile = -1;
		
		int period_size = 0, rate = 0;
		
		mt->AndroidAudio__SETUP_STUFF(&period_size, &rate,
					      android_dynamic_machine_entry,
					      inst,
					      &(inst->android_audio_callback),
					      &(inst->android_audio_stop_f)
			);
		
		inst->android_open = 1;
		
		/* set audio signal defaults */
 		mt->set_signal_defaults(mt, _0D, period_size, FTYPE_RESOLUTION, rate);	
		/* set midi signal defaults */
		mt->set_signal_defaults(mt, _MIDI, period_size, _PTR, rate);
		
		inst->mt = mt;

		RIFF_prepare(mt, inst, period_size * 2, &(inst->riff_file), rate);
	} else {
		DYNLIB_DEBUG("Trying to create two instances of an Android Audio output, that's not allowed.\n");	
	}
	
return_unlock:
	(void) pthread_mutex_unlock(&(__a_mutex));

        return inst;
}

void cleanup_android(MachineTable *mt) {
	mt->AndroidAudio__CLEANUP_STUFF();
}

void *init(MachineTable *mt, const char *name) {

	if(strcmp("liveoutmidi_in", name) == 0) {
		/* NO MIDI SUPPORT YET */
		return NULL;
	}
	 
	DYNLIB_DEBUG("\n\n\n***************CREATING AUDIOTRACK : %s****************\n\n\n", name);	
	
	/* Init all and android */
	
	AndroidInstance *inst = NULL;
	if((inst = init_android(mt)) == NULL) {
		DYNLIB_DEBUG("android audio instance NOT CREATED\n"); fflush(0);
		return NULL;
	}
	DYNLIB_DEBUG("android audio instance created: %p\n", inst); fflush(0);

	return inst;
}

void delete(void *data) {
	AndroidInstance *inst = (AndroidInstance *)data;
	DYNLIB_DEBUG("  DELETING LIVEOUT 1!\n");  fflush(0);
	cleanup_android(inst->mt);
	DYNLIB_DEBUG("  DELETING LIVEOUT 3!\n");  fflush(0);
	(void) pthread_mutex_lock(&(__a_mutex));

	valid_instance = NULL;
	inst->instance_is_invalid = 1;
	
	(void) pthread_mutex_unlock(&(__a_mutex));
	DYNLIB_DEBUG("  DELETING LIVEOUT 4!\n");  fflush(0);	
}

void *get_controller_ptr(MachineTable *mt, void *ignored,
			 const char *name,
			 const char *group) {
	if(strcmp("volume", name) == 0)
		return &volume;
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	AndroidInstance *inst = (AndroidInstance *)data;
	execute_sink(mt, inst);
}

// End ANDROID version

// Begin Android OpenSL ES implementation
#elif defined(USE_OPEN_SL_ES)

static int __opensl_buffer_queue_size = 5;
static int __opensl_buffer_factor = 2;

#define OPENSL_BUFFER_QUEUE_SIZE_MAX 8
//#define OPENSL_BUFFER_QUEUE_SIZE 3
//#define OPENSL_BUFFER_FACTOR 3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <sys/time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/prctl.h>

#define __DO_DYNLIB_DEBUG
#include "dynlib_debug.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include "opensl_ioX.c"

typedef struct __playBuffer {
	short *data;
} playBuffer_t;

typedef struct OpenSLInstance_ {
	/*
	 * 
	 *
	 */
	MachineTable *mt;

	OPENSL_STREAM *stream;
	int period_size;
	
	FTYPE *empty_buffer;
	short *temp_buffer;
	
	pthread_cond_t signal;
	pthread_mutexattr_t attr;
	pthread_mutex_t mutex;
	pthread_t thread;
	pthread_attr_t pta;
	playBuffer_t playback_buffer[OPENSL_BUFFER_QUEUE_SIZE_MAX];
	int buffer_to_render;
	int buffer_to_play;
	int buffer_segment_to_play;
	int unlock_me;
	
	/**********
	 *
	 * Data used for writing audio output to a file
	 *
	 **********/
	struct RIFF_WAVE_FILE riff_file;
	int doRecord;
	int isPlaying;
	int16_t *file_output_data;
	int file_output_channels, file_output_samples;
	
} OpenSLInstance;

static OpenSLInstance *singleton_instance = NULL;

// finalizes the audio into the temp_buffer, ready for writing when entering the fill_sink_callback()
void finalize_audio(OpenSLInstance *p, FTYPE *buffer, FTYPE vol) { 
	if(p == NULL) return;

	int size = __opensl_buffer_factor * p->stream->outBufSamples;
	
	if(buffer == NULL) {
		buffer = p->empty_buffer;
	}
	
	int i;
	for(i=0; i < size; i++){
		buffer[i] = mulFTYPE(buffer[i], vol);

		int32_t out;

#ifdef FTYPE_IS_FP8P24
		if(buffer[i] < ftofp8p24(-1.0)) {
			buffer[i] = ftofp8p24(-1.0);
		}
		if(buffer[i] >= (fp8p24_t)0x01000000) {
			buffer[i] = 0x00ffffff;
		}
		
		out = (buffer[i] << 7);
		out = (out & 0xffff0000) >> 16;
#else
		out = (int32_t)((float)buffer[i] * 32767.0f);
#endif

		p->temp_buffer[i] = (short)out;
	}
}

OpenSLInstance *init_OpenSL(MachineTable *mt) {
	int period_size = 44100 / 40, rate = 44100;

	int playback_mode = mt->AndroidAudio__get_native_audio_configuration_data(&rate, &period_size);
	DYNLIB_DEBUG("   -------------- playback mode: %s\n", playback_mode == __PLAYBACK_OPENSL_BUFFERED ? "OpenSL Buffered" : "OpenSL Direct");
	DYNLIB_DEBUG("   -------------- rate %d\n", rate);
	DYNLIB_DEBUG("   -------------- period size %d\n", period_size);

	float rate_f = (float)rate;
	float period_size_f = (float)period_size;
	int number_of_buffers = 2;

	switch(playback_mode) {
	case __PLAYBACK_OPENSL_BUFFERED:
	{
		int minimum_configuration_found = 0;
		
		do {
			float total = period_size_f * ((float)__opensl_buffer_queue_size) * ((float)__opensl_buffer_factor) *
				(1 / rate_f);
			if(total > 0.08 && (__opensl_buffer_queue_size > 2) && (__opensl_buffer_factor > 1)) {
				if(__opensl_buffer_queue_size > 2) {
					__opensl_buffer_queue_size--;
				} else {
					__opensl_buffer_factor--;
				}			
			} else {
				minimum_configuration_found = -1;
			}
		} while(!minimum_configuration_found);
	}
	break;

	case __PLAYBACK_OPENSL_DIRECT:
	{
		__opensl_buffer_factor = 1;
		__opensl_buffer_queue_size = 1;
	}
	break;

	default:
		DYNLIB_INFORM("    init_OpenSL() - unknown playback mode: %d\n", playback_mode);
		exit(0);
	}
	
	DYNLIB_DEBUG(" ------ FINAL - total buffer: %d (factor: %d, queue: %d)\n",
		     period_size * __opensl_buffer_queue_size * __opensl_buffer_factor,
		     __opensl_buffer_factor,
		     __opensl_buffer_queue_size);
	
	if((period_size_f / rate_f) < LOW_LATENCY_THRESHOLD) {
		DYNLIB_DEBUG("  -- Enabling low latency mode.\n");
		mt->enable_low_latency_mode();
	}
	
	OpenSLInstance *inst = (OpenSLInstance *)malloc(sizeof(OpenSLInstance));
	short *buffers = (short *)malloc(sizeof(short) * 2 * __opensl_buffer_factor * __opensl_buffer_queue_size * period_size); // 2 channels, __opensl_buffer_factor, __opensl_buffer_queue_size times the period size
	FTYPE *_empty_buffer = (FTYPE *)malloc(sizeof(FTYPE) * 2 * __opensl_buffer_factor * period_size); // 2 channels, __opensl_buffer_factor, period size
	
	if((inst != NULL) && (buffers != NULL) && (_empty_buffer != NULL)) {
		memset(buffers, 0, sizeof(short) * 2 * __opensl_buffer_factor * __opensl_buffer_queue_size * period_size);
		memset(inst, 0, sizeof(OpenSLInstance));
		memset(_empty_buffer, 0, sizeof(FTYPE) * 2 * __opensl_buffer_factor * period_size);

		if((inst->stream = android_OpenAudioDevice(rate, 2, period_size, number_of_buffers)) != NULL) {

			/* set audio signal defaults */
			mt->set_signal_defaults(mt, _0D, __opensl_buffer_factor * period_size, FTYPE_RESOLUTION, rate);
			/* set midi signal defaults */
			mt->set_signal_defaults(mt, _MIDI, __opensl_buffer_factor * period_size, _PTR, rate);
			
			RIFF_prepare(mt, inst, __opensl_buffer_factor * period_size * 2, &(inst->riff_file), rate);
		} else {
			// android_OpenAudioDevice() failed..
			free(inst);
			return NULL;
		}
		inst->buffer_to_play = __opensl_buffer_queue_size - 1;
		inst->buffer_segment_to_play = 0;
		inst->buffer_to_render = 0;

		int k;
		for(k = 0; k < __opensl_buffer_queue_size; k++) {
			inst->playback_buffer[k].data = &buffers[2 * k * __opensl_buffer_factor * period_size];
		}
		
		inst->empty_buffer = _empty_buffer;
		inst->temp_buffer = inst->playback_buffer[0].data;

		inst->period_size = period_size;
	} else {
		if(inst) free(inst);
		if(buffers) free(buffers);
		if(_empty_buffer) free(_empty_buffer);

		// make sure the return value is NULL
		inst = NULL;
	}

	return inst;
}

void cleanup_OpenSL(OpenSLInstance *inst) {
	if(inst) {
		android_CloseAudioDevice(inst->stream);
		if(inst->playback_buffer[0].data)
			free(inst->playback_buffer[0].data);
		if(inst->empty_buffer)
			free(inst->empty_buffer);
		free(inst);
	}
}

int fill_sink_callback(int callback_status, void *data) {
	OpenSLInstance *inst = (OpenSLInstance *)data;
	MachineTable *mt = NULL;
	mt = inst->mt;

	if(mt == NULL) {
		// non valid mt, just return...
		return _sinkCallbackOK;
	}
	
	switch(callback_status) {
	case _sinkJustPlay:
		inst->doRecord = 0;
		break;
	case _sinkRecord:
		if(!inst->doRecord) {
			inst->doRecord = 1;
			char file_name[1024];
			if(mt->get_recording_filename(
				   mt, file_name, sizeof(file_name)) == 0) {
				DYNLIB_DEBUG(" sinkRecord FNAME: ]%s[.\n", file_name); fflush(0);
				if(file_name[0] == '\0') {
					strncpy(file_name, "/mnt/sdcard/SATAN_OUTPUT.WAV", sizeof(file_name));
				}

				RIFF_create_file(&(inst->riff_file), file_name);				
			}
		}
		break;

	case _sinkResumed:
		inst->isPlaying = 1;
		return _sinkCallbackOK;
		
	default:
	case _sinkPaused:
	case _notSink:
	case _sinkException:
		inst->isPlaying = 0;
		RIFF_close_file(&(inst->riff_file));

		inst->doRecord = 0;
		
		memset(inst->file_output_data,
		       0,
		       sizeof(int16_t) * inst->file_output_channels * inst->file_output_samples);

		// clear the audio buffer and write it to the output
		(void) finalize_audio(inst, NULL, ftoFTYPE(0.0f));

		return _sinkCallbackOK;
	}		
	
	RIFF_write_data(
		&(inst->riff_file), inst->file_output_data,
		inst->file_output_samples * inst->file_output_channels * sizeof(int16_t));

	return _sinkCallbackOK;
}

#ifdef DO_ANALYZIS

void analyzis_thread(void *non_used) {
	while(1) {
		DYNLIB_DEBUG("current_time: %d - average: %d - worst: %d - faults: %d\n", current_time, average_time, worst_time, faults);
		worst_time = worst_time >> 1;
		usleep(250000);
	}
}

#endif

void openSL_thread_callback_standard(void *data) {
	OpenSLInstance *inst = (OpenSLInstance *)data;

	if(inst->buffer_segment_to_play == __opensl_buffer_factor) {
		inst->buffer_segment_to_play = 0;
		inst->buffer_to_play = (inst->buffer_to_play + 1) % __opensl_buffer_queue_size;
		
		if(inst->unlock_me > 0) {
			pthread_mutex_lock(&(inst->mutex));
			inst->unlock_me--;
			if(inst->unlock_me == 0) {
				pthread_cond_signal(&(inst->signal));
			}
			pthread_mutex_unlock(&(inst->mutex));
		}
	}

	short *bfr = inst->playback_buffer[inst->buffer_to_play].data;
	bfr = &bfr[(inst->stream->outBufSamples) * (inst->buffer_segment_to_play)];
	android_AudioOut(inst->stream, bfr);
	inst->buffer_segment_to_play++;
}

void openSL_thread_callback_minimum_latency(void *data) {
	OpenSLInstance *inst = (OpenSLInstance *)data;	
	MachineTable *mt = inst->mt;

	if(mt != NULL) {
		(void) mt->fill_sink(mt, fill_sink_callback, inst);
	}
	
	if(mt == NULL || inst->isPlaying == 0) {
		// either we are not playing, or
		// we have a non valid mt (i.e. == NULL) we should just finalize using NULL, then write the resulting empty audio.
		(void) finalize_audio(inst, NULL, ftoFTYPE(0.0f));
	}

	android_AudioOut(inst->stream, inst->temp_buffer);
}

void *openSL_feeder_thread(void *data) {
	OpenSLInstance *inst = (OpenSLInstance *)data;	
	MachineTable *mt = NULL;

	DYNLIB_DEBUG("setting thread name, returned: %d\n", prctl(PR_SET_NAME, "openSL_feeder_thread"));
	while(1) {
		mt = inst->mt;

		DYNLIB_DEBUG("openSL_feeder_thread loop...\n");
		if(mt != NULL) {
			(void) mt->fill_sink(mt, fill_sink_callback, inst);
		}
		
		if(mt == NULL || inst->isPlaying == 0) {
			// either we are not playing, or
			// we have a non valid mt (i.e. == NULL) we should just finalize using NULL, then write the resulting empty audio.
			(void) finalize_audio(inst, NULL, ftoFTYPE(0.0f));
		}

		inst->buffer_to_render = (inst->buffer_to_render + 1) % __opensl_buffer_queue_size;

		if(inst->buffer_to_play == inst->buffer_to_render) {
			pthread_mutex_lock(&(inst->mutex));
			inst->unlock_me = 1;
			pthread_cond_wait(&(inst->signal), &(inst->mutex));		
			pthread_mutex_unlock(&(inst->mutex));
		}

		inst->temp_buffer = inst->playback_buffer[inst->buffer_to_render].data;

	}

	// never reached
	return NULL;
}

void *init(MachineTable *mt, const char *name) {

	if(strcmp("liveoutmidi_in", name) == 0) {
		/* NO MIDI SUPPORT YET */
		return NULL;
	}
	 
	DYNLIB_DEBUG("\n\n\n***************CREATING OPEN SL ES: %s****************\n\n\n", name);	
	
	/* Init all and android */

	OpenSLInstance *inst = NULL;

	if(!singleton_instance) {
		if((inst = init_OpenSL(mt)) == NULL) {
			DYNLIB_DEBUG("android OpenSL audio instance NOT CREATED\n"); fflush(0);
			goto failure;
		}

		if(__opensl_buffer_queue_size == 1 &&
		   __opensl_buffer_factor == 1) {
			// minimum latency mode
			android_StartStream(inst->stream, openSL_thread_callback_minimum_latency, inst);
		} else {
			// standard mode
			{ // create feeder thread (the thread that actually generates data, but feeds the openSL which plays it)
				pthread_mutexattr_init(&(inst->attr));
				pthread_mutex_init(&(inst->mutex), &(inst->attr));
				
				pthread_cond_init (&(inst->signal), NULL);
				
				pthread_attr_init(&(inst->pta));
				if(pthread_create(
					   &(inst->thread),
					   &(inst->pta),
					   openSL_feeder_thread,
					   inst) != 0)
					goto failure;
			}
			android_StartStream(inst->stream, openSL_thread_callback_standard, inst);
		}
		
		singleton_instance = inst;
	} else {
		inst = singleton_instance;
	}

	inst->mt = mt;		

#ifdef DO_ANALYZIS
	mt->run_simple_thread(analyzis_thread, NULL);
#endif
	
	DYNLIB_DEBUG("android OpenSL audio instance created: %p\n", inst); fflush(0);

	return inst;

failure:
	if(inst != NULL) {
		cleanup_OpenSL(inst);
	}
	return NULL;
}

void delete(void *data) {
	OpenSLInstance *inst = (OpenSLInstance *)data;

	// invalidate machine table
	inst->mt = NULL;
}

float volume = 0.50f;
void *get_controller_ptr(MachineTable *mt, void *ignored,
			 const char *name,
			 const char *group) {
	if(strcmp("volume", name) == 0)
		return &volume;
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	return; /* nothing to do... */
}

void execute(MachineTable *mt, void *data) {
	OpenSLInstance *inst = (OpenSLInstance *)data;
	FTYPE vol = ftoFTYPE(volume);
	
	SignalPointer *s = mt->get_input_signal(mt, "stereo");

	if(s == NULL) {
		(void) finalize_audio(inst, NULL, vol);
		return;
	}
	
	FTYPE *in = mt->get_signal_buffer(s);

	int il = mt->get_signal_samples(s);
	int ic = mt->get_signal_channels(s);	

	if(inst->file_output_data == NULL ||
	   il != inst->file_output_samples ||
	   ic != inst->file_output_channels) {
		if(inst->file_output_data != NULL) free(inst->file_output_data);

		inst->file_output_samples = il;
		inst->file_output_channels = ic;
		
		inst->file_output_data = (int16_t *)malloc(
			inst->file_output_channels * inst->file_output_samples * sizeof(int16_t));

		if(inst->file_output_data == NULL) {
			DYNLIB_DEBUG(" WARNING - SATAN FAILED TO ALLOCATE BUFFER FOR DISK OUTPUT.\n"); fflush(0);
			inst->file_output_samples = inst->file_output_channels = 0;
		}
	}
	
	// XXX check for !0 return (== error!) and do something
	(void) finalize_audio(inst, in, vol);

	// at this point, the finalize_audio() function
	// is assumed to have mutliplied each sample with
	// volume factor...
	
	// mix input into disk output buffer
	int i;
	if(inst->file_output_data != NULL) {
		for(i = 0; i < il * ic; i++) {
			int32_t out;

#ifdef FTYPE_IS_FP8P24
			if(in[i] < ftofp8p24(-1.0)) {
				in[i] = ftofp8p24(-1.0);
			}
			if(in[i] >= (FTYPE)0x01000000) {
				in[i] = 0x00ffffff;
			}
			
			out = (in[i] << 7);
			out = (out & 0xffff0000) >> 16;
#else
			out = (int32_t)((float)in[i] * 32767.0f);
#endif
			inst->file_output_data[i] = (int16_t)out;
		}
	}
}

#endif
