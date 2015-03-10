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

#include "opensl_io2.h"

static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
circular_buffer* create_circular_buffer(int bytes);
int checkspace_circular_buffer(circular_buffer *p, int writeCheck);
int read_circular_buffer_bytes(circular_buffer *p, char *out, int bytes);
int write_circular_buffer_bytes(circular_buffer *p, const char *in, int bytes);
void free_circular_buffer (circular_buffer *p);

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

    // set the player's state to playing
    result = (*p->bqPlayerPlay)->SetPlayState(p->bqPlayerPlay, SL_PLAYSTATE_PLAYING);

    if((p->playBuffer = (short *) calloc(p->outBufSamples, sizeof(short))) == NULL) {
      return -1;
    }

    (*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue, 
				       p->playBuffer,p->outBufSamples*sizeof(short));
 
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
OPENSL_STREAM *android_OpenAudioDevice(int sr, int outchannels, int bufferframes){
  
  OPENSL_STREAM *p;
  p = (OPENSL_STREAM *) malloc(sizeof(OPENSL_STREAM));
  memset(p, 0, sizeof(OPENSL_STREAM));
  p->outchannels = outchannels;
  p->sr = sr;
 
  if((p->outBufSamples  =  bufferframes*outchannels) != 0) {
    if((p->outputBuffer = (short *) calloc(p->outBufSamples, sizeof(short))) == NULL) {
      android_CloseAudioDevice(p);
      return NULL;
    }
  }

  if((p->outrb = create_circular_buffer(p->outBufSamples*sizeof(short)*4)) == NULL) {
      android_CloseAudioDevice(p);
      return NULL; 
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

  if (p->outputBuffer != NULL) {
    free(p->outputBuffer);
    p->outputBuffer= NULL;
  }

  if (p->playBuffer != NULL) {
    free(p->playBuffer);
    p->playBuffer = NULL;
  }

  free_circular_buffer(p->outrb);

  free(p);
}


// this callback handler is called every time a buffer finishes playing
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
  OPENSL_STREAM *p = (OPENSL_STREAM *) context;
  int bytes = p->outBufSamples*sizeof(short);
  read_circular_buffer_bytes(p->outrb, (char *) p->playBuffer,bytes);
  (*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue,p->playBuffer,bytes);
}

// puts a buffer of size samples to the device
void android_AudioOut(OPENSL_STREAM *p, short *buffer) {
	int size = sizeof(short) * p->outBufSamples;
	char *bfr = (char *)buffer;
	int bytes = size * sizeof(short);
	int retry_index = 0;
	
	if(p == NULL  ||  p->outBufSamples ==  0)  return;

	while(size > 0) {
		bytes = write_circular_buffer_bytes(p->outrb, (char *) &bfr[retry_index], size);
		retry_index += bytes;
		size -= bytes;
		if(size > 0) {
			DYNLIB_DEBUG("Buffer underflow\n");
			usleep(1000); // 1000 is just an arbitrary small number for usleep...
		}
	}
}

circular_buffer* create_circular_buffer(int bytes){
  circular_buffer *p;
  if ((p = calloc(1, sizeof(circular_buffer))) == NULL) {
    return NULL;
  }
  p->size = bytes;
  p->wp = p->rp = 0;
   
  if ((p->buffer = calloc(bytes, sizeof(char))) == NULL) {
    free (p);
    return NULL;
  }
  return p;
}

int checkspace_circular_buffer(circular_buffer *p, int writeCheck){
  int wp = p->wp, rp = p->rp, size = p->size;
  if(writeCheck){
    if (wp > rp) return rp - wp + size - 1;
    else if (wp < rp) return rp - wp - 1;
    else return size - 1;
  }
  else {
    if (wp > rp) return wp - rp;
    else if (wp < rp) return wp - rp + size;
    else return 0;
  }	
}

int read_circular_buffer_bytes(circular_buffer *p, char *out, int bytes){
  int remaining;
  int bytesread, size = p->size;
  int i=0, rp = p->rp;
  char *buffer = p->buffer;
  if ((remaining = checkspace_circular_buffer(p, 0)) == 0) {
    return 0;
  }
  bytesread = bytes > remaining ? remaining : bytes;
  for(i=0; i < bytesread; i++){
    out[i] = buffer[rp++];
    if(rp == size) rp = 0;
  }
  p->rp = rp;
  return bytesread;
}

int write_circular_buffer_bytes(circular_buffer *p, const char *in, int bytes){
  int remaining;
  int byteswrite, size = p->size;
  int i=0, wp = p->wp;
  char *buffer = p->buffer;
  if ((remaining = checkspace_circular_buffer(p, 1)) == 0) {
    return 0;
  }
  byteswrite = bytes > remaining ? remaining : bytes;
  for(i=0; i < byteswrite; i++){
    buffer[wp++] = in[i];
    if(wp == size) wp = 0;
  }
  p->wp = wp;
  return byteswrite;
}

void
free_circular_buffer (circular_buffer *p){
  if(p == NULL) return;
  free(p->buffer);
  free(p);
}
