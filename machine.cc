/*
 * VuKNOB
 * Copyright (C) 2014 by Anton Persson
 *
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
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

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <jngldrum/jinformer.hh>

#include "machine.hh"

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <fixedpointmath.h>

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/*****************************
 *                           *
 * class Machine::Controller *
 *                           *
 *****************************/

Machine::Controller::Controller(
	Type tp,
	const std::string &_name, const std::string &_title,
	void *_ptr,
	const std::string &min,
	const std::string &max,
	const std::string &step,
	const std::map<int, std::string> _enumnames,
	bool _float_is_FTYPE)
	: has_midi_ctrl(false), coarse_controller(-1), fine_controller(-1) {

	float_is_FTYPE = _float_is_FTYPE;
	type = tp;
	name = _name;
	title = _title;
	ptr = _ptr;
	
	min_i = -32768;
	min_f = 0.0;
	if(min != "") {
		if(tp == c_int || tp == c_enum) {
			std::string strng = min;
			std::istringstream st(strng);
			st >> min_i; 
		} else if(tp == c_float) {
			std::string strng = min;
			std::istringstream st(strng);
			st >> min_f; 
		}		
	}
	
	max_i = 32767;
	max_f = 1.0;
	if(max != "") {
		if(tp == c_int || tp == c_enum) {
			std::string strng = max;
			std::istringstream st(strng);
			st >> max_i; 
		} else if(tp == c_float) {
			std::string strng = max;
			std::istringstream st(strng);
			st >> max_f; 
		}
	}
	step_f = (max_f - min_f) / 10.0;
	if(step != "") {
		if(tp == c_float) {
			std::string strng = step;
			std::istringstream st(strng);
			st >> step_f; 
		}
	}

	if(type == c_sigid) {
		min_i = 0;
		max_i = 255;
	}

	enumnames = _enumnames;
}

void Machine::Controller::set_midi_controller(int _coarse, int _fine) {
	if(has_midi_ctrl)
		throw jException("Controller already mapped to MIDI controller!", jException::sanity_error);

	if((_coarse < 0 || _coarse > 127) ||
	   ((_fine != -1) && (_fine < -1 || _fine > 127)) ||
	   _coarse == _fine)
		throw jException("Trying to set illegal MIDI controller values.",
				 jException::sanity_error);
	
	has_midi_ctrl = true;

	coarse_controller = _coarse;
	fine_controller = _fine;
}

std::string Machine::Controller::get_name() {
	return name;
}

std::string Machine::Controller::get_title() {
	return title;
}

Machine::Controller::Type Machine::Controller::get_type() {
	return type;
}

void Machine::Controller::get_min(float &val) {
	val = min_f;
}

void Machine::Controller::get_max(float &val) {
	val = max_f;
}

void Machine::Controller::get_step(float &val) {
	val = step_f;
}

void Machine::Controller::get_min(int &val) {
	val = min_i;
}

void Machine::Controller::get_max(int &val) {
	val = max_i;
}

void Machine::Controller::get_step(int &val) {
	val = step_i;
}

void Machine::Controller::internal_get_value(int *val) {
	*val = *((int *)ptr);
}

void Machine::Controller::internal_get_value(float *val) {
	if(float_is_FTYPE) 
		*val = FTYPEtof(*((FTYPE *)ptr));
	else
		*val = *((float *)ptr);
}

void Machine::Controller::internal_get_value(bool *val) {
	int k = *((int *)ptr);
	if(k) *val = true;
	else *val = false;
}

void Machine::Controller::internal_get_value(std::string *val) {
	*val = *((char *)ptr);	
}

void Machine::Controller::internal_set_value(int *val) {
	if((*val) >= min_i && (*val) <= max_i) 
		*((int *)ptr) = *val;
}

void Machine::Controller::internal_set_value(float *val) {
	if((*val) >= min_f && (*val) <= max_f) {
		if(float_is_FTYPE)
			*((FTYPE *)ptr) = ftoFTYPE(*val);
		else
			*((float *)ptr) = *val;
	}
}

void Machine::Controller::internal_set_value(bool *val) {
	*((int *)ptr) = (*val) ? -1 : 0;
}

void Machine::Controller::internal_set_value(std::string *val) {
	char *str = (char *)ptr;
	strncpy(str, val->c_str(), STRING_CONTROLLER_SIZE);
}

void Machine::Controller::CALL_internal_get_value_int(void *p) {
	std::pair<Machine::Controller *, int *> *d = (std::pair<Machine::Controller *, int *> *)p;
	d->first->internal_get_value(d->second);
}

void Machine::Controller::CALL_internal_get_value_float(void *p) {
	std::pair<Machine::Controller *, float *> *d = (std::pair<Machine::Controller *, float *> *)p;
	d->first->internal_get_value(d->second);
}

void Machine::Controller::CALL_internal_get_value_bool(void *p) {
	std::pair<Machine::Controller *, bool *> *d = (std::pair<Machine::Controller *, bool *> *)p;
	d->first->internal_get_value(d->second);
}

void Machine::Controller::CALL_internal_get_value_string(void *p) {
	std::pair<Machine::Controller *, std::string *> *d = (std::pair<Machine::Controller *, std::string *> *)p;
	d->first->internal_get_value(d->second);
}

void Machine::Controller::CALL_internal_set_value_int(void *p) {
	std::pair<Machine::Controller *, int *> *d = (std::pair<Machine::Controller *, int *> *)p;
	d->first->internal_set_value(d->second);
}

void Machine::Controller::CALL_internal_set_value_float(void *p) {
	std::pair<Machine::Controller *, float *> *d = (std::pair<Machine::Controller *, float *> *)p;
	d->first->internal_set_value(d->second);
}

void Machine::Controller::CALL_internal_set_value_bool(void *p) {
	std::pair<Machine::Controller *, bool *> *d = (std::pair<Machine::Controller *, bool *> *)p;
	d->first->internal_set_value(d->second);
}

void Machine::Controller::CALL_internal_set_value_string(void *p) {
	std::pair<Machine::Controller *, std::string *> *d = (std::pair<Machine::Controller *, std::string *> *)p;
	d->first->internal_set_value(d->second);
}

void Machine::Controller::get_value(int &val) {
	std::pair<Machine::Controller *, int *> param;
	param.first = this;
	param.second = &val;
	Machine::machine_operation_enqueue(CALL_internal_get_value_int, &param, true);
}

void Machine::Controller::get_value(float &val) {
	std::pair<Machine::Controller *, float *> param;
	param.first = this;
	param.second = &val;
	Machine::machine_operation_enqueue(CALL_internal_get_value_float, &param, true);
}

void Machine::Controller::get_value(bool &val) {
	std::pair<Machine::Controller *, bool *> param;
	param.first = this;
	param.second = &val;
	Machine::machine_operation_enqueue(CALL_internal_get_value_bool, &param, true);
}

void Machine::Controller::get_value(std::string &val) {
	std::pair<Machine::Controller *, std::string *> param;
	param.first = this;
	param.second = &val;
	Machine::machine_operation_enqueue(CALL_internal_get_value_string, &param, true);
}

std::string Machine::Controller::get_value_name(const int &val) {
	if(type == Machine::Controller::c_sigid) {
		std::ostringstream vstr;

		vstr << "[" << val << "] ";
		
		StaticSignal *ssig = StaticSignal::get_signal(val);

		if(ssig) {
			vstr << ssig->get_name();
		} else {
			vstr << "no file loaded";
		}
		return vstr.str();
	}	
	
	SATAN_DEBUG("get_value_name for %d\n", val);
	if(enumnames.find(val) == enumnames.end())
		throw jException("Value out of range for enum.", jException::sanity_error);
	SATAN_DEBUG("    get_value_name is %s\n", enumnames[val].c_str());

	return enumnames[val];
}

void Machine::Controller::set_value(int &val) {
	std::pair<Machine::Controller *, int *> param;
	param.first = this;
	param.second = &val;
	Machine::machine_operation_enqueue(CALL_internal_set_value_int, &param, true);
}

void Machine::Controller::set_value(float &val) {
	std::pair<Machine::Controller *, float *> param;
	param.first = this;
	param.second = &val;
	Machine::machine_operation_enqueue(CALL_internal_set_value_float, &param, true);
}

void Machine::Controller::set_value(bool &val) {
	std::pair<Machine::Controller *, bool *> param;
	param.first = this;
	param.second = &val;
	Machine::machine_operation_enqueue(CALL_internal_set_value_bool, &param, true);
}

void Machine::Controller::set_value(std::string &val) {
	std::pair<Machine::Controller *, std::string *> param;
	param.first = this;
	param.second = &val;
	Machine::machine_operation_enqueue(CALL_internal_set_value_string, &param, true);
}

bool Machine::Controller::has_midi_controller(int &_coarse, int &_fine) {
	if(has_midi_ctrl) {
		if(coarse_controller < 0 || coarse_controller > 127)
			throw jException("BUG! Midi controller erronous!", jException::sanity_error);
	
		_coarse = coarse_controller;
		if(fine_controller >= 0 && fine_controller < 128)
			_fine = fine_controller;
	}

	return has_midi_ctrl;
}

/***************************
 *                         *
 *      class Machine      *
 * Internal Representation *
 *     of Public functions *
 *                         *
 ***************************/

void Machine::internal_attach_input(Machine *m,
				    const std::string &output_name,
				    const std::string &input_name) {
	// is m valid?
	if(m == NULL)
		throw jException("Source machine pointer is NULL.", jException::sanity_error);

	// find output on m
	Signal *s = m->get_output(output_name);
	if(s == NULL)
		throw jException(std::string("[") + output_name + "]: No such output.", jException::sanity_error);

	// find input on this
	std::map<std::string, Signal::Description>::iterator des;
	des = input_descriptor.find(input_name);
	if(des == input_descriptor.end())
		throw jException(std::string("[") + input_name + "]: No such input", jException::sanity_error);

	// match signals here
	if(  /* if this logic is true, the signals can not
	      * be connected
	      */
		
		((*des).second.dimension !=
		 s->get_dimension())
		
		||

		// new check for MACHINE_PROJECT_INTERFACE_LEVEL >= 2
		// we now allow a mono signal to connect to a signal with more channels, if the destination is premixed
		// the mono signal will from now on be auto-duplexed. Hopefully this will improve the user experience...
		(
			((s->get_channels() != 1)  || !((*des).second.premix))

			&&

			(
				((*des).second.channels !=
				 s->get_channels())

			)
		)
		
		) {
		throw jException(
			std::string("The signal type of output [") +
			output_name + "] does not match input [" +
			input_name + "] when trying to connect [" +
			m->name +
			"] with [" +
			name +
			"]."
			, jException::sanity_error);
	}

	// check for loops
	if(m->find_machine_in_graph(this)) {
		throw jException("Connection would create a loop.\n",
				 jException::sanity_error);
	}
	
	// attach signal to input
	std::map<std::string, Signal *>::iterator k;
	k = input.find(input_name);
	if(k == input.end())
		input[input_name] = s;
	else
		(*k).second->link(this, s);
	s->attach(this);
	
	// increase dependant count
	std::map<Machine *, int>::iterator i;
	i = dependant.find(m);
	int c = 0;
	if(i != dependant.end()) {
		c = (*i).second;
	}
	c++;
	dependant[m] = c;

	recalculate_render_chain();

	{
		Machine *source_machine = m;
		Machine *destination_machine = this;

		broadcast_attach(source_machine, destination_machine, output_name, input_name);
	}
}

void Machine::internal_detach_input(Machine *m,
				    const std::string &output_name,
				    const std::string &input_name) {
	// is m valid?
	if(m == NULL)
		throw jException("Source machine pointer is NULL.", jException::sanity_error);
	
	// find output on m
	Signal *s = m->get_output(output_name);
	if(s == NULL)
		throw jException(std::string("[") + output_name + "]: No such output.", jException::sanity_error);

	// find input on this
	std::map<std::string, Signal::Description>::iterator des;
	des = input_descriptor.find(input_name);
	if(des == input_descriptor.end())
		throw jException(std::string("[") + input_name + "]: No such input", jException::sanity_error);

	// deattach signal from input
	std::map<std::string, Signal *>::iterator k;
	k = input.find(input_name);
	if(k == input.end())
		// no output attached at this input
		throw jException(std::string("[") + input_name + "]: Nothing attached to input", jException::sanity_error);		
	else {
		s->detach(this);
		
		Signal *head = (*k).second->unlink(this, s);
		if(head != NULL)
			input[(*k).first] = head;
		else
			input.erase(k);
	}
	
	// decrease dependant count
	std::map<Machine *, int>::iterator i;
	i = dependant.find(m);
	int c = 0;
	if(i == dependant.end()) {
		throw jException(std::string("[") + input_name + "]: dependant count error!", jException::sanity_error);
	}
	c = (*i).second;
	c--;
	if(c == 0) {
		dependant.erase(i);
	} else {
		dependant[m] = c;
	}
	recalculate_render_chain();
	{
		Machine *source_machine = m;
		Machine *destination_machine = this;
		
		broadcast_detach(source_machine, destination_machine, output_name, input_name);
	}
}

typedef struct {
	Machine *thiz;
	std::string result; 
} CALL_internal_get_base_xml_description_t;
__MACHINE_OPERATION_CALLBACK Machine::CALL_internal_get_base_xml_description(void *p) {
	CALL_internal_get_base_xml_description_t *input =
		(CALL_internal_get_base_xml_description_t *)p;
	input->result = input->thiz->internal_get_base_xml_description();
}

std::string Machine::internal_get_base_xml_description() {
	std::string result;

	result  = "<machine class=\"";
	result += get_class_name();
	result += "\" name=\"";
	result += name;
	result += "\">\n\n";

	result += get_descriptive_xml();

	result += get_controller_xml();
	
	result += "\n</machine>\n";

	return result;
}

typedef struct {
	Machine *thiz;
	std::string result; 
} CALL_internal_get_connection_xml_t;
__MACHINE_OPERATION_CALLBACK Machine::CALL_internal_get_connection_xml(void *p) {
	CALL_internal_get_connection_xml_t *input =
		(CALL_internal_get_connection_xml_t *)p;
	input->result = input->thiz->internal_get_connection_xml();
}

std::string Machine::internal_get_connection_xml() {
	std::multimap<std::string, std::pair<Machine *, std::string> >::iterator
		ic;
	std::multimap<std::string, std::pair<Machine *, std::string> >
		incons = internal_get_input_connections();
	
	std::string result;

	result  = "<connections name=\"";
	result += name;
	result += "\">\n";

	for(ic = incons.begin(); ic != incons.end(); ic++) {
		result += "    <connection input=\"";
		result += (*ic).first;
		result += "\" sourcemachine=\"";
		result += (*ic).second.first->name;
		result += "\" output=\"";
		result += (*ic).second.second;
		result += "\" />\n";
	}

	result += "</connections>\n";
	
	return result;
}

typedef struct {
	Machine *thiz;
	std::multimap<std::string, std::pair<Machine *, std::string> >  result;	
} CALL_internal_get_input_connections_t;
__MACHINE_OPERATION_CALLBACK Machine::CALL_internal_get_input_connections(void *p) {
	CALL_internal_get_input_connections_t *input =
		(CALL_internal_get_input_connections_t *)p;
	input->result = input->thiz->internal_get_input_connections();
}

std::multimap<std::string, std::pair<Machine *, std::string> > Machine::internal_get_input_connections() {
	std::pair<Machine *, std::string> pr;
	std::string name;
	std::map<std::string, Signal *>::iterator i;

	std::multimap<std::string, std::pair<Machine *, std::string> > retval;

	for(i = input.begin(); i != input.end(); i++) {
		name = (*i).first;

		Signal *crnt = (*i).second;

		while(crnt != NULL) {
			pr.first = crnt->get_originator();
			pr.second = crnt->get_name();
			
			retval.insert(std::pair<std::string, std::pair<Machine *, std::string> >
				      (name,pr));

			crnt = crnt->get_next(this);
		}
	}

	return retval;
}

typedef struct {
	Machine *thiz;
	std::vector<std::string> result;
} CALL_internal_get_input_names_t;
__MACHINE_OPERATION_CALLBACK Machine::CALL_internal_get_input_names(void *p) {
	CALL_internal_get_input_names_t *input =
		(CALL_internal_get_input_names_t *)p;
	input->result = input->thiz->internal_get_input_names();
}

std::vector<std::string> Machine::internal_get_input_names() {
	std::vector<std::string> retval;

	std::map<std::string, Signal::Description>::iterator i;

	for(i = input_descriptor.begin(); i != input_descriptor.end(); i++) {
		retval.push_back((*i).first);
	}

	return retval;
}

typedef struct {
	Machine *thiz;
	std::vector<std::string> result;
} CALL_internal_get_output_names_t;
__MACHINE_OPERATION_CALLBACK Machine::CALL_internal_get_output_names(void *p) {
	CALL_internal_get_output_names_t *input =
		(CALL_internal_get_output_names_t *)p;
	input->result = input->thiz->internal_get_output_names();
}

std::vector<std::string> Machine::internal_get_output_names() {
	std::vector<std::string> retval;

	std::map<std::string, Signal::Description>::iterator i;

	for(i = output_descriptor.begin(); i != output_descriptor.end(); i++) {
		retval.push_back((*i).first);
	}

	return retval;
}

typedef struct {
	Machine *thiz;
	const std::string &inp;
	int result;
} CALL_internal_get_input_index_t;
__MACHINE_OPERATION_CALLBACK Machine::CALL_internal_get_input_index(void *p) {
	CALL_internal_get_input_index_t *input =
		(CALL_internal_get_input_index_t *)p;
	input->result = input->thiz->internal_get_input_index(input->inp);
}

int Machine::internal_get_input_index(const std::string &inp) {
	int k;
	std::map<std::string, Signal::Description>::iterator i;

	for(k = 0, i = input_descriptor.begin();
	    i != input_descriptor.end();
	    k++, i++) {
		if((*i).first == inp)
			return k;
	}

	throw jException("No such input.", jException::sanity_error);
}

typedef struct {
	Machine *thiz;
	const std::string &out;
	int result;
} CALL_internal_get_output_index_t;
__MACHINE_OPERATION_CALLBACK Machine::CALL_internal_get_output_index(void *p) {
	CALL_internal_get_output_index_t *input =
		(CALL_internal_get_output_index_t *)p;
	input->result = input->thiz->internal_get_output_index(input->out);
}

int Machine::internal_get_output_index(const std::string &out) {
	int k;
	std::map<std::string, Signal::Description>::iterator i;

	for(k = 0, i = output_descriptor.begin();
	    i != output_descriptor.end();
	    k++, i++) {
		if((*i).first == out)
			return k;
	}

	throw jException("No such output.", jException::sanity_error);
}

typedef struct {
	Machine *thiz;
	const std::string &nm;
} CALL_internal_set_name_t;
__MACHINE_OPERATION_CALLBACK Machine::CALL_internal_set_name(void *p) {
	CALL_internal_set_name_t *input =
		(CALL_internal_set_name_t *)p;
	input->thiz->internal_set_name(input->nm);
}

void Machine::internal_set_name(const std::string &nm) {
	name = nm;
}

/***************************
 *                         *
 *      class Machine      *
 *   Public functions      *
 *                         *
 ***************************/

void Machine::attach_input(Machine *m,
			   const std::string &output_name,
			   const std::string &input_name) {
	machine_operation_enqueue(
		[this, m, &input_name, &output_name](void *){
			if(has_been_deregistered || m->has_been_deregistered) {
				throw jException("One or both machines are invalid.", jException::sanity_error);
			} else {
				internal_attach_input(m, output_name, input_name);
			}
		},
		NULL, true);
}

void Machine::detach_input(Machine *m,
			   const std::string &output_name,
			   const std::string &input_name) {
	machine_operation_enqueue(
		[this, m, &input_name, &output_name](void *){
			if(has_been_deregistered || m->has_been_deregistered) {
				throw jException("One or both machines are invalid.", jException::sanity_error);
			} else {
				internal_detach_input(m, output_name, input_name);
			}
		},
		NULL, true);
}

std::string Machine::get_base_xml_description() {
	CALL_internal_get_base_xml_description_t param = {
		.thiz = this
	};
	machine_operation_enqueue(CALL_internal_get_base_xml_description, &param, true);
	return param.result;
}

std::string Machine::get_connection_xml() {
	CALL_internal_get_connection_xml_t param = {
		.thiz = this
	};
	machine_operation_enqueue(CALL_internal_get_connection_xml, &param, true);
	return param.result;
}

std::multimap<std::string, std::pair<Machine *, std::string> > Machine::get_input_connections() {
	CALL_internal_get_input_connections_t param = {
		.thiz = this
	};
	machine_operation_enqueue(CALL_internal_get_input_connections, &param, true);	
	return param.result;
}

std::vector<std::string> Machine::get_input_names() {
	CALL_internal_get_input_names_t param = {
		.thiz = this
	};
	machine_operation_enqueue(CALL_internal_get_input_names, &param, true);
	return param.result;
}

std::vector<std::string> Machine::get_output_names() {
	CALL_internal_get_output_names_t param = {
		.thiz = this
	};
	machine_operation_enqueue(CALL_internal_get_output_names, &param, true);
	return param.result;
}

int Machine::get_input_index(const std::string &inp) {
	CALL_internal_get_input_index_t param = {
		.thiz = this,
		.inp = inp		
	};
	machine_operation_enqueue(CALL_internal_get_input_index, &param, true);
	return param.result;
}

int Machine::get_output_index(const std::string &out) {
	CALL_internal_get_output_index_t param = {
		.thiz = this,
		.out = out		
	};
	machine_operation_enqueue(CALL_internal_get_output_index, &param, true);
	return param.result;
}

std::string Machine::get_name() {
	std::string result;
	machine_operation_enqueue(
		[&result, this](void */* ignored */) {
			result = name;
		}
		,
		NULL, true);
	return result;
}

void Machine::set_name(const std::string &nm) {
	if(nm.find('=') == std:: string::npos) throw ParameterOutOfSpec();
	
	CALL_internal_set_name_t param = {
		.thiz = this,
		.nm = nm
	};
	machine_operation_enqueue(CALL_internal_set_name, &param, true);
}

float Machine::get_x_position() {
	return x_position;
}

float Machine::get_y_position() {
	return y_position;
}

void Machine::set_position(float x, float y) {
	x_position = x;
	y_position = y;
	SATAN_DEBUG("Machine::set_position(%f, %f) for %s\n", x, y, get_name().c_str());
}

/***************************
 *                         *
 *      class Machine      *
 *   MachineOperations     *
 *       Interface
 *                         *
 ***************************/

class MachineOperationSynchObject : public jThread::Event {
private:
	jThread::EventQueue synch_queue;
public:
	bool triggered = false;
	std::string exception_message;
	jException::Type exception_type;

	MachineOperationSynchObject() : exception_type(jException::NO_ERROR) {}
	virtual ~MachineOperationSynchObject() {}

	void wait() {
		(void) synch_queue.wait_for_event(); // ignore result since we already know it to be equal to *this
	}
	
	void trigger() {
		triggered = true;
		synch_queue.push_event(this);
	}
	
	virtual void dummy_event_function() {}	
};

class MachineOperation {
public:
	std::function<void(void *data)> callback;
	void *callback_data;
	MachineOperationSynchObject *synch_object;

	MachineOperation() : callback(NULL), synch_object(NULL) { memset(&callback_data, 0, sizeof(callback_data)); }
};

static moodycamel::ReaderWriterQueue<MachineOperation> machine_operation_queue(100);

// operation called ONLY by NON audio playback threads
void Machine::machine_operation_enqueue(std::function<void(void *data)> callback, void *callback_data, bool do_sync) {
	// we rely on the machine_space lock here to synchronize the check of the sink variable
	Machine::lock_machine_space();
	if((!sink) || (!low_latency_mode)) {
		// if sink is not set, or we dont have low latency suport,
		// we have noone that executes machine_operation calls, so we execute directly
		try {
			callback(callback_data);
		} catch(...) {
			// first unlock before rethrowing
			Machine::unlock_machine_space();
			throw;
		}
		Machine::unlock_machine_space();
	} else {
		// we write to the queue while we hold the machine space lock - to protect the queue from multiple writers
		// but also to protect from the fact that the sink object might otherwise be deleted between our check for
		// it and the actual write to the queue.
		MachineOperation mo;
		MachineOperationSynchObject synch_object;
		
		mo.callback = callback;
		mo.callback_data = callback_data;
		mo.synch_object = do_sync ? (&synch_object) : NULL;
						
		machine_operation_queue.enqueue(mo);

		Machine::unlock_machine_space();
		
		if(do_sync) {
			SATAN_DEBUG("do_sync -- wait for sync_object %p\n", &synch_object);
			synch_object.wait();
			SATAN_DEBUG("do_sync -- sync_object %p returned\n", &synch_object);

			// if there was a jException thrown during execution
			// we throw it here too
			if(synch_object.exception_type != jException::NO_ERROR) {
				SATAN_DEBUG("   oops - synch_object.wait() returned an exception. (%s)\n", synch_object.exception_message.c_str());
				throw jException(synch_object.exception_message,
						 synch_object.exception_type);
			}
		}
	}
}

// operation ONLY called by audio playback thread
void Machine::machine_operation_dequeue() {
	MachineOperation mo;
	
	bool d;
	int kount = 50; // limit the processing to 50 operations per call to this function
	while(kount > 0 && ((d = machine_operation_queue.try_dequeue(mo)) == true)) {
		kount--;
		// if there is a synch object attached we need to trigger it after the callback
		if(mo.synch_object) {
			// we can catch jExceptions and pass on the error through the synch_object
			SATAN_DEBUG(" Calling machine operation callback (with synch - %p)\n", mo.synch_object);
			try {
				mo.callback(mo.callback_data);
			} catch(jException e) {
				mo.synch_object->exception_message = e.message;
				mo.synch_object->exception_type = e.type;
			} catch(std::exception const& e) {
				SATAN_ERROR("Machine::machine_operation_dequeue() - Unexpected exception caught --> %s\n", e.what());
				exit(0);
			} catch(...) {
				SATAN_ERROR("Machine::machine_operation_dequeue() - Unknown exception caught\n");
				exit(0);
			}
			mo.synch_object->trigger();
			SATAN_DEBUG(" Machine operation callback returned, and synched\n");
		} else {
			// otherwise, just call the callback
			// if an exception is received here we will quit the application straight away, no mercy.
			try {
				SATAN_DEBUG(" Calling machine operation callback (NO synch)\n");
				mo.callback(mo.callback_data);
				SATAN_DEBUG(" Machine operation callback returned (NO synch)\n");
			} catch(const std::bad_function_call& e) {
				SATAN_ERROR("bad_function_call exception caught in machine_operation_dequeue.\n");
				exit(0);
			} catch(...) {
				SATAN_ERROR("Unknown exception caught in machine_operation_dequeue.\n");
				exit(0);
			}
		}
	}
}

pid_t machine_execution_thread = 0;

// Activates low latency features in SATAN
void Machine::activate_low_latency_mode() {
	Machine::lock_machine_space();

	machine_execution_thread = gettid();
	low_latency_mode = true;
	
	Machine::unlock_machine_space();
}

void Machine::deactivate_low_latency_mode() {
	Machine::lock_machine_space();

	low_latency_mode = false;
	machine_operation_dequeue();
	machine_execution_thread = 0;
	
	Machine::unlock_machine_space();
}

void Machine::push_to_render_chain() {
	Machine *m = top_render_chain;
	while(m != NULL && m != this) {
		m = m->next_render_chain;
	}
	if(m == NULL) { // make sure we are only ones in the chain
		SATAN_DEBUG("  MACHINE PUSHED TO CHAIN: %s\n", name.c_str());
		this->next_render_chain = top_render_chain;
		top_render_chain = this;

		// Push the machines we depend on on top
		{
			std::map<Machine *, int>::iterator i;
			for(i = dependant.begin(); i != dependant.end(); i++) {
				(*i).first->push_to_render_chain();
			}
		}
	}

}

void Machine::recalculate_render_chain() {
	top_render_chain = NULL;
	SATAN_DEBUG("-------- BEGIN NEW RENDER CHAIN\n");
	if(sink) sink->push_to_render_chain();
	SATAN_DEBUG("-------- END NEW RENDER CHAIN\n");
}

void Machine::render_chain() {
	Machine *m = top_render_chain;
	while(m != NULL) {
		m->execute();
		m = m->next_render_chain;
	}
}

// Registers this machine instance as the sink of the machine network
void Machine::register_this_sink(Machine *m) {
	internal_register_sink(m);	
}

// Fills this sink...
int Machine::fill_this_sink(
	Machine *m,
	int (*fill_sink_callback)(int status, void *cbd),
	void *callback_data) {

	int retval = 0;

	/* check if this is truly THE sink! */
	if(sink != m) {
		/* no it wasn't! */
		/* return AT ONCE! */
		return _notSink;
	}

	if(low_latency_mode) {
		/* first we fill the sink */
		retval = internal_fill_sink(fill_sink_callback, callback_data);

		// In low latency mode we use the machine operation queue for synchronization
		/* then we dequeue currently waiting machine_operation tasks */
		machine_operation_dequeue();		
		
	} else {
		// If not in low latency mode, we use the machine space lock
		Machine::lock_machine_space();
		/* then we fill the sink */
		try {
			retval = internal_fill_sink(fill_sink_callback, callback_data);			
		} catch(...) {
			Machine::unlock_machine_space();
		}

		Machine::unlock_machine_space();
	}
	/* or, if not, continue, please! */
	return retval;
}

void Machine::set_ssignal_defaults(Machine *m,
				   Dimension dim,
				   int len, Resolution res,
				   int frequency) {
	/* check if this is truly THE sink! */
	if(sink != m) {
		/* no it wasn't! */
		return; // at once!
	}

	Signal::set_defaults(dim, len, res, frequency);
}

/* creates signals. registers machine as sink, if requested, and sets
 * the default name. It also registers the machine in the static
 * machine map.
 */
void Machine::setup_machine() {
	std::map<std::string, Signal::Description>::iterator i;

	for(i = output_descriptor.begin();
	    i != output_descriptor.end();
	    i++) {
		output[(*i).first] = Signal::SignalFactory::create_signal((*i).second.channels, this, (*i).first, (Dimension)(*i).second.dimension);
	}
	
	if(base_name_is_name) 
		name = base_name; // set default name from basename
	else {
		std::ostringstream stream;
		stream << "m#" << machine_set.size() << ":" << base_name;
		name = stream.str();
	}

	Machine::internal_register_machine(this);       
	SATAN_DEBUG("  machine [%p] name set to ] %s [\n", this, name.c_str());
	
}

Machine::Machine() {
	// this should not be called from a child class
}

Machine::Machine(std::string name_base, bool _base_name_is_name, float _xpos, float _ypos) :
	base_name(name_base), base_name_is_name(_base_name_is_name), x_position(_xpos), y_position(_ypos) {
	CREATE_TIME_MEASURE_OBJECT(tmes_A, base_name.c_str(), 1000);
	CREATE_TIME_MEASURE_OBJECT(tmes_B, base_name.c_str(), 1000);
	CREATE_TIME_MEASURE_OBJECT(tmes_C, base_name.c_str(), 1000);
}

Machine::~Machine() {
	SATAN_DEBUG("Final destroyification of the MACHINE!\n");
	DELETE_TIME_MEASURE_OBJECT(tmes_A);
	DELETE_TIME_MEASURE_OBJECT(tmes_B);
	DELETE_TIME_MEASURE_OBJECT(tmes_C);
}

#define SET_CONTROLLER_VALUE(a,b,c,d)		\
			{ \
				b value; \
				KXML_GET_NUMBER(a,"value",value,d);	\
				c->internal_set_value(&value); \
			}

void Machine::setup_using_xml(const KXMLDoc &mxml) {
	// parse controller xml
	int c, c_max = 0;
	try {
		c_max = mxml["controller"].get_count();
	} catch(...) {
		// ignore
	}
	for(c = 0; c < c_max; c++) {
		KXMLDoc c_xml = mxml["controller"][c];
		Controller *c = NULL;
		try {
			 c = internal_get_controller(c_xml.get_attr("name"));
		} catch(jException je) {
			// ignore
		}
		if(c != NULL) {
			switch(c->get_type()) {
			case Controller::c_enum:
			case Controller::c_sigid:
			case Controller::c_int:
				SET_CONTROLLER_VALUE(c_xml,int,c, 0);
				break;
			case Controller::c_float:
				SET_CONTROLLER_VALUE(c_xml,float,c, 0.0);
				break;
			case Controller::c_bool:
				SET_CONTROLLER_VALUE(c_xml,bool,c, false);
				break;
			case Controller::c_string:
				SET_CONTROLLER_VALUE(c_xml,std::string,c,"");
				break;
			}
		}
	}

}

// xxx if we are trying to mix a one channel signal into an input with two channels (or more)
// we must also duplicate the single channel to all channels in the input...

#define __PREMIX_MACRO(Q,V,T) \
	cmax_s = s->get_channels(); \
	while(n != NULL) { \
		cmax_n = n->get_channels(); \
		V = (T *)n->get_buffer(); \
		c_n = c_s = 0; \
		while((c_n < cmax_n) || (c_s < cmax_s)) { \
			c_n = (c_n) < cmax_n ? c_n : (cmax_n - 1); \
			c_s = (c_s) < cmax_s ? c_s : (cmax_s - 1); \
			for(i = 0; i < max_i; i++) { \
				Q[i*cmax_s+c_s] =	\
					Q[i*cmax_s+c_s] + \
					V[i*cmax_n+c_n]; \
			} \
			c_n++; c_s++; \
		} \
		n = n->get_next(this); \
	}
 
void Machine::premix(Signal *s, Signal *n) {
	int cmax_s, cmax_n, c_n, c_s;
	int i, max_i;
	max_i = s->get_samples();
	int8_t *out_8, *in_8;
	int16_t *out_16, *in_16;
	int32_t *out_32, *in_32;
	float *out_fl, *in_fl;
	fp8p24_t *out_fx, *in_fx;
	Resolution res = s->get_resolution();

	out_32 = (int32_t *)s->get_buffer();
	out_16 = (int16_t *)out_32;
	out_8 = (int8_t *)out_16; 
	out_fl = (float *)out_16; 
	out_fx = (fp8p24_t *)out_16; 
	
	switch(res) {
	case _MAX_R:
		// ignore 'em
		break;
	case _8bit:
		__PREMIX_MACRO(out_8,in_8,int8_t);
		break;
	case _16bit:
		__PREMIX_MACRO(out_16,in_16,int16_t);
		break;
	case _32bit:
		__PREMIX_MACRO(out_32,in_32,int32_t);
		break;
	case _fl32bit:
		__PREMIX_MACRO(out_fl,in_fl,float);
		break;
	case _fx8p24bit:
		__PREMIX_MACRO(out_fx,in_fx,fp8p24_t);
		break;
	case _PTR:
		/* ignore */
		break;
	}
	
}

void Machine::destroy_tightly_attached_machines() {
	// check if <this> is tightly connected
	// if so - disconnect and destroy all
	// those machines too

	std::set<Machine *>::iterator k;
	for(k = tightly_connected.begin();
	    k != tightly_connected.end();
	    k++) {
		Machine *other = (*k);

		// first locate entry pointing to us and delete that
		// so that we do not get a dead-lock when it tries
		// to delete us back...
		{
			std::set<Machine *>::iterator reverse_pointer;
			reverse_pointer = other->tightly_connected.find(this);
			if(reverse_pointer != other->tightly_connected.end())
				other->tightly_connected.erase(reverse_pointer);
		}

		internal_disconnect_and_destroy(other);
	}
}

void Machine::execute() {
	// clear all midstorage pre-mix signals
	// we do this here so that in case
	// we do not have any attached signal to a slot that is
	// premixed we do not keep signal data from before
//	START_TIME_MEASURE((*tmes_A));
	{
		std::map<std::string, Signal *>::iterator i;

		for(i  = premixed_input.begin();
		    i != premixed_input.end();
		    i++) {
			if((*i).second != NULL)
				(*i).second->clear_buffer();
		}		
	}
//	STOP_TIME_MEASURE((*tmes_A), "buffers cleared");
//	START_TIME_MEASURE((*tmes_B));
	// pre-mix marked channels
	{
	static std::map<std::string, Signal *>::iterator i;
	for(i = input.begin(); i != input.end(); i++) {
		if(input_descriptor[(*i).first].premix && (*i).second != NULL) {
			if(premixed_input.find((*i).first) ==
			   premixed_input.end()) {
				Signal *s = (*i).second;
				
				premixed_input[(*i).first] =
					Signal::SignalFactory::create_signal(
						input_descriptor[(*i).first].channels, //s->get_channels(),
						this,
						name + "_" + (*i).first + "_premix",
						s->get_dimension()
						);
				// clear buffer - we don't want noise
				s->clear_buffer();
			}

			premix(premixed_input[(*i).first], (*i).second);
		}
	}
//	STOP_TIME_MEASURE((*tmes_B), "premix done");
	START_TIME_MEASURE((*tmes_C));
	}

	try {
		// OK, do da calculationz
		fill_buffers();
	} catch(...) {
		jInformer::inform(std::string("Caught an exception when trying to execute dynlib machine [") + name + "]");
		throw;
	}
	STOP_TIME_MEASURE((*tmes_C), "fill_buffers");	
}

Machine::Controller *Machine::create_controller(
	Controller::Type tp,
	std::string name,
	std::string title,
	void *ptr,
	std::string min,
	std::string max,
	std::string step,
	std::map<int, std::string> enumnames,
	bool float_is_FTYPE) {
	return new Controller(
		tp, name, title, ptr, min, max, step, enumnames, float_is_FTYPE);
}

void Machine::set_midi_controller(Controller *ctrl, int coarse, int fine) {
	ctrl->set_midi_controller(coarse, fine);
}

void decr_dep_count(std::map<Machine *, int> &dependant,
		    Machine *m) {
	// decrease dependant count
	std::map<Machine *, int>::iterator i;
	i = dependant.find(m);
	int c = 0;
	if(i != dependant.end()) {
		c = (*i).second;
		c--;
		if(c == 0) {
			dependant.erase(i);
		} else {
			dependant[m] = c;
		}
	}
}

void Machine::detach_all_inputs(Machine *m) {
	std::map<std::string, Signal *>::iterator k;
	std::map<std::string, Signal *>::iterator next;
	next = input.begin();
	for(k = input.begin(); next != input.end(); k = next) {
		next++;
		Signal *head = (*k).second;

		if(m == NULL) {
			while(head) {
				broadcast_detach(head->get_originator(), this,
						 head->get_name(), (*k).first);
				
				decr_dep_count(dependant, head->get_originator());
				head->detach(this);
				head = head->unlink(this, head);
			}
			input.erase(k);
		} else { // m != NULL
			Signal *s = head;
			while(s) {
				Machine *o = s->get_originator();
				if(o == m) {
					broadcast_detach(o, this,
							 s->get_name(), (*k).first);
					
					decr_dep_count(dependant, s->get_originator());
					s->detach(this);
					head = head->unlink(this, s);
					s = head;
				} else {
					s = s->get_next(this);
				}
			}
			if(head == NULL) {
				input.erase(k);
			} else
				input[(*k).first] = head;
		}
	}
	recalculate_render_chain();
}

void Machine::detach_all_outputs() {
	std::map<std::string, Signal *>::iterator k;

	for(k = output.begin(); k != output.end(); k++) {
		std::vector<Machine *> attached =
			(*k).second->get_attached_machines();

		std::vector<Machine *>::iterator l;
		for(l  = attached.begin();
		    l != attached.end();
		    l++) {
			(*l)->detach_all_inputs(this);
		}		
	}
	recalculate_render_chain();
}

int Machine::get_next_tick() {
	return __current_tick;
}

int Machine::get_next_sequence_position() {
	return __next_sequence_position;
}

int Machine::get_next_tick_at(Dimension d) {
	return __next_tick_at[d];
}

int Machine::get_samples_per_tick(Dimension d) {
	return __samples_per_tick[d];
}

int Machine::get_samples_per_tick_shuffle(Dimension d) {
	return __samples_per_tick_shuffle[d];
}

#define COPY_VALUE_TO_STREAM(a,b,c) \
		{ \
			a value; \
			c->internal_get_value(&value);	\
			b << value; \
		}

std::string Machine::get_controller_xml() {
	std::ostringstream result;
	
	for(auto k : internal_get_controller_names()) {
		Controller *c =
			internal_get_controller(k);
		std::string name = KXMLDoc::escaped_string(c->get_name());
		result << "<controller name=\""
		       << name 
		       << "\" value=\"";
		switch(c->get_type()) {
		case Controller::c_enum:
		case Controller::c_sigid:
		case Controller::c_int:
			COPY_VALUE_TO_STREAM(int,result,c);
			break;
		case Controller::c_float:
			COPY_VALUE_TO_STREAM(float,result,c);
			break;
		case Controller::c_bool:
			COPY_VALUE_TO_STREAM(bool,result,c);
			break;
		case Controller::c_string:
			COPY_VALUE_TO_STREAM(std::string,result,c);
			break;
		}
		result << "\" />\n";
	}
	return result.str();
}

bool Machine::find_machine_in_graph(Machine *m) {
	// first check if m is actually this
	if(m == this) {
		return true;
	}
	
	std::map<Machine *, int>::iterator i;
	
	i = dependant.find(m);
	if(i != dependant.end()) {
		return true;
	}

	// OK, scan subtrees
	for(i = dependant.begin(); i != dependant.end(); i++) {
		if((*i).first->find_machine_in_graph(m))
			return true;
	}

	// not found
	return false;
}

void Machine::tightly_connect(Machine *m) {
	// is m valid?
	if(m == NULL)
		throw jException("Source machine pointer is NULL.", jException::sanity_error);

	// check if already tight...
	if(this->tightly_connected.find(m) != this->tightly_connected.end())
		throw jException("Machines already tightly connected.", jException::sanity_error);
	if(m->tightly_connected.find(this) != m->tightly_connected.end())
		throw jException("Machines already tightly connected.", jException::sanity_error);

	// OK, all checks are OK. Connect them tight!
	this->tightly_connected.insert(m);
	m->tightly_connected.insert(this);

	// Finished!
}

Machine::Signal *Machine::get_output(const std::string &name) {
	std::map<std::string, Signal *>::iterator s = output.find(name);
	if(s != output.end()) {
		return (*s).second;
	}
	return NULL;
}

/*
 * static functions/data
 *
 */
jThread::Monitor *Machine::machine_space_lock = new jThread::Monitor(true); // true == allow recursive lock in single thread
int Machine::shuffle_factor = 0;
int Machine::__loop_start = 0;
int Machine::__loop_stop = 64;
bool Machine::__do_loop = true;
int Machine::__current_tick = 0;
int Machine::__next_sequence_position = 0;
int Machine::__next_tick_at[_MAX_D];
int Machine::__samples_per_tick[_MAX_D];
int Machine::__samples_per_tick_shuffle[_MAX_D];
int Machine::__bpm = 120;
int Machine::__lpb = 4;
bool Machine::low_latency_mode = false;
bool Machine::is_loading = false;
bool Machine::is_playing = false;
bool Machine::is_recording = false;
std::string Machine::record_fname = ""; // filename to record to
Machine *Machine::sink = NULL;
Machine *Machine::top_render_chain = NULL;
std::vector<Machine *> Machine::machine_set;
std::set<std::weak_ptr<Machine::MachineSetListener>, std::owner_less<std::weak_ptr<Machine::MachineSetListener> > > Machine::machine_set_listeners;

std::vector<std::pair<__MACHINE_PERIODIC_CALLBACK_F, int> > Machine::periodic_callback_set;

#ifdef SIMPLE_JTHREAD_DEBUG
extern jThread::Monitor *SIMPLE_JTHREAD_DEBUG_MUTX;
#endif

/*********
 *
 * Private internal static functions, unlocked! (lock before calling!)
 *
 *********/

void Machine::broadcast_attach(Machine *source_machine, Machine *destination_machine,
			       const std::string &output_name, const std::string &input_name) {
	for(auto w_mlist : machine_set_listeners) {
		
		std::shared_ptr<MachineSetListener> mlist = w_mlist.lock();
		if(mlist) {
			Machine::run_async_function(
				[mlist, source_machine, destination_machine, output_name, input_name]() {
					mlist->machine_input_attached(
						source_machine, destination_machine,
						output_name, input_name
						);
				}
				);
		}
	}
}

void Machine::broadcast_detach(Machine *source_machine, Machine *destination_machine,
			       const std::string &output_name, const std::string &input_name) {
	for(auto w_mlist : machine_set_listeners) {
		
		std::shared_ptr<MachineSetListener> mlist = w_mlist.lock();
		if(mlist) {
			Machine::run_async_function(
				[mlist, source_machine, destination_machine, output_name, input_name]() {
					mlist->machine_input_detached(
						source_machine, destination_machine,
						output_name, input_name
						);
				}
				);
		}
	}
}

void Machine::internal_register_sink(Machine *s) {
	{
		Machine::lock_machine_space();

		if(sink) {
			Machine::unlock_machine_space();
			throw jException("Multiple sinks not allowed.", jException::sanity_error);
		}
		
		sink = s;

		Machine::unlock_machine_space();
	}
}

void Machine::internal_unregister_sink(Machine *s) {
	Machine::lock_machine_space();
	
	if(sink != s) {
		Machine::unlock_machine_space();
		throw jException("Sink not registered.", jException::sanity_error);
	}
	
	sink = NULL;
	top_render_chain = NULL;

	Machine::unlock_machine_space();
}

void Machine::internal_register_machine(Machine *m) {
	static std::vector<Machine *>::iterator i;

	for(i = machine_set.begin(); i != machine_set.end(); i++) {
		if((*i) == m)
			throw jException("Trying to register machine twice.", jException::sanity_error);
		std::cout << "     PREVIOUSLY REGISTERD ptr: " << (*i) << " ]" << (*i)->name << "[\n";
	}

	machine_set.push_back(m);

	m->has_been_deregistered = false;	
	m->reference_counter = 1 + machine_set_listeners.size();
	
	for(auto w_mlist : machine_set_listeners) {
		std::shared_ptr<MachineSetListener> mlist = w_mlist.lock();
		if(mlist) {
			Machine::run_async_function(
				[mlist, m]() {
					mlist->machine_registered(m);
				}
				);
		}
	}
}

void Machine::internal_deregister_machine(Machine *m) {
	// notify listeners that the machine is no longer in use
	for(auto w_mlist : machine_set_listeners) { 
		std::shared_ptr<MachineSetListener> mlist = w_mlist.lock();
		
		if(mlist) { 
			Machine::run_async_function(
				[mlist, m]() {
					mlist->machine_unregistered(m);
				}
				);
		}		
	}

	{ // remove the machine from our internal set.
		std::vector<Machine *>::iterator i;
		
		for(i = machine_set.begin(); i != machine_set.end(); i++) {
			if((*i) == m) {
				m->has_been_deregistered = true;
				machine_set.erase(i);

				if(sink == m) {
					internal_unregister_sink(m);
				}

				internal_dereference_machine(m);
				return;
			}
		}
	}
	
	throw jException("Trying to deregister unknown machine.", jException::sanity_error);
}

void Machine::internal_dereference_machine(Machine *m) {
	m->reference_counter -= 1;
	SATAN_DEBUG("Machine reference counter for %p is now %d\n", m, m->reference_counter);
	if(m->reference_counter <= 0) {
		SATAN_DEBUG(" Machine %p will be deleted.\n", m);

		delete m;		
	}
}

void Machine::calculate_samples_per_tick() {
	int d;
	int samples, frequency; Resolution resolution;
	int samples_per_tick, samples_per_tick_shuffle;
	
	for(d = 0; d < _MAX_D; d++) {
		Signal::get_defaults((Dimension)d, samples, resolution, frequency);
		samples_per_tick = frequency * 60 / (__bpm * __lpb * MACHINE_TICKS_PER_LINE);
		
		__samples_per_tick[d] = samples_per_tick;

		// calculate shuffle offset
		samples_per_tick_shuffle = samples_per_tick >> 3;
		samples_per_tick_shuffle =
			(shuffle_factor * 3 * samples_per_tick_shuffle) / SHUFFLE_FACTOR_DIVISOR;
		__samples_per_tick_shuffle[d] = samples_per_tick_shuffle;
	}

}

void Machine::calculate_next_tick_at_and_sequence_position() {

	int d;

	for(d = 0; d < _MAX_D; d++) {
		int samples, frequency; Resolution resolution;
		Signal::get_defaults((Dimension)d, samples, resolution, frequency);

		int nta = __next_tick_at[d];
		int spt = __samples_per_tick[d];
		int sptf = __samples_per_tick_shuffle[d];

		bool even_line = ((__next_sequence_position % 2) == 0) ? true : false;
		
		while(nta < samples) {

			// only do this synchronization during the midi dimension
			// it is anyway equal to all other dimensions
			if(d == _MIDI) {
				if(__current_tick == 0) {
					trigger_periodic_functions();

				}
				__current_tick = (__current_tick + 1) % MACHINE_TICKS_PER_LINE;

				if(__current_tick == 0) {
					__next_sequence_position += 1;
					
					if(__do_loop && __next_sequence_position >= __loop_stop)
						__next_sequence_position = __loop_start;
					
					even_line = ((__next_sequence_position % 2) == 0) ? true : false;
				}
			}
			if(even_line)
				nta += spt - sptf;
			else
				nta += spt + sptf;
		}

		__next_tick_at[d] = nta - samples;
	}
}

void Machine::trigger_periodic_functions() {
	std::vector<std::pair<__MACHINE_PERIODIC_CALLBACK_F, int> >::iterator
		periodic_callback;
		
	for(periodic_callback  = periodic_callback_set.begin();
	    periodic_callback != periodic_callback_set.end();
	    periodic_callback++) {
		int value =
			__next_sequence_position % ((*periodic_callback).second);
		if(value == 0) {
			auto f = (*periodic_callback).first;
			auto pos = __next_sequence_position;
			Machine::run_async_function(
				[f, pos]() {
					f(pos);
				}
				);
		}
	}
}

// when we enter here the machine space lock will be locked already, we just have to take
// care of unlocking it before we return... however, we do not have to do it if we throw an exception..
// in the case of an exception the caller (fill_this_sink()) will take care of unlocking..
int Machine::internal_fill_sink(int (*fill_sink_callback)(int status, void *cbd), void *callback_data) {
	DECLARE_TIME_MEASURE_OBJECT_STATIC(filler_sinker,"internal_fill_sink", 5000);
	int retval = _sinkPaused;

#ifdef SIMPLE_JTHREAD_DEBUG
	static bool notset = true;
	if(notset) {
		SATAN_DEBUG("fill thread id %d\n", gettid());
		notset = false;
	}

#endif
	static bool was_playing = true; // was_playing defaults to true, we want to simulate it...
	if(!is_playing) {
		if(was_playing) {
			retval = fill_sink_callback(_sinkPaused, callback_data);
		} 
		was_playing = false;
		return retval;
	}

	if(!was_playing) {
		was_playing = true;

		retval = fill_sink_callback(_sinkResumed, callback_data);		

		return retval;
	}
	
	if(sink) {
		try {
			calculate_samples_per_tick();

			START_TIME_MEASURE(filler_sinker);
			render_chain();
			STOP_TIME_MEASURE(filler_sinker, "chain rendered");

			calculate_next_tick_at_and_sequence_position();
		} catch(jException e) {
			jInformer::inform(std::string("Caught an jException in fill_sink: ") + e.message);
			throw;
		} catch(...) {
			jInformer::inform("Caught an exception in fill_sink. You should quit the application...");
			throw;
		}
	}
	
	retval = fill_sink_callback(is_recording ? _sinkRecord : _sinkJustPlay, callback_data);
	
	return retval;
}

void Machine::internal_disconnect_and_destroy(Machine *m) {
	if(m->detach_and_destroy()) {
		m->destroy_tightly_attached_machines();
		
		// deregister machine
		Machine::internal_deregister_machine(m);
	}
	recalculate_render_chain();
}

void Machine::reset_all_machines() {
	std::vector<Machine *>::iterator machine;

	for(machine  = machine_set.begin();
	    machine != machine_set.end();
	    machine++) {
		(*machine)->reset();
	}
}

/****************************************
 *
 * machine_operation callback functions
 *
 ****************************************/

bool Machine::internal_get_load_state() {
	return is_loading;
}

void Machine::internal_set_load_state(bool __is_loading) {
	is_loading = __is_loading;

	if(!is_loading) {
		for(auto w_mlist : machine_set_listeners) {
			std::shared_ptr<MachineSetListener> mlist = w_mlist.lock();
			if(mlist) {
				Machine::run_async_function(
					[mlist]() {
						mlist->project_loaded();
					}
					);
			}
		}
	}
}

void Machine::internal_get_rec_fname(std::string *rval) {
	*rval = record_fname;
}

Machine *Machine::internal_get_by_name(const std::string &name) {
	Machine *m = NULL;
	
	static std::vector<Machine *>::iterator i;

	for(i = machine_set.begin(); i != machine_set.end(); i++) {
		if((*i)->name == name) {
			m = (*i);
			break;
		}
	}

	return m;
}

/****************************************
 *
 * external Machine interface - these functions (in most cases) trigger machine_operation callbacks
 *
 ****************************************/

void Machine::prepare_baseline() {
	// start the async oeprations thread if it does not yet exist
	if(async_ops == NULL) {
		async_ops = AsyncOperations::start_async_operations_thread();
	}	
}

void Machine::register_periodic(__MACHINE_PERIODIC_CALLBACK_F callback_function,
				int interval_in_playback_positions) {
	std::pair<__MACHINE_PERIODIC_CALLBACK_F, int> _newval;
	
	_newval.first = callback_function;
	_newval.second = interval_in_playback_positions;

	Machine::machine_operation_enqueue(
		[] (void *d) {
			std::pair<__MACHINE_PERIODIC_CALLBACK_F, int> newval = *((std::pair<__MACHINE_PERIODIC_CALLBACK_F, int> *)d);

			// search for existing entry for the callback
			// if found, erase it and add with a new value
			for(auto periodic_callback = periodic_callback_set.begin();
			    periodic_callback != periodic_callback_set.end();
			    periodic_callback++) {
				if((*periodic_callback).first == newval.first) {
					periodic_callback_set.erase(periodic_callback);
					periodic_callback_set.push_back(newval);
					return;
				}
				
			}
			
			// if we are here, the function was not found, so we just add it
			periodic_callback_set.push_back(newval);			
		},
		&_newval, true);
}

void Machine::register_machine_set_listener(std::weak_ptr<MachineSetListener> mset_listener) {
	Machine::machine_operation_enqueue(
		[&mset_listener] (void *d) {
			machine_set_listeners.insert(mset_listener);

			std::shared_ptr<MachineSetListener> mlist = mset_listener.lock();
			for(auto mch : machine_set) {
				mch->reference_counter++;
				Machine::run_async_function(
					[mch, mlist]() {
						mlist->machine_registered(mch);
					}
					);
			}
		},
		NULL, true);
}

void Machine::dereference_machine(Machine *m_ptr) {
	Machine::machine_operation_enqueue(
		[m_ptr] (void *d) {
			internal_dereference_machine(m_ptr);
		},
		NULL, false);
}

std::vector<Machine *> Machine::get_machine_set() {
	typedef struct {
		std::vector<Machine *> retval;
	} Param;
	Param param;
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->retval = machine_set;
		},
		&param, true);
	return param.retval;
}

void Machine::jump_to(int _position) {
	Machine::machine_operation_enqueue(
		[] (void *d) {			
			int position = *((int *)d);

			reset_all_machines();
			if(__do_loop) {
				if(position < __loop_start) {
					position = __loop_start;
				} else if(position > __loop_stop) {
					position = __loop_start;
				}
			}
			__current_tick = 0;
			__next_sequence_position = position;
		},
		&_position, true);
}

void Machine::rewind() {
	Machine::machine_operation_enqueue(
		[] (void *this_is_null_and_ignored) {
			reset_all_machines();
			if(__do_loop) {
				__next_sequence_position = __loop_start;
			} else {
				__next_sequence_position = 0;
			}
			__current_tick = 0;
		},
		NULL, false);
}

bool Machine::get_low_latency_mode() {
	return low_latency_mode;
}

int Machine::get_shuffle_factor() {
	return shuffle_factor;
}

void Machine::set_shuffle_factor(int _nf) {
	Machine::machine_operation_enqueue(
		[] (void *d) {
			shuffle_factor = *((int *)d);
		},
		&_nf, true);
}

bool Machine::get_loop_state() {
	return __do_loop;
}

int Machine::get_loop_start() {
	return __loop_start;
}

int Machine::get_loop_length() {
	return __loop_stop - __loop_start;
}

void Machine::set_load_state(bool __is_loading) {
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Machine::internal_set_load_state(*((bool *)d));
		},
		&__is_loading, true);
}

void Machine::set_loop_state(bool do_loop) {
	Machine::machine_operation_enqueue(
		[] (void *d) {
			__do_loop = *((bool *)d);
		},
		&do_loop, true);
}

void Machine::set_loop_start(int line) {
	if(line < 0) throw ParameterOutOfSpec();	
	Machine::machine_operation_enqueue(
		[] (void *d) {
			__loop_start = *((int *)d);
		},
		&line, true);
}

void Machine::set_loop_length(int loop_length) {
	if(loop_length < 4) throw ParameterOutOfSpec();
	Machine::machine_operation_enqueue(
		[] (void *d) {
			__loop_stop = __loop_start + (*((int *)d));
		},
		&loop_length, true);
}

void Machine::set_bpm(int bpm) {
	if(bpm < 20 || bpm > 200) throw ParameterOutOfSpec();
	Machine::machine_operation_enqueue(
		[] (void *d) {
			__bpm = (*((int *)d));
		},
		&bpm, true);
}

void Machine::set_lpb(int lpb) {
	if(lpb < 2 || lpb > 24) throw ParameterOutOfSpec();	
	Machine::machine_operation_enqueue(
		[] (void *d) {
			__lpb = (*((int *)d));
		},
		&lpb, true);
}

int Machine::get_bpm() {
	return __bpm;
}

int Machine::get_lpb() {
	return __lpb;
}

void Machine::set_rec_status(bool _dorec) {
	Machine::machine_operation_enqueue(
		[] (void *d) {
			is_recording = (*((bool *)d));
		},
		&_dorec, true);
}

bool Machine::get_rec_status() {
	return is_recording;
}

void Machine::set_rec_fname(std::string fnm) {
	Machine::machine_operation_enqueue(
		[] (void *d) {
			record_fname = *((std::string *)d);
		},
		&fnm, true);
}

std::string Machine::get_rec_fname() {
	std::string retval;
	Machine::machine_operation_enqueue(
		[] (void *d) {
			internal_get_rec_fname((std::string *)d);
		},
		&retval, true);
	return retval;
}

Machine *Machine::get_by_name(std::string _name) {
	typedef struct {
		std::string name;
		Machine *retval;
	} Param;
	Param param = {
		.name = _name,
		.retval = NULL
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->retval = internal_get_by_name(p->name);
		},
		&param, true);

	return param.retval;
}

Machine *Machine::get_sink() {
	return sink;
}

void Machine::play() {
	Machine::machine_operation_enqueue(
		[] (void *d) {
			is_playing = true;
		},
		NULL, false);
}

void Machine::stop() {
	Machine::machine_operation_enqueue(
		[] (void *d) {
			is_playing = false;			
			reset_all_machines();
		},
		NULL, true);
	TIME_MEASURE_PRINT_ALL_STATS();
	TIME_MEASURE_CLEAR_ALL_STATS();
}

bool Machine::is_it_playing() {
	// we are only reading this single integer, no need for synchronization in this trivial case
	return is_playing;
}

void Machine::disconnect_and_destroy(Machine *m) {
	typedef struct {
		Machine *_m;
	} Param;
	Param param = {
		._m = m
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			internal_disconnect_and_destroy(p->_m);
		},
		&param, true);
}

void Machine::destroy_all_machines() {
	std::vector<std::string> machine_names;
        
	// we cannot directly use this set to disconnect and destroy
	// all machines, because disconnect_and_destroy() might destroy
	// tightly coupled machines resulting in invalid pointers in this
	// set - therefore we get the names instead
	std::vector<Machine *>::iterator k;
	for(auto m : Machine::get_machine_set()) {
		machine_names.push_back(m->get_name());
	}
	
	// then we use the names to find the machines in the set
	// if a machine in the set was destroyed (because it was tightly connected)
	// we will get a NULL pointer, then we just skip that and go to the next
	for(auto name : machine_names) {
		Machine *m = Machine::get_by_name(name);
		if(m != NULL)
			(void)Machine::disconnect_and_destroy(m);
	}
}

void Machine::exit_application() {
	destroy_all_machines();
	Machine::machine_operation_enqueue(
		[] (void *d) {
			SATAN_DEBUG("Will exit VuKNOB now.\n");
			exit(0);
		},
		NULL, true);
}

pid_t tupla;
int tupla_p = 0;
void Machine::lock_machine_space() {
	machine_space_lock->enter();
	tupla = gettid();
	tupla_p++;
}

void Machine::unlock_machine_space() {
	tupla_p--;
	if(tupla_p == 0)
		tupla = -2;
	if(tupla_p < 0) {
		while(1) {
			SATAN_ERROR("Complete failure.\n");
			usleep(1000);
		}
	}
	machine_space_lock->leave();
}

std::vector<std::string> Machine::get_controller_groups() {
	typedef struct {
		Machine *thiz;
		std::vector<std::string> retval;
	} Param;
	Param param = {
		.thiz = this
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->retval = p->thiz->internal_get_controller_groups();
		},
		&param, true);
	return param.retval;
}

std::vector<std::string> Machine::get_controller_names() {
	typedef struct {
		Machine *thiz;
		std::vector<std::string> retval;
	} Param;
	Param param = {
		.thiz = this
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->retval = p->thiz->internal_get_controller_names();
		},
		&param, true);
	return param.retval;
}

std::vector<std::string> Machine::get_controller_names(const std::string &_group_name) {
	typedef struct {
		Machine *thiz;
		const std::string &gnam;
		std::vector<std::string> retval;
	} Param;
	Param param = {
		.thiz = this,
		.gnam = _group_name
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->retval = p->thiz->internal_get_controller_names(p->gnam);
		},
		&param, true);
	return param.retval;
}

Machine::Controller *Machine::get_controller(const std::string &_name) {
	typedef struct {
		const std::string &n;
		Machine *thiz;
		Machine::Controller *retval;
	} Param;
	Param param = {
		.n = _name,
		.thiz = this,
		.retval = NULL
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->retval = p->thiz->internal_get_controller(p->n);
		},
		&param, true);
	return param.retval;
}

std::string Machine::get_hint() {
	typedef struct {
		Machine *thiz;
		std::string retval;
	} Param;
	Param param = {
		.thiz = this,
		.retval = NULL
	};
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			p->retval = p->thiz->internal_get_hint();
		},
		&param, true);
	return param.retval;
}

/************************
 *
 * AsyncOperations - the interface
 *
 ************************/

AsyncOperations * Machine::async_ops = NULL;

void Machine::run_async_operation(AsyncOp *op) {
	async_ops->run_async_operation(op);
}

void Machine::run_async_function(std::function<void()> f) {
	async_ops->run_async_function(f);
}
