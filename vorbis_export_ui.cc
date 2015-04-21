/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#ifdef ANDROID
//#include <arm_neon.h>
#include <cpu-features.h>
#endif

#include <kamogui.hh>
#include <fstream>
#include <algorithm>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <jngldrum/jinformer.hh>

#include "machine.hh"
#include "vorbis_encoder.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

class BusyExportData {
public:
	FILE *input, *output;

	std::string title, artist, genre;
};

static void busy_export_ogg(void *data) {
	BusyExportData *beo_data = (BusyExportData *)data;
	if(beo_data == NULL) {
		jInformer::inform("Sorry, internal insanity error in ogg export.");
		return;
	}
	
	FILE *input = beo_data->input;
	FILE *output = beo_data->output;

	std::string title, artist, genre;
	title = beo_data->title;
	artist = beo_data->artist;
	genre = beo_data->genre;
	
	delete beo_data;
	
	if(input == NULL || output == NULL) {
		jInformer::inform("Sorry, two counts of internal insanity error in ogg export.");
		return;
	} else {
		int result = vorbis_encoder(
			input, output, title, artist, genre
			);
		
		fclose(input); fclose(output);
		
		switch(result) {
		case 0:
			// A-O-K, nothing to do
			break;
			
		case -1:
			jInformer::inform("Sorry, input file is too short.");
			break;
			
		case -2:
			jInformer::inform("Sorry, input file is not correct.");
			break;
			
		case 1:
			jInformer::inform("Sorry, vorbis init failed.");
			break;
			
		default:
			jInformer::inform("Sorry, OGG encoder returned an error.");
			break;
		}
	}
}

static std::string export_path;

KammoEventHandler_Declare(vorbisExportGui,"ExportWav2Vorbis");

static void yes(void *ignored) {	
	if(Machine::is_it_playing()) {
		KammoGUI::display_notification("Disabled:",
					       "Cannot export vorbis while playback is activated.");
	} else {
		SATAN_DEBUG("wav file: %s\n", Machine::get_record_fname().c_str());
		
		FILE *input = NULL, *output = NULL;
		
		std::string iname = Machine::get_record_file_name();
		iname += ".wav";
		input = fopen(
			iname.c_str(),
			"r");
		
		if(input == NULL) {
			KammoGUI::display_notification(
				"No WAV source found:",
				"Did you record a wav for this project?");
		} else {
			output = fopen(
				export_path.c_str(),
				"w");
			
			if(output == NULL) {
				fclose(input);
				KammoGUI::display_notification(
					"Output failure:",
					"Could not write output");
			} else {
				BusyExportData *beo_data = new BusyExportData();
				if(beo_data == NULL) {
					fclose(input);
					fclose(output);
				} else {					
					beo_data->input = input;
					beo_data->output = output;
					
					static KammoGUI::Entry *title = NULL;
					KammoGUI::get_widget((KammoGUI::Widget **)&title, "saveUI_titleEntry");
					static KammoGUI::Entry *artist = NULL;
					KammoGUI::get_widget((KammoGUI::Widget **)&artist, "saveUI_artistEntry");
					static KammoGUI::Entry *genre = NULL;
					KammoGUI::get_widget((KammoGUI::Widget **)&genre, "saveUI_genreEntry");
					
					if(title && artist && genre) {
						beo_data->title = title->get_text();
						beo_data->artist = artist->get_text();
						beo_data->genre = genre->get_text();
					}
					
					
					KammoGUI::do_busy_work("Exporting OGG...", "This may take a while....",
							       busy_export_ogg, beo_data);
				}
			}
		}
	}
}

static void no(void *ignored) {
	// do not do anything
}


virtual void on_click(KammoGUI::Widget *wid) {
#ifdef ANDROID
	// check if we have neon-FPU, we depend on that for OGG/Vorbis
	uint64_t features;
	features = android_getCpuFeatures();
	if (
		(features & ANDROID_CPU_ARM_FEATURE_ARMv7) == 0 ||
		(features & ANDROID_CPU_ARM_FEATURE_NEON) == 0
		) {
		KammoGUI::display_notification("Disabled:",
					       "Cannot export vorbis on device lacking Neon FPU instructions...");
		return;
	}	
#endif

	export_path = Machine::get_record_file_name() + ".ogg";

	if(export_path == ".ogg") {
		KammoGUI::display_notification(
			"Information",
			"Please save the project one time first...");
		return;
	}

	KammoGUI::ask_yes_no("Do you want to export to this OGG?", export_path,
			     yes, NULL,
			     no, NULL);

}

KammoEventHandler_Instance(vorbisExportGui);

