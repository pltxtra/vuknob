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

#ifndef CLASS_MACHINE_SEQUENCER
#define CLASS_MACHINE_SEQUENCER

#include <stdint.h>

#include <jngldrum/jthread.hh>
#include "machine.hh"
#include "readerwriterqueue/readerwriterqueue.h"

#include "midi_export.hh"

#include <iostream>
#include <fstream>
#include <queue>

// Each machine with a "midi" input will automatically
// be attached to a MachineSequencer machine (invisible from the
// user interface...) The MachineSequencer will be "tightly connected"
// to the "midi"-machine which means that it will be automatically
// destroyed when the "midi"-machine is destroyed.

// I intentionaly kept it at 28 bits, so don't change it..
#define MAX_SEQUENCE_LENGTH (0x0fffffff)
#define NOTE_NOT_SET -1
#define LOOP_NOT_SET -1
#define MACHINE_SEQUENCER_MIDI_INPUT_NAME "midi"
#define MACHINE_SEQUENCER_MIDI_OUTPUT_NAME "midi_OUTPUT"
#define MACHINE_SEQUENCER_INITIAL_SEQUENCE_LENGTH 16
#define MACHINE_SEQUENCER_MAX_EDITABLE_LOOP_OFFSET 4096

#define MAX_PAD_FINGERS 5
#define MAX_PAD_CHORD 6
#define PAD_HZONES 8
#define PAD_VZONES 8

#define MAX_ACTIVE_NOTES 16
#define MAX_BUILTIN_ARP_PATTERNS 6
#define MAX_ARP_PATTERN_LENGTH 16

// PAD_TIME(line,tick)
#define PAD_TIME(A,B) ((int)(((A << BITS_PER_LINE) & (0xffffffff & (0xffffffff << BITS_PER_LINE))) | (B)))
#define PAD_TIME_LINE(A) (A >> BITS_PER_LINE)
#define PAD_TIME_TICK(B) (B & (((0xffffffff << BITS_PER_LINE) & 0xffffffff) ^ 0xffffffff))

struct ltstr {
	bool operator()(std::string s1, std::string s2) const {
		return strcmp(s1.c_str(), s2.c_str()) < 0;
	}
};

typedef void (*MachineSequencerSetChangeCallbackF_t)(void *);

typedef struct _MidiEventChain {
	uint8_t data[sizeof(size_t) + sizeof(uint8_t) * 4];
	char separator_a[9];
	struct _MidiEventChain *next_in_chain;
	struct _MidiEventChain *last_in_chain;
	char separator_b[9];
} MidiEventChain;


class MachineSequencer : public Machine {
	/***** OK - REAL WORK AHEAD *****/

public:
	/*
	 * exception classes
	 *
	 */	
	class NoFreeLoopsAvailable : public std::exception {
	public:
		virtual const char* what() const noexcept {
			return "The application cannot allocate more loops for this machine.";
		};
	};

	class NoSuchLoop : public std::exception {
	public:
		virtual const char* what() const noexcept {
			return "A bad loop id was used.";
		};
	};

	// declare comming classes
	class Loop;
	class Pad;
	class Arpeggiator;
	class NoteEntry;
	
private:
	
	class MidiEventChainControler {
	private:
		static MidiEventChain *separation_zone;
		static MidiEventChain *next_free_midi_event;
		
		static void init_free_midi_event_chain();
	public:
		static void check_separation_zone();

		/// please note - the MidiEvent pointers here are really a hack
		/// and really point to MidiEventChain objects.
		///
		/// Do not atempt to push a MidiEvent that was NOT retrieved using pop
		/// in this class. It will epicly FAIL.
		static MidiEvent *pop_next_free_midi(size_t size);
		static void push_next_free_midi(MidiEvent *_element);

		/// external stacks of events allocated from the main stack
		/// same warning here - do not play around with MidiEvent pointers
		/// that you got from external allocators - use only ones received from pop_next_free_midi()
		static MidiEvent *get_chain_head(MidiEventChain **queue);
		static void chain_to_tail(MidiEventChain **chain, MidiEvent *event);
		static void join_chains(MidiEventChain **destination, MidiEventChain **source);
	};

	class MidiEventBuilder {
	private:
		void **buffer;
		int buffer_size;
		int buffer_position;
		
		// chain of remaining midi events that need to be
		// transmitted ASAP
		MidiEventChain *remaining_midi_chain;

		// chain of freeable midi events that can be returned
		// to the main stack
		MidiEventChain *freeable_midi_chain;

		void chain_event(MidiEvent *mev);
		void process_freeable_chain();
		void process_remaining_chain();

		
	public:
		MidiEventBuilder();
		
		void use_buffer(void **buffer, int buffer_size);
		void skip_to(int new_buffer_position);
		int tell();
		
		virtual void queue_note_on(int note, int velocity, int channel = 0);
		virtual void queue_note_off(int note, int velocity, int channel = 0);
		virtual void queue_controller(int controller, int value, int channel = 0);
	};

	class PadMidiExportBuilder : public MidiEventBuilder {
	private:
		Loop *loop;
		std::map<int, NoteEntry *> active_notes;
		int export_tick;
	public:
		int current_tick;
		
		PadMidiExportBuilder(int loop_offset, Loop *loop);
		~PadMidiExportBuilder();
		
		virtual void queue_note_on(int note, int velocity, int channel = 0);
		virtual void queue_note_off(int note, int velocity, int channel = 0);
		virtual void queue_controller(int controller, int value, int channel = 0);

		void step_tick();
	};
	
public:
	class NoSuchController : public jException {
	public:
		NoSuchController(const std::string &name);
	};
	
	enum PadEvent_t {
		ms_pad_press, ms_pad_slide, ms_pad_release, ms_pad_no_event
	};
	
	class NoteEntry {
	public:
		uint8_t channel, // only values that are within (channel & 0x0f)
			program, // only values that are within (program & 0x7f)
			velocity,// only values that are within (velocity & 0x7f)
			note; // only values that are within (note & 0x7f)

		// note on position and length in the loop, encoded with PAD_TIME(line,tick)
		int on_at, length;
		
		// double linked list of notes in a Loop object
		// please observe that the last note in the loop does _NOT_ link
		// to the first to actually create a looping linked list..
		NoteEntry *prev;
		NoteEntry *next;

		// playback counters
		int ticks2off;
		
		NoteEntry(const NoteEntry *original);
		NoteEntry();
		virtual ~NoteEntry();

		void set_to(const NoteEntry *new_values);
	};

	class ControllerEnvelope {
	private:
		// playback data
		int t; // last processed t value
		std::map<int,int>::iterator next_c; // next control point
		std::map<int,int>::iterator previous_c; // previous control point

		
		
		// control_point maps t to y, where
		// t = PAD_TIME(line,tick)
		// y = 14 bit value (coarse + fine, as defined by the MIDI standard)
		// coarse value = (y & 0x7f80) >> 7; fine value = (y & 0x007f)
		std::map<int, int> control_point;

		void refresh_playback_data();

	public:
		// factual data
		bool enabled;
		int controller_coarse, controller_fine;

		void process_envelope(int tick, MidiEventBuilder *_meb);

		ControllerEnvelope();
		ControllerEnvelope(const ControllerEnvelope *orig);

		// replace the control points of this envelope with the ones from other
		void set_to(const ControllerEnvelope *other);

		// set a control point at PAD_TIME(line,tick) to y (either change an exisint value, or set a new control point)
		void set_control_point(int line, int tick, int y);

		// create a line between two control points, eliminating any control point between them
		void set_control_point_line(
			int first_line, int first_tick, int first_y,
			int second_line, int second_tick, int second_y
			);

		// delete a range of control points
		void delete_control_point_range(
			int first_line, int first_tick,
			int second_line, int second_tick
			);

		// delete an existing control point at PAD_TIME(line,tick), silently fail if no point at t exists.
		void delete_control_point(int line, int tick);

		// get a const reference to the content of this envelope
		const std::map<int, int> &get_control_points();

		void write_to_xml(const std::string &id, std::ostringstream &stream); 
		std::string read_from_xml(const KXMLDoc &env_xml);	
	};

	class PadConfiguration {
	private:
		PadConfiguration();
	public:
		enum PadMode {
			pad_normal = 0,
			pad_arpeggiator = 1
		};

		enum ChordMode {
			chord_off = 0,
			chord_triad = 1
		};
		
		PadConfiguration(PadMode mode, int scale, int octave);
		PadConfiguration(const PadConfiguration *config);
		
		PadMode mode;
		ChordMode chord_mode;
		int scale, octave, arpeggio_pattern;

		 // if coarse == -1 default to using pad to set velocity.. otherwise pad will set the assigned controller
		int pad_controller_coarse, pad_controller_fine;

		void set_coarse_controller(int c);
		void set_fine_controller(int c);		
		void set_mode(PadMode mode);
		void set_arpeggio_pattern(int arp_pattern);
		void set_chord_mode(ChordMode mode);
		void set_scale(int scale);
		void set_octave(int octave);

		void get_configuration_xml(std::ostringstream &stream);
		void load_configuration_from_xml(const KXMLDoc &pad_xml);

		static std::vector<std::string> get_scale_names();
		static std::vector<int> get_scale_key_names(const std::string &scale_name);
	};

private:
	class LoopCreator {
	public:
		static Loop *create(const KXMLDoc &loop_xml);
		static Loop *create();
	};

	class PadEvent {
	public:
		PadEvent();
		PadEvent(int finger_id, PadEvent_t t, int x, int y);

		PadEvent_t t;
		int finger, x, y;
	};

	class PadMotion : public PadConfiguration {
	private:
		int index; // when replaying a motion, this is used to keep track of where we are..
		int crnt_tick; // number of ticks since we started this motion 
		
		int last_chord[MAX_PAD_CHORD];
		int last_x;
		
		std::vector<int> x;
		std::vector<int> y;
		std::vector<int> t; // relative number of ticks from the start_tick

		int start_tick;
		
		bool terminated, to_be_deleted;

		// notes should be an array with the size of MAX_PAD_CHORD
		// unused entries will be marked with a -1
		void build_chord(int *scale_data, int scale_offset, int *notes, int pad_column);
		
	public:
		void get_padmotion_xml(int finger, std::ostringstream &stream);
		
		PadMotion(Pad *parent_config,
			  int sequence_position, int x, int y);

		// used to parse PadMotion xml when using project level < 5
		PadMotion(Pad *parent_config,
			  int &start_offset_return,
			  const KXMLDoc &motion_xml);		

		// used to parse PadMotion xml when using project level >= 5
		PadMotion(Pad *parent_config,
			  const KXMLDoc &motion_xml);
		
		void quantize();
		void add_position(int x, int y);
		void terminate();
		static void can_be_deleted_now(PadMotion *can_be_deleted);
		static void delete_motion(PadMotion *to_be_deleted);		

		// returns true if the motion could start at the current position
		bool start_motion(int session_position);

		// resets a currently playing motion
		void reset();
		
		// returns true if the motion finished playing
		bool process_motion(MidiEventBuilder *_meb, Arpeggiator *arpeggiator);

		PadMotion *prev, *next;

		static void record_motion(PadMotion **head, PadMotion *target);
	};

	class PadFinger {
	private:
		bool to_be_deleted;
		
	public:
		Pad *parent;
		
		PadMotion *recorded;
		PadMotion *next_motion_to_play;
		
		std::vector<PadMotion *> playing_motions; // currently playing recorded motions
		PadMotion *current; // current user controlled motion		

		PadFinger();
		~PadFinger();

		void start_from_the_top();

		void process_finger_events(const PadEvent &pe, int session_position);

		// Returns true if we've completed all the recorded motions for this finger
		bool process_finger_motions(bool do_record, bool mute,
					    int session_position,
					    MidiEventBuilder *_meb, Arpeggiator *arpeggiator,
					    bool quantize);

		void reset();
		
		// Designate a finger object for deletion.
		void delete_pad_finger();

		// Terminate all incomplete motions during recording
		void terminate();
	};
	
public:

#define MAX_ARP_FINGERS 5
	class Arpeggiator {
	public:
		class Note {
		public:
			int on_length, off_length;
			int octave_offset;
			bool slide;

			Note() : on_length(0), off_length(0), octave_offset(0), slide(false) {}
			Note(int _on_length, int _off_length, int _octave_offset, bool _slide) :
				on_length(_on_length), off_length(_off_length), octave_offset(_octave_offset),
				slide(_slide) {}
		};

		class Pattern {
		public:
			int length;
			Note note[MAX_ARP_PATTERN_LENGTH];
			
			Pattern() : length(0) {}
			void append(const Note &nn) {
				// disallow too long patterns
				if(length >= MAX_ARP_PATTERN_LENGTH) return;

				note[length++] = nn; // copy
			}

			static Pattern *built_in;

			static void initiate_built_in();
		};
		
	private:
		// pattern playback
		int phase; // 0 = go to on, 1 = on, 2 = go to off, 3 = off
		int ticks_left, note, velocity;
		int current_finger;
		int pattern_index;
		Pattern *current_pattern;

		// Finger data
		class ArpFinger {
		public:
			int key, velocity, counter; // counter is a reference counter
		};

		int finger_count;
		ArpFinger finger[MAX_ARP_FINGERS];

		void swap_keys(ArpFinger &a, ArpFinger &b);
		void sort_keys();
	public:
		
		Arpeggiator();
		
		int enable_key(int key, int velocity); // returns the key if succesfull, -1 if not
		void disable_key(int key);

		void process_pattern(bool mute, MidiEventBuilder *_meb);

		void set_pattern(int id);

		void reset();
	};
	
	class Loop {
	private:
		// during playback this is used to keep track of the
		// current playing line in the loop
		int loop_position;

		/******** notes *******/
		NoteEntry *note;                      // all notes
		NoteEntry *next_note_to_play;         // short cut to the next note to play
		NoteEntry *active_note[MAX_ACTIVE_NOTES];

		// let the loop creator create objects of our class
		friend class LoopCreator;
		// let the pad midi export builder insert notes using the internal functions
		friend class PadMidiExportBuilder;
		
		Loop(const KXMLDoc &loop_xml);
		void process_note_on(bool mute, MidiEventBuilder *_meb);
		void process_note_off(MidiEventBuilder *_meb);
		
		NoteEntry *internal_delete_note(const NoteEntry *net);
		void internal_update_note(const NoteEntry *original, const NoteEntry *new_entry);
		const NoteEntry *internal_insert_note(NoteEntry *new_entry);
		NoteEntry *internal_clear();
		NoteEntry *internal_insert_notes(NoteEntry *new_notes);

		bool activate_note(NoteEntry *);
		void deactivate_note(NoteEntry *);
	public:
		Loop();
		~Loop();
		
		void get_loop_xml(std::ostringstream &stream, int id);
		
		void start_to_play();

		void process(bool mute, MidiEventBuilder *meb);

		const NoteEntry *notes_get() const;
		
		void note_delete(const NoteEntry *net);
		void note_update(const NoteEntry *original, const NoteEntry *new_entry);
		const NoteEntry *note_insert(const NoteEntry *new_entry);
		void clear_loop();
		void copy_loop(const Loop *source);
	};

	class PadSession {
	private:
		bool to_be_deleted, terminated;
		
	public:
		PadSession(Pad *parent, int start_at_tick, bool terminated);
		PadSession(Pad *parent, int start_at_tick, PadMotion *padmot, int finger_id);
		PadSession(Pad *parent, const KXMLDoc &ps_xml);
		
		PadFinger finger[MAX_PAD_FINGERS];

		int start_at_tick;
		int playback_position;
		bool in_play;

		bool start_play(int crnt_tick);
		
		// if this object has been designated for deletion, this function will return true when all currently playing
		// motions has been completed. If this returns true, you can delete this object.
		bool process_session(bool do_record, bool mute,
				     Arpeggiator *arpeggiator,
				     MidiEventBuilder *_meb,
				     bool quantize);

		void reset();

		// Designate a session object for deletion.
		void delete_session();

		// Indicate that the recording ended, terminate all open motions in each finger
		void terminate();

		void get_session_xml(std::ostringstream &stream);
	};
		
	class Pad : public PadConfiguration {
	private:
		friend class PadMotion;
		
		int pad_width, pad_height;
		moodycamel::ReaderWriterQueue<PadEvent> *padEventQueue;
		
		PadSession *current_session;
		std::vector<PadSession *>recorded_sessions;
		
		bool do_record;
		bool do_quantize;

		void process_events(int tick);
		void process_motions(bool mute, int tick, MidiEventBuilder *_meb);

		void internal_set_record(bool do_record);
		void internal_clear_pad();
		
	public: // not really public... (not lock protected..)
		// please - don't call this from anything else but inside MachineSequencer class function...
		void process(bool mute, int tick, MidiEventBuilder *_meb);
		void get_pad_xml(std::ostringstream &stream);
		void load_pad_from_xml(int project_interface_level, const KXMLDoc &pad_xml);

		void export_to_loop(int start_tick, int stop_tick, Loop *loop);
		
		void reset();

		Arpeggiator arpeggiator;
		
	public: // actually public (lock protected)
		void set_pad_resolution(int width, int height);
		void enqueue_event(int finger_id, PadEvent_t t, int x, int y);		
		void set_record(bool do_record);
		void set_quantize(bool do_quantize);
		void clear_pad();

		Pad();
		~Pad();
	};
	
	/*************
	 *
	 * Public interface
	 *
	 *************/
public:
	// id == -1 for "no entry"
	// id >= 0 for valid entry
	void get_loop_ids_at(int *position_vector, int length);
	int get_loop_id_at(int position);
	void set_loop_id_at(int position, int loop_id);
	Loop *get_loop(int loop_id);
	int get_nr_of_loops();
	int add_new_loop(); // returns id of new loop
	void delete_loop(int id);

	bool is_mute();
	void set_mute(bool muted);
	
       	// used to list controllers that can be accessed via MIDI
	std::set<std::string> available_midi_controllers();
	// assign the pad to a specific MIDI controller - if "" or if the string does not match a proper MIDI controller
	// the pad will be assigned to velocity
	// if the pad is assigned to a midi controller, the velocity will default to full amplitude
	void assign_pad_to_midi_controller(const std::string &);

	Pad *get_pad();
	
	void set_pad_arpeggio_pattern(const std::string identity);
	void set_pad_chord_mode(PadConfiguration::ChordMode pconf);
	int export_pad_to_loop(int loop_id = LOOP_NOT_SET);

	const ControllerEnvelope *get_controller_envelope(const std::string &name) const;
	void update_controller_envelope(const ControllerEnvelope *original, const ControllerEnvelope *new_entry);

	/// returns a pointer to the machine this MachineSequencer is feeding.
	Machine *get_machine();
	
	/*************
	 *
	 * Inheritance stuff
	 *
	 *************/
protected:
	/// Returns a set of controller groups
	virtual std::vector<std::string> internal_get_controller_groups();
	
	/// Returns a set of controller names
	virtual std::vector<std::string> internal_get_controller_names();
	virtual std::vector<std::string> internal_get_controller_names(const std::string &group_name);
	
	/// Returns a controller pointer (remember to delete it...)
	virtual Controller *internal_get_controller(const std::string &name);

	/// get a hint about what this machine is (for example, "effect" or "generator") 
	virtual std::string internal_get_hint();

	virtual bool detach_and_destroy();
	
	// Additional XML-formated descriptive data.
	// functions, Machine::*, like get_base_xml_description()
	virtual std::string get_class_name();
	virtual std::string get_descriptive_xml();

	virtual void fill_buffers();

	// reset a machine to a defined state
	virtual void reset(); 

	/*************
	 *
	 * Dynamic class/object stuff
	 *
	 *************/
private:
	/* stuff related to the fill_buffers() function and sub-functions */
	Pad pad;
	MidiEventBuilder _meb;
	Loop *current_loop;
	
	void prep_empty_loop_sequence();
	int internal_get_loop_id_at(int position);
	void internal_get_loop_ids_at(int *position_vector, int length);
	void internal_set_loop_id_at(int position, int loop_id);
	int internal_add_new_loop();
	void get_loops_xml(std::ostringstream &stream);
	void get_loop_sequence_xml(std::ostringstream &stream);
	
	std::set<std::string> internal_available_midi_controllers();
	
	/*************
	 *
	 * controller envelopes - the envelopes start at global line 0 and continue as long as they have control
	 *                        points. It's one envelope for each controller that supports MIDI. The envelopes
	 *                        are not stored in the loops, so you have one for the whole sequence
	 *
	 *****/
	void create_controller_envelopes();
	void process_controller_envelopes(int current_tick, MidiEventBuilder *_meb);
	std::map<std::string, ControllerEnvelope *> controller_envelope; // a map of controller names to their envelopes
	
	/* rest of it */
	bool mute; // don't start to play new tones..
	std::string sibling_name; // this MachineSequencer generates events to a specific sibling
	Loop **loop_store;
	int loop_store_size, last_free_loop_store_position;

	void double_loop_store();
	
	int *loop_sequence;
	int loop_sequence_length;

	void refresh_length();

	virtual ~MachineSequencer();
	MachineSequencer(int project_interface_level, const KXMLDoc &machine_xml, const std::string &name);
	MachineSequencer(const std::string &name_root);

	// get the coarse and fine midi controller id:s for a named controller
	void get_midi_controllers(const std::string &name, int &coarse, int &fine);

	// write MIDI information track to file
	static void write_MIDI_info_track(MidiExport *target);
	// returns the nr of loops exported (= midi tracks)
	int export_loops2midi_file(MidiExport *target);
	// will export entire sequence as one track, returns true if the sequence contains data, false if empty.
	bool export_sequence2midi_file(MidiExport *target);

	/****************************
	 *
	 *    Static private interface and data
	 *
	 ****************************/
private:
	// static book-keeping
	static std::vector<MachineSequencer *> machine_from_xml_cache; // this cache is used for temporary storing machine sequencer objects until all machines in the xml file has been loaded. After that we can connect machine sequencers to their siblings.
	static std::map<Machine *, MachineSequencer *> machine2sequencer;
	static int sequence_length;
	static std::vector<MachineSequencerSetChangeCallbackF_t> set_change_callback;
	
	// tell all listeners something was updated
	static void inform_registered_callbacks_async();
	
	// Set the length of the sequences of ALL MachineSequencer objects.
	// To keep everything synchronized there is no
	// public interface to set a specific machine sequencer's sequence
	// length.
	static void set_sequence_length(int new_size);

	/****************************
	 *
	 *    Static public interface
	 *
	 ****************************/
public:
	static int quantize_tick(int start_tick); // helper, quantize a tick value into the closest absolute line
	
	static void presetup_from_xml(int project_interface_level, const KXMLDoc &machine_xml);
	static void finalize_xml_initialization();
	
	static void create_sequencer_for_machine(Machine *m);
	static MachineSequencer *get_sequencer_for_machine(Machine *m);
	static std::map<std::string, MachineSequencer *, ltstr> get_sequencers();

	static std::vector<std::string> get_pad_arpeggio_patterns();
	
	/// register a call-back that will be called when the MachineSequencer set is changed
	static void register_change_callback(
		void (*callback_f)(void *));

	/// This will export the entire sequence to a MIDI, type 2, file
	static void export2MIDI(bool just_loops, const std::string &output_path);
	
};

#endif
