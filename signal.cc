/*
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

#include "signal.hh"
#include "wavloader.hh"
#include <jngldrum/jexception.hh>
#include <jngldrum/jinformer.hh>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include <jngldrum/jinformer.hh>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#include <fixedpointmath.h>

#include "static_signal_preview.hh"

/*******************************
 *                             *
 * class Machine::SignalBase   *
 *                             *
 *******************************/

Machine::SignalBase::SignalBase(const std::string &nm) : name(nm), buffer(NULL) {
}

Dimension Machine::SignalBase::get_dimension() {
	return dimension;
}

Resolution Machine::SignalBase::get_resolution() {
	return resolution;
}

int Machine::SignalBase::get_channels() {
	return channels;
}

int Machine::SignalBase::get_samples() {
	return samples;
}

int Machine::SignalBase::get_frequency() {
	return frequency;
}

void *Machine::SignalBase::get_buffer() {
	return buffer;
}

std::string Machine::SignalBase::get_name() {
	return name;
}

void Machine::SignalBase::internal_clear_buffer() {
	switch(resolution) {
	case _8bit:
		memset(buffer, 0,
		       samples * channels * sizeof(uint8_t));
		break;
	case _16bit:
		memset(buffer, 0,
		       samples * channels * sizeof(uint16_t));
		break;
	case _32bit:
		memset(buffer, 0,
		       samples * channels * sizeof(uint32_t));
		break;
	case _fl32bit:
		memset(buffer, 0,
		       samples * channels * sizeof(float));
		break;
	case _fx8p24bit:
		memset(buffer, 0,
		       samples * channels * sizeof(fp8p24_t));
		break;
	case _PTR:
		memset(buffer, 0,
		       samples * channels * sizeof(void *));
		break;

	case _MAX_R:
	default:
		break;
	}
}

void Machine::SignalBase::clear_buffer() {
	internal_clear_buffer();
}

/********************************************************
 *                                                      *
 * class StaticSignal::ProjectEntry Saving/LOADING XML  *
 *                                                      *
 ********************************************************/

Machine::StaticSignal::ProjectEntry Machine::StaticSignal::ProjectEntry::this_will_register_us_as_a_project_entry;

Machine::StaticSignal::ProjectEntry::ProjectEntry() : SatanProjectEntry("staticsignals", 0, SIGNAL_PROJECT_INTERFACE_LEVEL) {}

std::string Machine::StaticSignal::ProjectEntry::get_xml_attributes() {
	return "";
}

void Machine::StaticSignal::ProjectEntry::generate_xml(std::ostream &output) {
	// OK, save static signal data
	int k;
	for(k = 0; k < MAX_STATIC_SIGNALS; k++) {
		output << Machine::StaticSignal::save_signal_xml(k);
	}
}

void Machine::StaticSignal::ProjectEntry::parse_xml(int project_interface_level, KXMLDoc &xml_node) {
	// we can safely ignore the project_interface_level since we are compatible with all versions currently
	// however - if we would like we could use that value to support different stuff in different versions...

	// before we load the static signals for this project we must clear out the old
	{
		int k;
		for(k = 0; k < MAX_STATIC_SIGNALS; k++) {
			Machine::StaticSignal::clear_signal(k);
		}
	}

	// OK, done! time to read in the new project data
	unsigned int s, s_max = 0;
	
	// Parse static signals
	try {
		s_max = xml_node["staticsignal"].get_count();
	} catch(jException e) { s_max = 0;}

	for(s = 0; s < s_max; s++) {
		Machine::StaticSignal::load_signal_xml(xml_node["staticsignal"][s]);
	}
}

void Machine::StaticSignal::ProjectEntry::set_defaults() {
	// default state is to clear all signals
	{
		int k;
		for(k = 0; k < MAX_STATIC_SIGNALS; k++) {
			Machine::StaticSignal::clear_signal(k);
		}
 	}
}

/*****************************
 *                           *
 * class StaticSignalLoader  *
 *                           *
 *****************************/

std::vector<Machine::StaticSignalLoader *> *Machine::StaticSignalLoader::registered_loaders = NULL;

Machine::StaticSignalLoader::StaticSignalLoader(const std::string &_identity) : identity(_identity) {
	if(registered_loaders == NULL) {
		registered_loaders = new std::vector<StaticSignalLoader *>();
		
	}
	registered_loaders->push_back(this);

	SATAN_DEBUG("Registered static signal loader (%p): %s\n", registered_loaders, identity.c_str());
}

void Machine::StaticSignalLoader::replace_signal(
	int index,
	Dimension d, Resolution r,
	int channels, int samples, int frequency,
	std::string file_path,
	std::string name,
	void *data) {
			
	Machine::StaticSignal::replace_signal(
		index,
		d, r,
		channels, samples, frequency,
		file_path, name, data);
}

void Machine::StaticSignalLoader::preview_signal(
	Dimension d, Resolution r,
	int channels, int samples, int frequency,
	void *data) {
		
	StaticSignalPreview::preview(
		d, r,
		channels, samples, frequency,
		data);
}

void Machine::StaticSignalLoader::internal_load_signal(	
	int index,
	const std::string &file_path) {

	bool succeeded_loading = false;
	
	std::vector<StaticSignalLoader *>::iterator k;	
	for(k  = registered_loaders->begin();
	    k != registered_loaders->end();
	    k++) {

		if(!succeeded_loading) {
			try {
				(*k)->load_static_signal(file_path, index == -1 ? true : false, index);
				succeeded_loading = true;
			} catch(jException e) {
				jInformer::inform(e.message);
 			} catch(...) {
				jInformer::inform("Failed to load sample, reason unknown...");
			}
		}
	}

	if(!succeeded_loading) 
		jInformer::inform("Could not load signal.");
}

std::map<int, std::string> Machine::StaticSignalLoader::get_all_signal_names() {
	std::map<int, std::string> retval;

	Machine::machine_operation_enqueue(
		[&retval](void *) {
			auto sigs = StaticSignal::get_all_signals();
			for(auto sig : sigs) {
				retval[sig.first] = sig.second->get_name();
			}
		},
		NULL, true);
	return retval;
}

std::string Machine::StaticSignalLoader::get_signal_name_for_slot(int index) {
	typedef struct {
		char bfr[128];
		int idx;
	} Param;
	Param param;
	param.idx = index;
	param.bfr[127] = '\0'; // make sure it's a null terminated string
	
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			StaticSignal *s = StaticSignal::get_signal(p->idx);
			if(s) {
				strncpy(p->bfr, s->get_name().c_str(), 127); // only copy 127 bytes of the string - to guarantee that it's null terminated
			} else {
				throw jException("No signal loaded at specified slot.", jException::sanity_error);
			}
		},
		&param, true);
	return param.bfr;
}

bool Machine::StaticSignalLoader::is_valid(const std::string &path) {
	std::vector<StaticSignalLoader *>::iterator k;

	SATAN_DEBUG_("IN is_valid( (%p))\n", registered_loaders);
	for(k  = registered_loaders->begin();
	    k != registered_loaders->end();
	    k++) {
		SATAN_DEBUG("Verifying signal using loader: %s\n",
			    (*k)->identity.c_str());
		if((*k)->is_signal_valid(path)) return true;
	}

	return false;
}

void Machine::StaticSignalLoader::preview_signal(
	const std::string &file_path) {
	internal_load_signal(-1, file_path);
}

void Machine::StaticSignalLoader::load_signal(	
	int index,
	const std::string &file_path) {
	internal_load_signal(index, file_path);
}

/*****************************
 *                           *
 * class StaticSignal        *
 *                           *
 *****************************/


std::map<int, Machine::StaticSignal *> Machine::StaticSignal::signals;

Machine::StaticSignal::StaticSignal(
		Dimension d, Resolution r,
		int c, int s, int f,
		std::string fp,
		std::string n,
		void *b) : SignalBase(n), file_path(fp) {
	dimension = d;
	resolution = r;
	channels = c;
	samples = s;
	frequency = f;
	buffer = b;
}

Machine::StaticSignal::~StaticSignal() {
	if(buffer != NULL)
		free(buffer);
}

std::string Machine::StaticSignal::get_file_path() {
	return file_path;
}

void Machine::StaticSignal::clear_signal(int index) {
	if(signals.find(index) != signals.end()) {
		delete signals.find(index)->second;
		signals.erase(signals.find(index));
	}
}

void Machine::StaticSignal::save_0D_signal_xml(int index) {
	std::ostringstream fname_s;
	fname_s << "static_sig_nr_" << index << ".dat";

	int f_out = open(
		fname_s.str().c_str(),
		O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	SATAN_DEBUG(" OPEN: %d\n", f_out);
	if(f_out == -1) {
		std::ostringstream emsg;
		emsg << "[name] : " 
		     << "Failed to open storage file "
		     << fname_s.str()
		     << " , aborting save.";
		SATAN_DEBUG("  EXCEPTION MESSAGE: %s\n", emsg.str().c_str());
		throw jException(
			emsg.str(),
			jException::sanity_error);
	}

	int k, k_max;

	k_max = channels * samples;
	
	std::ostringstream result;
#define _0D_SAVE_BUFFER_SIZE 1024 * 8
	uint8_t output_buffer[_0D_SAVE_BUFFER_SIZE];
	int output_buffer_index = 0;
	SATAN_DEBUG("WRITING %d SAMPLES....", k_max);
	for(k = 0; k < k_max; k++) {
		uint8_t output[] = {0, 0, 0, 0, 0, 0, 0, 0};
		ssize_t output_size = 0;
		
		switch(resolution) {
		case _8bit:
		{
			int8_t *d = (int8_t *)buffer;
			int8_t *t = (int8_t *)output;
			*t = d[k];
			output_size = sizeof(int8_t);
		}
			break;
		case _16bit:
		{
			int16_t *d = (int16_t *)buffer;
			uint16_t *bin_native;
			uint16_t *bin_be = (uint16_t *)output;
			bin_native = (uint16_t *)&d[k];
			*bin_be = htons(*bin_native);
			output_size = sizeof(int16_t);
		}
			break;
		case _32bit:
		{
			int32_t *d = (int32_t *)buffer;
			uint32_t *bin_native;
			uint32_t *bin_be = (uint32_t *)output;
			bin_native = (uint32_t *)&d[k];
			*bin_be = htonl(*bin_native);
			output_size = sizeof(int32_t);
		}
			break;
		case _fl32bit:
		{
			float *d = (float *)buffer;
			uint32_t *bin_native;
			uint32_t *bin_be = (uint32_t *)output;
			bin_native = (uint32_t *)&d[k];
			*bin_be = htonl(*bin_native);
			output_size = sizeof(float);
		}
			break;
		case _fx8p24bit:
		{
			fp8p24_t *d = (fp8p24_t *)buffer;
			uint32_t *bin_native;
			uint32_t *bin_be = (uint32_t *)output;
			bin_native = (uint32_t *)&d[k];
			*bin_be = htonl(*bin_native);
			output_size = sizeof(fp8p24_t);
		}
		break;
		case _PTR:
		case _MAX_R:
			break;
		}

		if(output_buffer_index + output_size >= _0D_SAVE_BUFFER_SIZE) {
//			if(write(f_out, output, output_size) != output_size) {
			if(write(f_out, output_buffer, output_buffer_index) != output_buffer_index) {
				close(f_out);
				std::ostringstream emsg;
				emsg << "[name] : " 
				     << "Failed to write data to "
				     << fname_s.str()
				     << " , aborting save.";
				SATAN_DEBUG("   EXCEPTION MESSAGE: %s\n", emsg.str().c_str());
				throw jException(
					emsg.str(),
					jException::sanity_error);
			}
			output_buffer_index = 0;
		}
		memcpy(&output_buffer[output_buffer_index], output, output_size);
		output_buffer_index += output_size;
	}
	if(output_buffer_index != 0) {
		if(write(f_out, output_buffer, output_buffer_index) != output_buffer_index) {
			close(f_out);
			std::ostringstream emsg;
			emsg << "[name] : " 
			     << "Failed to write data to "
			     << fname_s.str()
			     << " , aborting save.";
			SATAN_DEBUG("   EXCEPTION MESSAGE: %s\n", emsg.str().c_str());
			throw jException(
				emsg.str(),
				jException::sanity_error);
		}
	}

	SATAN_DEBUG_("WROTE SAMPLES\n");
	close(f_out);
}

std::string Machine::StaticSignal::save_signal_xml(int index) {
	SATAN_DEBUG_("Sig %d, S1\n", index);
	SATAN_DEBUG_("Step TWO\n");

	if(signals.find(index) == signals.end())
		return "";
	
	StaticSignal *s = signals[index];
	if(s == NULL)
		return "";

	std::ostringstream result;
	result << "<staticsignal "
	       << "index=\"" << index << "\" "
	       << "path=\"" << KXMLDoc::escaped_string(s->get_file_path()) << "\" "
		
	       << "dimension=\"" << s->get_dimension() << "\" "
	       << "resolution=\"" << s->get_resolution() << "\" "
	       << "channels=\"" << s->get_channels() << "\" "
	       << "samples=\"" << s->get_samples() << "\" "
	       << "frequency=\"" << s->get_frequency() << "\" "
	       << "name=\"" << KXMLDoc::escaped_string(s->get_name()) << "\" >\n";
	SATAN_DEBUG_("Step 3\n");

	// write data
	Dimension d = s->get_dimension();
	switch(d) {
	case _0D:
		SATAN_DEBUG_(" saving 0D signal...\n");
		s->save_0D_signal_xml(index);
		SATAN_DEBUG_("        0D signal saved!\n");
		break;

	case _1D:
	case _2D:
	case _MIDI:
	case _MAX_D:
	default:
		std::ostringstream emesg;
		emesg << "Can not export static signal of dimension ";
		if(d == _MIDI) {
			emesg << "MIDI";
		} else {
			emesg << d;
		}
		emesg << ".";
			
		throw jException(emesg.str(), jException::sanity_error);
	}
	
	SATAN_DEBUG_("Step 4\n");
	result << "</staticsignal>\n";
	SATAN_DEBUG_("Step 5\n");

	return result.str();
}

// from PROJECT_INTERFACE_LEVEL == 7 we convert
// integer data into f8p24_t format
void Machine::StaticSignal::load_0D_signal_xml(
	const KXMLDoc &sigxml,
	int index,
	Dimension d, Resolution s_r, // s_r == source resolution
	int channels, int samples, int frequency,
	std::string file_path,
	std::string name) {

	Resolution t_r = _8bit; // target resolution
	ssize_t sr_size = 0; // source size
	ssize_t tg_size = 0; // target size

	switch(s_r) {
	case _8bit:
		sr_size = sizeof(int8_t);
		tg_size = sizeof(fp8p24_t);
		t_r = _fx8p24bit;
		break;
	case _16bit:
		sr_size = sizeof(int16_t);
		tg_size = sizeof(fp8p24_t);
		t_r = _fx8p24bit;
		break;
	case _32bit:
		sr_size = sizeof(int32_t);
		tg_size = sizeof(fp8p24_t);
		t_r = _fx8p24bit;
		break;
	case _fl32bit:
		sr_size = sizeof(float);
		tg_size = sizeof(float);
		t_r = _fl32bit;
		break;
	case _fx8p24bit:
		sr_size = sizeof(fp8p24_t);
		tg_size = sizeof(fp8p24_t);
		t_r = _fx8p24bit;
		break;
	case _PTR:
	case _MAX_R:
		return;
		break;
	}
	
	// Allocate memory and clear it
	void *data = (void *)malloc(samples * channels * tg_size);
	if(data == NULL) {
		throw jException("Could not allocate enough memory for "
				 "static signal", jException::sanity_error);
	}
	
	memset(data, 0, samples * channels * tg_size);
	
	std::ostringstream fname_s;
	int v, v_max = 0;
	int f_in = -1;

	fname_s << "static_sig_nr_" << index << ".dat";
	
	v_max = samples * channels;
	f_in = open(
		fname_s.str().c_str(),
		O_RDONLY);
	if(f_in == -1) {
		std::ostringstream emsg;
		emsg << "[" << name << "] : " 
		     << "Failed to open storage file "
		     << fname_s.str()
		     << " , aborting load.";
		
		throw jException(
			emsg.str(),
			jException::sanity_error);
	}

	for(v = 0; v < v_max; v++) {
		uint8_t input[] = {0,0,0,0, 0,0,0,0};
		
		if(read(f_in,input, sr_size) != sr_size) {
			close(f_in);
			std::ostringstream emsg;
			emsg << "[" << name << "] : " 
			     << "Failed to read data from "
			     << fname_s.str()
			     << " , aborting load.";
			
			throw jException(
				emsg.str(),
				jException::sanity_error);
		}

		switch(s_r) {
		case _8bit:
		{
			fp8p24_t *d = (fp8p24_t *)data;
			int8_t *b = (int8_t *)input;
			int32_t t = (*b) << 16;
			d[v] = t;
		}
			break;
		case _16bit:
		{
			fp8p24_t *d = (fp8p24_t *)data;
			uint16_t *bin_be = (uint16_t *)input;
			uint16_t bin_native;
			int16_t *tmp = (int16_t *)&bin_native;
			bin_native = ntohs(*bin_be);
			d[v] = (*tmp) << 7;
		}
			break;
		case _32bit:
		{
			fp8p24_t *d = (fp8p24_t *)data;
			uint32_t *bin_be = (uint32_t *)input;
			uint32_t bin_native;
			int32_t *tmp = (int32_t *)&bin_native;
			bin_native = ntohl(*bin_be);
			d[v] = (*tmp) >> 8;
		}
			break;
		case _fl32bit:
		{
			float *d = (float *)data;
			uint32_t *bin_be = (uint32_t *)input;
			uint32_t *bin_native;
			bin_native = (uint32_t *)&d[v];
			*bin_native = ntohl(*bin_be);
		}
			break;
		case _fx8p24bit:
		{
			fp8p24_t *d = (fp8p24_t *)data;
			uint32_t *bin_be = (uint32_t *)input;
			uint32_t *bin_native;
			bin_native = (uint32_t *)&d[v];
			*bin_native = ntohl(*bin_be);
		}
			break;
		case _PTR:
		case _MAX_R:
			break;
		}
	}
	
	replace_signal(index, d, t_r, channels, samples, frequency,
		       file_path, name, data);
}

void Machine::StaticSignal::load_signal_xml(const KXMLDoc &sigxml) {
	std::string name, fpath;
	int i, _d, _r, c, s, f;

	KXML_GET_NUMBER(sigxml, "index", i, -1);
	KXML_GET_NUMBER(sigxml, "dimension", _d, -1);
	KXML_GET_NUMBER(sigxml, "resolution", _r, -1);
	KXML_GET_NUMBER(sigxml, "channels", c, -1);
	KXML_GET_NUMBER(sigxml, "samples", s, -1);
	KXML_GET_NUMBER(sigxml, "frequency", f, -1);

	if(
		(i == -1)
		||
		(_d == -1) 
		||
		(_r == -1) 
		||
		(c == -1) 
		||
		(s == -1) 
		||
		(f == -1)
		) {
		throw jException("Faulty signal entry in XML file, missing attribute(s).",
				 jException::sanity_error);
	}
	
	name = sigxml.get_attr("name");
	fpath = sigxml.get_attr("path");

	Dimension d = (Dimension)_d;
	Resolution r = (Resolution)_r;

	switch(d) {
	case _0D:
		load_0D_signal_xml(sigxml, i, d, r, c, s, f, fpath, name);
		break;

	case _1D:
	case _2D:
	case _MIDI:
	case _MAX_D:
	default:
		std::ostringstream emesg;
		emesg << "Can not import static signal of dimension ";
		if(d == _MIDI) {
			emesg << "MIDI";
		} else {
			emesg << d;
		}
		emesg << ".";
			
		throw jException(emesg.str(), jException::sanity_error);
	}
}

void Machine::StaticSignal::replace_signal(
	int index,
	Dimension d, Resolution r,
	int channels, int samples, int frequency,
	std::string file_path,
	std::string name,
	void *data) {

	StaticSignal *s = NULL;

	s = new StaticSignal(
		d, r, channels, samples, frequency,
		file_path,
		name, data);

	typedef struct {
		std::map<int, StaticSignal *> &sigs;
		int idx;
		StaticSignal *sig;
	} Param;
	Param param = {
		.sigs = signals,
		.idx = index,
		.sig = s
	};
	
	Machine::machine_operation_enqueue(
		[] (void *d) {
			Param *p = (Param *)d;
			std::map<int, StaticSignal *> &sigs = p->sigs;
			
			if(sigs.find(p->idx) != sigs.end()) {
				delete sigs.find(p->idx)->second;
				sigs.erase(sigs.find(p->idx));
			}
			
			sigs[p->idx] = p->sig;
		},
		&param, true);
}

auto Machine::StaticSignal::get_all_signals() -> std::map<int, StaticSignal *> {
	return signals;
}

Machine::StaticSignal *Machine::StaticSignal::get_signal(int index) {
	if(signals.find(index) == signals.end()) {
		return NULL;
	}
	return signals[index];       
}

/*****************************
 *                           *
 * class Signal::Description *
 *                           *
 *****************************/

Machine::Signal::Description::Description() {
}

Machine::Signal::Description::Description(int d, int c, bool p) :
	dimension(d), channels(c), premix(p) {
}

/***************************
 *                         *
 * class Signal            *
 *                         *
 ***************************/

Resolution Machine::Signal::def_resolution[_MAX_D];
int Machine::Signal::def_frequency[_MAX_D];
int Machine::Signal::def_samples[_MAX_D];
bool Machine::Signal::initiated = false;
std::map<Dimension, std::set<Machine::Signal *> > Machine::Signal::signal_map;

// allocate a signal
Machine::Signal::Signal(int c, Machine *orig, const std::string &nm, Dimension d) :
	SignalBase(nm), 
	originator(orig)

	{
	if(!(initiated && (def_samples[d] > 0))) {
		switch(d) {
		case _0D:
			internal_set_defaults(d, 1024, _fx8p24bit, 44100);
			break;
		case _2D:
			internal_set_defaults(d, 12, _fl32bit, 24);
			break;
		case _MIDI:
			internal_set_defaults(d, 1024, _PTR, 44100);
			break;
		default:
		case _1D:
		case _MAX_D:
			throw jException("Unsupported dimension.", jException::sanity_error);
			break;
		}
	}
		
	// Prepare parameters
	dimension = d;
	channels = c;
		
	// Place signal in signal heap
	internal_register_signal(this);
		
	if(!buffer) {
		throw jException("Out of memory.", jException::sanity_error);
	}
}

Machine::Signal::~Signal() {
	// Place signal in signal heap
	internal_deregister_signal(this);		
	
}

void Machine::Signal::attach(Machine *m) {
	std::map<Machine *, int>::iterator k = 
		attached_machines.find(m);
	
	if(k == attached_machines.end()) {
		attached_machines[m] = 1;
	} else {
		(*k).second += 1;
	}
}

void Machine::Signal::detach(Machine *m) {
	std::map<Machine *, int>::iterator k = 
		attached_machines.find(m);
	
	if(k != attached_machines.end()) {
		(*k).second -= 1;

		if((*k).second == 0)
			attached_machines.erase(k);
	}
}

std::vector<Machine *> Machine::Signal::get_attached_machines() {
	std::vector<Machine *> retval;

	std::map<Machine *, int>::iterator k;
	for(k = attached_machines.begin();
	    k != attached_machines.end();
	    k++) {
		retval.push_back((*k).first);
	}

	return retval;
}

void Machine::Signal::link(Machine *m, Signal *ns) {
	if(dimension == _MIDI)
		throw jException("Cannot attach more than one midi signal to an input.", jException::sanity_error);
	
	if(ns == NULL)
		throw jException("Trying to link NULL signal.", jException::sanity_error);
	
	// to be quick, just insert it before current next..
	Signal *last_next = next[m];
	
	next[m] = ns;	
	next[m]->previous[m] = this;
	next[m]->next[m] = last_next;

	if(last_next != NULL)
		last_next->previous[m] = next[m];
}

Machine::Signal *Machine::Signal::get_next(Machine *m) {
	if(next.find(m) == next.end()) return NULL;
	
	return next[m];
}

Machine::Signal *Machine::Signal::unlink(Machine *m, Signal *ns) {
	Signal *head = this;
	Signal *s = this;

	while(s != NULL) {
		if(s == ns) {
			if(s->previous[m] != NULL)
				s->previous[m]->next[m] = s->next[m];
			if(s->next[m] != NULL)
				s->next[m]->previous[m] = s->previous[m];

			if(s == head)
				head = s->next[m];

			s->next.erase(s->next.find(m));
			s->previous.erase(s->previous.find(m));
			
			return head;
		}
		s = s->next[m];
	}

	throw jException("Failed to unlink signal, not in list.", jException::sanity_error);
}
	
Machine *Machine::Signal::get_originator() {
	return originator;
}

/*******************************
 *                             *
 *                             *
 *      static functions       *
 *                             *
 *                             *
 *******************************/

void Machine::Signal::set_defaults(Dimension d, int s, Resolution r, int f) {
	internal_set_defaults(d,s,r,f);
	
}

void Machine::Signal::get_defaults(Dimension d, int &s, Resolution &r, int &f) {
	internal_get_defaults(d,s,r,f);	
}

/******************/

// allocate a buffer of dimension 0
void Machine::Signal::internal_alloc_0d_buffer(Signal *s) {
	s->frequency = def_frequency[s->dimension];
	s->resolution = def_resolution[s->dimension];
	s->samples = def_samples[s->dimension];
	
	// Calculate memory size
	int bsiz = 0;

	switch(s->resolution) {
	case _8bit:
		bsiz = sizeof(uint8_t);
		break;
	case _16bit:
		bsiz = sizeof(uint16_t);
		break;
	case _32bit:
		bsiz = sizeof(uint32_t);
		break;
	case _fl32bit:
		bsiz = sizeof(float);
		break;
	case _fx8p24bit:
		bsiz = sizeof(fp8p24_t);
		break;		
		
	default: /* error */
	case _MAX_R:
		throw jException("Unsupported resolution for 0D signal.", jException::sanity_error);
	}
	
	// XXX len may overflow!
	int len = bsiz * s->channels * s->samples + 42; // pad with 42 for checking buffer overflows

	// Allocate memory
	void *old_buffer = s->buffer;
	void *new_buffer = NULL;
	if(old_buffer == NULL) {
		new_buffer = malloc(len);
	} else {		
		new_buffer = realloc(old_buffer, len);
	}
	s->buffer = new_buffer;

	if(s->buffer == NULL) throw jException("Failed to allocate buffer.", jException::sanity_error);

	s->internal_clear_buffer();

	{ // print verification pattern
		char *bfr = (char *)new_buffer;
		sprintf(&bfr[len - 42], "THIS_PATTERN_IS_RIGHT");
	}
}

// allocate a buffer for MIDI signals
void Machine::Signal::internal_alloc_MIDI_buffer(Signal *s) {
	s->frequency = def_frequency[s->dimension];
	s->resolution = def_resolution[s->dimension];
	s->samples = def_samples[s->dimension];
	
	// Calculate memory size
	int bsiz = 0;

	switch(s->resolution) {
	case _PTR:
		bsiz = sizeof(void *);
		break;
	default: /* error */
	case _8bit:
	case _16bit:
	case _32bit:;
	case _fl32bit:		
	case _fx8p24bit:
	case _MAX_R:
		throw jException("Unsupported resolution for MIDI signal.", jException::sanity_error);
	}
	
	// XXX len may overflow!
	int len = bsiz * s->channels * s->samples;

	// Allocate memory
	void *old_buffer = s->buffer;
	void *new_buffer = NULL;
	if(old_buffer == NULL) {
		new_buffer = malloc(len);
	} else {		
		new_buffer = realloc(old_buffer, len);
	}
	s->buffer = new_buffer;

	if(s->buffer == NULL) throw jException("Failed to allocate buffer.", jException::sanity_error);
}

void Machine::Signal::internal_register_signal(Signal *s) {
	// allocate buffer data
	switch(s->dimension) {
	case _0D:
		internal_alloc_0d_buffer(s);
		break;
	case _MIDI:
		internal_alloc_MIDI_buffer(s);
		break;
	case _2D:
	case _1D:
	case _MAX_D:
		throw jException("Unsupported dimension.", jException::sanity_error);
	}

	// add signal to map
	signal_map[s->get_dimension()].insert(s);	
}

void Machine::Signal::internal_deregister_signal(Signal *s) {
	// free buffer data
	if(s->buffer != NULL)
		free(s->buffer);
	
	// remove signal from map
	std::set<Signal *>::iterator i;
	i = signal_map[s->get_dimension()].find(s);
	if(i != signal_map[s->get_dimension()].end()) {
		signal_map[s->get_dimension()].erase(i);		
	}
}

void Machine::Signal::internal_set_defaults(Dimension d, int s, Resolution r, int f) {
	if(!initiated) {
		int i;
		for(i = 0; i < _MAX_D; i++) def_samples[i] = 0;

		initiated = true;
	}
	def_samples[d] = s;
	def_resolution[d] = r;
	def_frequency[d] = f;

	// re-allocate all signals of dimension d
	std::set<Signal *>::iterator sig;
	for(sig = signal_map[d].begin();
	    sig != signal_map[d].end();
	    sig++) {
		switch(d) {
		case _0D:
			internal_alloc_0d_buffer((*sig));
			break;
		case _MIDI:
			internal_alloc_MIDI_buffer((*sig));
			break;
		case _2D:
		case _1D:
		case _MAX_D:
		default:
			// this should never be thrown...
			throw jException("Unsupported dimension", jException::sanity_error);
			break;
		}
	}
}

void Machine::Signal::internal_get_defaults(Dimension d, int &s, Resolution &r, int &f) {
	s = def_samples[d];
	r = def_resolution[d];
	f = def_frequency[d];
}

/*******************************
 *                             *
 *                             *
 *      SignalFactory          *
 *                             *
 *                             *
 *******************************/

Machine::Signal *Machine::Signal::SignalFactory::create_signal(int c, Machine *originator,
							       const std::string &name, Dimension d) {
	return new Signal(c, originator, name, d);
}

void Machine::Signal::SignalFactory::destroy_signal(Signal *signal) {
	delete signal;
}
