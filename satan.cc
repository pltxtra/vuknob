/*
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

#include <sched.h>  
//#define _DONT_DO_GUI

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"
#include "information_catcher.hh"

#include "machine_sequencer.hh"
#include "remote_interface.hh"

extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

#include <iostream>
#include "machine.hh"

using namespace std;

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

//#include <jngldrum/jcommunicator.hh>

//#include "dummy_object.hh"

#include "machine_sequencer.hh"
#include "dynamic_machine.hh"

#include <kamogui.hh>
#include <fstream>

#ifdef HAVE_LTDL
extern "C" {
#include <ltdl.h>
};
#endif

#ifdef HAVE_REALTIME_SUPPORT

int set_realtime_priority(void)
{
	struct sched_param schp;
        /*
         * set the process to realtime privs
         */
        memset(&schp, 0, sizeof(schp));
        schp.sched_priority = sched_get_priority_max(SCHED_FIFO);

        if (sched_setscheduler(0, SCHED_FIFO, &schp) != 0) {
                perror("sched_setscheduler");
                return -1;
        }

        return 0;

}

#endif
	
KammoEventHandler_Declare(MainWindowHandler, "MainWindow:quitnow");

virtual void on_click(KammoGUI::Widget *wid) {
	RemoteInterface::Server::stop_server();
	RemoteInterface::Client::disconnect();
	Machine::exit_application();
}

virtual bool on_delete(KammoGUI::Widget *wid) {
	exit(0);
}

KammoEventHandler_Instance(MainWindowHandler);

//KammoEventHandler_Declaration(LoopHandler, "LoopHandler");
KammoEventHandler_Declare(LoopHandler,
			  "refreshProject:"
			  "decreaseBPM:scaleBPM:increaseBPM:"
			  "decreaseLPB:scaleLPB:increaseLPB:"
			  "decreaseShuffle:scaleShuffle:increaseShuffle"
	);

void refresh_bpm_label(int _new_bpm) {
	static KammoGUI::Label *bpmLabel;
	KammoGUI::get_widget((KammoGUI::Widget **)&bpmLabel, "scaleBPMlabel");
	ostringstream stream;
	stream << "BPM: " << _new_bpm;
	bpmLabel->set_title(stream.str());
}

void refresh_lpb_label(int _new_bpm) {
	static KammoGUI::Label *lpbLabel;
	KammoGUI::get_widget((KammoGUI::Widget **)&lpbLabel, "scaleLPBlabel");
	ostringstream stream;
	stream << "LPB: " << _new_bpm;
	lpbLabel->set_title(stream.str());
}

void refresh_shuffle_label(int _new_shuffle) {
	static KammoGUI::Label *shuffleLabel;
	KammoGUI::get_widget((KammoGUI::Widget **)&shuffleLabel, "scaleShufflelabel");
	ostringstream stream;
	stream << "Shuffle: " << _new_shuffle;
	shuffleLabel->set_title(stream.str());
}

void refresh_all() {
	{
		int bpm = Machine::get_bpm();
		
		static KammoGUI::Scale *bpmScale;
		KammoGUI::get_widget((KammoGUI::Widget **)&bpmScale, "scaleBPM");
		bpmScale->set_value(bpm);
		
		refresh_bpm_label(bpm);
	}
	{
		int lpb = Machine::get_lpb();
		
		static KammoGUI::Scale *lpbScale;
		KammoGUI::get_widget((KammoGUI::Widget **)&lpbScale, "scaleLPB");
		lpbScale->set_value(lpb);

		refresh_lpb_label(lpb);
	}
	{
		int shuffle = (int)(Machine::get_shuffle_factor());
		
		static KammoGUI::Scale *shuffleScale;
		KammoGUI::get_widget((KammoGUI::Widget **)&shuffleScale, "scaleShuffle");
		shuffleScale->set_value(shuffle);

		refresh_shuffle_label(shuffle);
	}
}

virtual void on_user_event(KammoGUI::UserEvent *ue, std::map<std::string, void *> args) {
	refresh_all();
}

virtual void on_init(KammoGUI::Widget *wid) {
	SATAN_DEBUG("on refreshProject - calling refresh_all()\n");
	refresh_all();
	SATAN_DEBUG("on refreshProject - returned from refresh_all()\n");
}

virtual void on_click(KammoGUI::Widget *wid) {
	int changeBPM = 0;
	int changeLPB = 0;
	int changeShuffle = 0;
	if(wid->get_id() == "decreaseBPM") {
		changeBPM--;
	}
	if(wid->get_id() == "increaseBPM") {
		changeBPM++;
	}
	if(wid->get_id() == "decreaseLPB") {
		changeLPB--;
	}
	if(wid->get_id() == "increaseLPB") {
		changeLPB++;
	}
	if(wid->get_id() == "decreaseShuffle") {
		changeShuffle--;
	}
	if(wid->get_id() == "increaseShuffle") {
		changeShuffle++;
	}
	if(changeBPM) {
		int bpm = Machine::get_bpm();

		bpm += changeBPM;

		// we just try and set the new value, in case of an error we just ignore
		try {Machine::set_bpm(bpm);} catch(...) { /* ignore */ }
		// if there was an error we must make sure we have the correct value, so we read it back
		bpm = Machine::get_bpm();

		
		static KammoGUI::Scale *bpmScale;
		KammoGUI::get_widget((KammoGUI::Widget **)&bpmScale, "scaleBPM");
		bpmScale->set_value(bpm);

		refresh_bpm_label(bpm);
	}
	if(changeLPB) {
		int lpb = Machine::get_lpb();

		lpb += changeLPB;

		// we just try and set the new value, in case of an error we just ignore
		try {Machine::set_lpb(lpb);} catch(...) { /* ignore */ }
		// if there was an error we must make sure we have the correct value, so we read it back
		lpb = Machine::get_lpb();
		
		static KammoGUI::Scale *lpbScale;
		KammoGUI::get_widget((KammoGUI::Widget **)&lpbScale, "scaleLPB");
		lpbScale->set_value(lpb);

		refresh_lpb_label(lpb);
	}
	if(changeShuffle) {
		int shuffle = (int)(Machine::get_shuffle_factor());

		shuffle += changeShuffle;

		// we just try and set the new value, in case of an error we just ignore
		try {Machine::set_shuffle_factor(shuffle);} catch(...) { /* ignore */ }
		// if there was an error we must make sure we have the correct value, so we read it back
		shuffle = (int)(Machine::get_shuffle_factor());
		
		static KammoGUI::Scale *shuffleScale;
		KammoGUI::get_widget((KammoGUI::Widget **)&shuffleScale, "scaleShuffle");
		shuffleScale->set_value(shuffle);

		refresh_shuffle_label(shuffle);
	}
}

virtual void on_value_changed(KammoGUI::Widget *wid) {
	int value = (int)((KammoGUI::Scale *)wid)->get_value();
	if(wid->get_id() == "scaleBPM") {
		// we just try and set the new value, in case of an error we just ignore
		try {Machine::set_bpm(value);} catch(...) { /* ignore */ }
		// if there was an error we must make sure we have the correct value, so we read it back
		value = Machine::get_bpm();

		refresh_bpm_label(value);
	}
	if(wid->get_id() == "scaleLPB") {
		// we just try and set the new value, in case of an error we just ignore
		try {Machine::set_lpb(value);} catch(...) { /* ignore */ }
		// if there was an error we must make sure we have the correct value, so we read it back
		value = Machine::get_lpb();

		refresh_lpb_label(value);
	}
	if(wid->get_id() == "scaleShuffle") {
		// we just try and set the new value, in case of an error we just ignore
		try {Machine::set_shuffle_factor(value);} catch(...) { /* ignore */ }
		// if there was an error we must make sure we have the correct value, so we read it back
		value = (int)(Machine::get_shuffle_factor());

		refresh_shuffle_label(value);
	}
}

KammoEventHandler_Instance(LoopHandler);

class Satan {
private:
//	jCommunicator *comm;

	
public:
	void usage() {
		cout << "\n"
		     << "SATAN NG\n"
		     << " version: " << VERSION << "\n"
		     << "\n"
		     << " USAGE:\n"
		     << "   satan [-h] <-u name> [-l] [-c host]\n"
		     << "\n"
		     << " OPTIONS:\n"
		     << "   -h                      : Displays this help text.\n"
		     << "   -u name                 : Use name as identity.\n"
		     << "   -l                      : listen for incomming connections.\n"
		     << "   -c                      : connect to host.\n"
		     << "   -t                      : run test code for distributed object.\n"
		     << "\n\n";
	}

	int main(int argc, char **argv) {
		int ch;
		bool listen = false;
		bool connect = false;
		string connect_to = "";
		string user_name = "";
		bool do_dummy_distributedobject_test = false;
		while ((ch = getopt(argc, argv, "hu:lc:t")) != -1) {
			switch (ch) {
			case 'h':
				usage();
				exit(0);
				break;
			case 'u':
				user_name = string(optarg);
				break;
			case 'l':
				listen = true;
				break;
			case 'c':
				connect = true;
				connect_to = string(optarg);
				break;
			case 't':
				do_dummy_distributedobject_test = true;
				break;
			case '?':
			default:
				usage();
				exit(1);
			}
		}
		if(user_name == "") {
			cout << "No name defined, use option -u.\n";
			exit(1);
		}		

		printf("   listen: %d, connect: %d\n", listen, connect);
		
		/*
		comm = new jCommunicator(user_name, listen);

		if(connect) {
			comm->connect(connect_to);
		}
		*/
#ifndef ANDROID // the UI is already loaded through Android

		// Start kammoGUI
		fstream file;
		file.open("./satanUI.xml", fstream::in);
		if(file.fail()) {
			std::cout << "Failed to open local config.\n";
			sleep(1);
			file.open((std::string(CONFIG_DIR) + "/satanUI.xml").c_str(),
				  fstream::in);
			if(file.fail()) {
				std::cout << "No UI file found.\n";
				exit(1);
			}
		}

		try {
			std::cout << "\n\n*************** REGISTERING GUI ****************\n\n";
			KammoGUI::register_xml(file);			
			std::cout << "\n\n*************** GUI REGISTERED ****************\n\n";
		} catch(jException e) {
			std::cout << "blasted![" << e.message << "]\n";
			exit(1);
		}
		
		file.close();
#endif
		KammoGUI::start(); // this must be called prior to clearing project and calling refreshProject, otherwise any code trying to call run_on_GUI_thread_synchronized() will deadlock...
			
#ifdef ANDROID
		// for android, the gui starts when we return...
		return 0;
#else
		while(1) {
			sleep(1000);
		}
#endif		
	}
};

#ifdef ANDROID

void kamoflage_android_main() {
	char *argv[] = {
		strdup("satan"), strdup("-u"), strdup("kalle")
	};
	int argc = 3;

#ifdef _DROP_STDOUT_STDERR_TO_FILE
	// redirect stdout // stderr to a file since android flagrantry just
	// drops all to /dev/null
	int debug_out;
	debug_out = open(_STDOUT_DROP_FILE,
			 O_CREAT | O_TRUNC | O_WRONLY,
			 S_IRUSR | S_IWUSR);
	if(debug_out == -1) {
		exit(1);
	}

	// replace stdin and stdout fd:s with the fd for the new file
	dup2(debug_out, 1);
	dup2(debug_out, 2);	

	printf("Hello brave new world!\n"); fflush(0);
#endif
	
#else

int main(int argc, char **argv) {
#ifdef HAVE_REALTIME_SUPPORT
	set_realtime_priority();
#endif
	// Initialise libltdl.
	lt_dlinit ();

#endif
	mkdir(DEFAULT_SATAN_ROOT, 0755);
	mkdir(DEFAULT_PROJECT_SAVE_PATH, 0755);
	mkdir(DEFAULT_EXPORT_PATH, 0755);

	InformationCatcher::init();
	
	Satan satan;
	satan.main(argc, argv);

}
