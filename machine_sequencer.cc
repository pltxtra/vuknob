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

#include "machine_sequencer.hh"
#include "dynlib/dynlib.h"

#include <stddef.h>

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/*************************************
 *
 * Exception classes
 *
 *************************************/

MachineSequencer::NoSuchController::NoSuchController(const std::string &name) :
	jException(name + " : no such controller", jException::sanity_error) {}

/*************************************
 *
 * Global free chain of midi events
 *
 *************************************/

#define DEFAULT_MAX_FREE_CHAIN 256
MidiEventChain *MachineSequencer::MidiEventChainControler::next_free_midi_event = NULL;
MidiEventChain *MachineSequencer::MidiEventChainControler::separation_zone = NULL;

#define container_of(ptr, type, member) ({ \
                const typeof( ((type *)0)->member ) *__mptr = (ptr); \
                (type *)( (char *)__mptr - offsetof(type,member) );})

void MachineSequencer::MidiEventChainControler::init_free_midi_event_chain() {
	MidiEventChain *data, *next = NULL;

	data = (MidiEventChain *)malloc(sizeof(MidiEventChain) * (DEFAULT_MAX_FREE_CHAIN + 1));

	if(data == NULL) {
		throw std::bad_alloc();
	}

	memset(data, 0, sizeof(MidiEventChain) * DEFAULT_MAX_FREE_CHAIN);
	separation_zone = &data[DEFAULT_MAX_FREE_CHAIN];

	int k;
	for(k = DEFAULT_MAX_FREE_CHAIN - 1; k >= 0; k--) {
		next_free_midi_event = &data[k];
		next_free_midi_event->next_in_chain = next;

		next = next_free_midi_event;
	}
}

void MachineSequencer::MidiEventChainControler::check_separation_zone() {
	if(separation_zone == NULL) return;
}

inline MidiEvent *MachineSequencer::MidiEventChainControler::pop_next_free_midi(size_t size) {
	if(size > 4) throw MachineSequencer::ParameterOutOfSpec();

	if(next_free_midi_event == NULL) {
		init_free_midi_event_chain();
	}

	MidiEventChain *retval = next_free_midi_event;

	next_free_midi_event = next_free_midi_event->next_in_chain;

	// drop tail from the chain we return
	retval->next_in_chain = NULL;
	retval->last_in_chain = retval; // first is last..

	// we will return a MidiEvent, not the chain pointer
	MidiEvent *mev = (MidiEvent *)(&(retval->data));

	mev->length = size;

	return mev;
}

inline void __get_element(MidiEventChain **element, MidiEvent *_element) {
	*element = (MidiEventChain *)(((char *)_element) - offsetof(MidiEventChain, data));
}

inline void MachineSequencer::MidiEventChainControler::push_next_free_midi(MidiEvent *_element) {
	MidiEventChain *element; __get_element(&element, _element);
//	MidiEventChain *element = container_of(_element, MidiEventChain, data);

	if(next_free_midi_event != NULL) {
		element->last_in_chain = next_free_midi_event->last_in_chain;
	} else {
		element->last_in_chain = element;
	}

	element->next_in_chain = next_free_midi_event;
	next_free_midi_event = element;
}

MidiEvent *MachineSequencer::MidiEventChainControler::get_chain_head(MidiEventChain **queue) {
	if((*queue) == NULL) return NULL;

	MidiEventChain *chain = *queue;

	MidiEvent *mev = (MidiEvent *)((*queue)->data);


	if((*queue)->next_in_chain != NULL)
		(*queue)->next_in_chain->last_in_chain = (*queue)->last_in_chain;

	(*queue) = (*queue)->next_in_chain;
	chain->next_in_chain = NULL;
	chain->last_in_chain = chain;

	return mev;
}

void MachineSequencer::MidiEventChainControler::chain_to_tail(MidiEventChain **chain, MidiEvent *event) {
	if(event == NULL) return;
	MidiEventChain *source; __get_element(&source, event);
	join_chains(chain, &source);
}

void MachineSequencer::MidiEventChainControler::join_chains(MidiEventChain **destination, MidiEventChain **source) {
	if(*source == NULL) return;
	// if destination is empty it's easy
	if(*destination == NULL) {
		*destination = *source;
		*source = NULL;
		return;
	}

	// otherwise it's a bit more work...
	MidiEventChain *last_entry = (*destination)->last_in_chain;

	// OK, chain source to last entry in destination, set last_in_chain to new last, then clear source pointer
	last_entry->next_in_chain = *source;
	(*destination)->last_in_chain = (*source)->last_in_chain;
	*source = NULL;

}

/*************************************
 *
 * Class MachineSequencer::MidiEventBuilder
 *
 *************************************/

MachineSequencer::MidiEventBuilder::MidiEventBuilder() : remaining_midi_chain(NULL), freeable_midi_chain(NULL) {}

void MachineSequencer::MidiEventBuilder::chain_event(MidiEvent *mev) {
	if(buffer_position >= buffer_size) {
		MidiEventChainControler::chain_to_tail(&(remaining_midi_chain), mev);
	} else {
		buffer[buffer_position++] = mev;
		MidiEventChainControler::chain_to_tail(&(freeable_midi_chain), mev);
	}
}

void MachineSequencer::MidiEventBuilder::process_freeable_chain() {
	while(freeable_midi_chain != NULL) {
		MidiEvent *mev = MidiEventChainControler::get_chain_head(&freeable_midi_chain);
		MidiEventChainControler::push_next_free_midi(mev);
	}
}

void MachineSequencer::MidiEventBuilder::process_remaining_chain() {
	while(buffer_position < buffer_size && remaining_midi_chain != NULL) {
		MidiEvent *mev = MidiEventChainControler::get_chain_head(&remaining_midi_chain);

		buffer[buffer_position++] = mev;
		MidiEventChainControler::chain_to_tail(&freeable_midi_chain, mev);
	}
}

void MachineSequencer::MidiEventBuilder::use_buffer(void **_buffer, int _buffer_size) {
	buffer = _buffer;
	buffer_size = _buffer_size;
	buffer_position = 0;

	process_remaining_chain();
	process_freeable_chain();
}

void MachineSequencer::MidiEventBuilder::finish_current_buffer() {
	buffer_size = 0;
	buffer_position = 1;
	buffer = 0;
}

void MachineSequencer::MidiEventBuilder::skip_to(int new_buffer_position) {
	buffer_position = new_buffer_position;
}

int MachineSequencer::MidiEventBuilder::tell() {
	return buffer_position;
}

void MachineSequencer::MidiEventBuilder::queue_midi_data(size_t len, const char *data) {
	size_t offset = 0;

	while(offset < len) {
		switch(data[offset] & 0xf0) {
		case 0x80: // note off
			queue_note_off(data[offset + 1], data[offset + 2], data[offset] & 0x0f);
			offset += 3;
			break;
		case 0x90: // note on
			queue_note_on(data[offset + 1], data[offset + 2], data[offset] & 0x0f);
			offset += 3;
			break;
		case 0xb0: // change controller
			queue_controller(data[offset + 1], data[offset + 2], data[offset] & 0x0f);
			offset += 3;
			break;
		default: // not implemented - skip rest of data
			SATAN_ERROR("MidiEventBuilder::queue_midi_data() - Unimplemented status byte - skip rest of data.\n");
			return;
		}
	}
}

void MachineSequencer::MidiEventBuilder::queue_note_on(int note, int velocity, int channel) {
	MidiEvent *mev = MidiEventChainControler::pop_next_free_midi(3);

	SET_MIDI_DATA_3(
		mev,
		(MIDI_NOTE_ON) | (channel & 0x0f),
		note,
		velocity);

	chain_event(mev);
}

void MachineSequencer::MidiEventBuilder::queue_note_off(int note, int velocity, int channel) {
	MidiEvent *mev = MidiEventChainControler::pop_next_free_midi(3);

	SET_MIDI_DATA_3(
		mev,
		(MIDI_NOTE_OFF) | (channel & 0x0f),
		note,
		velocity);

	chain_event(mev);
}

void MachineSequencer::MidiEventBuilder::queue_controller(int controller, int value, int channel) {
	MidiEvent *mev = MidiEventChainControler::pop_next_free_midi(3);

	SET_MIDI_DATA_3(
		mev,
		(MIDI_CONTROL_CHANGE) | (channel & 0x0f),
		controller,
		value);

	chain_event(mev);
}

/*************************************
 *
 * Class MachineSequencer::PadMidiExportBuilder
 *
 *************************************/

MachineSequencer::PadMidiExportBuilder::PadMidiExportBuilder(int start_tick, Loop *_loop) :
	loop(_loop), export_tick(0), current_tick(start_tick)
{
}

MachineSequencer::PadMidiExportBuilder::~PadMidiExportBuilder() {
	for(auto n : active_notes) {
		delete n.second;
	}
}

void MachineSequencer::PadMidiExportBuilder::queue_note_on(int note, int velocity, int channel) {
	NoteEntry *n = new NoteEntry();

	n->channel = channel;
	n->program = 0;
	n->velocity = velocity;
	n->note = note;
	n->on_at = export_tick;

	if(active_notes.find(note) == active_notes.end()) {
		active_notes[note] = n;
	}
}

void MachineSequencer::PadMidiExportBuilder::queue_note_off(int note, int velocity, int channel) {
	std::map<int, NoteEntry *>::iterator n;
	n = active_notes.find(note);
	if(n != active_notes.end()) {
		(*n).second->length = export_tick - (*n).second->on_at;
		(void) loop->internal_insert_note((*n).second);

		SATAN_DEBUG("(%p) insert note %d at %d\n", loop, (*n).second->note, (*n).second->on_at / 16);

		active_notes.erase(n);

		{
			NoteEntry *top = loop->note;
			while(top) {
				SATAN_DEBUG("   loop has note (%d) at (%d)\n", top->note, top->on_at / 16);
				top = top->next;
			}
		}
	}
}

void MachineSequencer::PadMidiExportBuilder::queue_controller(int controller, int value, int channel) {
	/* ignore, not supported */
}

void MachineSequencer::PadMidiExportBuilder::step_tick() {
	current_tick++;
	export_tick++;
}

/*************************************
 *
 * Class MachineSequencer::NoteEntry
 *
 *************************************/

MachineSequencer::NoteEntry::NoteEntry(const NoteEntry *original) : prev(NULL), next(NULL) {
	this->set_to(original);
}

MachineSequencer::NoteEntry::NoteEntry() : prev(NULL), next(NULL) {
	channel = program = note = 0;
	velocity = 0x7f;
	on_at = length = -1;
}

MachineSequencer::NoteEntry::~NoteEntry() {
}

void MachineSequencer::NoteEntry::set_to(const NoteEntry *original) {
	this->channel = (original->channel & 0x0f);
	this->program = (original->program & 0x7f);
	this->velocity = (original->velocity & 0x7f);
	this->note = (original->note & 0x7f);
	this->on_at = original->on_at;
	this->length = original->length;
}

/*************************************
 *
 * Class MachineSequencer::ControllerEnvelope
 *
 *************************************/

MachineSequencer::ControllerEnvelope::ControllerEnvelope() : enabled(true), controller_coarse(-1), controller_fine(-1) {
}

MachineSequencer::ControllerEnvelope::ControllerEnvelope(const ControllerEnvelope *original) : t(0), enabled(true), controller_coarse(-1), controller_fine(-1) {
	controller_coarse = controller_fine = -1;
	this->set_to(original);
}

void MachineSequencer::ControllerEnvelope::refresh_playback_data() {
	if(control_point.size() == 0) return;

	next_c = control_point.lower_bound(t);
	previous_c = next_c;

	if(next_c == control_point.end() || next_c == control_point.begin()) {
		return;
	}

	previous_c--;
}

// returns true if the controller finished playing
void MachineSequencer::ControllerEnvelope::process_envelope(int t, MidiEventBuilder *_meb) {
	if(!enabled || control_point.size() == 0) {
		return;
	} else if(next_c == control_point.end()) {
		next_c = control_point.lower_bound(t);
		previous_c = next_c;
		if(next_c == control_point.end()) {
			return;
		} else if(next_c != control_point.begin()) {
			previous_c--;
		}
	}

	if((*next_c).first > t && next_c == control_point.begin())
		return;

	int y;

	if((*next_c).first == t) {
		y = (*next_c).second;
		next_c++;
	} else {
		// interpolate
		y = (*previous_c).second + ((((*next_c).second - (*previous_c).second) * (t - (*previous_c).first)) / ((*next_c).first - (*previous_c).first));
	}
	if(controller_coarse != -1) {
		int y_c = (y >> 7) & 0x7f;
		SATAN_DEBUG("[%5x]y: %d (%x) -> coarse: %x\n", t, y, y, y_c);
		_meb->queue_controller(controller_coarse, y_c);
		if(controller_fine != -1) {
			int y_f = y & 0x7f;
			_meb->queue_controller(controller_fine, y_f);
		}
	}
}

void MachineSequencer::ControllerEnvelope::set_to(const ControllerEnvelope *original) {
	control_point = original->control_point;
	enabled = original->enabled;

	refresh_playback_data();
}

void MachineSequencer::ControllerEnvelope::set_control_point(int line, int tick, int y) {
	control_point[PAD_TIME(line, tick)] = y;
	refresh_playback_data();
}

void MachineSequencer::ControllerEnvelope::set_control_point_line(
	int first_line, int first_tick, int first_y,
	int second_line, int second_tick, int second_y) {

	int first_x = PAD_TIME(first_line, first_tick);
	int second_x = PAD_TIME(second_line, second_tick);

	std::map<int, int>::iterator k1, k2;

	k1 = control_point.lower_bound(first_x);
	k2 = control_point.upper_bound(second_x);

	// this code erases all points between the first and the second
	if(k1 != control_point.end()) {
		control_point.erase(k1, k2);
	}

	// then we update the points at first and second
	control_point[first_x] = first_y;
	control_point[second_x] = second_y;

	refresh_playback_data();
}

void MachineSequencer::ControllerEnvelope::delete_control_point_range(
	int first_line, int first_tick,
	int second_line, int second_tick) {

	int first_x = PAD_TIME(first_line, first_tick);
	int second_x = PAD_TIME(second_line, second_tick);

	std::map<int, int>::iterator k1, k2;

	k1 = control_point.lower_bound(first_x);
	k2 = control_point.upper_bound(second_x);

	// this code erases all points between the first and the second
	if(k1 != control_point.end()) {
		control_point.erase(k1, k2);
	}

	refresh_playback_data();
}

void MachineSequencer::ControllerEnvelope::delete_control_point(int line, int tick) {
	std::map<int, int>::iterator k;

	k = control_point.find(PAD_TIME(line, tick));

	if(k != control_point.end()) {
		control_point.erase(k);
	}
	refresh_playback_data();
}

const std::map<int, int> & MachineSequencer::ControllerEnvelope::get_control_points() {
	return control_point;
}

void MachineSequencer::ControllerEnvelope::write_to_xml(const std::string &id, std::ostringstream &stream) {
	stream << "<envelope id=\"" << id << "\" enabled=\"" << (enabled ? "true" : "false") << "\" "
	       << "coarse=\"" << controller_coarse << "\" "
	       << "fine=\"" << controller_fine << "\" >\n";

	std::map<int, int>::iterator k;
	for(k  = control_point.begin();
	    k != control_point.end();
	    k++) {
		stream << "<p t=\"" << (*k).first << "\" y=\"" << (*k).second << "\" />\n";
	}

	stream << "</envelope>\n";
}

std::string MachineSequencer::ControllerEnvelope::read_from_xml(const KXMLDoc &env_xml) {
	std::string id = env_xml.get_attr("id");
	std::string enabled_str = env_xml.get_attr("enabled");
	KXML_GET_NUMBER(env_xml,"coarse",controller_coarse,-1);
	KXML_GET_NUMBER(env_xml,"fine",controller_fine,-1);

	int c, c_max = 0;
	try {
		c_max = env_xml["p"].get_count();
	} catch(...) {
		// ignore
	}

	for(c = 0; c < c_max; c++) {
		KXMLDoc p_xml = env_xml["p"][c];
		int _t, _y;

		KXML_GET_NUMBER(p_xml,"t",_t,-1);
		KXML_GET_NUMBER(p_xml,"y",_y,-1);
		if(!(_t == -1 || _y == -1)) {
			control_point[_t] = _y;
		}
	}

	enabled = (enabled_str == "true") ? true : false;

	refresh_playback_data();

	return id;
}

/*************************************
 *
 * Class MachineSequencer::PadEvent
 *
 *************************************/

MachineSequencer::PadEvent::PadEvent() : x(0), y(0) {}
MachineSequencer::PadEvent::PadEvent(int finger_id, PadEvent_t _t, int _x, int _y) : t(_t), finger(finger_id), x(_x), y(_y) {}

/*************************************
 *
 * Class MachineSequencer::Arpeggiator::Pattern
 *
 *************************************/

MachineSequencer::Arpeggiator::Pattern *MachineSequencer::Arpeggiator::Pattern::built_in = NULL;

void MachineSequencer::Arpeggiator::Pattern::initiate_built_in() {
	if(built_in != NULL) return;

	built_in = (Pattern *)malloc(sizeof(Pattern) * MAX_BUILTIN_ARP_PATTERNS);
	memset(built_in, 0, sizeof(Pattern) * MAX_BUILTIN_ARP_PATTERNS);

	/* create the built in patterns */
	int i = -1; // just for purity

	// new pattern 0
	i++;
	SATAN_DEBUG("new pattern: %d\n", i);
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1),
				(MACHINE_TICKS_PER_LINE >> 1),
				0, 0));

	// new pattern 1
	i++;
	SATAN_DEBUG("new pattern: %d\n", i);
	built_in[i].append(Note((MACHINE_TICKS_PER_LINE >> 1) - (MACHINE_TICKS_PER_LINE >> 2),
				(MACHINE_TICKS_PER_LINE >> 2),
				0, 0));

	// new pattern 2
	i++;
	SATAN_DEBUG("new pattern: %d\n", i);
	SATAN_DEBUG("--new note\n");
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1),
				(MACHINE_TICKS_PER_LINE >> 1),
				0, 0));
	SATAN_DEBUG("--new note\n");
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1),
				(MACHINE_TICKS_PER_LINE >> 1),
				1, 0));
	SATAN_DEBUG("--new note\n");

	// new pattern 3
	i++;
	SATAN_DEBUG("new pattern: %d\n", i);
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1),
				(MACHINE_TICKS_PER_LINE >> 1),
				0, 0));
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1),
				(MACHINE_TICKS_PER_LINE >> 1),
				1, 0));
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1),
				(MACHINE_TICKS_PER_LINE >> 1),
				-1, 0));

	// new pattern 4
	i++;
	SATAN_DEBUG("new pattern: %d\n", i);
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1),
				(MACHINE_TICKS_PER_LINE >> 1),
				0, 0));
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1) + 1,
				(MACHINE_TICKS_PER_LINE >> 1) - 1,
				0, 0));
	// new pattern 5
	i++;
	SATAN_DEBUG("new pattern: %d\n", i);
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1),
				(MACHINE_TICKS_PER_LINE >> 1),
				0, 0));
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1) + 1,
				(MACHINE_TICKS_PER_LINE >> 1) - 1,
				1, 0));
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1),
				(MACHINE_TICKS_PER_LINE >> 1),
				0, 0));
	built_in[i].append(Note(MACHINE_TICKS_PER_LINE - (MACHINE_TICKS_PER_LINE >> 1) + 1,
				(MACHINE_TICKS_PER_LINE >> 1) - 1,
				1, 0));
	// new pattern 6
	i++;
	SATAN_DEBUG("new pattern: %d\n", i);
	built_in[i].append(Note((MACHINE_TICKS_PER_LINE >> 2) - (MACHINE_TICKS_PER_LINE >> 3),
				(MACHINE_TICKS_PER_LINE >> 3),
				0, 0));
}

/*************************************
 *
 * Class MachineSequencer::Arpeggiator
 *
 *************************************/

MachineSequencer::Arpeggiator::Arpeggiator() : phase(0), ticks_left(0), note(0), velocity(0), current_finger(0),
					       pattern_index(0), current_pattern(NULL), finger_count(0) {
	for(int k = 0; k < MAX_ARP_FINGERS; k++) {
		finger[k].counter = 0;
	}

	Pattern::initiate_built_in();

	current_pattern = &Pattern::built_in[0]; // default
}

void MachineSequencer::Arpeggiator::reset() {
	SATAN_DEBUG("Arpeggiator reset.\n");
	phase = ticks_left = current_finger = pattern_index = 0;
	note = -1;
}

void MachineSequencer::Arpeggiator::swap_keys(ArpFinger &a, ArpFinger &b) {
	ArpFinger temp = b;
	b = a;
	a = temp;
}

void MachineSequencer::Arpeggiator::sort_keys() {
	int k, l;

	if(finger_count < 2) return;

	for(k = 0; k < finger_count -1; k++) {
		for(l = k + 1; l < finger_count; l++) {
			if(finger[l].key < finger[k].key) {
				swap_keys(finger[l], finger[k]);
			}
		}
	}
}

int MachineSequencer::Arpeggiator::enable_key(int key, int velocity) {
	for(int k = 0; k < finger_count; k++) {
		if(finger[k].key == key) {
			finger[k].counter++;
			finger[k].velocity = velocity;
			return key;
		}
	}
	if(finger_count < MAX_ARP_FINGERS) {
		finger[finger_count].key = key;
		finger[finger_count].velocity = velocity;
		finger[finger_count].counter = 1;
		finger_count++;

		sort_keys();

		return key;
	}

	return -1;
}

void MachineSequencer::Arpeggiator::disable_key(int key) {
	for(int k = 0; k < finger_count; k++) {
		if(finger[k].key == key) {
			finger[k].counter--;

			if(finger[k].counter == 0) {
				finger_count--;

				for(int l = k; l < finger_count; l++) {
					finger[l] = finger[l + 1];
				}
			}

			break;
		}
	}

	sort_keys();

	if(finger_count == 0) current_finger = 0;
}

void MachineSequencer::Arpeggiator::process_pattern(bool mute, MidiEventBuilder *_meb) {
	if(current_pattern == NULL || current_pattern->length <= 0)
		return;

	if(pattern_index >= current_pattern->length) {
		pattern_index = 0;
		phase = 0;
	}

	int old_note = note;

	switch(phase) {
	case 0:
	{
		if((!mute) && (finger_count > 0)) {
			int finger_id = (current_finger) % finger_count;
			current_finger++;

			note = finger[finger_id].key + current_pattern->note[pattern_index].octave_offset * 12;
			velocity = finger[finger_id].velocity;

			// clamp note to [0 127]
			if(note < 0) note = 0;
			if(note > 127) note = 127;

			_meb->queue_note_on(note, velocity);
		} else {
			note = -1;
		}
		if(old_note != -1) { // this is to enable "sliding"
			_meb->queue_note_off(old_note, velocity);
		}
		phase = 1;
		ticks_left = current_pattern->note[pattern_index].on_length - 1; // -1 to include phase 0 which takes one tick..
	}
	break;

	case 1:
	{
		ticks_left--;
		if(ticks_left <= 0)
			phase = 2;
	}
	break;

	case 2:
	{
		if(note != -1 && (!(current_pattern->note[pattern_index].slide))) {
			_meb->queue_note_off(note, velocity);
			note = -1;
		}
		phase = 3;
		ticks_left = current_pattern->note[pattern_index].off_length - 1; // -1 to include phase 2 which takes one tick..
	}
	break;

	case 3:
	{
		ticks_left--;
		if(ticks_left <= 0) {
			phase = 0;
			pattern_index++;
		}
	}
	break;

	}
}

void MachineSequencer::Arpeggiator::set_pattern(int id) {
	current_pattern = &Pattern::built_in[id]; // default
}

/*************************************
 *
 * Class MachineSequencer::Loop
 *
 *************************************/

MachineSequencer::Loop *MachineSequencer::LoopCreator::create(const KXMLDoc &loop_xml) {
	return new Loop(loop_xml);
}

MachineSequencer::Loop *MachineSequencer::LoopCreator::create() {
	return new Loop();
}

MachineSequencer::Loop::Loop(const KXMLDoc &loop_xml) :
	note(NULL), next_note_to_play(NULL) {

	for(int x = 0; x < MAX_ACTIVE_NOTES; x++) {
		active_note[x] = NULL;
	}

	int c, c_max = 0;

	try {
		c_max = loop_xml["note"].get_count();
	} catch(...) {
		// ignore
	}

	NoteEntry temp_note_NULL;
	for(c = 0; c < c_max; c++) {
		KXMLDoc note_xml = loop_xml["note"][c];
		NoteEntry temp_note;

		temp_note.set_to(&temp_note_NULL);

		int value, line, tick, length;
		KXML_GET_NUMBER(note_xml,"channel",value, 0); temp_note.channel = value;
		KXML_GET_NUMBER(note_xml,"program", value, 0); temp_note.program = value;
		KXML_GET_NUMBER(note_xml,"velocity", value, 127); temp_note.velocity = value;
		KXML_GET_NUMBER(note_xml,"note", value, 0); temp_note.note = value;
		KXML_GET_NUMBER(note_xml,"on", line, -1);
		KXML_GET_NUMBER(note_xml,"on_tick", tick, 0);
		KXML_GET_NUMBER(note_xml,"length", length, -1);
		if(line != -1) { // old style, project_interface_level < 6
			temp_note.on_at = PAD_TIME(line, tick);
			temp_note.length = PAD_TIME(length, 0);
		} else {
			temp_note.on_at = tick;
			temp_note.length = length;
		}

		if(temp_note.length >= 0) {
			internal_insert_note(new NoteEntry(&temp_note));
		}
	}
}

MachineSequencer::Loop::Loop() :
	note(NULL), next_note_to_play(NULL) {

	for(int x = 0; x < MAX_ACTIVE_NOTES; x++) {
		active_note[x] = NULL;
	}
}

MachineSequencer::Loop::~Loop() {
	// XXX no proper cleanup done yet, we never delete a loop...
}

void MachineSequencer::Loop::get_loop_xml(std::ostringstream &stream, int id) {
	stream << "<loop id=\"" << id << "\" >\n";

	NoteEntry *crnt = note;

	while(crnt != NULL) {
		stream << "    <note "
		       << " channel=\"" << (int)(crnt->channel) << "\""
		       << " program=\"" << (int)(crnt->program) << "\""
		       << " velocity=\"" << (int)(crnt->velocity) << "\""
		       << " note=\"" << (int)(crnt->note) << "\""
		       << " on_tick=\"" << crnt->on_at << "\""
		       << " length=\"" << crnt->length << "\""
		       << " />\n"
			;

		crnt = crnt->next;
	}

	stream << "</loop>\n";
}

void MachineSequencer::Loop::start_to_play() {
	loop_position = 0;
	next_note_to_play = note;
}

bool MachineSequencer::Loop::activate_note(NoteEntry *note) {
	for(int x = 0; x < MAX_ACTIVE_NOTES; x++) {
		if(active_note[x] == NULL) {
			active_note[x] = note;
			return true;
		}
	}
	return false;
}

void MachineSequencer::Loop::deactivate_note(NoteEntry *note) {
	for(int x = 0; x < MAX_ACTIVE_NOTES; x++) {
		if(active_note[x] == note) {
			active_note[x] = NULL;
			return;
		}
	}
}

void MachineSequencer::Loop::process_note_on(bool mute, MidiEventBuilder *_meb) {
	while(next_note_to_play != NULL && (next_note_to_play->on_at == loop_position)) {
		NoteEntry *n = next_note_to_play;

		if((!mute) && activate_note(n)) {
			_meb->queue_note_on(n->note, n->velocity, n->channel);
			n->ticks2off = n->length + 1;
		}
		// then move on to the next in queue
		next_note_to_play = n->next;
	}
}

void MachineSequencer::Loop::process_note_off(MidiEventBuilder *_meb) {
	for(int k = 0; k < MAX_ACTIVE_NOTES; k++) {
		if(active_note[k] != NULL) {
			NoteEntry *n = active_note[k];
			if(n->ticks2off > 0) {
				n->ticks2off--;
			} else {
				deactivate_note(n);
				_meb->queue_note_off(n->note, 0x80, n->channel);
			}
		}
	}
}

void MachineSequencer::Loop::process(bool mute, MidiEventBuilder *_meb) {
	process_note_on(mute, _meb);
	process_note_off(_meb);

	loop_position++;
}

const MachineSequencer::NoteEntry *MachineSequencer::Loop::notes_get() const {
	return note;
}

MachineSequencer::NoteEntry *MachineSequencer::Loop::internal_delete_note(const NoteEntry *net) {
	NoteEntry *crnt = note;
	while(crnt != NULL) {
		if(crnt == net) {
			if(crnt->prev != NULL) {
				crnt->prev->next = crnt->next;
			}
			if(crnt->next != NULL) {
				crnt->next->prev = crnt->prev;
			}
			if(crnt == note)
				note = crnt->next;

			return crnt;
		}
		crnt = crnt->next;
	}
	return NULL;
}

void MachineSequencer::Loop::note_delete(const NoteEntry *net) {
	typedef struct {
		MachineSequencer::Loop *thiz;
		const MachineSequencer::NoteEntry *n;
		MachineSequencer::NoteEntry *deletable;
	} Param;
	Param param = {
		.thiz = this,
		.n = net,
		.deletable = NULL
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->deletable = p->thiz->internal_delete_note(p->n);
		},
		&param, true);
	if(param.deletable) delete param.deletable;
}

void MachineSequencer::Loop::internal_update_note(
	const NoteEntry *original, const NoteEntry *new_entry) {

	NoteEntry *crnt = note;
	while(crnt != NULL) {
		if(crnt == original) {
			crnt->set_to(new_entry);
			return;
		}
		crnt = crnt->next;
	}
}

void MachineSequencer::Loop::note_update(const MachineSequencer::NoteEntry *original,
					 const MachineSequencer::NoteEntry *new_entry) {
	typedef struct {
		MachineSequencer::Loop *thiz;
		const MachineSequencer::NoteEntry *org;
		const MachineSequencer::NoteEntry *trg;
	} Param;
	Param param = {
		.thiz = this,
		.org = original,
		.trg = new_entry
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->thiz->internal_update_note(p->org, p->trg);
		},
		&param, true);
}

const MachineSequencer::NoteEntry *MachineSequencer::Loop::internal_insert_note(
	MachineSequencer::NoteEntry *new_one) {

	NoteEntry *crnt = note;
	while(crnt != NULL) {
		if(crnt->on_at > new_one->on_at) {
			new_one->prev = crnt->prev;
			new_one->next = crnt;

			if(crnt->prev) {
				crnt->prev->next = new_one;
			} else { // must be the first one, then
				note = new_one;
			}
			crnt->prev = new_one;

			return new_one;
		}

		if(crnt->next == NULL) {
			// end of the line...
			crnt->next = new_one;
			new_one->prev = crnt;
			new_one->next = NULL;
			return new_one;
		}

		crnt = crnt->next;
	}

	// reached only if no notes entries exist
	note = new_one;
	note->next = NULL;
	note->prev = NULL;

	return new_one;
}

const MachineSequencer::NoteEntry *MachineSequencer::Loop::note_insert(const MachineSequencer::NoteEntry *__new_entry) {
	typedef struct {
		MachineSequencer::Loop *thiz;
		MachineSequencer::NoteEntry *new_entry;
		const MachineSequencer::NoteEntry *retval;
	} Param;
	Param param = {
		.thiz = this,
		.new_entry = new NoteEntry(__new_entry),
		.retval = NULL
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->retval = p->thiz->internal_insert_note(p->new_entry);
		},
		&param, true);

	return param.retval;
}

MachineSequencer::NoteEntry *MachineSequencer::Loop::internal_clear() {
	NoteEntry *notes_to_delete = note;
	note = NULL;
	return notes_to_delete;
}

void MachineSequencer::Loop::clear_loop() {
	typedef struct {
		MachineSequencer::Loop *thiz;
		NoteEntry *notes_to_delete;
	} Param;
	Param param = {
		.thiz = this,
		.notes_to_delete = NULL
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->notes_to_delete = p->thiz->internal_clear();
		},
		&param, true);

	while(param.notes_to_delete) {
		NoteEntry *delete_me = param.notes_to_delete;
		param.notes_to_delete = param.notes_to_delete->next;
		delete delete_me;
	}
}

MachineSequencer::NoteEntry *MachineSequencer::Loop::internal_insert_notes(MachineSequencer::NoteEntry *new_notes) {
	NoteEntry *notes_to_delete = internal_clear();

	NoteEntry *tmp = new_notes;
	while(tmp != NULL) {
		NoteEntry *crnt = tmp;
		tmp = tmp->next;
		(void) internal_insert_note(crnt);
	}
	return notes_to_delete;
}

void MachineSequencer::Loop::copy_loop(const MachineSequencer::Loop *src) {
	typedef struct {
		MachineSequencer::Loop *thiz;
		MachineSequencer::NoteEntry *new_notes;
		NoteEntry *notes_to_delete;
	} Param;
	Param param = {
		.thiz = this,
		.new_notes = NULL,
		.notes_to_delete = NULL
	};

	const NoteEntry *original = src->notes_get();
	NoteEntry *new_ones = NULL, *clone = NULL;
	while(original) {
		if(clone == NULL) {
			new_ones = clone = new NoteEntry(original);
			clone->prev = clone->next = NULL;
		} else {
			clone->next = new NoteEntry(original);
			clone->next->prev = clone;
			clone = clone->next;
			clone->next = NULL;
		}
		original = original->next;
	}
	param.new_notes = new_ones;

	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->notes_to_delete = p->thiz->internal_insert_notes(p->new_notes);
		},
		&param, true);

	while(param.notes_to_delete) {
		NoteEntry *delete_me = param.notes_to_delete;
		param.notes_to_delete = param.notes_to_delete->next;
		delete delete_me;
	}
}

/*************************************
 *
 * Class MachineSequencer::PadConfiguration
 *
 *************************************/

#include "scales.hh"

MachineSequencer::PadConfiguration::PadConfiguration() : chord_mode(chord_off) {}

MachineSequencer::PadConfiguration::PadConfiguration(PadMode _mode, int _scale, int _octave) : mode(_mode), chord_mode(chord_off), scale(_scale), last_scale(-1), octave(_octave), arpeggio_pattern(0), pad_controller_coarse(-1), pad_controller_fine(-1) {}

MachineSequencer::PadConfiguration::PadConfiguration(const PadConfiguration *parent) : mode(parent->mode), chord_mode(parent->chord_mode), scale(parent->scale), last_scale(-1), octave(parent->octave), arpeggio_pattern(0), pad_controller_coarse(parent->pad_controller_coarse), pad_controller_fine(parent->pad_controller_fine) {}

void MachineSequencer::PadConfiguration::set_coarse_controller(int c) {
	pad_controller_coarse = c;
}

void MachineSequencer::PadConfiguration::set_fine_controller(int c) {
	pad_controller_fine = c;
}

void MachineSequencer::PadConfiguration::set_mode(PadMode _mode) {
	mode = _mode;
}

void MachineSequencer::PadConfiguration::set_arpeggio_pattern(int arp_pattern) {
	arpeggio_pattern = arp_pattern;
}

void MachineSequencer::PadConfiguration::set_chord_mode(ChordMode _mode) {
	chord_mode = _mode;
}

void MachineSequencer::PadConfiguration::set_scale(int _scale) {
	scale = _scale;
}

void MachineSequencer::PadConfiguration::set_octave(int _octave) {
	octave = _octave;
}

void MachineSequencer::PadConfiguration::get_configuration_xml(std::ostringstream &stream) {
	stream << "<c "
	       << "m=\"" << (int)mode << "\" "
	       << "c=\"" << (int)chord_mode << "\" "
	       << "s=\"" << scale << "\" "
	       << "o=\"" << octave << "\" "
	       << "arp=\"" << arpeggio_pattern << "\" "
	       << "cc=\"" << pad_controller_coarse << "\" "
	       << "cf=\"" << pad_controller_fine << "\" "
	       << "/>\n";
}

void MachineSequencer::PadConfiguration::load_configuration_from_xml(const KXMLDoc &pad_xml) {
	KXMLDoc c = pad_xml;
	try {
		c = pad_xml["c"];
	} catch(...) { /* ignore */ }

	int _mode = 0;
	KXML_GET_NUMBER(c, "m", _mode, _mode);
	mode = (PadMode)_mode;

	int c_mode = 0;
	KXML_GET_NUMBER(c, "c", c_mode, c_mode);
	chord_mode = (ChordMode)c_mode;

	KXML_GET_NUMBER(c, "s", scale, scale);
	KXML_GET_NUMBER(c, "o", octave, octave);
	KXML_GET_NUMBER(c, "arp", arpeggio_pattern, arpeggio_pattern);

	KXML_GET_NUMBER(c, "cc", pad_controller_coarse, -1);
	KXML_GET_NUMBER(c, "cf", pad_controller_fine, -1);
}

/*************************************
 *
 * Class MachineSequencer::PadMotion
 *
 *************************************/

void MachineSequencer::PadMotion::get_padmotion_xml(int finger, std::ostringstream &stream) {
	stream << "<m f=\"" << finger << "\" start=\"" << start_tick << "\">\n";

	get_configuration_xml(stream);

	unsigned int k;
	for(k = 0; k < x.size(); k++) {
		stream << "<d x=\"" << x[k] << "\" y=\"" << y[k] << "\" t=\"" << t[k] << "\" />\n";
	}

	stream << "</m>\n";
}

MachineSequencer::PadMotion::PadMotion(Pad *parent_config,
				       int session_position, int _x, int _y) :
	PadConfiguration(parent_config), index(-1), crnt_tick(-1), start_tick(session_position),
	terminated(false), to_be_deleted(false), prev(NULL), next(NULL)
{
	for(int x = 0; x < MAX_PAD_CHORD; x++)
		last_chord[x] = -1;

	add_position(_x, _y);

}

MachineSequencer::PadMotion::PadMotion(Pad *parent_config, int &start_offset_return, const KXMLDoc &pad_xml) :
	PadConfiguration(parent_config), index(-1), start_tick(0),
	terminated(true), to_be_deleted(false), prev(NULL), next(NULL)
{
	for(int x = 0; x < MAX_PAD_CHORD; x++)
		last_chord[x] = -1;

	load_configuration_from_xml(pad_xml);

	int mk = 0;
	try {
		mk = pad_xml["d"].get_count();
	} catch(...) { /* ignore */ }

	int first_tick = 0;

	// because of the difference between project_level 4 and 5
	// this code get's a bit ugly (it converts level 4 representation to level 5 and beyond
	for(int k = 0; k < mk; k++) {
		KXMLDoc dxml = pad_xml["d"][k];
		int _x, _y, _abs_time, _t;

		KXML_GET_NUMBER(dxml, "x", _x, -1);
		KXML_GET_NUMBER(dxml, "y", _y, -1);
		KXML_GET_NUMBER(dxml, "t", _abs_time, -1);

		int lin = PAD_TIME_LINE(_abs_time);
		int tick = PAD_TIME_TICK(_abs_time);

		if(k == 0) {
			start_offset_return = _abs_time;
			first_tick = (lin * MACHINE_TICKS_PER_LINE) + tick;
			_t = 0;
		} else {
			_t = (lin * MACHINE_TICKS_PER_LINE) + tick - first_tick;
		}

		// _t might be < 0 - a.k.a. wrap failure
		// this was the major problem with the old style, it did not wrap very good..
		// so here we just discard it
		if(_t >= 0) {
			x.push_back(_x);
			y.push_back(_y);
			t.push_back(_t);
		}
	}
}

MachineSequencer::PadMotion::PadMotion(Pad *parent_config, const KXMLDoc &pad_xml) :
	PadConfiguration(parent_config), index(-1), terminated(true), to_be_deleted(false), prev(NULL), next(NULL)
{
	for(int x = 0; x < MAX_PAD_CHORD; x++)
		last_chord[x] = -1;

	load_configuration_from_xml(pad_xml);

	KXML_GET_NUMBER(pad_xml, "start", start_tick, -1);

	int mk = 0;
	try {
		mk = pad_xml["d"].get_count();
	} catch(...) { /* ignore */ }

	for(int k = 0; k < mk; k++) {
		KXMLDoc dxml = pad_xml["d"][k];
		int _x, _y, _t;

		KXML_GET_NUMBER(dxml, "x", _x, -1);
		KXML_GET_NUMBER(dxml, "y", _y, -1);
		KXML_GET_NUMBER(dxml, "t", _t, -1);

		x.push_back(_x);
		y.push_back(_y);
		t.push_back(_t);
	}
}

void MachineSequencer::PadMotion::quantize() {
#if 0
	if(prev) {
		int prev_dist = quantize_tick(start_tick - prev->start_tick);
		prev_dist = prev->start_tick + prev_dist;
		SATAN_DEBUG("quantizing PadMotion...(%d -> %d)\n", start_tick, prev_dist);
		start_tick = prev_dist;

	}
#else
	int qtick = quantize_tick(start_tick);
	SATAN_DEBUG("quantizing PadMotion...(%d -> %d)\n", start_tick, qtick);
	start_tick = qtick;
#endif
}

void MachineSequencer::PadMotion::add_position(int _x, int _y) {
	if(terminated) return;

	x.push_back(_x);
	y.push_back(_y);
	t.push_back(crnt_tick + 1); // when add_position is called the crnt_tick has not been upreved yet by "process_motion", so we have to do + 1
}

void MachineSequencer::PadMotion::terminate() {
	terminated = true;

	// ensure that we stop at least one tick after we start.
	int last = ((int)t.size()) - 1;
	if(last > 0 && t[0] == t[last]) {
		t[last] = t[last] + 1;
	}
}

void MachineSequencer::PadMotion::can_be_deleted_now(PadMotion *to_delete) {
	if(to_delete->to_be_deleted) {
		delete to_delete;
	}
}

void MachineSequencer::PadMotion::delete_motion(PadMotion *to_delete) {
	if(to_delete->index != -1) {
		// it's being played, mark for deletion later
		to_delete->to_be_deleted = true;
		return;
	}

	// it's not in use, delete straight away
	delete to_delete;
}

bool MachineSequencer::PadMotion::start_motion(int session_position) {
	// first check if the motion is not running (i.e. index == -1)
	if(index == -1) {
		if(session_position == start_tick) {
			index = 0;
			crnt_tick = -1; // we start at a negative index, it will be stepped up in process motion
			last_x = -1;
			return true;
		}
	}

	return false;
}

void MachineSequencer::PadMotion::reset() {
	index = -1;
	crnt_tick = -1;
	last_x = -1;
}

void MachineSequencer::PadMotion::build_chord(const int *scale_data, int scale_offset, int *chord, int pad_column) {
	// this version of build_chord() builds a simple triad
	chord[0] = octave * 12 + scale_data[pad_column + 0 + scale_offset];
	chord[1] = octave * 12 + scale_data[pad_column + 2 + scale_offset];
	chord[2] = octave * 12 + scale_data[pad_column + 4 + scale_offset];
	chord[3] = -1; // indicate end
	chord[4] = -1; // indicate end
	chord[5] = -1; // indicate end
}

bool MachineSequencer::PadMotion::process_motion(MachineSequencer::MidiEventBuilder *_meb, MachineSequencer::Arpeggiator *arpeggiator) {
	// check if we have reached the end of the line
	if(terminated && index >= (int)x.size()) {
		index = -1; // reset index
		return true;
	}

	crnt_tick++;

	int scale_offset = 0;
	if(scale != last_scale) {
		last_scale = scale;

		if(auto scalo = Scales::get_scales_object_serverside()) {
			scalo->get_scale_keys(scale, scale_data);

			for(auto k = 0; k < 7; k++) {
				scale_data[ 7 + k] = scale_data[k] + 12;
				scale_data[14 + k] = scale_data[k] + 24;
			}
		} else {
			// default to standard C scale
			static const int def_scale_data[] = {
				0,  2,   4,  5,  7,  9, 11,
				12, 14, 16, 17, 19, 21, 23,
				24, 26, 28, 29, 31, 33, 35
			};
			for(auto k = 0; k < 21; k++)
				scale_data[k] = def_scale_data[k];
		}
	}

	int max = (int)x.size();

	while( (t[index] <= crnt_tick) && (index < max) ) {
		int pad_column = (x[index] >> 5) >> 4; // first right shift 5 for "coarse" data, then shift by 4 to get wich of the 8 columns we are in..
		int note = octave * 12 + scale_data[pad_column + scale_offset];
		int chord[MAX_PAD_CHORD];

		if( (!to_be_deleted) && (chord_mode != chord_off) && ((!terminated) || (index < (max - 1))) ) {
			build_chord(scale_data, scale_offset, chord, pad_column);
		} else {
			for(int k = 0; k < MAX_PAD_CHORD; k++) {
				chord[k] = -1;
			}
		}

		int y_c = (y[index] >> 5);
		int y_f = (y[index] & 0x1f) << 2;

		int velocity = (pad_controller_coarse == -1) ? y_c : 0x7f;

		if(pad_controller_coarse != -1) {
			_meb->queue_controller(pad_controller_coarse, y_c);
			if(pad_controller_fine != -1) {
				_meb->queue_controller(pad_controller_fine, y_f);
			}
		}

		if(mode == pad_normal) {
			if(chord_mode != chord_off) {
				for(int k = 0; k < MAX_PAD_CHORD; k++) {
					if(last_chord[k] != chord[k]) {
						if(last_chord[k] != -1) {
							_meb->queue_note_off(last_chord[k], 0x7f);
						}
						if(chord[k] != -1) {
							_meb->queue_note_on(chord[k], velocity);
						}
					}
				}
			} else {
				if((!to_be_deleted) && (last_x != note) ) {
					if((!terminated) || (index < (max - 1)))
						_meb->queue_note_on(note, velocity);

					if(last_x != -1)
						_meb->queue_note_off(last_x, 0x7f);

				} else if( to_be_deleted || (terminated && (index == (max - 1)) )  ) {
					_meb->queue_note_off(last_x, 0x7f);
				}
			}
		} else if(mode == pad_arpeggiator) {
			if(chord_mode != chord_off) {
				for(int k = 0; k < MAX_PAD_CHORD; k++) {
					if(last_chord[k] != -1) {
						arpeggiator->disable_key(last_chord[k]);
					}
					if(chord[k] != -1) {
						chord[k] = arpeggiator->enable_key(chord[k], velocity);
					}
				}
			} else {
				if(!to_be_deleted) {
					if(last_x != -1) {
						arpeggiator->disable_key(last_x);
					}
					if((!terminated) || (index < (max - 1))) {
						note = arpeggiator->enable_key(note, velocity);
					}
				} else if( to_be_deleted || (terminated && (index == (max - 1)) ) ) {
					arpeggiator->disable_key(last_x);
				}
			}
		}

		for(int k = 0; k < MAX_PAD_CHORD; k++)
			last_chord[k] = chord[k];
		last_x = note;

		// step to next index
		index++;
	}

	return false;
}

void MachineSequencer::PadMotion::record_motion(PadMotion **head, PadMotion *target) {
	// if the head is empty, then we have a simple task...
	if((*head) == NULL) {
		*head = target;
		target->next = NULL;
		target->prev = NULL;
		return;
	}

	// ouch - more difficult...
	PadMotion *top = *head;

	while(top != NULL) {

		// if we can, insert before top
		if(target->start_tick < top->start_tick) {
			target->next = top;
			target->prev = top->prev;

			if(target->prev == NULL) {
				// in this case we are to insert before the head ...
				*head = target;
			} else {
				// in this case we are to insert somewhere inside the chain...
				top->prev->next = target;
			}

			top->prev = target;

			return;
		}

		// if we reached the tail, attach to it...
		if(top->next == NULL) {
			top->next = target;
			target->prev = top;
			target->next = NULL;

			return;
		}

		// no matchin position found yet, check next..
		top = top->next;
	}

	// this should actually never be reached...
}

/*************************************
 *
 * Class MachineSequencer::PadFinger
 *
 *************************************/

#define PAD_RESOLUTION 4096

MachineSequencer::PadFinger::PadFinger() : to_be_deleted(false), parent(NULL), recorded(NULL), next_motion_to_play(NULL), current(NULL) {
}

MachineSequencer::PadFinger::~PadFinger() {
	PadMotion *t = recorded;
	while(t != NULL) {
		recorded = t->next;
		delete t;
		t = recorded;
	}

	if(current) delete current;
}

void MachineSequencer::PadFinger::start_from_the_top() {
	next_motion_to_play = recorded;
	playing_motions.clear();
}

void MachineSequencer::PadFinger::process_finger_events(const PadEvent &pe, int session_position) {
	if(to_be_deleted) return;

	session_position++; // at this stage the session_position is at the last position still, we need to increase it by one.

	int x = pe.x, y = pe.y;
	PadEvent_t t = pe.t;

	switch(t) {
	case ms_pad_press:
		current = new PadMotion(parent, session_position, x, y);
		if(current->start_motion(session_position)) {
			playing_motions.push_back(current);
		} else {
			SATAN_ERROR("MachineSequencer::PadFinger::process_finger_events() - Failed to start motion.\n");
			delete current; // highly unlikely - shouldn't happen, but if it does just silently fail...
			current = NULL;
		}

		break;

	case ms_pad_release:
		if(current) {
			current->add_position(x, y);
			current->terminate();
		}
		break;
	case ms_pad_slide:
		if(current) {
			current->add_position(x, y);
		}
		break;
	case ms_pad_no_event:
		break;
	}
}

bool MachineSequencer::PadFinger::process_finger_motions(bool do_record, bool mute, int session_position,
							 MidiEventBuilder *_meb, Arpeggiator *arpeggiator,
							 bool quantize) {
	PadMotion *top = next_motion_to_play;

	// OK, check if we have anything at all now..
	if((!to_be_deleted) && (!mute) && top) {
		while(top && top->start_motion(session_position)) {
			playing_motions.push_back(top);
			top = top->next;
		}

		next_motion_to_play = top;
	} else if(mute) next_motion_to_play = NULL;

	std::vector<PadMotion *>::iterator k = playing_motions.begin();
	while(k != playing_motions.end()) {
		if((*k)->process_motion(_meb, arpeggiator)) {
			if((*k) == current) {
				if(do_record) {
					PadMotion::record_motion(&recorded, current);
					if(quantize)
						 current->quantize();
				} else {
					delete current;
				}
				current = NULL;
			} else {
				PadMotion::can_be_deleted_now((*k));
			}

			k = playing_motions.erase(k);
		} else {
			k++;
		}
	}

	if(playing_motions.size() == 0 && next_motion_to_play == NULL) {
		return true; // OK, we've completed all recorded motions here
	}
	return false; // We still got work to do
}

void MachineSequencer::PadFinger::reset() {
	for(auto motion : playing_motions) {
		motion->reset();
	}

	playing_motions.clear();
	next_motion_to_play = recorded;
}

void MachineSequencer::PadFinger::delete_pad_finger() {
	while(recorded) {
		PadMotion *to_delete = recorded;
		recorded = recorded->next;
		PadMotion::delete_motion(to_delete);
	}
	recorded = NULL;
	next_motion_to_play = NULL;

	to_be_deleted = true;
}

void MachineSequencer::PadFinger::terminate() {
	if(current) {
		current->terminate();
	}
}

/*************************************
 *
 * Class MachineSequencer::PadSession
 *
 *************************************/

MachineSequencer::PadSession::PadSession(Pad *parent, int start, bool _terminated) :
	to_be_deleted(false), terminated(_terminated), start_at_tick(start),
	playback_position(-1), in_play(false)
{
	for(int k = 0; k < MAX_PAD_FINGERS; k++) {
		finger[k].parent = parent;
	}
}


MachineSequencer::PadSession::PadSession(Pad *parent, const KXMLDoc &ps_xml) :
	to_be_deleted(false), terminated(true),
	playback_position(-1), in_play(false)
{
	for(int k = 0; k < MAX_PAD_FINGERS; k++) {
		finger[k].parent = parent;
	}

	KXML_GET_NUMBER(ps_xml, "start", start_at_tick, 0);

	int mx = 0;
	mx = ps_xml["m"].get_count();

	for(int k = 0; k < mx; k++) {
		KXMLDoc mxml = ps_xml["m"][k];

		int _f;
		KXML_GET_NUMBER(mxml, "f", _f, 0);

		PadMotion *m = new PadMotion(parent, mxml);
		PadMotion::record_motion(&(finger[_f].recorded), m);
	}

}


MachineSequencer::PadSession::PadSession(Pad *parent, int start, PadMotion *padmot, int finger_id) :
	to_be_deleted(false), terminated(true), start_at_tick(start),
	playback_position(-1), in_play(false) {

	for(int k = 0; k < MAX_PAD_FINGERS; k++) {
		finger[k].parent = parent;
	}

	PadMotion::record_motion(&(finger[finger_id].recorded), padmot);
}

bool MachineSequencer::PadSession::start_play(int crnt_tick) {
	if(in_play) return true;

	if(start_at_tick == crnt_tick) {
		SATAN_DEBUG(" starting session.\n");

		in_play = true;
		playback_position = -1;

		for(int _f = 0; _f < MAX_PAD_FINGERS; _f++) {
			finger[_f].start_from_the_top();
		}

		return true;
	}

	return false;
}

bool MachineSequencer::PadSession::process_session(bool do_record, bool mute,
						   Arpeggiator *arpeggiator,
						   MidiEventBuilder*_meb,
						   bool quantize) {
	playback_position++;

	bool session_completed = true;
	for(int _f = 0; _f < MAX_PAD_FINGERS; _f++) {
		bool finger_completed =
			finger[_f].process_finger_motions(do_record, mute, playback_position,
							  _meb, arpeggiator,
							  quantize);
		session_completed = session_completed && finger_completed;

	}

	// if session is completed, we are no longer in play
	in_play = !(session_completed && terminated);

	if(!in_play)
		SATAN_DEBUG("    session_completed\n");

	return session_completed && to_be_deleted;
}

void MachineSequencer::PadSession::reset() {
	in_play = false;

	for(int _f = 0; _f < MAX_PAD_FINGERS; _f++) {
		finger[_f].reset();
	}
}

void MachineSequencer::PadSession::delete_session() {
	to_be_deleted = true;

	for(int _f = 0; _f < MAX_PAD_FINGERS; _f++) {
		finger[_f].delete_pad_finger();
	}
}

void MachineSequencer::PadSession::terminate() {
	terminated = true;
	for(int _f = 0; _f < MAX_PAD_FINGERS; _f++) {
		finger[_f].terminate();
	}
}

void MachineSequencer::PadSession::get_session_xml(std::ostringstream &stream) {
	stream << "<s start=\"" << start_at_tick << "\">\n";
	for(int _f = 0; _f < MAX_PAD_FINGERS; _f++) {
		PadMotion *k = finger[_f].recorded;

		while(k != NULL) {
			k->get_padmotion_xml(_f, stream);
			k = k->next;
		}
	}
	stream << "</s>\n";
}

/*************************************
 *
 * Class MachineSequencer::Pad
 *
 *************************************/

MachineSequencer::Pad::Pad() : PadConfiguration(pad_arpeggiator, 0, 4), pad_width(4096), pad_height(4096), // 4096 in this context is just bogus, wee need to be properly configured before use anyway
			       current_session(NULL), do_record(false), do_quantize(false) {
	padEventQueue = new moodycamel::ReaderWriterQueue<PadEvent>(100);
}

MachineSequencer::Pad::~Pad() {
	delete padEventQueue;
}

void MachineSequencer::Pad::reset() {
	arpeggiator.reset();

	auto session_iterator = recorded_sessions.begin();
	while(session_iterator != recorded_sessions.end()) {
		if((*session_iterator) == current_session) {
			session_iterator = recorded_sessions.erase(session_iterator);
			delete current_session;
			current_session = NULL;
		} else {
			(*session_iterator)->reset();
			session_iterator++;
		}
	}
}

void MachineSequencer::Pad::process_events(int tick) {
	PadEvent pe;
	while(padEventQueue->try_dequeue(pe)) {
		if(pe.finger >= 0 && pe.finger < MAX_PAD_FINGERS) {
			if(current_session == NULL) {
				int start_tick = tick;
				if(do_quantize) {
					int qtick = quantize_tick(start_tick);
					SATAN_DEBUG("  WILL QUANTIZE THIS SESSION. (%x -> %x)\n", start_tick, qtick);
					start_tick = qtick;
				}

				// if we are looping and we end up at or after __loop_stop, move us back
				if(Machine::get_loop_state()) {
					int loop_start = Machine::get_loop_start();
					int loop_end = Machine::get_loop_length() + loop_start;
					if(start_tick >= (loop_end << BITS_PER_LINE))
						start_tick -= (loop_start << BITS_PER_LINE);
				}

				current_session = new PadSession(this, start_tick, false);
				current_session->in_play = true;
				current_session->playback_position = -1;
				recorded_sessions.push_back(current_session);
			}

			current_session->finger[pe.finger].process_finger_events(pe, current_session->playback_position);
		}
	}
}

void MachineSequencer::Pad::process_motions(bool mute, int tick, MidiEventBuilder *_meb) {
	// Process recorded sessions
	std::vector<PadSession *>::iterator t;
	for(t = recorded_sessions.begin(); t != recorded_sessions.end(); ) {
		// check if in play, or if we should start it
		if((*t)->start_play(tick)) {
			if((*t)->process_session(do_record, mute,
						 &arpeggiator,
						 _meb, do_quantize)) {
				SATAN_DEBUG(" --- session object will be deleted %p\n", (*t));
				delete (*t);
				recorded_sessions.erase(t);
			} else {
				t++;
			}
		} else {
			t++;
		}
	}
}

void MachineSequencer::Pad::process(bool mute, int tick, MidiEventBuilder *_meb) {
	process_events(tick);
	process_motions(mute, tick, _meb);
	arpeggiator.process_pattern(mute, _meb);
}

void MachineSequencer::Pad::get_pad_xml(std::ostringstream &stream) {
	stream << "<pad>\n";

	get_configuration_xml(stream);

	for(auto k : recorded_sessions) {
		if(k != current_session) {
			k->get_session_xml(stream);
		}
	}

	stream << "</pad>\n";
}

void MachineSequencer::Pad::load_pad_from_xml(int project_interface_level, const KXMLDoc &p_xml) {
	//
	//  project_interface_level  < 5 == PadMotion xml uses old absolute coordinates
	//                          => 5 == PadMotion xml uses new relative coordinates
	//
	try {
		KXMLDoc pad_xml = p_xml["pad"];

		load_configuration_from_xml(pad_xml);

		if(project_interface_level < 5) {
			int mx = 0;
			mx = pad_xml["m"].get_count();

			for(int k = 0; k < mx; k++) {
				KXMLDoc mxml = pad_xml["m"][k];

				int _f;
				KXML_GET_NUMBER(mxml, "f", _f, 0);

				int start_offset;
				PadMotion *m = new PadMotion(this, start_offset, mxml);

				PadSession *nses = new PadSession(this, start_offset, m, _f);
				recorded_sessions.push_back(nses);
			}
		} else {
			int mx = 0;
			mx = pad_xml["s"].get_count();

			for(int k = 0; k < mx; k++) {
				KXMLDoc ps_xml = pad_xml["s"][k];

				PadSession *nses = new PadSession(this, ps_xml);
				recorded_sessions.push_back(nses);
			}
		}

	} catch(...) {
		// ignore
	}
	arpeggiator.set_pattern(arpeggio_pattern);
}

void MachineSequencer::Pad::export_to_loop(int start_tick, int stop_tick, MachineSequencer::Loop *loop) {
	if(stop_tick < start_tick) {
		// force stop_tick to the highest integer value (always limited to 32 bit)
		stop_tick = 0x7fffffff;
	}
	PadMidiExportBuilder pmxb(start_tick, loop);

	std::vector<PadSession *> remaining_sessions;
	std::vector<PadSession *> active_sessions;

	// reset all prior to exporting
	SATAN_DEBUG("Start export...\n");
	reset();
	SATAN_DEBUG("Reset pre export...\n");

	// copy all the recorded sessions that we should
	// export to the remaining sessions vector
	for(auto session : recorded_sessions) {
		if(session != current_session &&
		   session->start_at_tick >= start_tick &&
		   session->start_at_tick < stop_tick) {
			remaining_sessions.push_back(session);
		}
	}

	while(pmxb.current_tick < stop_tick || active_sessions.size() > 0) {
		{
			// scan remaining sessions to see if anyone can start at this tick
			auto session = remaining_sessions.begin();
			while(session != remaining_sessions.end()) {
				if((*session)->start_play(pmxb.current_tick)) {
					active_sessions.push_back(*session);
					session = remaining_sessions.erase(session);
				} else {
					session++;
				}
			}
		}

		{
			// process the active_sessions for export
			auto session = active_sessions.begin();
			while(session != active_sessions.end()) {
				(void)(*session)->process_session(false, false, &arpeggiator, &pmxb, false);
				if((*session)->start_play(-1)) { // check if it's currently playing
					session++;
				} else { //  if not, erase it from the active vector
 					session = active_sessions.erase(session);
				}
			}
		}

		// export arpeggiator data for this tick
		arpeggiator.process_pattern(false, &pmxb);

		// step no next tick
		pmxb.step_tick();
	}

	// we also reset all after exporting
	SATAN_DEBUG("Reset post export...\n");
	reset();
	SATAN_DEBUG("Done export!\n");
}

void MachineSequencer::Pad::set_pad_resolution(int width, int height) {
	pad_width = width;
	pad_height = height;
}

void MachineSequencer::Pad::enqueue_event(int finger_id, PadEvent_t t, int x, int y) {

	x = PAD_RESOLUTION * x / pad_width;
	y = PAD_RESOLUTION * (pad_height - y) / pad_height;

	padEventQueue->enqueue(PadEvent(finger_id, t, x, y));
}

void MachineSequencer::Pad::internal_set_record(bool _do_record) {
	if(do_record == _do_record) return; // no change...

	if(current_session) {
		if(!do_record) {
			SATAN_DEBUG(" session deleted.\n");
			current_session->delete_session(); // mark it for deletion
		} else {
			SATAN_DEBUG(" session terminated.\n");
			current_session->terminate(); // terminate recording
		}
		current_session = NULL;
		SATAN_DEBUG(" recorded session count: %d\n", recorded_sessions.size());
	}
	do_record = _do_record;
}

void MachineSequencer::Pad::set_record(bool _do_record) {
	typedef struct {
		MachineSequencer::Pad *thiz;
		bool dorec;
	} Param;
	Param param = {
		.thiz = this,
		.dorec = _do_record
	};

	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->thiz->internal_set_record(p->dorec);
		},
		&param, true);
}

void MachineSequencer::Pad::set_quantize(bool _do_quantize) {
	typedef struct {
		MachineSequencer::Pad *thiz;
		bool doqua;
	} Param;
	Param param = {
		.thiz = this,
		.doqua = _do_quantize
	};

	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->thiz->do_quantize = p->doqua;
		},
		&param, true);
}

void MachineSequencer::Pad::internal_clear_pad() {
	// Mark all recorded sessions for deletion
	for(auto k : recorded_sessions) {
		k->delete_session();
	}

	// forget current session
	current_session = NULL;

}

void MachineSequencer::Pad::clear_pad() {
	typedef struct {
		MachineSequencer::Pad *thiz;
	} Param;
	Param param = {
		.thiz = this
	};

	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->thiz->internal_clear_pad();
		},
		&param, true);
}

/*************************************
 *
 * Class MachineSequencer - Public Inheritance stuff
 *
 *************************************/

std::vector<std::string> MachineSequencer::internal_get_controller_groups() {
	std::vector<std::string> retval;
	retval.clear();
	return retval;
}

std::vector<std::string> MachineSequencer::internal_get_controller_names() {
	std::vector<std::string> retval;
	retval.clear();
	return retval;
}

std::vector<std::string> MachineSequencer::internal_get_controller_names(const std::string &group_name) {
	std::vector<std::string> retval;
	retval.clear();
	return retval;
}

Machine::Controller *MachineSequencer::internal_get_controller(const std::string &name) {
	throw jException("No such controller.", jException::sanity_error);
}

std::string MachineSequencer::internal_get_hint() {
	return "sequencer";
}

/*************************************
 *
 * Class MachineSequencer - Protected Inheritance stuff
 *
 *************************************/

bool MachineSequencer::detach_and_destroy() {
	detach_all_inputs();
	detach_all_outputs();

	return true;
}

std::string MachineSequencer::get_class_name() {
	return "MachineSequencer";
}

std::string MachineSequencer::get_descriptive_xml() {
	std::ostringstream stream;

	if(sibling_name == "") throw jException("MachineSequencer created without sibling, failure.\n",
						     jException::sanity_error);

	stream << "<siblingmachine name=\"" << sibling_name << "\" />\n";

	get_loops_xml(stream);
	get_loop_sequence_xml(stream);
	pad.get_pad_xml(stream);

	// write controller envelopes to the xml stream
	std::map<std::string, ControllerEnvelope *>::iterator k;
	for(k  = controller_envelope.begin();
	    k != controller_envelope.end();
	    k++) {
		(*k).second->write_to_xml((*k).first, stream);
	}
	return stream.str();
}

/*************************************
 *
 * Class MachineSequencer - Private object functions
 *
 *************************************/

int MachineSequencer::internal_get_loop_id_at(int position) {
	if(!(position >= 0 && position < loop_sequence_length)) {
		return NOTE_NOT_SET;
	}

	return loop_sequence[position];
}

void MachineSequencer::internal_get_loop_ids_at(int *position_vector, int length) {
	int k;
	for(k = 0; k < length; k++) {
		position_vector[k] = internal_get_loop_id_at(position_vector[k]);
	}
}

void MachineSequencer::internal_set_loop_id_at(int position, int loop_id) {
	if(position < 0) throw ParameterOutOfSpec();

	if(!(position >= 0 && position < loop_sequence_length)) {
		try {
			set_sequence_length(position + 16);
		} catch(...) {
			throw ParameterOutOfSpec();
		}
		refresh_length();
	}
	if((loop_id != NOTE_NOT_SET) &&
	   (loop_id < 0 || loop_id >= loop_store_size || loop_store[loop_id] == NULL)
		) {
		throw ParameterOutOfSpec();
	}

	loop_sequence[position] = loop_id;
	inform_registered_callbacks_async();
}

int MachineSequencer::internal_add_new_loop() {
	// don't allow more than 99 loops... (current physical limit of pncsequencer.cc GUI design..)
	if(last_free_loop_store_position == 100) throw NoFreeLoopsAvailable();

	Loop *new_loop = LoopCreator::create();

	int k;
	for(k = 0; k < loop_store_size; k++) {
		if(loop_store[k] == NULL) {
			loop_store[k] = new_loop;
			last_free_loop_store_position = k + 1;
			SATAN_DEBUG("New loop added - nr loops: %d\n", last_free_loop_store_position);
			return k;
		}
	}
	try {
		double_loop_store();
	} catch(...) {
		delete new_loop;
		throw;
	}

	loop_store[k] = new_loop;
	last_free_loop_store_position = k + 1;
	SATAN_DEBUG("New loop added (store doubled) - nr loops: %d\n", last_free_loop_store_position);
	return k;
}

void MachineSequencer::prep_empty_loop_sequence() {
	loop_sequence_length = sequence_length;

	loop_sequence = (int *)malloc(sizeof(int) * loop_sequence_length);
	if(loop_sequence == NULL) {
		free(loop_store);
		throw jException("Failed to allocate memory for loop sequence.",
				 jException::syscall_error);
	}

	SATAN_DEBUG(" allocated seq at [%p], length: %d\n",
		    loop_sequence, loop_sequence_length);

	// init sequence to NOTE_NOT_SET
	int k;
	for(k = 0; k < loop_sequence_length; k++) {
		loop_sequence[k] = NOTE_NOT_SET;
	}
}

void MachineSequencer::get_loops_xml(std::ostringstream &stream) {
	int k;

	for(k = 0; k < loop_store_size; k++) {
		if(loop_store[k] != NULL) {
			loop_store[k]->get_loop_xml(stream, k);
		}
	}
}

void MachineSequencer::get_loop_sequence_xml(std::ostringstream &stream) {
	int k;

	stream << "<sequence>\n";
	for(k = 0; k < loop_sequence_length; k++) {
		int loopid = internal_get_loop_id_at(k);

		if(loopid != NOTE_NOT_SET)
			stream << "<entry pos=\"" << k << "\" id=\"" << loopid << "\" />\n";
	}
	stream << "</sequence>\n";
}

void MachineSequencer::get_midi_controllers(const std::string &name, int &coarse, int &fine) {
	Machine::Controller *c = NULL;
	coarse = -1;
	fine = -1;

	try {

		Machine *target = Machine::internal_get_by_name(sibling_name);
		c = target->internal_get_controller(name);

		if(c) {
			// we do not need the return value here, we can assume that
			// if the controller has a midi equivalent the coarse and fine integers will
			// be set accordingly. If the controller does NOT have a midi equivalent, the values
			// will both remain -1...
			(void) c->has_midi_controller(coarse, fine);

			delete c; // remember to delete c when done, otherwise we get a mem leak.. (we do not need it since we will use MIDI to control it)
		}

	} catch(...) {
		// before rethrowing, delete c if needed
		if(c) delete c;
		throw;
	}
}

std::set<std::string> MachineSequencer::internal_available_midi_controllers() {
	std::set<std::string> retval;

	Machine *target = Machine::internal_get_by_name(sibling_name);

	for(auto k : target->internal_get_controller_names()) {
		Machine::Controller *c = target->internal_get_controller(k);
		int coarse = -1, fine = -1;

		if(c->has_midi_controller(coarse, fine)) {
			retval.insert(k);
		}

		delete c; // remember to delete c when done, otherwise we get a mem leak..
	}

	return retval;
}

void MachineSequencer::create_controller_envelopes() {
	std::set<std::string>::iterator k;
	std::set<std::string> ctrs = internal_available_midi_controllers();
	for(k = ctrs.begin(); k != ctrs.end(); k++) {

		// only create ControllerEnvelope objects if non existing.
		std::map<std::string, ControllerEnvelope *>::iterator l;
		if((l = controller_envelope.find((*k))) == controller_envelope.end()) {

			ControllerEnvelope *nc = new ControllerEnvelope();
			controller_envelope[(*k)] = nc;
			get_midi_controllers((*k), nc->controller_coarse, nc->controller_fine);

		}
	}
}

void MachineSequencer::process_controller_envelopes(int tick, MidiEventBuilder *_meb) {

	std::map<std::string, ControllerEnvelope *>::iterator k;
	for(k  = controller_envelope.begin();
	    k != controller_envelope.end();
	    k++) {
		(*k).second->process_envelope(tick, _meb);
	}
}

void MachineSequencer::fill_buffers() {
	// get output signal buffer and clear it.
	Signal *out_sig = output[MACHINE_SEQUENCER_MIDI_OUTPUT_NAME];

	int output_limit = out_sig->get_samples();
	void **output_buffer = (void **)out_sig->get_buffer();

	memset(output_buffer,
	       0,
	       sizeof(void *) * output_limit
		);

	// synch to click track
	int sequence_position = get_next_sequence_position();
	int current_tick = get_next_tick();
	int next_tick_at = get_next_tick_at(_MIDI);
	int samples_per_tick = get_samples_per_tick(_MIDI);
	bool do_loop = get_loop_state();
	int loop_start = get_loop_start();
	int loop_stop = get_loop_length() + loop_start;
	int samples_per_tick_shuffle = get_samples_per_tick_shuffle(_MIDI);

	_meb.use_buffer(output_buffer, output_limit);

	while(next_tick_at < output_limit) {
		int overdraft = _meb.tell() - next_tick_at + 1;
		if(overdraft < 0) overdraft = 0;

		_meb.skip_to(next_tick_at + overdraft);

		pad.process(mute, PAD_TIME(sequence_position, current_tick), &_meb);

		if(current_tick == 0) {
			int loop_id = internal_get_loop_id_at(sequence_position);

			if(loop_id != NOTE_NOT_SET) {
				current_loop = loop_store[loop_id];
				current_loop->start_to_play();
			}
		}

		if(current_loop) {
			current_loop->process(mute, &_meb);
		}

		process_controller_envelopes(PAD_TIME(sequence_position, current_tick), &_meb);

		current_tick = (current_tick + 1) % MACHINE_TICKS_PER_LINE;

		if(current_tick == 0) {
			sequence_position++;

			if(do_loop && sequence_position >= loop_stop) {
				sequence_position = loop_start;
			}
		}

		if(sequence_position % 2 == 0) {
			next_tick_at += samples_per_tick - samples_per_tick_shuffle;
		} else {
			next_tick_at += samples_per_tick + samples_per_tick_shuffle;
		}

	}

	_meb.finish_current_buffer();

	// wrap around for next buffer
	next_tick_at -= output_limit;
}

void MachineSequencer::reset() {
	pad.reset();
}

/*************************************
 *
 * Class MachineSequencer - public object interface
 *
 *************************************/

int MachineSequencer::get_loop_id_at(int position) {
	typedef struct {
		MachineSequencer *thiz;
		int pos;
		int retval;
	} Param;
	Param param = {
		.thiz = this,
		.pos = position
	};

	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->retval = p->thiz->internal_get_loop_id_at(p->pos);
		},
		&param, true);

	return param.retval;
}

void MachineSequencer::get_loop_ids_at(int *position_vector, int length) {
	typedef struct {
		MachineSequencer *thiz;
		int *pos;
		int len;
	} Param;
	Param param = {
		.thiz = this,
		.pos = position_vector,
		.len = length
	};

	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->thiz->internal_get_loop_ids_at(p->pos, p->len);
		},
		&param, true);
}

void MachineSequencer::set_loop_id_at(int position, int loop_id) {
	typedef struct {
		MachineSequencer *thiz;
		int pos, l_id;
		bool parameter_out_of_spec;
	} Param;
	Param param = {
		.thiz = this,
		.pos = position,
		.l_id = loop_id,
		.parameter_out_of_spec = false
	};

	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;

			try {
				p->thiz->internal_set_loop_id_at(p->pos, p->l_id);
			} catch(ParameterOutOfSpec poos) {
				p->parameter_out_of_spec = true;
			}
		},
		&param, true);
	if(param.parameter_out_of_spec)
		throw ParameterOutOfSpec();
}

MachineSequencer::Loop *MachineSequencer::get_loop(int loop_id) {
	typedef struct {
		MachineSequencer *thiz;
		int lid;
		MachineSequencer::Loop *retval;
	} Param;
	Param param = {
		.thiz = this,
		.lid = loop_id
	};

	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;

			if(!(p->lid >= 0 && p->lid < p->thiz->loop_store_size)) {
				p->retval = NULL;
			} else
				p->retval = p->thiz->loop_store[p->lid];
		},
		&param, true);

	if(param.retval == NULL) {
		// do not return NULL pointers, if user tries to get an unallocated loop
		throw NoSuchLoop();
	}

	return param.retval;
}

int MachineSequencer::get_nr_of_loops() {
	return last_free_loop_store_position;
}

int MachineSequencer::add_new_loop() {
	typedef struct {
		MachineSequencer *thiz;
		int retval;
	} Param;
	Param param = {
		.thiz = this,
		.retval = -1
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			try {
				p->retval = p->thiz->internal_add_new_loop();
			} catch(NoFreeLoopsAvailable nfla) {
				p->retval = -1;
			}
		},
		&param, true);

	if(param.retval == -1)
		throw NoFreeLoopsAvailable();

	return param.retval;
}

void MachineSequencer::delete_loop(int id) {
	typedef struct {
		MachineSequencer *thiz;
		int _id;
	} Param;
	Param param = {
		.thiz = this,
		._id = id
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;

			int k;
			for(k = 0; k < p->thiz->loop_sequence_length; k++) {
				if(p->thiz->loop_sequence[k] == p->_id)
					p->thiz->loop_sequence[k] = NOTE_NOT_SET;

				if(p->thiz->loop_sequence[k] > p->_id)
					p->thiz->loop_sequence[k] = p->thiz->loop_sequence[k] - 1;
			}

			for(k = p->_id; k < p->thiz->loop_store_size - 1; k++) {
				p->thiz->loop_store[k] = p->thiz->loop_store[k + 1];
			}
			p->thiz->last_free_loop_store_position--;

		},
		&param, true);
}

bool MachineSequencer::is_mute() {
	return mute;
}

void MachineSequencer::set_mute(bool m) {
	mute = m;
}

std::set<std::string> MachineSequencer::available_midi_controllers() {
	typedef struct {
		MachineSequencer *thiz;
		std::set<std::string> retval;
	} Param;
	Param param = {
		.thiz = this
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->retval = p->thiz->internal_available_midi_controllers();
		},
		&param, true);

	return param.retval;
}

void MachineSequencer::assign_pad_to_midi_controller(const std::string &name) {
	typedef struct {
		MachineSequencer *thiz;
		const std::string &n;
	} Param;
	Param param = {
		.thiz = this,
		.n = name
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;

			int pad_controller_coarse = -1;
			int pad_controller_fine = -1;
			try {
				p->thiz->get_midi_controllers(p->n, pad_controller_coarse, pad_controller_fine);
			} catch(...) {
				/* ignore fault here */
			}
			p->thiz->pad.set_coarse_controller(pad_controller_coarse);
			p->thiz->pad.set_fine_controller(pad_controller_fine);
		},
		&param, true);
}

MachineSequencer::Pad *MachineSequencer::get_pad() {
	return &pad;
}

static std::vector<std::string> arpeggio_pattern_id_list;
static bool arpeggio_pattern_id_list_ready = false;
static void build_arpeggio_pattern_id_list() {
	if(arpeggio_pattern_id_list_ready) return;
	arpeggio_pattern_id_list_ready = true;

	for(int k = 0; k < MAX_BUILTIN_ARP_PATTERNS; k++) {
		std::stringstream bi_name;
		bi_name << "built-in #" << k;
		arpeggio_pattern_id_list.push_back(bi_name.str());
	}
}

void MachineSequencer::set_pad_arpeggio_pattern(const std::string identity) {
	build_arpeggio_pattern_id_list();

	typedef struct {
		MachineSequencer *thiz;
		int id;
	} Param;
	Param param = {
		.thiz = this,
		.id = -1 // -1 == disable arpeggio
	};

	for(unsigned int k = 0; k < arpeggio_pattern_id_list.size(); k++) {
		if(arpeggio_pattern_id_list[k] == identity) {
			param.id = k;
		}
	}

	machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			if(p->id == -1) {
				p->thiz->pad.set_mode(PadConfiguration::pad_normal);
				p->thiz->pad.set_arpeggio_pattern(-1);
			} else {
				p->thiz->pad.set_mode(PadConfiguration::pad_arpeggiator);
				p->thiz->pad.set_arpeggio_pattern(p->id);
				p->thiz->pad.arpeggiator.set_pattern(p->id);
			}
		},
		&param, true);
}

void MachineSequencer::set_pad_chord_mode(PadConfiguration::ChordMode pconf) {
	typedef struct {
		MachineSequencer *thiz;
		PadConfiguration::ChordMode cnf;
	} Param;
	Param param = {
		.thiz = this,
		.cnf = pconf
	};

	machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			SATAN_DEBUG("Chord selected (%p): %d\n", &(p->thiz->pad), p->cnf);
			p->thiz->pad.set_chord_mode(p->cnf);
		},
		&param, true);
}

void MachineSequencer::export_pad_to_loop(int loop_id) {
	if(loop_id == LOOP_NOT_SET)
		loop_id = add_new_loop();

	SATAN_DEBUG("Will export to loop number %d\n", loop_id);
	Loop *loop = get_loop(loop_id);

	loop->clear_loop();

	typedef struct {
		MachineSequencer *thiz;
		Loop *l;
	} Param;
	Param param = {
		.thiz = this,
		.l = loop
	};

	machine_operation_enqueue(
		[] (void *d) {

			int start_tick = 0;
			int stop_tick = -1;

			if(Machine::get_loop_state()) {
				start_tick = Machine::get_loop_start() * MACHINE_TICKS_PER_LINE;
				stop_tick = (Machine::get_loop_start() + Machine::get_loop_length()) * MACHINE_TICKS_PER_LINE;
			}

			Param *p = (Param *)d;
			try {
				p->thiz->pad.export_to_loop(start_tick, stop_tick, p->l);
			} catch(...) {
				SATAN_DEBUG("MachineSequencer::export_to_loop() - Exception caught.\n");
				throw;
			}
		},
		&param, true);
}

std::vector<std::string> MachineSequencer::get_pad_arpeggio_patterns() {
	build_arpeggio_pattern_id_list();
	return arpeggio_pattern_id_list;
}

Machine *MachineSequencer::get_machine() {
	typedef struct {
		MachineSequencer *thiz;
		Machine *m;
		std::function<Machine *(const std::string &name)> gbn;
	} Param;
	Param param = {
		.thiz = this
	};
	param.gbn = Machine::internal_get_by_name;

	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->m = p->gbn(p->thiz->sibling_name);
		},
		&param, true);

	return param.m;
}

const MachineSequencer::ControllerEnvelope *MachineSequencer::get_controller_envelope(const std::string &name) const {
	typedef struct {
		const MachineSequencer *thiz;
		const std::string &n;
		const MachineSequencer::ControllerEnvelope *retval;
	} Param;
	Param param = {
		.thiz = this,
		.n = name,
		.retval = NULL
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			if(p->thiz->controller_envelope.find(p->n) != p->thiz->controller_envelope.end()) {
				p->retval = (*(p->thiz->controller_envelope.find(p->n))).second;
			}
		},
		&param, true);

	if(param.retval == NULL)
		throw NoSuchController(name);

	return param.retval;
}

void MachineSequencer::update_controller_envelope(
	const ControllerEnvelope *original, const ControllerEnvelope *new_entry) {

	typedef struct {
		MachineSequencer *thiz;
		const ControllerEnvelope *orig;
		const ControllerEnvelope *newe;
	} Param;
	Param param = {
		.thiz = this,
		.orig = original,
		.newe = new_entry
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;

			std::map<std::string, ControllerEnvelope *>::iterator k;
			for(k  = p->thiz->controller_envelope.begin();
			    k != p->thiz->controller_envelope.end();
			    k++) {
				if((*k).second == p->orig) {
					(*k).second->set_to(p->newe);
					return;
				}
			}
		},
		&param, true);
}

void MachineSequencer::enqueue_midi_data(size_t len, const char* data) {
	char* data_copy = (char*)malloc(len);
	if(!data_copy) return;
	memcpy(data_copy, data, len);

	Machine::machine_operation_enqueue(
		[this, len, data_copy] (void *) {

			_meb.queue_midi_data(len, data_copy);
			free(data_copy);

		}, NULL, false);
}

/*************************************
 *
 * Class MachineSequencer - NON static stuff
 *
 *************************************/

MachineSequencer::~MachineSequencer() {
	{ // unmap machine2sequencer
		std::map<Machine *, MachineSequencer *>::iterator k;

		for(k = machine2sequencer.begin();
		    k != machine2sequencer.end();
		    k++) {
			if((*k).second == this) {
				machine2sequencer.erase(k);
				break;
			}
		}
	}

	{ // erase all ControllerEnvelope objects
		std::map<std::string, ControllerEnvelope *>::iterator k;
		for(k  = controller_envelope.begin();
		    k != controller_envelope.end();
		    k++) {
			delete ((*k).second);
		}
	}

	inform_registered_callbacks_async();
}

MachineSequencer::MachineSequencer(int project_interface_level, const KXMLDoc &machine_xml, const std::string &name) :
	//
	//  project_interface_level  < 5 == Pad uses old absolute coordinates
	//                          => 5 == Pad uses new relative coordinates
	//
	Machine(name, true, 0.0f, 0.0f),
	current_loop(NULL),
	mute(false),
	sibling_name(""),
	loop_store(NULL),
	loop_store_size(0),
	last_free_loop_store_position(0x0000),
	loop_sequence(NULL),
	loop_sequence_length(0)
{
	// "c" for "counter", used to count through xml nodes..
	int c, c_max = 0;

	output_descriptor[MACHINE_SEQUENCER_MIDI_OUTPUT_NAME] =
		Signal::Description(_MIDI, 1, false);
	setup_machine();

	pad.load_pad_from_xml(project_interface_level, machine_xml);

	sibling_name = machine_xml["siblingmachine"].get_attr("name");

	/**********************
	 *
	 *  Read in loops
	 *
	 */

	// get number of loop entries in XML
	try {
		c_max = machine_xml["loop"].get_count();
	} catch(...) {
		// ignore
	}

	// get maximum loop id, so we can set up loop store correctly
	int max_loop_id = -1;
	for(c = 0; c < c_max; c++) {
		KXMLDoc loop_xml = machine_xml["loop"][c];
		int loop_id;
		loop_id = -1;
		KXML_GET_NUMBER(loop_xml,"id",loop_id,-1);
		if(loop_id > max_loop_id) max_loop_id = loop_id;
	}

	// allocate loop store and clear it
	loop_store_size = MACHINE_SEQUENCER_INITIAL_SEQUENCE_LENGTH > max_loop_id ?
		MACHINE_SEQUENCER_INITIAL_SEQUENCE_LENGTH : max_loop_id;
	loop_store = (Loop **)malloc(sizeof(Loop *) * loop_store_size);
	if(loop_store == NULL)
		throw jException("Failed to allocate memory for initial loop store.",
				 jException::syscall_error);
	memset(loop_store, 0, sizeof(Loop *) * loop_store_size);

	// then we read the actual loops here
	for(c = 0; c < c_max; c++) {
		KXMLDoc loop_xml = machine_xml["loop"][c];

		int loop_id;
		loop_id = -1;
		KXML_GET_NUMBER(loop_xml,"id",loop_id,-1);

		if(loop_id != -1) {
			Loop *new_loop = LoopCreator::create(loop_xml);
			loop_store[loop_id] = new_loop;
			if(loop_id > last_free_loop_store_position)
				last_free_loop_store_position = loop_id;
		}
	}
	last_free_loop_store_position++;

	/*****************
	 *
	 * OK - loops are done, read the sequence
	 *
	 */
	KXMLDoc sequence_xml = machine_xml["sequence"];
	// get number of sequence entries in XML
	try {
		c_max = sequence_xml["entry"].get_count();
	} catch(...) {
		c_max = 0; // no "entry" nodes available
	}

	SATAN_DEBUG_("---------------------------------------------\n");
	SATAN_DEBUG("  READING SEQ FOR [%s]\n", name.c_str());

	prep_empty_loop_sequence();

	for(c = 0; c < c_max; c++) {
		KXMLDoc s_entry_xml = sequence_xml["entry"][c];

		int pos, id;
		KXML_GET_NUMBER(s_entry_xml,"pos",pos,-1);
		KXML_GET_NUMBER(s_entry_xml,"id",id,-1);

		if(pos != -1 && id != -1)
			internal_set_loop_id_at(pos, id);

		SATAN_DEBUG("    SEQ[%d] = %d\n", pos, id);
	}

	SATAN_DEBUG("  SEQ WAS FOR [%s]\n", name.c_str());
	SATAN_DEBUG_("---------------------------------------------\n");

	/*********************
	 *
	 * Time to read the controller envelopes
	 *
	 */
	// get number of envelope entries in XML
	try {
		c_max = machine_xml["envelope"].get_count();
	} catch(...) {
		c_max = 0; // no "entry" nodes available
	}

	for(c = 0; c < c_max; c++) {
		KXMLDoc e_entry_xml = machine_xml["envelope"][c];

		{
			ControllerEnvelope *nc = new ControllerEnvelope();
			try {
				std::string env_id = nc->read_from_xml(e_entry_xml);

				{ // if there is already a controller envelope object, delete it and remove the entry
					std::map<std::string, ControllerEnvelope *>::iterator k;
					k = controller_envelope.find(env_id);
					if(k != controller_envelope.end() &&
					   (*k).second != NULL) {
						delete (*k).second;
						controller_envelope.erase(k);
					}
				}
				// insert new entry
				controller_envelope[env_id] = nc;
			} catch(...) {
				delete nc;
				throw;
			}
		}
	}
}

MachineSequencer::MachineSequencer(const std::string &name_root) :
	Machine(name_root + "___MACHINE_SEQUENCER", true, 0.0f, 0.0f),
	current_loop(NULL),
	mute(false),
	sibling_name(name_root),
	loop_store(NULL),
	loop_store_size(0),
	last_free_loop_store_position(0x0000),
	loop_sequence(NULL),
	loop_sequence_length(0)
{
	// create the controller envelopes
	create_controller_envelopes();

	output_descriptor[MACHINE_SEQUENCER_MIDI_OUTPUT_NAME] =
		Signal::Description(_MIDI, 1, false);
	setup_machine();

	loop_store_size = MACHINE_SEQUENCER_INITIAL_SEQUENCE_LENGTH;
	loop_store = (Loop **)malloc(sizeof(Loop *) * loop_store_size);
	if(loop_store == NULL)
		throw jException("Failed to allocate memory for initial loop store.",
				 jException::syscall_error);
	memset(loop_store, 0, sizeof(Loop *) * loop_store_size);

	prep_empty_loop_sequence();

	// Create a loop to start
	internal_add_new_loop();
}

void MachineSequencer::double_loop_store() {
	int new_size = loop_store_size * 2;
	Loop **new_store = (Loop **)malloc(sizeof(Loop *) * new_size);

	if(new_store == NULL)
		throw jException("Failed to allocate memory for doubled loop store.",
				 jException::syscall_error);

	memset(new_store, 0, sizeof(Loop *) * new_size);

	int k;
	for(k = 0; k < loop_store_size; k++)
		new_store[k] = loop_store[k];

	free(loop_store);
	loop_store = new_store;
	loop_store_size = new_size;
}

void MachineSequencer::refresh_length() {
	if(loop_sequence_length >= sequence_length) {
		return; // no need to change
	}

	int new_size = sequence_length;

	int *new_seq = (int *)malloc(sizeof(int) * new_size);

	int
		k,
		mid_term = new_size < loop_sequence_length ? new_size : loop_sequence_length;
	for(k = 0;
	    k < mid_term;
	    k++) {
		new_seq[k] = loop_sequence[k];
	}
	for(k = mid_term;
	    k < new_size;
	    k++) {
		new_seq[k] = NOTE_NOT_SET;
	}

	if(loop_sequence != NULL) free(loop_sequence);

	loop_sequence = new_seq;
	loop_sequence_length = new_size;
}

void MachineSequencer::write_MIDI_info_track(MidiExport *target) {
	// create the midi chunk
	MidiExport::Chunk *midi_chunk = target->append_new_chunk("MTrk");

	uint32_t msPerQuarterNote;

	msPerQuarterNote = 60000000 / Machine::get_bpm();

	// push "Copyright notice" meta event
	midi_chunk->push_u32b_word_var_len(0);
	midi_chunk->push_u8b_word(0xff);
	midi_chunk->push_u8b_word(0x02);
	midi_chunk->push_string("Copyrighted Material");

	// push "Cue point" meta event
	midi_chunk->push_u32b_word_var_len(0);
	midi_chunk->push_u8b_word(0xff);
	midi_chunk->push_u8b_word(0x07);
	midi_chunk->push_string("Created by SATAN for Android");

	// push "Set Tempo" meta event
	midi_chunk->push_u32b_word_var_len(0);
	midi_chunk->push_u8b_word(0xff);
	midi_chunk->push_u8b_word(0x51);
	midi_chunk->push_u32b_word_var_len(3);
	midi_chunk->push_u8b_word((msPerQuarterNote & 0xff0000) >> 16);
	midi_chunk->push_u8b_word((msPerQuarterNote & 0x00ff00) >>  8);
	midi_chunk->push_u8b_word((msPerQuarterNote & 0x0000ff) >>  0);

	// push "Time Signature" meta event
	midi_chunk->push_u32b_word_var_len(0);
	midi_chunk->push_u8b_word(0xff);
	midi_chunk->push_u8b_word(0x58);
	midi_chunk->push_u32b_word_var_len(4);
	midi_chunk->push_u8b_word(0x02);
	midi_chunk->push_u8b_word(0x04);
	midi_chunk->push_u8b_word(0x24);
	midi_chunk->push_u8b_word(0x08);

	// push "end of track" event
	midi_chunk->push_u32b_word_var_len(0);
	midi_chunk->push_u8b_word(0xff);
	midi_chunk->push_u8b_word(0x2f);
	midi_chunk->push_u32b_word_var_len(0);
}

// returns the nr of loops exported (= midi tracks)
int MachineSequencer::export_loops2midi_file(MidiExport *target) {
	return 0;
}

// will export entire sequence as one track
bool MachineSequencer::export_sequence2midi_file(MidiExport *target) {
	bool was_dropped = true; // assume most sequences will be dropped from the final midi file

	// create the midi chunk
	MidiExport::Chunk *midi_chunk = target->append_new_chunk("MTrk");

	// push "Track Name" meta event
	midi_chunk->push_u32b_word_var_len(0);
	midi_chunk->push_u8b_word(0xff);
	midi_chunk->push_u8b_word(0x03);
	midi_chunk->push_string(sibling_name);

	/*************
	 *
	 *  Generate note related midi data
	 *
	 *************/
	Loop *l = NULL; // loop currently being exported

	int s_p = 0, l_s = 0, l_p = 0; // sequence position, loop sequence position, current loop position
	int last_p = 0;
	// keep track of the off position which is last
	int highest_off_p = -1;

	const NoteEntry *ndata = NULL; // note data

	std::map<int, NoteEntry *> notes_to_off; // currently active notes that we should stop (integer value is at which value of s_p when we should stop it)

	for(s_p = 0; s_p < loop_sequence_length || s_p < highest_off_p; s_p++, l_p++) {
		if(s_p < loop_sequence_length && loop_sequence[s_p] != NOTE_NOT_SET) {
			l_p = 0;
			l_s = s_p;
			l = loop_store[loop_sequence[s_p]];
			ndata = l->notes_get();
		}

		if(ndata != NULL) {
			while(ndata != NULL && ndata->on_at <= PAD_TIME(l_p, 0)) {
				NoteEntry *new_one = new NoteEntry(ndata);
				int off_position = l_s + PAD_TIME_LINE(ndata->on_at) + ndata->length + 1;

				SATAN_DEBUG("MEXPORT: on: %d, len: %d\n",
					    ndata->on_at, ndata->length);

				if(off_position > highest_off_p)
					highest_off_p = off_position;

				/***
				 * first we handle the book keeping
				 ***/
				std::map<int, NoteEntry *>::iterator k;

				k = notes_to_off.find(off_position);

				// if others exist on this off position, chain them to the new one first
				if(k != notes_to_off.end()) {
					new_one->next = (*k).second;
				}

				notes_to_off[off_position] = new_one;

				ndata = ndata->next;

				/***
				 * then we generate the MIDI export data, a note on message
				 ***/
				was_dropped = false; // indicate we wrote useful data to this track.
				uint32_t offset = (s_p - last_p) * MACHINE_TICKS_PER_LINE;
				last_p = s_p;
				midi_chunk->push_u32b_word_var_len(offset);
				midi_chunk->push_u8b_word(0x90 | (0x0f & new_one->channel));
				midi_chunk->push_u8b_word(new_one->note);
				midi_chunk->push_u8b_word(new_one->velocity);

			}
		}

		std::map<int, NoteEntry *>::iterator current_to_off = notes_to_off.find(s_p);
		if(current_to_off != notes_to_off.end()) {
			NoteEntry *tip = (*current_to_off).second;
			notes_to_off.erase(current_to_off);

			while(tip != NULL) {
				uint32_t offset = (s_p - last_p) * MACHINE_TICKS_PER_LINE;
				last_p = s_p;
				midi_chunk->push_u32b_word_var_len(offset);
				midi_chunk->push_u8b_word(0x80 | (0x0f & tip->channel));
				midi_chunk->push_u8b_word(tip->note);
				midi_chunk->push_u8b_word(tip->velocity);

				NoteEntry *deletable_off = tip;
				tip = tip->next; delete deletable_off;
			}
		}
	}

	// push "end of track" event
	midi_chunk->push_u32b_word_var_len(0);
	midi_chunk->push_u8b_word(0xff);
	midi_chunk->push_u8b_word(0x2f);
	midi_chunk->push_u32b_word_var_len(0);

	if(was_dropped) midi_chunk->drop();

	return !was_dropped;
}

/*************************************
 *
 * Class MachineSequencer - STATIC stuff
 *
 *************************************/

std::vector<MachineSequencer *> MachineSequencer::machine_from_xml_cache;
std::map<Machine *, MachineSequencer *> MachineSequencer::machine2sequencer;
int MachineSequencer::sequence_length = MACHINE_SEQUENCER_INITIAL_SEQUENCE_LENGTH;
std::vector<MachineSequencerSetChangeCallbackF_t> MachineSequencer::set_change_callback;

void MachineSequencer::inform_registered_callbacks_async() {
	if(Machine::internal_get_load_state()) return; // if in loading state, don't bother with this.

	std::vector<MachineSequencerSetChangeCallbackF_t>::iterator k;
	for(k  = set_change_callback.begin();
	    k != set_change_callback.end();
	    k++) {
		auto f = (*k);
		run_async_function(
			[f]() {
				f(NULL);
			}
			);
	}
}

void MachineSequencer::set_sequence_length(int newv) {
	sequence_length = newv;

	{
		std::map<Machine *, MachineSequencer *>::iterator k;

		for(k =  machine2sequencer.begin();
		    k !=  machine2sequencer.end();
		    k++) {
			(*k).second->refresh_length();
		}
	}
}

/*************************************
 *
 * Class MachineSequencer - PUBLIC STATIC stuff
 *
 *************************************/

int MachineSequencer::quantize_tick(int start_tick) {
	int offset = start_tick & MACHINE_TICK_BITMASK;

	// chop of the additional ticks
	int new_tick = start_tick & MACHINE_LINE_BITMASK;


	SATAN_DEBUG("offset: (%x => %x) %x > %x ? %s\n",
		    start_tick, new_tick,
		    offset, ((0x1 << BITS_PER_LINE) >> 1),
		    offset > ((0x1 << BITS_PER_LINE) >> 1) ? "yes" : "no");

	// if offset is larger than half a line, move to next line
	if(offset > ((0x1 << BITS_PER_LINE) >> 1)) {
		new_tick += (0x1 << BITS_PER_LINE);
	}

	return new_tick;
}

void MachineSequencer::presetup_from_xml(int project_interface_level, const KXMLDoc &machine_xml) {
	typedef struct {
		const KXMLDoc &mxml;
		int pil;
	} Param;
	Param param = {
		.mxml = machine_xml,
		.pil = project_interface_level
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			std::string iname = p->mxml.get_attr("name"); // instance name
			MachineSequencer *result = new MachineSequencer(p->pil, p->mxml, iname);
			machine_from_xml_cache.push_back(result);
		},
		&param, true);
}

void MachineSequencer::finalize_xml_initialization() {
	typedef struct {
		std::function<Machine *(const std::string &name)> internal_get_by_name;
	} Param;
	Param param;
	param.internal_get_by_name = Machine::internal_get_by_name;

	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			std::vector<MachineSequencer *>::iterator k;

			for(k  = machine_from_xml_cache.begin();
			    k != machine_from_xml_cache.end();
			    k++) {
				MachineSequencer *mseq = (*k);
				if(mseq) {
					Machine *sibling = NULL;

					sibling = p->internal_get_by_name(mseq->sibling_name);

					if(sibling == NULL)
						throw jException("Project file contains a MachineSequencer, but no matching sibling ]" +
								 mseq->sibling_name +
								 "[", jException::sanity_error);

					mseq->tightly_connect(sibling);
					machine2sequencer[sibling] = mseq;

					// create the controller envelopes
					mseq->create_controller_envelopes();
				}
			}

			// we're done now - clear it out
			machine_from_xml_cache.clear();

			// this will make sure all sequence lengths are synchronized
			set_sequence_length(sequence_length);

			inform_registered_callbacks_async();
		},
		&param, true);

}

void MachineSequencer::create_sequencer_for_machine(Machine *sibling) {
	{
		bool midi_input_found = false;

		try {
			(void) sibling->get_input_index(MACHINE_SEQUENCER_MIDI_INPUT_NAME);
			midi_input_found = true;
		} catch(...) {
			// ignore
		}

		if(!midi_input_found) return; // no midi input to attach MachineSequencer too
	}

	{
		Machine::machine_operation_enqueue(
			[] (void *d) {
				Machine *sib = (Machine *)d;
				if(machine2sequencer.find(sib) !=
				   machine2sequencer.end()) {
					throw jException("Trying to create MachineSequencer for a machine that already has one!",
							 jException::sanity_error);
				}
			},
			sibling, true);
	}
	MachineSequencer *mseq =
		new MachineSequencer(sibling->get_name());

	sibling->attach_input(mseq,
			      MACHINE_SEQUENCER_MIDI_OUTPUT_NAME,
			      MACHINE_SEQUENCER_MIDI_INPUT_NAME);

	{
		typedef struct {
			MachineSequencer *ms;
			Machine *sib;
		} Param;
		Param param = {
			.ms = mseq,
			.sib = sibling
		};
		Machine::machine_operation_enqueue(
			[] (void *d) {
				Param *p = (Param *)d;
				/* Make sure these machines are tightly connected
				 * so that they can not exist without the other
				 */
				p->ms->tightly_connect(p->sib);
				machine2sequencer[p->sib] = p->ms;
				inform_registered_callbacks_async();
			},
			&param, true);
	}
}

MachineSequencer *MachineSequencer::get_sequencer_for_machine(Machine *m) {
	typedef struct {
		Machine *machine;
		MachineSequencer *retval;
	} Param;
	Param param = {
		.machine = m,
		.retval = NULL
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;

			std::map<Machine *, MachineSequencer *>::iterator k;

			for(k =  machine2sequencer.begin();
			    k !=  machine2sequencer.end();
			    k++) {
				if((*k).first == p->machine) {
					p->retval = (*k).second;
				}
			}
		},
		&param, true);

	return param.retval;
}

std::map<std::string, MachineSequencer *, ltstr> MachineSequencer::get_sequencers() {
	std::map<Machine *, MachineSequencer *> m2s;

	Machine::machine_operation_enqueue(
		[] (void *d) {
			std::map<Machine *, MachineSequencer *> *_m2s =
				(std::map<Machine *, MachineSequencer *> *)d;
			*_m2s = machine2sequencer;
		},
		&m2s, true);

	std::map<std::string, MachineSequencer *, ltstr> retval;

	// rest of processing done outside the lock
	std::map<Machine *, MachineSequencer *>::iterator k;
	for(k =  m2s.begin();
	    k !=  m2s.end();
	    k++) {
		retval[(*k).first->get_name()] = (*k).second;
	}

	return retval;
}

void MachineSequencer::register_change_callback(void (*callback_f_p)(void *)) {
	typedef struct {
		void (*cfp)(void *);
	} Param;
	Param param = {
		.cfp = callback_f_p
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			set_change_callback.push_back(p->cfp);
		},
		&param, true);
}

void MachineSequencer::export2MIDI(bool _just_loops, const std::string &_output_path) {
	typedef struct {
		bool jloops;
		const std::string &opth;
	} Param;
	Param param = {
		.jloops = _just_loops,
		.opth = _output_path
	};

	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			bool just_loops = p->jloops;
			const std::string &output_path = p->opth;

			MidiExport *target = new MidiExport(output_path);

			try {
				// first we create the header-chunk, since it must be first in the file
				// please note that the contents of the header chunk is created last
				// since it depends on how many tracks we will export which we do not know yet
				MidiExport::Chunk *header = target->append_new_chunk("MThd");
				uint16_t nr_of_tracks = 1;

				// write information container track
				write_MIDI_info_track(target);

				// step through all sequences / loops
				std::map<Machine *, MachineSequencer *>::iterator k;
				for(k  = machine2sequencer.begin();
				    k != machine2sequencer.end();
				    k++) {
					if(just_loops)
						nr_of_tracks +=
							(*k).second->export_loops2midi_file(target);
					else {
						if((*k).second->export_sequence2midi_file(target))
							nr_of_tracks++;
					}
				}

				// create the content of the header chunk
				header->push_u16b_word(1);
				header->push_u16b_word(nr_of_tracks);
				header->push_u16b_word(MACHINE_TICKS_PER_LINE * get_lpb());

				// OK, finalize the file (dump it to disk...)
				target->finalize_file();

				// then we delete the construct.
				delete target;
			} catch(...) {
				delete target;

				throw;
			}
		},
		&param, true);
}
