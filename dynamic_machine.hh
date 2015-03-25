/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
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

#ifndef __DYNAMIC_MACHINE
#define __DYNAMIC_MACHINE

#define DYNAMIC_MACHINE_PROJECT_INTERFACE_LEVEL 2

#include "signal.hh"

#include <kamo_xml.hh>

#include "dynlib/dynlib.h"

#include <string>
#include <map>
#include <vector>
#include <set>
#include <jngldrum/jthread.hh>
#include <iostream>

extern "C" {
#ifdef ANDROID
#include <dlfcn.h>
#else
#include <ltdl.h>
#endif
};

#include "machine.hh"

class DynamicMachine : public Machine {
private:
	
	typedef const char* declare_dynamic (void);
	typedef void* init_dynamic (MachineTable *, const char *);
	typedef void execute_dynamic (MachineTable *, void *);
	typedef void reset_dynamic (MachineTable *, void *);
	typedef void delete_dynamic (void *);
	typedef void* controller_ptr_dynamic (
		MachineTable *, void *, const char *, const char *);

	class ProjectEntry : public SatanProjectEntry {
	public:
		ProjectEntry();
		
		virtual std::string get_xml_attributes();
		virtual void generate_xml(std::ostream &output);
		virtual void parse_xml(int project_interface_level, KXMLDoc &xml_node);
		virtual void set_defaults();

	private:
		static ProjectEntry this_will_register_us_as_a_project_entry;
	};
	
	class HandleSetException {
	public:
		KXMLDoc handle_set;
		std::string base_name;
		
		HandleSetException(std::string &bname, KXMLDoc &hset);
	};
	
	class Handle : public jThread::Monitor {
	private:
		std::string dl_name;
		std::string name;
		std::string hint;
		bool act_as_sink;
		bool dynlib_is_loaded;

#ifdef ANDROID
		void *module;
#else
		lt_dlhandle module;
#endif
		Handle *set_parent; /* if this handle is part of a connected set, this is the parent handle. */

		std::vector<std::string> groups; // vector of group names
		std::vector<std::string> controllers; // vector of controller names
		
		std::map<std::string, std::string> controller_group; // controller to group mapping
		std::map<std::string, std::string> controller_title; // user displayed title for controller
		std::map<std::string, Machine::Signal::Description> output;
		std::map<std::string, Machine::Signal::Description> input;
		std::map<std::string, Machine::Controller::Type> controller_type;
		std::map<std::string, bool> controller_is_FTYPE;
		std::map<std::string, std::string> controller_min;
		std::map<std::string, std::string> controller_max;
		std::map<std::string, std::string> controller_step;
		std::map<std::string, bool> controller_has_midi;
		std::map<std::string, int> coarse_midi_controller;
		std::map<std::string, int> fine_midi_controller;
		std::map<std::string, std::map<int, std::string> > controller_enumnames;

		void parse_iodeclaration(const KXMLDoc &io, int &dimension, int &channels, bool &premix);
		void parse_controller(const KXMLDoc &ctr_xml);
		void parse_machine_declaration(const KXMLDoc &decl);
		void prep_dynlib();

		/* Used when creating a connected set of handles
		 * (handles using the same instance of a dynamic module.)
		 */
		Handle(std::string base_name, KXMLDoc decl, Handle *set_parent);
		/* Used when creating an ordinary handle. */
		Handle(std::string dynlib);
		~Handle();
	public:		
		declare_dynamic *decl;
		init_dynamic *init;
		execute_dynamic *exct;
		reset_dynamic *rset;
		delete_dynamic *delt;
		controller_ptr_dynamic *cptr;
		
		std::string get_name();

		bool is_sink();

		std::map<std::string, Machine::Signal::Description> get_output_descriptions();
		std::map<std::string, Machine::Signal::Description> get_input_descriptions();
		std::vector<std::string> get_controller_groups();
		std::vector<std::string> get_controller_names();
		std::string get_controller_title(const std::string &ctrl);
		std::string get_controller_group(const std::string &ctrl);
		Machine::Controller::Type get_controller_type(const std::string &ctrl);
		bool get_controller_is_FTYPE(const std::string &ctrl);
		std::string get_controller_min(const std::string &ctrl);
		std::string get_controller_max(const std::string &ctrl);
		std::string get_controller_step(const std::string &ctrl);
		std::map<int, std::string> get_controller_enumnames(const std::string &ctrl);

		bool get_controller_has_midi(const std::string &ctrl);
		int get_coarse_midi_controller(const std::string &ctrl);
		int get_fine_midi_controller(const std::string &ctrl);

		std::string get_hint();
		
		/*
		 * Statics
		 *
		 */
	private:
		// a handle is inserted into both maps, one contains the filename of the declaration
		// mapped to the handle, the other contains the machine name (stored in the declaration)
		// mapped to the handle...
		static std::map<std::string, Handle *> declaration2handle;
		static std::map<std::string, Handle *> name2handle;

		static std::string handle_directory;
		
		static void load_handle(std::string fname);
	public:
		static void refresh_handle_set();
		static std::set<std::string> get_handle_set();
		static Handle *get_handle(std::string name);
		static std::string get_handle_hint(std::string name);
	};

	/* Dynamic machine data */
	bool has_failed;
	std::string failure_notice;
	void *dynamic_data;
	const char *module_name;
	MachineTable dt;
	std::map<std::string, void *> controller_ptr;
	
	Handle *dh;
		
	// constructor
	DynamicMachine(Handle *dh, float _xpos, float _ypos);
	DynamicMachine(Handle *dh, const std::string &new_name);

	// setup_dynamic_machine is called from the constructor
	void setup_dynamic_machine();

protected:
	/**** inheritance - virtual ***/
	/// Returns a set of controller groups
	virtual std::vector<std::string> internal_get_controller_groups();	
	/// Returns a set of controller names
	virtual std::vector<std::string> internal_get_controller_names();
	virtual std::vector<std::string> internal_get_controller_names(const std::string &group_name);
	/// Returns a controller pointer (remember to delete it...)
	virtual Controller *internal_get_controller(const std::string &name);
	/// get a hint about what this machine is (for example, "effect" or "generator")
	virtual std::string internal_get_hint();
	
	virtual void fill_buffers();
	virtual void reset();
	virtual bool detach_and_destroy();
	
	// used to get a XML serialized version of
	// the sequence object
	virtual std::string get_class_name();
	virtual std::string get_descriptive_xml();

	virtual ~DynamicMachine();
public:

	std::string get_handle_name();
	
	/*
	 * static functions and data
	 *
	 */
private:
	static jThread::Mutex static_lock; // lock this when you access static stuff...

	static std::map<std::string, Handle *> handle_map; // global handle data
	static std::map<std::string, Handle *> dynamic_file_handle;

public:
	/// returns the set of registered machine handles
	static std::set<std::string> get_handle_set();

	/// return the hint from a handle
	static std::string get_handle_hint(std::string handle);
	
	/// refreshes the set of registered machine handles
	static void refresh_handle_set();
	
	/// create a dynamic machine instance from a handle
	static Machine *instance(const std::string &dynamic_machine_handle, float xpos = 0.0, float ypos = 0.0);

	/// create a new dynamic machine instance using XML data
	static void instance_from_xml(const KXMLDoc &dyn_xml);

	/* NOTE! This is for _ALL_ functions below this comment.
	 *
	 * called from the dynamic library itself, do not call these from within satan
	 *
	 */
private:
	static int fill_sink(MachineTable *mt,
			     int (*fill_sink_callback)(int status, void *cbd),
			     void *callback_data);

	static void enable_low_latency_mode();
	static void disable_low_latency_mode();
	
	static void set_signal_defaults(MachineTable *mt,
					int dim,
					int len, int res,
					int frequency);	
	static SignalPointer *get_output_signal(MachineTable *, const char *);
	static SignalPointer *get_input_signal(MachineTable *, const char *);
	static SignalPointer *get_next_signal(MachineTable *, SignalPointer *);
	static SignalPointer *get_static_signal(int index);
	
	static int get_signal_dimension(SignalPointer *);
	static int get_signal_channels(SignalPointer *);
	static int get_signal_resolution(SignalPointer *);
	static int get_signal_frequency(SignalPointer *);
	static int get_signal_samples(SignalPointer *);
	static void *get_signal_buffer(SignalPointer *);

	static void run_simple_thread(void (*thread_function)(void *), void *);
	
	static int get_recording_filename(
		struct _MachineTable *, char *dst, unsigned int len); 
	
	// inform the system about an internal failure
	static void register_failure(void *machine_instance, const char *);

	static void get_math_tables(
		float **sine_table,
		FTYPE (**__pow_fun)(FTYPE x, FTYPE y),
		FTYPE **sine_table_FTYPE
		);

	// KISS FFT interface
	static kiss_fftr_cfg prepare_fft(int samples, int inverse_fft);
	static void do_fft(kiss_fftr_cfg cfg, FTYPE *timedata, kiss_fft_cpx *freqdata);
	static void inverse_fft(kiss_fftr_cfg cfg, kiss_fft_cpx *freqdata, FTYPE *timedata);
};

#endif
