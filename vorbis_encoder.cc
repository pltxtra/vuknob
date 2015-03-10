/* THIS ENCODER IS A MODIFIED VERSION OF THE encoder_example.c FILE
 * THAT IS PART OF THE libvorbis PACKAGE.
 *
 * FOR LICENSE INFORMATION READ COPYING_XIPH, NOT COPYING
 *
 * Modifications are COPYRIGHT (C) 2011 by Anton Persson
 * Licensed with the same license as the original encoder_example.c file
 *
 * Enjoy!
 *
 */

/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2007             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: simple example encoder
 last mod: $Id: encoder_example.c 16946 2010-03-03 16:12:40Z xiphmont $

********************************************************************/

/* takes a stereo 16bit 44.1kHz WAV file from stdin and encodes it into
   a Vorbis bitstream */

/* Note that this is POSIX, not ANSI, code */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <vorbis/vorbisenc.h>
#include <iostream>

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#define READ 1024
signed char readbuffer[READ*4+44]; /* out of the data segment, not the stack */

int vorbis_encoder(FILE *input_file, FILE *output_file,
		   const std::string &title,
		   const std::string &artist,
		   const std::string &genre
	){
	ogg_stream_state os; /* take physical pages, weld into a logical
				stream of packets */
	ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
	ogg_packet       op; /* one raw packet of data for decode */

	vorbis_info      vi; /* struct that stores all the static vorbis bitstream
				settings */
	vorbis_comment   vc; /* struct that stores all the user comments */

	vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
	vorbis_block     vb; /* local working space for packet->PCM decode */

	uint32_t sample_rate; // this is read from the RIFF/WAVE file
	uint32_t byte_rate; // this is read from the file
	
	int eos=0,ret;
	
	/*
	 * We expect a 44 byte block of data before the
	 * PCM encoded audio data.
	 */
	if(fread(readbuffer,1,44,input_file) != 44) {
		return -1;
	}
	unsigned char *riff_ptr = (unsigned char *)readbuffer;
	int failure = 0;
	/* verify wav coherency */
	if(!(
		   (riff_ptr[0x00] == 'R') &&
		   (riff_ptr[0x01] == 'I') &&
		   (riff_ptr[0x02] == 'F') &&
		   (riff_ptr[0x03] == 'F')
		   )) {
		failure = -2;
		SATAN_DEBUG_("NO RIFF");
	}
	if(!(
		   (riff_ptr[0x08] == 'W') &&
		   (riff_ptr[0x09] == 'A') &&
		   (riff_ptr[0x0a] == 'V') &&
		   (riff_ptr[0x0b] == 'E')

		   )) {
		failure = -2;
		SATAN_DEBUG_("NO WAVE");
	}
	if(!(
		   (riff_ptr[0x0c] == 'f') &&
		   (riff_ptr[0x0d] == 'm') &&
		   (riff_ptr[0x0e] == 't') &&
		   (riff_ptr[0x0f] == ' ') 

		   )) {
		failure = -2;
		SATAN_DEBUG_("NO fmt");
	}
	if(!(
		   (riff_ptr[0x10] == 0x10) && // "fmt " size is always 16, 0x00000010
		   (riff_ptr[0x11] == 0x00) &&
		   (riff_ptr[0x12] == 0x00) &&
		   (riff_ptr[0x13] == 0x00) 

		   )) {
		failure = -2;
		SATAN_DEBUG_("wrong header size");
	}
	if(!(
		   (riff_ptr[0x14] == 0x01) && // this means PCM
		   (riff_ptr[0x15] == 0x00) 
		
		   )) {
		failure = -2;
		SATAN_DEBUG_("NO PCM");
	}
	if(!(
		   (riff_ptr[0x16] == 0x02) && // this means two channels
		   (riff_ptr[0x17] == 0x00) 

		   )) {
		failure = -2;
		SATAN_DEBUG_("NO stereo");
	}
	
	sample_rate =
		((riff_ptr[0x18] <<  0) & 0x000000ff) |
		((riff_ptr[0x19] <<  8) & 0x0000ff00) |
		((riff_ptr[0x1a] << 16) & 0x00ff0000) |
		((riff_ptr[0x1b] << 24) & 0xff000000); 
	byte_rate =
		((riff_ptr[0x1c] <<  0) & 0x000000ff) |
		((riff_ptr[0x1d] <<  8) & 0x0000ff00) |
		((riff_ptr[0x1e] << 16) & 0x00ff0000) |
		((riff_ptr[0x1f] << 24) & 0xff000000); 

	if(byte_rate != (sample_rate * 2 * 2)) {
		failure = -2;
		SATAN_DEBUG_("Sample rate and byte rate do not match (%x %x, %x, %x)",
			     riff_ptr[0x1c],
			     riff_ptr[0x1d],
			     riff_ptr[0x1e],
			     riff_ptr[0x1f]);
	}
	if(!(
		   (riff_ptr[0x20] == 0x04) && // encode align 0x0004 (num channels * num bits per sample / 8)
		   (riff_ptr[0x21] == 0x00) 
		
		   )) {
		failure = -2;
		SATAN_DEBUG_("NO align");
	}
	if(!(
		   (riff_ptr[0x22] == 0x10) &&  // bits per sample is 16 (0x0010)
		   (riff_ptr[0x23] == 0x00) 

		   )) {
		failure = -2;
		SATAN_DEBUG_("NO 16 bit per sample");
	}
	if(!(
		   (riff_ptr[0x24] == 'd') &&
		   (riff_ptr[0x25] == 'a') &&
		   (riff_ptr[0x26] == 't') &&
		   (riff_ptr[0x27] == 'a')
		   )) {
		failure = -2;
		SATAN_DEBUG_("NO data");
	}

	if(failure != 0) return -2;
	
	/********** Encode setup ************/

	vorbis_info_init(&vi);

	/* choose an encoding mode.  A few possibilities commented out, one
	   actually used: */

	/*********************************************************************
   Encoding using a VBR quality mode.  The usable range is -.1
   (lowest quality, smallest file) to 1. (highest quality, largest file).
   Example quality mode .4: 44kHz stereo coupled, roughly 128kbps VBR

   ret = vorbis_encode_init_vbr(&vi,2,44100,.4);

   ---------------------------------------------------------------------

   Encoding using an average bitrate mode (ABR).
   example: 44kHz stereo coupled, average 128kbps VBR

   ret = vorbis_encode_init(&vi,2,44100,-1,128000,-1);

   ---------------------------------------------------------------------

   Encode using a quality mode, but select that quality mode by asking for
   an approximate bitrate.  This is not ABR, it is true VBR, but selected
   using the bitrate interface, and then turning bitrate management off:

   ret = ( vorbis_encode_setup_managed(&vi,2,44100,-1,128000,-1) ||
           vorbis_encode_ctl(&vi,OV_ECTL_RATEMANAGE2_SET,NULL) ||
           vorbis_encode_setup_init(&vi));

	*********************************************************************/

	ret=vorbis_encode_init_vbr(&vi,2,sample_rate,0.75);

	/* do not continue if setup failed; this can happen if we ask for a
	   mode that libVorbis does not support (eg, too low a bitrate, etc,
	   will return 'OV_EIMPL') */

	if(ret) return 1;

	/* add a comment */
	vorbis_comment_init(&vc);
	vorbis_comment_add_tag(&vc,"ENCODER","VuKNOB");
	vorbis_comment_add_tag(&vc,"TITLE", title.c_str());
	vorbis_comment_add_tag(&vc,"ARTIST", artist.c_str());
	vorbis_comment_add_tag(&vc,"GENRE", genre.c_str());
	
	/* set up the analysis state and auxiliary encoding storage */
	vorbis_analysis_init(&vd,&vi);
	vorbis_block_init(&vd,&vb);

	/* set up our packet->stream encoder */
	/* pick a random serial number; that way we can more likely build
	   chained streams just by concatenation */
	srand(time(NULL));
	ogg_stream_init(&os,rand());

	/* Vorbis streams begin with three headers; the initial header (with
	   most of the codec setup parameters) which is mandated by the Ogg
	   bitstream spec.  The second header holds any comment fields.  The
	   third header holds the bitstream codebook.  We merely need to
	   make the headers, then pass them to libvorbis one at a time;
	   libvorbis handles the additional Ogg bitstream constraints */

	{
		ogg_packet header;
		ogg_packet header_comm;
		ogg_packet header_code;

		vorbis_analysis_headerout(&vd,&vc,&header,&header_comm,&header_code);
		ogg_stream_packetin(&os,&header); /* automatically placed in its own
						     page */
		ogg_stream_packetin(&os,&header_comm);
		ogg_stream_packetin(&os,&header_code);

		/* This ensures the actual
		 * audio data will start on a new page, as per spec
		 */
		while(!eos){
			int result=ogg_stream_flush(&os,&og);
			if(result==0)break;
			fwrite(og.header,1,og.header_len,output_file);
			fwrite(og.body,1,og.body_len,output_file);
		}

	}

	while(!eos){
		long i;
		long bytes=fread(readbuffer,1,READ*4,input_file); /* stereo hardwired here */

		if(bytes==0){
			/* end of file.  this can be done implicitly in the mainline,
			   but it's easier to see here in non-clever fashion.
			   Tell the library we're at end of stream so that it can handle
			   the last frame and mark end of stream in the output properly */
			vorbis_analysis_wrote(&vd,0);

		}else{
			/* data to encode */

			/* expose the buffer to submit data */
			float **buffer=vorbis_analysis_buffer(&vd,READ);

			/* uninterleave samples */
			for(i=0;i<bytes/4;i++){
				buffer[0][i]=((readbuffer[i*4+1]<<8)|
					      (0x00ff&(int)readbuffer[i*4]))/32768.f;
				buffer[1][i]=((readbuffer[i*4+3]<<8)|
					      (0x00ff&(int)readbuffer[i*4+2]))/32768.f;
			}

			/* tell the library how much we actually submitted */
			vorbis_analysis_wrote(&vd,i);
		}

		/* vorbis does some data preanalysis, then divvies up blocks for
		   more involved (potentially parallel) processing.  Get a single
		   block for encoding now */
		while(vorbis_analysis_blockout(&vd,&vb)==1){

			/* analysis, assume we want to use bitrate management */
			vorbis_analysis(&vb,NULL);
			vorbis_bitrate_addblock(&vb);

			while(vorbis_bitrate_flushpacket(&vd,&op)){

				/* weld the packet into the bitstream */
				ogg_stream_packetin(&os,&op);

				/* write out pages (if any) */
				while(!eos){
					int result=ogg_stream_pageout(&os,&og);
					if(result==0)break;
					fwrite(og.header,1,og.header_len,output_file);
					fwrite(og.body,1,og.body_len,output_file);

					/* this could be set above, but for illustrative purposes, I do
					   it here (to show that vorbis does know where the stream ends) */

					if(ogg_page_eos(&og))eos=1;
				}
			}
		}
	}

	/* clean up and exit.  vorbis_info_clear() must be called last */

	ogg_stream_clear(&os);
	vorbis_block_clear(&vb);
	vorbis_dsp_clear(&vd);
	vorbis_comment_clear(&vc);
	vorbis_info_clear(&vi);

	/* ogg_page and ogg_packet structs always point to storage in
	   libvorbis.  They're never freed or manipulated directly */

	return 0;
}
