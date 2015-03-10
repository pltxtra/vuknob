/*
  opensl_io.c:
  Android OpenSL input/output module
  Copyright (c) 2012, Victor Lazzarini
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  * Neither the name of the <organization> nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "opensl_ioX.h"

static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

// creates the OpenSL ES audio engine
static SLresult openSLCreateEngine(OPENSL_STREAM *p)
{
	SLresult result;
	// create engine
	result = slCreateEngine(&(p->engineObject), 0, NULL, 0, NULL, NULL);
	if(result != SL_RESULT_SUCCESS) goto  engine_end;

	// realize the engine 
	result = (*p->engineObject)->Realize(p->engineObject, SL_BOOLEAN_FALSE);
	if(result != SL_RESULT_SUCCESS) goto engine_end;

	// get the engine interface, which is needed in order to create other objects
	result = (*p->engineObject)->GetInterface(p->engineObject, SL_IID_ENGINE, &(p->engineEngine));
	if(result != SL_RESULT_SUCCESS) goto  engine_end;

engine_end:
	return result;
}

// opens the OpenSL ES device for output
static SLresult openSLPlayOpen(OPENSL_STREAM *p)
{
	SLresult result;
	SLuint32 sr = p->sr;
	SLuint32  channels = p->outchannels;

	if(channels){
		// configure audio source
		SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

		switch(sr){

		case 8000:
			sr = SL_SAMPLINGRATE_8;
			break;
		case 11025:
			sr = SL_SAMPLINGRATE_11_025;
			break;
		case 16000:
			sr = SL_SAMPLINGRATE_16;
			break;
		case 22050:
			sr = SL_SAMPLINGRATE_22_05;
			break;
		case 24000:
			sr = SL_SAMPLINGRATE_24;
			break;
		case 32000:
			sr = SL_SAMPLINGRATE_32;
			break;
		case 44100:
			sr = SL_SAMPLINGRATE_44_1;
			break;
		case 48000:
			sr = SL_SAMPLINGRATE_48;
			break;
		case 64000:
			sr = SL_SAMPLINGRATE_64;
			break;
		case 88200:
			sr = SL_SAMPLINGRATE_88_2;
			break;
		case 96000:
			sr = SL_SAMPLINGRATE_96;
			break;
		case 192000:
			sr = SL_SAMPLINGRATE_192;
			break;
		default:
			return -1;
		}
   
		const SLInterfaceID ids[] = {SL_IID_VOLUME};
		const SLboolean req[] = {SL_BOOLEAN_FALSE};
		result = (*p->engineEngine)->CreateOutputMix(p->engineEngine, &(p->outputMixObject), 1, ids, req);
		if(result != SL_RESULT_SUCCESS) goto end_openaudio;

		// realize the output mix
		result = (*p->outputMixObject)->Realize(p->outputMixObject, SL_BOOLEAN_FALSE);
   
		int speakers;
		if(channels > 1) 
			speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
		else speakers = SL_SPEAKER_FRONT_CENTER;
		SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,channels, sr,
					       SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
					       speakers, SL_BYTEORDER_LITTLEENDIAN};

		SLDataSource audioSrc = {&loc_bufq, &format_pcm};

		// configure audio sink
		SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, p->outputMixObject};
		SLDataSink audioSnk = {&loc_outmix, NULL};

		// create audio player
		const SLInterfaceID ids1[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
		const SLboolean req1[] = {SL_BOOLEAN_TRUE};
		result = (*p->engineEngine)->CreateAudioPlayer(p->engineEngine, &(p->bqPlayerObject), &audioSrc, &audioSnk,
							       1, ids1, req1);
		if(result != SL_RESULT_SUCCESS) goto end_openaudio;

		// realize the player
		result = (*p->bqPlayerObject)->Realize(p->bqPlayerObject, SL_BOOLEAN_FALSE);
		if(result != SL_RESULT_SUCCESS) goto end_openaudio;

		// get the play interface
		result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_PLAY, &(p->bqPlayerPlay));
		if(result != SL_RESULT_SUCCESS) goto end_openaudio;

		// get the buffer queue interface
		result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
							    &(p->bqPlayerBufferQueue));
		if(result != SL_RESULT_SUCCESS) goto end_openaudio;

		// register callback on the buffer queue
		result = (*p->bqPlayerBufferQueue)->RegisterCallback(p->bqPlayerBufferQueue, bqPlayerCallback, p);
		if(result != SL_RESULT_SUCCESS) goto end_openaudio;
 
	end_openaudio:
		return result;
	}
	return SL_RESULT_SUCCESS;
}

// close the OpenSL IO and destroy the audio engine
static void openSLDestroyEngine(OPENSL_STREAM *p){

	// destroy buffer queue audio player object, and invalidate all associated interfaces
	if (p->bqPlayerObject != NULL) {
		SLuint32 state = SL_PLAYSTATE_PLAYING;
		(*p->bqPlayerPlay)->SetPlayState(p->bqPlayerPlay, SL_PLAYSTATE_STOPPED);
		while(state != SL_PLAYSTATE_STOPPED)
			(*p->bqPlayerPlay)->GetPlayState(p->bqPlayerPlay, &state);
		(*p->bqPlayerObject)->Destroy(p->bqPlayerObject);
		p->bqPlayerObject = NULL;
		p->bqPlayerPlay = NULL;
		p->bqPlayerBufferQueue = NULL;
		p->bqPlayerEffectSend = NULL;
	}

	// destroy output mix object, and invalidate all associated interfaces
	if (p->outputMixObject != NULL) {
		(*p->outputMixObject)->Destroy(p->outputMixObject);
		p->outputMixObject = NULL;
	}

	// destroy engine object, and invalidate all associated interfaces
	if (p->engineObject != NULL) {
		(*p->engineObject)->Destroy(p->engineObject);
		p->engineObject = NULL;
		p->engineEngine = NULL;
	}

}


// open the android audio device for input and/or output
OPENSL_STREAM *android_OpenAudioDevice(int sr, int outchannels, int bufferframes, int nrbuffers){
	
	OPENSL_STREAM *p;
	p = (OPENSL_STREAM *) malloc(sizeof(OPENSL_STREAM));
	if(p == NULL) return NULL;
	
	memset(p, 0, sizeof(OPENSL_STREAM));
	p->outchannels = outchannels;
	p->sr = sr;
	
	p->outBufSamples = bufferframes * outchannels;
	p->nrbuffers = nrbuffers;
	p->outBufBytes = p->outBufSamples * sizeof(short);

	short *bfr = (short *)calloc(p->outBufSamples * p->nrbuffers, sizeof(short));	
	if(bfr == NULL) {
		free(p);
		return NULL;
	}

	p->buffer = (short **)malloc(sizeof(short *) * p->nrbuffers);
	if(p->buffer == NULL) {
		free(p);
		free(bfr);
	}
	
	int k;
	for(k = 0; k < p->nrbuffers; k++) {
		p->buffer[k] = &bfr[k * p->outBufSamples];
	}

	if(openSLCreateEngine(p) != SL_RESULT_SUCCESS) {
		android_CloseAudioDevice(p);
		return NULL;
	}

	if(openSLPlayOpen(p) != SL_RESULT_SUCCESS) {
		android_CloseAudioDevice(p);
		return NULL;
	}  

	return p;
}

// close the android audio device
void android_CloseAudioDevice(OPENSL_STREAM *p){

	if (p == NULL)
		return;

	openSLDestroyEngine(p);

	if(p->buffer) free(p->buffer);
	
	free(p);
}

//#define DO_ANALYZIS

#ifdef DO_ANALYZIS
int current_time = 0;
int worst_time = 0;
int average_time = 0;
int faults = 0;
#endif

// this callback handler is called every time a buffer finishes playing
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	OPENSL_STREAM *p = (OPENSL_STREAM *) context;

#ifdef DO_ANALYZIS
	SLresult result = SL_RESULT_SUCCESS;
	struct timespec tp_start, tp_end;
	(void) clock_gettime(CLOCK_MONOTONIC, &tp_start);
#endif
	
	p->callBack(p->callBackData);

#ifdef DO_ANALYZIS
	result =
#else
	(void)
#endif
		(*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue, p->buffer[p->bfr], p->outBufBytes);

	p->bfr = (p->bfr + 1) % p->nrbuffers;
	
#ifdef DO_ANALYZIS
	if(result != SL_RESULT_SUCCESS) {
		faults++;
	}
	
	(void) clock_gettime(CLOCK_MONOTONIC, &tp_end);

	if ((tp_end.tv_nsec - tp_start.tv_nsec) < 0) {
		tp_end.tv_sec = tp_end.tv_sec - tp_start.tv_sec - 1;
		tp_end.tv_nsec = 1000000000 + tp_end.tv_nsec - tp_start.tv_nsec;
	} else {
		tp_end.tv_sec = tp_end.tv_sec - tp_start.tv_sec;
		tp_end.tv_nsec = tp_end.tv_nsec - tp_start.tv_nsec;
	}

	// convert not to ms, but to 100x ms
	current_time = (tp_end.tv_nsec / 100000) + (tp_end.tv_sec * 100000);

	average_time = (current_time + average_time) >> 1;
	
	if(worst_time < current_time) worst_time = current_time;
#endif

}

// puts a buffer of size samples to the device
void android_AudioOut(OPENSL_STREAM *p, short *buffer) {
	memcpy(p->buffer[p->bfr], buffer, p->outBufBytes);
}


void android_StartStream(OPENSL_STREAM *p, void (*cb)(void *), void *cbd) {
	p->callBack = cb;
	p->callBackData = cbd;

	DYNLIB_DEBUG("starStream 1\n");
	
	int k;
	for(k = 0; k < p->nrbuffers; k++) {
		DYNLIB_DEBUG("starStream k %d - a\n", k);
		memset(p->buffer[k], 0, p->outBufBytes);
		DYNLIB_DEBUG("starStream k %d - b\n", k);
		(*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue,
						   p->buffer[k],
						   p->outBufBytes);
		DYNLIB_DEBUG("starStream k %d - c\n", k);
	}

	DYNLIB_DEBUG("starStream 2\n");


	// set the player's state to playing
	(void) (*p->bqPlayerPlay)->SetPlayState(p->bqPlayerPlay, SL_PLAYSTATE_PLAYING);

}

void android_ResumeStream(OPENSL_STREAM *p) {
	// set the player's state to playing
	(void) (*p->bqPlayerPlay)->SetPlayState(p->bqPlayerPlay, SL_PLAYSTATE_PLAYING);	
}

void android_PauseStream(OPENSL_STREAM *p) {
	(void) (*p->bqPlayerPlay)->SetPlayState(p->bqPlayerPlay, SL_PLAYSTATE_PAUSED);
}
