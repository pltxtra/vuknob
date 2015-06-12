/*
 * VuKNOB
 * Copyright (C) 2014 by Anton Persson
 *
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

#ifndef __MACHINE
#define __MACHINE

#define MACHINE_PROJECT_INTERFACE_LEVEL 7
#define SIGNAL_PROJECT_INTERFACE_LEVEL 7

#include "signal.hh"

#include <kamo_xml.hh>

#include "dynlib/dynlib.h"

#include <string>
#include <map>
#include <vector>
#include <set>
#include <memory>
#include <jngldrum/jthread.hh>
#include <iostream>
#include <functional>

#include "readerwriterqueue/readerwriterqueue.h"
#include "async_operations.hh"
#include "satan_project_entry.hh"
#include "time_measure.hh"

class Machine;

#define BITS_PER_LINE 4
#define MACHINE_TICKS_PER_LINE (1 << BITS_PER_LINE)
#define MACHINE_LINE_BITMASK 0xffffff0
#define MACHINE_TICK_BITMASK 0x000000f
#define MAX_STATIC_SIGNALS 256

typedef std::function<void(int)> __MACHINE_PERIODIC_CALLBACK_F;
typedef void __MACHINE_OPERATION_CALLBACK;

class Machine {
public:
	class Controller;
	class StaticSignalLoader;

	/*
	 * exception classes
	 *
	 */
	class ParameterOutOfSpec : public std::runtime_error {
		public:
			ParameterOutOfSpec() : runtime_error("Internal parameter outside of acceptable scope.") {}
			virtual ~ParameterOutOfSpec() {}
		};

private:

	// this class will make sure that all Machine global data
	// and specific Machine objects will be loaded from and saved to Satan project files
	class ProjectEntry : public SatanProjectEntry {
	public:
		ProjectEntry();

		virtual std::string get_xml_attributes();
		virtual void generate_xml(std::ostream &output);
		virtual void set_defaults();
		virtual void parse_xml(int project_interface_level, KXMLDoc &xml_node);

	private:
		static ProjectEntry this_will_register_us_as_a_project_entry;

		static void parse_machine(int project_interface_level, const KXMLDoc &machine_x);
		static void parse_machines(int project_interface_level, const KXMLDoc &satanproject);
		static void parse_connection_entry(
			const std::string machine_name,
			const KXMLDoc &con);
		static void parse_connections_entry(const KXMLDoc &cons);
		static void parse_connections_entries(const KXMLDoc &satanproject);
		static void parse_static_signals(const KXMLDoc &satanproject);

	};

	friend class ProjectEntry;
	friend class Controller;
	friend class MachineSequencer;

public:

	class MachineSetListener {
	public:
		virtual void project_loaded() = 0;

		virtual void machine_registered(Machine *m_ptr) = 0;
		virtual void machine_unregistered(Machine *m_ptr) = 0;

		virtual void machine_input_attached(Machine *source, Machine *destination,
						    const std::string &output_name,
						    const std::string &input_name) = 0;
		virtual void machine_input_detached(Machine *source, Machine *destination,
						    const std::string &output_name,
						    const std::string &input_name) = 0;

		void machine_dereference(Machine *m_ptr); // call this from your MachineSetListener if you don't intend to use the m_ptr anymore
	};

	class Controller {
	public:
		enum Type {
			c_int,
			c_float,
			c_bool,
			c_string,
			c_enum,
			c_sigid
		};


	private:
		friend class Machine;

		std::string name;
		std::string title;
		Type type;
		void *ptr;
		int min_i, max_i, step_i;
		float min_f, max_f, step_f;

		bool float_is_FTYPE;

		std::map<int, std::string> enumnames;

		bool has_midi_ctrl;
		int coarse_controller, fine_controller;

		Controller(
			Type tp,
			const std::string &name, // internal name
			const std::string &title, // displayed to user
			void *ctrl_ptr,
			const std::string &min,
			const std::string &max,
			const std::string &step,
			const std::map<int, std::string> enumvalues,
			bool _float_is_FTYPE);

		// fine_controller < 0 means no fine controller used
		void set_midi_controller(int coarse_controller, int fine_controller);

		void internal_get_value(int *val);
		void internal_get_value(float *val);
		void internal_get_value(bool *val);
		void internal_get_value(std::string *str);

		void internal_set_value(int *val);
		void internal_set_value(float *val);
		void internal_set_value(bool *val);
		void internal_set_value(std::string *val);

		static __MACHINE_OPERATION_CALLBACK CALL_internal_get_value_int(void *p);
		static __MACHINE_OPERATION_CALLBACK CALL_internal_get_value_float(void *p);
		static __MACHINE_OPERATION_CALLBACK CALL_internal_get_value_bool(void *p);
		static __MACHINE_OPERATION_CALLBACK CALL_internal_get_value_string(void *p);

		static __MACHINE_OPERATION_CALLBACK CALL_internal_set_value_int(void *p);
		static __MACHINE_OPERATION_CALLBACK CALL_internal_set_value_float(void *p);
		static __MACHINE_OPERATION_CALLBACK CALL_internal_set_value_bool(void *p);
		static __MACHINE_OPERATION_CALLBACK CALL_internal_set_value_string(void *p);

	public:
		std::string get_name();
		std::string get_title();
		Type get_type();

		void get_min(float &val);
		void get_max(float &val);
		void get_step(float &val);
		void get_min(int &val);
		void get_max(int &val);
		void get_step(int &val);

		void get_value(int &val);
		void get_value(float &val);
		void get_value(bool &val);
		void get_value(std::string &val);

		std::string get_value_name(const int &val);

		void set_value(int &val);
		void set_value(float &val);
		void set_value(bool &val);
		void set_value(std::string &val);

		// return true if a controller has a MIDI equivalent, if so
		// the coarse_controller integer is set
		// and if a fine controller exists, the fine_controller int is set
		// to verify if a value has been set, make sure you pass a negative (<0) value
		// when calling.
		bool has_midi_controller(int &coarse_controller, int &fine_controller);
	};

protected:

	class SignalBase  {
	protected:
		Dimension dimension;
		Resolution resolution;
		int channels;
		int samples;
		int frequency;

		std::string name;

		void *buffer;

		SignalBase(const std::string &nm);
		/*
		 * private static functions, also used by class Machine
		 *
		 */

		friend class Machine;
		friend class StaticSignalLoader;

		void internal_clear_buffer();

	public:
		Dimension get_dimension();
		Resolution get_resolution();
		int get_channels();
		int get_samples();
		int get_frequency();

		void *get_buffer();

		std::string get_name();

		void clear_buffer();
	};

	class StaticSignal : public SignalBase {
	private:
		// this class will make sure that all Machine global data
		// and specific Machine objects will be loaded from and saved to Satan project files
		class ProjectEntry : public SatanProjectEntry {
		public:
			ProjectEntry();

			virtual std::string get_xml_attributes();
			virtual void generate_xml(std::ostream &output);
			virtual void parse_xml(int project_interface_level, KXMLDoc &xml_node);
			virtual void set_defaults();

			static ProjectEntry this_will_register_us_as_a_project_entry;
		};
		friend class ProjectEntry;

		std::string file_path;

		StaticSignal(
			Dimension d, Resolution r,
			int channels, int samples, int frequency,
			std::string file_path,
			std::string name,
			void *data);

		~StaticSignal();

		static std::map<int, StaticSignal *> signals;

		void save_0D_signal_xml(int index);
		static void load_0D_signal_xml(
			const KXMLDoc &sigxml,
			int index,
			Dimension d, Resolution r,
			int channels, int samples, int frequency,
			std::string file_path,
			std::string name);

		/* loader friend */
		friend class StaticSignalLoader;

		/* called from loaders */
		static void replace_signal(
			int index,
			Dimension d, Resolution r,
			int channels, int samples, int frequency,
			std::string file_path,
			std::string name,
			void *data);

		std::string get_file_path();

		static std::string save_signal_xml(int index);
		static void load_signal_xml(const KXMLDoc &sigxml);
		static void clear_signal(int index);
	public:
		static StaticSignal *get_signal(int index);
		static std::map<int, StaticSignal *> get_all_signals();
	};

	class Signal : public SignalBase {
	public:
	private:
		/* Signal is also a double linked list. */
		std::map<Machine *, Signal *> previous;
		std::map<Machine *, Signal *> next;

		// a map of which machines are listening
		// and a count of how many times the machine
		// is listening..
		std::map<Machine *, int> attached_machines;

		Machine *originator;

		//bool alloc_2d_buffer(Signal *s);

		// "globals"
		static bool initiated;
		static Resolution def_resolution[_MAX_D];
		static int def_samples[_MAX_D];
		static int def_frequency[_MAX_D];
		static std::map<Dimension, std::set<Signal *> > signal_map;

		/*
		 * internal static functions
		 *
		 */

		/// allocates a buffer of dimension 0.
		static void internal_alloc_0d_buffer(Signal *s);
		/// allocates a MIDI buffer.
		static void internal_alloc_MIDI_buffer(Signal *s);

		static void internal_set_defaults(Dimension d, int samples, Resolution r, int frequency);
		static void internal_get_defaults(Dimension d, int &samples, Resolution &r, int &frequency);
		static void internal_register_signal(Signal *s);
		static void internal_deregister_signal(Signal *s);

		/// 0 dimension ("sound")
		Signal(int c, Machine *originator, const std::string &name, Dimension d = _0D);

		/// destructor
		~Signal();

	public:
		class SignalFactory {
		private:
			friend class Machine;

			static Signal *create_signal(int c, Machine *originator, const std::string &name, Dimension d = _0D);
			static void destroy_signal(Signal *signal);
		};

		class Description {
		public:
			Description();
			Description(int d, int c, bool p);
			int dimension;
			int channels;
			bool premix;
		};

		Machine *get_originator();

		// used to indicate that a machine is listening to this signal
		void attach(Machine *m);
		// used to indicate that a machine has stopped listening to this signal
		void detach(Machine *m);

		std::vector<Machine *> get_attached_machines();

		/* linked list functionality */
		void link(Machine *, Signal *);
		Signal *get_next(Machine *);
		Signal *unlink(Machine *, Signal *); // find and unlink the given signal, returns the new head, if the head was detached..

		/*
		 *  Static functions
		 *
		 */
		/// Set the defaults for signals of dimension d
		static void set_defaults(Dimension d, int samples, Resolution r, int frequency);
		static void get_defaults(Dimension d, int &samples, Resolution &r, int &frequency);

	};

public:

	class StaticSignalLoader {
	private:
		std::string identity;

		static std::vector<StaticSignalLoader *>*registered_loaders;

		static void internal_load_signal(int index, const std::string &file_path);

	protected:
		StaticSignalLoader(const std::string &identity);

		virtual bool is_signal_valid(const std::string &fname) = 0;
		virtual void load_static_signal(const std::string &fname, bool only_preview, int static_index) = 0;

		void replace_signal(
			int index,
			Dimension d, Resolution r,
			int channels, int samples, int frequency,
			std::string file_path,
			std::string name,
			void *data);
		void preview_signal(
			Dimension d, Resolution r,
			int channels, int samples, int frequency,
			void *data);

	public:
		static std::map<int, std::string> get_all_signal_names();
		static std::string get_signal_name_for_slot(int index);
		static bool is_valid(const std::string &path);
		static void preview_signal(const std::string &path);
		static void load_signal(
			int index,
			const std::string &file_path);
	};

private:
	DECLARE_TIME_MEASURE_OBJECT(tmes_A);
	DECLARE_TIME_MEASURE_OBJECT(tmes_B);
	DECLARE_TIME_MEASURE_OBJECT(tmes_C);

	// just so no one uses it...
	Machine();

	/* General machine data */
	bool has_been_deregistered = true; // default to true - this will be unset when it's registered
	int reference_counter = 0;
	std::set<Machine *> tightly_connected; // these are machines that should be destroyed when we are destroyed. (Tightly connected machines should be considered "one")
	std::map<Machine *, int> dependant; // machines which have output that we depend on.
	std::string name;
	std::string base_name; /* used to create the default value of name */
	bool base_name_is_name; // indicates that base_name should be use as is
	Machine *next_render_chain;
	std::vector<std::string> controller_groups;

	// if visualized - the graphical x and y position
	float x_position, y_position;

	void destroy_tightly_attached_machines();
	void execute();
	void premix(Signal *result, Signal *head);
	bool find_machine_in_graph(Machine *machine); // this function is used to detect loops
	Signal *get_output(const std::string &name);
	std::string get_controller_xml(); // this function is used to export control values to xml

	/********************************************
	 * Internal Representation of the
	 * Public interface to the Machine object
	 * this should either be called from the
	 * proper audio thread, or properly locked
	 *
	 ********************************************/
private:
	void internal_attach_input(Machine *source_machine,
				   const std::string &output_name,
				   const std::string &input_name);
	void internal_detach_input(Machine *source_machine,
				   const std::string &output_name,
				   const std::string &input_name
		);
	std::string internal_get_base_xml_description();
	std::string internal_get_connection_xml();
	std::multimap<std::string, std::pair<Machine *, std::string> > internal_get_input_connections();
	std::vector<std::string> internal_get_input_names();
	std::vector<std::string> internal_get_output_names();
	int internal_get_input_index(const std::string &input);
	int internal_get_output_index(const std::string &output);
	std::string internal_get_name();
	void internal_set_name(const std::string &nm);

	static __MACHINE_OPERATION_CALLBACK CALL_internal_get_base_xml_description(void *p);
	static __MACHINE_OPERATION_CALLBACK CALL_internal_get_connection_xml(void *p);
	static __MACHINE_OPERATION_CALLBACK CALL_internal_get_input_connections(void *p);
	static __MACHINE_OPERATION_CALLBACK CALL_internal_get_input_names(void *p);
	static __MACHINE_OPERATION_CALLBACK CALL_internal_get_output_names(void *p);
	static __MACHINE_OPERATION_CALLBACK CALL_internal_get_input_index(void *p);
	static __MACHINE_OPERATION_CALLBACK CALL_internal_get_output_index(void *p);
	static __MACHINE_OPERATION_CALLBACK CALL_internal_get_name(void *p);
	static __MACHINE_OPERATION_CALLBACK CALL_internal_set_name(void *p);

	/********************************************
	 *
	 * Public interface to the Machine object
	 *
	 ********************************************/
public:
	void attach_input(Machine *source_machine,
			  const std::string &output_name,
			  const std::string &input_name);
	void detach_input(Machine *source_machine,
			  const std::string &output_name,
			  const std::string &input_name
			  );

	std::string get_base_xml_description();
	std::string get_connection_xml();

	/// returns a multimap of input-connections -> machine/output
	std::multimap<std::string, std::pair<Machine *, std::string> > get_input_connections();
	/// Returns a set of input names
	std::vector<std::string> get_input_names();
	/// Returns a set of output names
	std::vector<std::string> get_output_names();
	/// Returns the index of the named input
	int get_input_index(const std::string &input);
	/// Returns the index of the named output
	int get_output_index(const std::string &output);

	std::string get_name();
	void set_name(const std::string &nm);

	float get_x_position();
	float get_y_position();
	void set_position(float x, float y);

	/// Returns a set of controller groups
	std::vector<std::string> get_controller_groups();
	/// Returns the set of all controller names
	std::vector<std::string> get_controller_names();
	/// Returns the set of all controller names in a given group
	std::vector<std::string> get_controller_names(const std::string &group_name);
	/// Returns a controller pointer (remember to delete it...)
	Controller *get_controller(const std::string &name);
	/// get a hint about what this machine is (for example, "effect" or "generator")
	std::string get_hint();

	/******************************************************************
	 *
	 * a "machine_operation" is an interface that a non audio playback
	 * thread can use to enqueue a task for execution on the playback thread.
	 *
	 ******************************************************************/
public:
	// operation called ONLY by NON audio playback threads
	static void machine_operation_enqueue(std::function<void(void *data)> callback, void *callback_data, bool do_synch);

private:
	// operation ONLY called by audio playback thread
	static void machine_operation_dequeue();

	/***************************************
	 *
	 *  For inheritance!
	 *
	 ***************************************/
protected:

	static jThread::Monitor *machine_space_lock; // lock down the machine space

	std::map<std::string, Signal::Description> input_descriptor;
	std::map<std::string, Signal::Description> output_descriptor;

	// premixed_input is used to mix together the signals connected
	// to the input when it is declared as "premix"
	std::map<std::string, Signal *> premixed_input;
	std::map<std::string, Signal *> input;
	std::map<std::string, Signal *> output;

	// constructor/destructor
	Machine(std::string name_base, bool _base_name_is_name, float _xpos, float _ypos);
	virtual ~Machine();
	// Call this AFTER you have initiated
	// all data in your derived class from the XML block
	void setup_using_xml(const KXMLDoc &mxml);

	// create a new controller handle
	Controller *create_controller(
		Controller::Type tp,
		std::string name,
		std::string title,
		void *ptr,
		std::string min,
		std::string max,
		std::string step,
		std::map<int, std::string> enumnames,
		bool float_is_FTYPE);
	// map a machine controller to a MIDI controller
	// fine_controller < 0 means no fine controller used
	void set_midi_controller(Controller *ctrl, int coarse_controller, int fine_controller);

	// used to tightly connect this machine with Machine *m
	void tightly_connect(Machine *m);
	// detaches all inputs
	void detach_all_inputs(Machine *m = NULL);
	// detaches all outputs
	void detach_all_outputs();

	// get the first tick of the current buffer (updated before each call to filL_buffers)
	int get_next_tick();
	// get the first sequence position of the current buffer (updated before each call to filL_buffers)
	int get_next_sequence_position();
	// get the first tick of the current buffer (updated before each call to fill_buffers)
	int get_next_tick_at(Dimension d);
	// get the number of samples per tick (updtd before each call to fill_buffers to reflect bpm/lpb changes)
	int get_samples_per_tick(Dimension d);
	// get the number of samples per tick, shuffle offset
	int get_samples_per_tick_shuffle(Dimension d);

	// this will detach all connections to/from this
	// machine and then it returns a boolean which
	// tells you if you should delete it or not.
	virtual bool detach_and_destroy() = 0;

	// Additional XML-formated descriptive data.
	// Should only be called from monitor protected
	// functions, Machine::*, like get_base_xml_description()
	virtual std::string get_class_name() = 0;
	virtual std::string get_descriptive_xml() = 0;

	// setup_machine should be called as soon as you have filled the input/output_descriptor pairs.
	void setup_machine();

	virtual void fill_buffers() = 0;

	// reset a machine to a defined state
	virtual void reset() = 0;

	/// Returns a set of controller groups
	virtual std::vector<std::string> internal_get_controller_groups() = 0;
	/// Returns the set of all controller names
	virtual std::vector<std::string> internal_get_controller_names() = 0;
	/// Returns the set of all controller names in a given group
	virtual std::vector<std::string> internal_get_controller_names(const std::string &group_name) = 0;
	/// Returns a controller pointer (remember to delete it...)
	virtual Controller *internal_get_controller(const std::string &name) = 0;
	/// get a hint about what this machine is (for example, "effect" or "generator")
	virtual std::string internal_get_hint() = 0;

	/***************************************
	 *
	 * static functions and data
	 *
	 ***************************************/

	// Activate/deactivate low latency functions
	static void activate_low_latency_mode();
	static void deactivate_low_latency_mode();

	// Registers this machine instance as the sink of the machine network
	static void register_this_sink(Machine *m);

	// Fills this sink...
	static int fill_this_sink(Machine *,
				  int (*fill_sink_callback)(int status, void *cbd), void *callback_data);

	static void set_ssignal_defaults(Machine *m,
					 Dimension dim,
					 int len, Resolution res,
					 int frequency);

	// Async Operations
	static void run_async_operation(AsyncOp *op);
	static void run_async_function(std::function<void()> f);

private:
	static void lock_machine_space();
	static void unlock_machine_space();

private:
#define SHUFFLE_FACTOR_DIVISOR 100
	static int shuffle_factor; // a value in the range [0..SHUFFLE_FACTOR_DIVISOR)

	static int __loop_start, __loop_stop;
	static bool __do_loop;

	static int __current_tick;
	static int __next_sequence_position;
	static int __next_tick_at[];
	static int __samples_per_tick[];
	static int __samples_per_tick_shuffle[];
	static int __bpm;
	static int __lpb;

	static bool low_latency_mode; // if this is true we can do low latency...

	static bool is_loading; // if the system is currently loading a project or not
	static bool is_playing; // if the user has pressed "play" or not.
	static bool is_recording; // should the sink record to file or not?
	static std::string record_fname; // filename to record to

	static Machine *top_render_chain; // whenever a machine is connected to another the chain is recalculated
	static Machine *sink; // There can only be one...
	static std::vector<Machine *>machine_set; // global array of all machines

	static std::set<std::weak_ptr<MachineSetListener>, std::owner_less<std::weak_ptr<MachineSetListener> > > machine_set_listeners;
	static std::vector<__MACHINE_PERIODIC_CALLBACK_F> periodic_callback_set;

	// async operations object
	static AsyncOperations *async_ops;

	/***********************************
	 *
	 * Internal static functions
	 *
	 **********************************/
	// render chain functions
	void push_to_render_chain();
	static void recalculate_render_chain();
	static void render_chain();

	// inform all machine_set_listeners of a attached or detached signal
	static void broadcast_attach(Machine *s, Machine *d, const std::string &output, const std::string &input);
	static void broadcast_detach(Machine *s, Machine *d, const std::string &output, const std::string &input);

	// sink registration
	static void internal_register_sink(Machine *s);
	static void internal_unregister_sink(Machine *s);

	// register a machine in the global set
	static void internal_register_machine(Machine *m);
	// deregister a machine in the global set (will also dereference it)
	static void internal_deregister_machine(Machine *m);
	// dereference a machine
	static void internal_dereference_machine(Machine *m);

	static void calculate_samples_per_tick();
	static void calculate_next_tick_at_and_sequence_position();
	static void trigger_periodic_functions();

	// generate data for the sink, returns 0 on success
	static int internal_fill_sink(int (*fill_sink_callback)(int status, void *cbd), void *callback_data);

	static void internal_disconnect_and_destroy(Machine *m);

	// reset all machines to quiet state
	static void reset_all_machines();

	/*******************************************
	 *
	 * internal representations of external static interface functions
	 * but they can be called from other functions as well - as long as the thread
	 * is either the audio thread - OR the machine_space mutex is taken
	 *
	 *******************************************/
protected:
	static bool internal_get_load_state();
	static void internal_set_load_state(bool is_loading);
	static void internal_get_rec_fname(std::string *fname);
	static Machine *internal_get_by_name(const std::string &name);

	/********************************************
	 *
	 * Singleton/static interface to the Machine network
	 *
	 ********************************************/
public:
	/// called only once from startup thread
	static void prepare_baseline();

	/// Call this to register a function that should be called periodically, where the period is defined by the interval_in_playback_positions parameter
	static void register_periodic(__MACHINE_PERIODIC_CALLBACK_F fp);

	/// Register a machine set listener (will be called on machine registered/unregistered events)
	static void register_machine_set_listener(std::weak_ptr<MachineSetListener> mset_listener);

	/// If you use the MachineSetListener API - you must also use this dereference function when you are no longer interested in a machine
	static void dereference_machine(Machine *m_ptr);

	/// returns the set of registered machines (deprecated)
	static std::vector<Machine *> get_machine_set();

	/// check if we are playing
	static bool is_it_playing();

	/// Set machines into play mode
	static void play();

	/// Stop play mode
	static void stop();

	/// set playback position
	static void jump_to(int position);

	/// rewind playback
	static void rewind();

	/// shuffle settings in the range [0, 100]
	static int get_shuffle_factor();
	static void set_shuffle_factor(int shuffle_factor);

	/// Returns true if the current setup supports low latency features
	static bool get_low_latency_mode();

	/// get loop state and positions
	static bool get_loop_state();
	static int get_loop_start();
	static int get_loop_length();

	/// Hint to machine if we are loading from a file
	/// this can be used to minimize "chatter" from machines, which can make loading faster
	static void set_load_state(bool is_loading);

	/// set loop state and positions
	static void set_loop_state(bool do_loop);
	static void set_loop_start(int line);
	static void set_loop_length(int line);

	/// set BPM (beats per minute)
	static void set_bpm(int bpm);
	/// set LPB (lines per beat)
	static void set_lpb(int bpm);
	/// get BPM (beats per minute)
	static int get_bpm();
	/// get LPB (lines per beat)
	static int get_lpb();

	/// set/get recording status.
	static void set_record_state(bool status);
	static bool get_record_state();
	static void set_record_file_name(std::string file_name);
	static std::string get_record_file_name();

	/// get machine by name
	static Machine *get_by_name(std::string name);

	/// get the sink machine, if any
	static Machine *get_sink();

	/// Destroy a single machine
	static void disconnect_and_destroy(Machine *m);

	/// Destroy ALL machines
	static void destroy_all_machines();

	/// This will end up as a call to exit()
	static void exit_application();
};

#endif
