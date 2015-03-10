/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2012 by Anton Persson
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
 * the GNU General Public License version 2; see COPYING for the complete License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __WHISTLE_ANALYZER
#define __WHISTLE_ANALYZER

#include "wavloader.hh"
#include "machine_sequencer.hh"

class WhistleAnalyzer {
private:
	static void add_note_to_sequence(
		MachineSequencer *mseq, int loop_id,
		int sratei,
		int current_chunk, int current_note, int note_length_in_chunks);
	static void analyze_bsy(void *ppp);
	
public:
	static void analyze(MachineSequencer *mseq, int loop_id, const std::string &input_wave);
};

#endif


