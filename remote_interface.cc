/*
 * VuKNOB
 * Copyright (C) 2015 by Anton Persson
 *
 * http://www.vuknob.com/
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

/*
 * Parts of this file - Copyright (c) 2003-2014 Christopher M. Kohlhoff (chris at kohlhoff dot com)
 *
 * Original code was distributed under the Boost Software License, Version 1.0. (See http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "CAN'T FIND config.h"
#endif

#include <string>
#include <mutex>
#include <condition_variable>

#include "remote_interface.hh"
#include "dynamic_machine.hh"
#include "machine_sequencer.hh"
#include "common.hh"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

/***************************
 *
 *  Local definitions
 *
 ***************************/

#define VUKNOB_SERVER_PORT 6543
#define VUKNOB_SERVER_PORT_STRING "6543"

#define __MSG_CREATE_OBJECT -1
#define __MSG_FLUSH_ALL_OBJECTS -2
#define __MSG_DELETE_OBJECT -3
#define __MSG_FAILURE_RESPONSE -4

#define __FCT_HANDLELIST     "HandleList"
#define __FCT_RIMACHINE      "RIMachine"

/***************************
 *
 *  Class RemoteInterface::Message
 *
 ***************************/

RemoteInterface::Message::Message() : context(), ostrm(&sbuf), data2send(0) {}

RemoteInterface::Message::Message(RemoteInterface::Context *_context) : context(_context), ostrm(&sbuf), data2send(0) {}

void RemoteInterface::Message::set_value(const std::string &key, const std::string &value) {
	if(key.find(';') != std::string::npos) throw IllegalChar();
	if(key.find('=') != std::string::npos) throw IllegalChar();
	if(value.find(';') != std::string::npos) throw IllegalChar();
	if(value.find('=') != std::string::npos) throw IllegalChar();
	
	key2val[key] = value;

	data2send += key.size() + value.size() + 2; // +2 for the '=' and ';' that encode will add later

	encoded = false;
}

std::string RemoteInterface::Message::get_value(const std::string &key) const {
	auto result = key2val.find(key);
	if(result == key2val.end()) throw NoSuchKey();
	return result->second;
}

bool RemoteInterface::Message::decode_header() {
	std::istream is(&sbuf);
	std::string header;
	is >> header;
	sscanf(header.c_str(), "%08x", &body_length);

	SATAN_DEBUG("in sbuf received header: ]%s[ --> %08x\n", header.c_str(), body_length);

	return true;
}

static std::string decode_string(const std::string &encoded) {
	char inp[encoded.size() + 1];
	(void) strncpy(inp, encoded.c_str(), sizeof(inp));
	int k = 0;
	char *tok = inp;
	
	std::string output = "";
	
	while(inp[k] != '\0') {
		if(inp[k] == '%') { // OK, encoded value - decode it
			inp[k] = '\0';
			output = output + tok;
			
			char v[] = {inp[k + 1], inp[k + 2]};
			for(int u = 0; u < 2; u++) {
				if(v[u] <= 'f' && v[u] >= 'a') v[u] = 0xA + v[u] - 'a';
				else if(v[u] <= 'F' && v[u] >= 'A') v[u] = 0xA + v[u] - 'A';
				else if(v[u] <= '9' && v[u] >= '0') v[u] = v[u] - '0';
			}
			v[0] = (v[0] << 8) | v[1];
			v[1] = '\0';

			output += v;

			k += 3;
			
			tok = &inp[k];			
		} else {
			k++;
		}
	}
	if(*tok != '\0')
		output = output + tok;

//	SATAN_DEBUG("  encoded string: %s\n", encoded.c_str());
//	SATAN_DEBUG("   -> decoded   : %s\n", output.c_str());
	
	return output;
}

static std::string encode_string(const std::string &uncoded) {
	char inp[uncoded.size() + 1];
	(void) strncpy(inp, uncoded.c_str(), sizeof(inp));

	char *tok = inp;	
	std::string output = "";
//	SATAN_DEBUG("  uncoded string: %s\n", uncoded.c_str());
//	SATAN_DEBUG("            temp: %s\n", inp);
	for(int k = 0; inp[k] != '\0'; k++) {
		switch(inp[k]) {
		case '%':
			inp[k] = '\0';
			output = output + tok;
			output = output + "%25";
			tok = &inp[k + 1];
			break;
		case '=':
			inp[k] = '\0';
			output = output + tok;
			output = output + "%3d";
			tok = &inp[k + 1];
			break;
		case ';':
			inp[k] = '\0';
			output = output + tok;
			output = output + "%3b";
			tok = &inp[k + 1];
			break;
		break;
		}
	}
	if(*tok != '\0')
		output = output + tok;

//	SATAN_DEBUG("   -> encoded   : %s\n", output.c_str());

	return output;
}

bool RemoteInterface::Message::decode_body() {
	std::istream is(&sbuf);
	SATAN_DEBUG("Will decode sbuf message, oh yeah...\n");

	std::string key, val;

	std::getline(is, key, '=');
	while(!is.eof() && key != "") {
		std::getline(is, val, ';');
		SATAN_DEBUG("Message decode %s = %s\n", key.c_str(), val.c_str());
		if(key != "" && val != "") {
			key2val[decode_string(key)] = decode_string(val);
		}
		std::getline(is, key, '=');
	}
	
	SATAN_DEBUG("Message decoded...\n");

	return true;
}

void RemoteInterface::Message::encode() const {
	if(encoded) return;
	encoded = true;

	// add header
	char header[9];
	snprintf(header, 9, "%08x", data2send);
	ostrm << header;

	SATAN_DEBUG("  header to send: ]%s[\n", header);
	
	// add body
	for(auto k2v : key2val) {
		ostrm << encode_string(k2v.first) << "=" << encode_string(k2v.second) << ";";
	}

	data2send += header_length; // add header length to the total data size
}

void RemoteInterface::Message::recycle(Message *msg) {
	if(msg->context) {
		msg->sbuf.consume(msg->data2send);
		msg->data2send = 0;
		msg->key2val.clear();
		msg->context->recycle_message(msg);
	} else {
		delete msg;
	}
}

/***************************
 *
 *  Class RemoteInterface::Context
 *
 ***************************/

RemoteInterface::Context::~Context() {
	for(auto msg : available_messages)
		delete msg;
}

std::shared_ptr<RemoteInterface::Message> RemoteInterface::Context::acquire_message() {
	Message *m_ptr = NULL;
	
	if(available_messages.size() == 0) {
		m_ptr = new Message(this);
	} else {
		m_ptr = available_messages.front();
		available_messages.pop_front();
	}
	
	return std::shared_ptr<Message>(m_ptr, Message::recycle);
}

void RemoteInterface::Context::recycle_message(RemoteInterface::Message* used_message) {
	SATAN_DEBUG("Returning used message to MessageStore.");
	available_messages.push_back(used_message);
}

/***************************
 *
 *  Class RemoteInterface::MessageHandler
 *
 ***************************/

void RemoteInterface::MessageHandler::do_read_header() {
	auto self(shared_from_this());
	asio::async_read(
		my_socket,
		read_msg.sbuf.prepare(Message::header_length),
		[this, self](std::error_code ec, std::size_t length) {
			if(!ec) read_msg.sbuf.commit(length);
			
			if(!ec && read_msg.decode_header()) {				
				do_read_body();
			} else {
				on_connection_dropped();
			}
		}
		);
}

void RemoteInterface::MessageHandler::do_read_body() {
	auto self(shared_from_this());
	asio::async_read(
		my_socket,
		read_msg.sbuf.prepare(read_msg.body_length),
		[this, self](std::error_code ec, std::size_t length) {
			SATAN_DEBUG("  maybe will call on_message_received....(%s)\n", ec.message().c_str());
			if(!ec) read_msg.sbuf.commit(length);
			
			if(!ec && read_msg.decode_body()) {
				SATAN_DEBUG(" will call on_message_received()\n");

				try {
					on_message_received(read_msg);
				} catch (std::exception& e) {
					SATAN_DEBUG("exception caught: %s\n", e.what());
					on_connection_dropped();
				}

				do_read_header();
			} else {
				SATAN_DEBUG("   ooops - error code!\n");
				on_connection_dropped();
			}
		}
		);
}

void RemoteInterface::MessageHandler::do_write() {
	auto self(shared_from_this());
	asio::async_write(
		my_socket,
		write_msgs.front()->sbuf.data(),
		[this, self](std::error_code ec, std::size_t length) {
			SATAN_DEBUG("Wrote %d bytes of data.\n", length);
			if (!ec) {
				write_msgs.pop_front();
				if (!write_msgs.empty()) {
					do_write();
				}
			} else {
				on_connection_dropped();
			}
		}
		);
}

RemoteInterface::MessageHandler::MessageHandler(asio::io_service &io_service) : my_socket(io_service) {
}

RemoteInterface::MessageHandler::MessageHandler(asio::ip::tcp::socket _socket) :
	my_socket(std::move(_socket)) {
}

void RemoteInterface::MessageHandler::start_receive() {
	do_read_header();
}

void RemoteInterface::MessageHandler::deliver_message(std::shared_ptr<Message> &msg) {
	msg->encode();
	
	bool write_in_progress = !write_msgs.empty();
	write_msgs.push_back(msg);
	if (!write_in_progress){
		do_write();
	}
}

/***************************
 *
 *  Class RemoteInterface::BaseObject::Factory
 *
 ***************************/

RemoteInterface::BaseObject::Factory::Factory(const char* _type) : type(_type) {
	if(factories.find(type) != factories.end()) throw FactoryAlreadyCreated();

	factories[type] = this;
}

RemoteInterface::BaseObject::Factory::~Factory() {
	auto factory_iterator = factories.find(type);
	if(factory_iterator != factories.end()) {
		factories.erase(factory_iterator);
	}
}

const char* RemoteInterface::BaseObject::Factory::get_type() const {
	return type;
}

/***************************
 *
 *  Class RemoteInterface::BaseObject
 *
 ***************************/

RemoteInterface::BaseObject::BaseObject(const Factory *_factory, const Message &serialized) : __is_server_side(false), my_factory(_factory) {
	// this constructor should only be used client side
	obj_id = std::stol(serialized.get_value("new_objid"));
}

RemoteInterface::BaseObject::BaseObject(int32_t new_obj_id, const Factory *_factory) : __is_server_side(true), obj_id(new_obj_id), my_factory(_factory) {
	// this constructor should only be used server side
}

void RemoteInterface::BaseObject::send_object_message(std::function<void(std::shared_ptr<Message> &msg_to_send)> complete_message) {
	context->dispatch_action(
		[this, complete_message]() {
			std::shared_ptr<Message> msg2send = context->acquire_message();

			{
				// fill in object id header
				msg2send->set_value("id", std::to_string(obj_id));
			}
			{
				// call complete message
				complete_message(msg2send);
			}

			context->distribute_message(msg2send);
		}
		);
}

int32_t RemoteInterface::BaseObject::get_obj_id() {
	return obj_id;
}

auto RemoteInterface::BaseObject::get_type() -> ObjectType {
	return {my_factory->get_type()};
}

void RemoteInterface::BaseObject::set_context(Context* _context) {
	context = _context;
}

std::shared_ptr<RemoteInterface::BaseObject> RemoteInterface::BaseObject::create_object_from_message(const Message &msg) {
	std::string factory_type = msg.get_value("factory");
	
	auto factory_iterator = factories.find(factory_type);
	if(factory_iterator == factories.end()) throw NoSuchFactory();

	std::shared_ptr<RemoteInterface::BaseObject> o = factory_iterator->second->create(msg);

	SATAN_DEBUG("BaseObject -- calling post_constructor_client.\n");
	o->post_constructor_client();
	
	return o;
}

std::shared_ptr<RemoteInterface::BaseObject> RemoteInterface::BaseObject::create_object_on_server(int32_t new_obj_id,
												  const std::string &factory_type) {
	auto factory_iterator = factories.find(factory_type);
	if(factory_iterator == factories.end()) throw NoSuchFactory();
	
	return factory_iterator->second->create(new_obj_id);
}

std::map<std::string, RemoteInterface::BaseObject::Factory *> RemoteInterface::BaseObject::factories;

/***************************
 *
 *  Class RemoteInterface::HandleList::HandleListFactory
 *
 ***************************/

RemoteInterface::HandleList::HandleListFactory::HandleListFactory() : Factory(__FCT_HANDLELIST) {}

std::shared_ptr<RemoteInterface::BaseObject> RemoteInterface::HandleList::HandleListFactory::create(const Message &serialized) {
	std::shared_ptr<HandleList> hlst = std::make_shared<HandleList>(this, serialized);
	clientside_handle_list = hlst;
	return hlst;
}

std::shared_ptr<RemoteInterface::BaseObject> RemoteInterface::HandleList::HandleListFactory::create(int32_t new_obj_id) {
	return std::make_shared<HandleList>(new_obj_id, this);
}

/***************************
 *
 *  Class RemoteInterface::HandleList
 *
 ***************************/

RemoteInterface::HandleList::HandleList(const Factory *factory, const Message &serialized) : BaseObject(factory, serialized) {
	SATAN_DEBUG("Handle list created - client side.\n");

	SATAN_DEBUG("Will decode handles...\n");

	std::stringstream handles(serialized.get_value("handles"));
	std::string handle;

	std::getline(handles, handle, ':');
	while(!handles.eof() && handle != "") {
		std::stringstream hint_key;
		hint_key << "hint_" << handle;
		std::string hint = serialized.get_value(hint_key.str());
		
		SATAN_DEBUG("Handle decode %s = %s\n", handle.c_str(), hint.c_str());

		handle2hint[handle] = hint;

		std::getline(handles, handle, ':');
	}
	
	SATAN_DEBUG("Handles decoded...\n");
}

RemoteInterface::HandleList::HandleList(int32_t new_obj_id, const Factory *factory) : BaseObject(new_obj_id, factory) {
	SATAN_DEBUG("Handle list created - server side.\n");
}

void RemoteInterface::HandleList::post_constructor_client() {}

void RemoteInterface::HandleList::process_message(Server *context, const Message &msg) {
	std::string command = msg.get_value("command");
	
	SATAN_DEBUG("HandleList - server - command [%s]\n", command.c_str());

	if(command == "instance") {
		std::string new_m_handle = msg.get_value("handle");
		double xpos = atof(msg.get_value("xpos").c_str());
		double ypos = atof(msg.get_value("ypos").c_str());
		SATAN_DEBUG("   create instance with handle [%s]\n", new_m_handle.c_str());
		Machine *new_machine = DynamicMachine::instance(new_m_handle, (float)xpos, (float)ypos);
		if(new_machine) {
			try {
				MachineSequencer::create_sequencer_for_machine(new_machine);
			} catch(...) {
				// something went wrong - destroy the new machine
				Machine::disconnect_and_destroy(new_machine);
			}
		}
	}
}

void RemoteInterface::HandleList::process_message(Client *context, const Message &msg) {
}

void RemoteInterface::HandleList::serialize(std::shared_ptr<Message> &target) {
	std::set<std::string> handles = DynamicMachine::get_handle_set();
	std::stringstream handles_serialized;

	for(auto handle : handles) {
		handles_serialized << handle << ":";

		std::stringstream hint_key;
		hint_key << "hint_" << handle;
		target->set_value(hint_key.str(), DynamicMachine::get_handle_hint(handle));
	}
	target->set_value("handles", handles_serialized.str());
}

void RemoteInterface::HandleList::on_delete(Client *context) { /* noop */ }

std::map<std::string, std::string> RemoteInterface::HandleList::get_handles_and_hints() {
	std::map<std::string, std::string> retval;

	if(auto hlst = clientside_handle_list.lock()) {
		hlst->context->dispatch_action(
			[hlst, &retval]() {
				retval = hlst->handle2hint;
			}
			);
	} else {
		throw RemoteInterface::Client::ClientNotConnected();
	}
	
	return retval;
}

void RemoteInterface::HandleList::create_instance(const std::string &handle, double xpos, double ypos) {
	if(auto hlst = clientside_handle_list.lock()) {
		hlst->send_object_message(
			[hlst, &handle, xpos, ypos](std::shared_ptr<Message> &msg2send) {
				
				msg2send->set_value("command", "instance");
				msg2send->set_value("handle", handle);
				msg2send->set_value("xpos", std::to_string(xpos));
				msg2send->set_value("ypos", std::to_string(ypos));
				
				SATAN_DEBUG("Sent instance command to server HandleList object.");
			}
			);
	} else {
		throw RemoteInterface::Client::ClientNotConnected();
	}
}

std::weak_ptr<RemoteInterface::HandleList> RemoteInterface::HandleList::clientside_handle_list;
RemoteInterface::HandleList::HandleListFactory RemoteInterface::HandleList::handlelist_factory;

/***************************
 *
 *  Class RemoteInterface::RIMachine::RIMachineFactory
 *
 ***************************/

RemoteInterface::RIMachine::RIMachineFactory::RIMachineFactory() : Factory(__FCT_RIMACHINE) {}

std::shared_ptr<RemoteInterface::BaseObject> RemoteInterface::RIMachine::RIMachineFactory::create(const Message &serialized) {
	std::shared_ptr<RIMachine> hlst = std::make_shared<RIMachine>(this, serialized);
	return hlst;
}

std::shared_ptr<RemoteInterface::BaseObject> RemoteInterface::RIMachine::RIMachineFactory::create(int32_t new_obj_id) {
	return std::make_shared<RIMachine>(new_obj_id, this);
}

/***************************
 *
 *  Class RemoteInterface::RIMachine
 *
 ***************************/

static void parse_io(std::vector<std::string> &target, const std::string source) {
	std::stringstream is(source);

	std::string io;
	
	// get first source objid
	std::getline(is, io, ':');
	while(!is.eof() && io != "") {
		target.push_back(io);
				
		// find start of next
		std::getline(is, io, ':');
	}
}

RemoteInterface::RIMachine::RIMachine(const Factory *factory, const Message &serialized) : BaseObject(factory, serialized) {
	name = serialized.get_value("name");
	type = serialized.get_value("type");
	xpos = atof(serialized.get_value("xpos").c_str());
	ypos = atof(serialized.get_value("ypos").c_str());

	SATAN_DEBUG("RIMachine created - client side. (%f, %f) -- [%s]\n", xpos, ypos, serialized.get_value("xpos").c_str());

	parse_serialized_connections_data(serialized.get_value("connections"));
	try {
		parse_io(inputs, serialized.get_value("inputs"));
	} catch(Message::NoSuchKey &e) { /* ignore empty inputs */ }
	try {
		parse_io(outputs, serialized.get_value("outputs"));
	} catch(Message::NoSuchKey &e) { /* ignore empty inputs */ }
}

RemoteInterface::RIMachine::RIMachine(int32_t new_obj_id, const Factory *factory) : BaseObject(new_obj_id, factory) {
	SATAN_DEBUG("RIMachine created - server side.\n");
}


void RemoteInterface::RIMachine::parse_serialized_connections_data(std::string serialized) {
	std::stringstream is(serialized);
	SATAN_DEBUG("Will decode serialized connection data...\n");

	std::string srcid, src_name, dst_name;

	// get first source objid
	std::getline(is, srcid, '@');
	while(!is.eof() && srcid != "") {
		// get source output name
		std::getline(is, src_name, '{');

		// get destination input name
		std::getline(is, dst_name, '}');

		// store connection
		SATAN_DEBUG("Connection decode [%s]:%s => %s\n", srcid.c_str(), src_name.c_str(), dst_name.c_str());
		if(srcid != "" && src_name != "" && dst_name != "") {
			connection_data.insert({(int32_t)std::stol(srcid), {src_name, dst_name}});
		}
		
		// find start of next
		std::getline(is, srcid, '@');
	}
	
	SATAN_DEBUG("Serialized connections decoded...\n");
	
}

void RemoteInterface::RIMachine::call_listeners(std::function<void(std::shared_ptr<RIMachineStateListener> listener)> callback) {

	SATAN_DEBUG("RIMachine -- registered state listeners for %p : %d\n", this, state_listeners.size());
	auto weak_listener = state_listeners.begin();
	while(weak_listener != state_listeners.end()) {
		if(auto listener = (*weak_listener).lock()) {
			ri_machine_mutex.unlock();
			callback(listener);
			ri_machine_mutex.lock();
			weak_listener++;
		} else { // if we cannot lock, the listener doesn't exist so we remove it from the set.
			weak_listener = state_listeners.erase(weak_listener);
		}
	}
}

void RemoteInterface::RIMachine::serverside_init_from_machine_ptr(Machine *m_ptr) {
	name = m_ptr->get_name();

	// set the machine type
	type = "unknown"; // default

	{ // see if it's a MachineSequencer
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((m_ptr));
		if(mseq != NULL) type = "MachineSequencer";
	}
	{ // see if it's a DynamicMachine
		DynamicMachine *dmch = dynamic_cast<DynamicMachine *>((m_ptr));
		if(dmch != NULL) type = "DynamicMachine";
	}

	real_machine_ptr = m_ptr;

	xpos = m_ptr->get_x_position();
	ypos = m_ptr->get_y_position();

	inputs = m_ptr->get_input_names();
	outputs = m_ptr->get_output_names();
}

void RemoteInterface::RIMachine::attach_input(std::shared_ptr<RIMachine> source_machine,
					      const std::string &source_output_name,
					      const std::string &destination_input_name) {
	if(is_server_side()) {
		/* server side */
		int32_t source_objid = source_machine->get_obj_id();

		// add the data to our internal storage
		connection_data.insert({source_objid, {source_output_name, destination_input_name}});

		// transmit the update to the connected clients
		send_object_message(
			[source_objid, source_output_name, destination_input_name](std::shared_ptr<Message> &msg2send) {
				
				msg2send->set_value("command", "attach");				
				msg2send->set_value("s_objid", std::to_string(source_objid));
				msg2send->set_value("source", source_output_name);
				msg2send->set_value("destination", destination_input_name);
				
			}
			);
	} else {
		/* client side */
		int32_t source_objid = source_machine->get_obj_id();

		// transmit the attach request to the server
		send_object_message(
			[source_objid, source_output_name, destination_input_name](std::shared_ptr<Message> &msg2send) {
				
				msg2send->set_value("command", "r_attach");				
				msg2send->set_value("s_objid", std::to_string(source_objid));
				msg2send->set_value("source", source_output_name);
				msg2send->set_value("destination", destination_input_name);
				
			}
			);
	}
}

void RemoteInterface::RIMachine::detach_input(std::shared_ptr<RIMachine> source_machine,
					      const std::string &source_output_name,
					      const std::string &destination_input_name) {
	if(is_server_side()) {
		/* server side */
		int32_t source_objid = source_machine->get_obj_id();

		// remove the connection data from our internal storage
		auto range = connection_data.equal_range(source_objid);

		for(auto k = range.first; k != range.second; ) {
			if((*k).second.first == source_output_name &&
			   (*k).second.second == destination_input_name) {
				k = connection_data.erase(k);
			} else {
				k++;
			}
		}

		// transmit the update to the connected clients
		send_object_message(
			[source_objid, source_output_name, destination_input_name](std::shared_ptr<Message> &msg2send) {
				msg2send->set_value("command", "detach");
				msg2send->set_value("s_objid", std::to_string(source_objid));
				msg2send->set_value("source", source_output_name);
				msg2send->set_value("destination", destination_input_name);
			}
			);
	} else {
		/* client side */
		int32_t source_objid = source_machine->get_obj_id();

		// transmit the attach request to the server
		send_object_message(
			[source_objid, source_output_name, destination_input_name](std::shared_ptr<Message> &msg2send) {
				
				msg2send->set_value("command", "r_detach");				
				msg2send->set_value("s_objid", std::to_string(source_objid));
				msg2send->set_value("source", source_output_name);
				msg2send->set_value("destination", destination_input_name);
				
			}
			);
	}
}


std::string RemoteInterface::RIMachine::get_name() {
	std::lock_guard<std::mutex> lock_guard(ri_machine_mutex);
	return name;
}

std::string RemoteInterface::RIMachine::get_machine_type() {
	std::lock_guard<std::mutex> lock_guard(ri_machine_mutex);
	return type;
}

double RemoteInterface::RIMachine::get_x_position() {
	std::lock_guard<std::mutex> lock_guard(ri_machine_mutex);
	return xpos;
}	

double RemoteInterface::RIMachine::get_y_position() {
	std::lock_guard<std::mutex> lock_guard(ri_machine_mutex);
	return ypos;
}

void RemoteInterface::RIMachine::set_position(double xp, double yp) {
	if(is_server_side()) {
		// we also need to update all the clients of this change
		auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
		send_object_message(
			[this, thiz, xp, yp](std::shared_ptr<Message> &msg2send) {
				msg2send->set_value("command", "setpos");
				msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away
				msg2send->set_value("xpos", std::to_string(xp));
				msg2send->set_value("ypos", std::to_string(yp));
			}
			);		
	} else {
		SATAN_DEBUG("RIMachine - client - sending new coords to server... (%f, %f)\n", xp, yp);
		
		auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
		send_object_message(
			[this, xp, yp, thiz](std::shared_ptr<Message> &msg2send) {
				std::lock_guard<std::mutex> lock_guard(ri_machine_mutex);
				
				msg2send->set_value("command", "setpos");
				msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away
				msg2send->set_value("xpos", std::to_string(xp));
				msg2send->set_value("ypos", std::to_string(yp));
			}
			);
	}
}

void RemoteInterface::RIMachine::delete_machine() {
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[this, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "deleteme");
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away
		}
		);
}

std::vector<std::string> RemoteInterface::RIMachine::get_input_names() {
	std::lock_guard<std::mutex> lock_guard(ri_machine_mutex);
	return inputs;
}

std::vector<std::string> RemoteInterface::RIMachine::get_output_names() {
	std::lock_guard<std::mutex> lock_guard(ri_machine_mutex);
	return outputs;
}

void RemoteInterface::RIMachine::set_state_change_listener(std::weak_ptr<RIMachineStateListener> state_listener) {
	{
		std::lock_guard<std::mutex> lock_guard(ri_machine_mutex);
		state_listeners.insert(state_listener);
		SATAN_DEBUG("    ********** INSERT STATE LISTENER for %p - size is: %d\n",
			    this, state_listeners.size());
	}
	// call all state listener callbacks
	if(auto listener = state_listener.lock()) {
		listener->on_move();

		ri_machine_mutex.lock();		
		for(auto connection : connection_data) {
			auto mch = std::dynamic_pointer_cast<RIMachine>(context->get_object(connection.first));

			ri_machine_mutex.unlock();
			listener->on_attach(mch, connection.second.first, connection.second.second);
			ri_machine_mutex.lock();
		}
		ri_machine_mutex.unlock();
	}
	SATAN_DEBUG("    ********** STATE LISTENER INSERTED.\n");
}

void RemoteInterface::RIMachine::post_constructor_client() {
	for(auto weak_listener : listeners) {
		if(auto listener = weak_listener.lock()) {
			auto mch = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
			listener->ri_machine_registered(mch);
		}
	}
}

void RemoteInterface::RIMachine::process_message(Server *context, const Message &msg) {
	std::string command = msg.get_value("command");
	
	SATAN_DEBUG("RIMachine - server - command [%s]\n", command.c_str());

	if(command == "deleteme") {
		SATAN_DEBUG("   --> calling Machine::disconnect_and_destroy(%p)\n", real_machine_ptr);
		Machine::disconnect_and_destroy(real_machine_ptr);
	} else if(command == "setpos") {
		xpos = atof(msg.get_value("xpos").c_str());
		ypos = atof(msg.get_value("ypos").c_str());
		real_machine_ptr->set_position(xpos, ypos);

		// we also need to update all the clients of this change
		auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
		send_object_message(
			[this, thiz](std::shared_ptr<Message> &msg2send) {
				msg2send->set_value("command", "setpos");
				msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away
				msg2send->set_value("xpos", std::to_string(xpos));
				msg2send->set_value("ypos", std::to_string(ypos));
			}
			);		
	} else if(command == "r_attach") {
		process_attach_message(context, msg);
	} else if(command == "r_detach") {
		process_detach_message(context, msg);
	}
}

void RemoteInterface::RIMachine::process_attach_message(Context *context, const Message &msg) {
	int32_t identifier = std::stol(msg.get_value("s_objid"));
	std::string src_output = msg.get_value("source");
	std::string dst_input = msg.get_value("destination");

	if(is_server_side()) {
		SATAN_DEBUG(" process attach, server side...\n");
		try {
			auto mch = std::dynamic_pointer_cast<RIMachine>(context->get_object(identifier));
			real_machine_ptr->attach_input(mch->real_machine_ptr, src_output, dst_input);
		} catch(NoSuchObject exp) {
			/* ignored for now */
		} catch(jException exp) {
			throw Context::FailureResponse(exp.message);
		}
		
	} else {
		SATAN_DEBUG(" process attach, client side...\n");
		
		try {
			auto mch = std::dynamic_pointer_cast<RIMachine>(context->get_object(identifier));
			
			SATAN_DEBUG(" process attach -- call listeners \n");
			
			call_listeners(
				[this, &mch, &src_output, &dst_input](std::shared_ptr<RIMachineStateListener> listener) {
					SATAN_DEBUG(" process attach -- call listener->on_attach() \n");
					listener->on_attach(mch, src_output, dst_input);
				}
				);			
		} catch(NoSuchObject exp) { /* ignored for now */ }
		
		// add the data to our internal storage
		connection_data.insert({identifier, {src_output, dst_input}});
	}
}

void RemoteInterface::RIMachine::process_detach_message(Context *context, const Message &msg) {
	int32_t identifier = std::stol(msg.get_value("s_objid"));
	std::string src_output = msg.get_value("source");
	std::string dst_input = msg.get_value("destination");
	
	if(is_server_side()) {
		SATAN_DEBUG(" process detach, server side...\n");
		try {
			auto mch = std::dynamic_pointer_cast<RIMachine>(context->get_object(identifier));
			real_machine_ptr->detach_input(mch->real_machine_ptr, src_output, dst_input);
		} catch(NoSuchObject exp) {
			/* ignored for now */
		} catch(jException exp) {
			throw Context::FailureResponse(exp.message);
		}
	} else {
		SATAN_DEBUG(" process detach, client side...\n");
		try {
			auto mch = std::dynamic_pointer_cast<RIMachine>(context->get_object(identifier));
			
			call_listeners(
				[this, &mch, &src_output, &dst_input](std::shared_ptr<RIMachineStateListener> listener) {
					listener->on_detach(mch, src_output, dst_input);
				}
				);
			
		} catch(NoSuchObject exp) { /* ignored for now */ }
		
		// remove the connection data from our internal storage
		auto range = connection_data.equal_range(identifier);
		
		for(auto k = range.first; k != range.second; ) {
			if((*k).second.first == src_output &&
			   (*k).second.second == dst_input) {
				k = connection_data.erase(k);
			} else {
				k++;
			}
		}
	}
}

void RemoteInterface::RIMachine::process_message(Client *context, const Message &msg) {
	std::lock_guard<std::mutex> lock_guard(ri_machine_mutex);
	std::string command = msg.get_value("command");
	
	SATAN_DEBUG("RIMachine - client - command [%s]\n", command.c_str());
	
	if(command == "setpos") {
		xpos = atof(msg.get_value("xpos").c_str());
		ypos = atof(msg.get_value("ypos").c_str());
		SATAN_DEBUG("RIMachine - client - position updated (%f, %f)\n", xpos, ypos);
		
		call_listeners(
			[](std::shared_ptr<RIMachineStateListener> listener) {
				listener->on_move();
			}
			);
		
	} else if(command == "attach") {
		process_attach_message(context, msg);
	} else if(command == "detach") {
		process_detach_message(context, msg);
	}
}

void RemoteInterface::RIMachine::serialize(std::shared_ptr<Message> &target) {
	{
		target->set_value("name", name);
		target->set_value("type", type);
		target->set_value("xpos", std::to_string(xpos));
		target->set_value("ypos", std::to_string(ypos));
	}
	{
		std::stringstream connection_string;
		for(auto connection : connection_data) {
			connection_string << connection.first << "@" << connection.second.first << "{" << connection.second.second << "}";
		}
		if(connection_string.str() == "") {
			target->set_value("connections", "@{}"); // no connections
		} else {
			target->set_value("connections", connection_string.str());
		}
	}

	{
		std::stringstream inputs_string;
		for(auto input : inputs) {
			inputs_string << input << ":";
		}
		target->set_value("inputs", inputs_string.str());
	}
	{
		std::stringstream outputs_string;
		for(auto output : outputs) {
			outputs_string << output << ":";
		}
		target->set_value("outputs", outputs_string.str());
	}
}

void RemoteInterface::RIMachine::on_delete(Client *context) {
	for(auto weak_listener : listeners) {
		if(auto listener = weak_listener.lock()) {
			auto mch = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
			listener->ri_machine_unregistered(mch);
		}
	}
}

void RemoteInterface::RIMachine::register_ri_machine_set_listener(std::weak_ptr<RIMachineSetListener> listener) {
	listeners.insert(listener);
}

RemoteInterface::RIMachine::RIMachineFactory RemoteInterface::RIMachine::rimachine_factory;
std::set<std::weak_ptr<RemoteInterface::RIMachine::RIMachineSetListener>,
	 std::owner_less<std::weak_ptr<RemoteInterface::RIMachine::RIMachineSetListener> > > RemoteInterface::RIMachine::listeners;

/***************************
 *
 *  Class RemoteInterface::Client
 *
 ***************************/

std::shared_ptr<RemoteInterface::Client> RemoteInterface::Client::client;
std::mutex RemoteInterface::Client::client_mutex;
asio::io_service RemoteInterface::Client::io_service;

RemoteInterface::Client::Client(const std::string &server_host,
				std::function<void()> _disconnect_callback,
				std::function<void(const std::string &failure_response)> _failure_response_callback) :
	MessageHandler(io_service), resolver(io_service), disconnect_callback(_disconnect_callback)
{
	failure_response_callback = _failure_response_callback;
	auto endpoint_iterator = resolver.resolve({server_host, VUKNOB_SERVER_PORT_STRING });

	SATAN_DEBUG("RemoteInterface::Client::Client() trying to connect...\n");
	asio::async_connect(my_socket, endpoint_iterator,
			    [this](std::error_code ec, asio::ip::tcp::resolver::iterator)
			    {
				    SATAN_DEBUG("RemoteInterface::Client::Client() connected!\n");
				    if (!ec)
				    {
					    SATAN_DEBUG("RemoteInterface::Client::Client() start to receive...!\n");
					    start_receive();
				    }
			    }
		);
}

void RemoteInterface::Client::flush_all_objects() {
	for(auto objid2obj : all_objects) {
		objid2obj.second->on_delete(this);
	}
	all_objects.clear();
	
}

void RemoteInterface::Client::on_message_received(const Message &msg) {
	SATAN_DEBUG(" on_message_received, client\n");
	int identifier = std::stol(msg.get_value("id"));

	switch(identifier) {
	case __MSG_CREATE_OBJECT:
	{
		std::shared_ptr<BaseObject> new_obj = BaseObject::create_object_from_message(msg);
		new_obj->set_context(this);
		
		int32_t obj_id = new_obj->get_obj_id();
		
		if(all_objects.find(obj_id) != all_objects.end()) throw BaseObject::DuplicateObjectId();
		if(obj_id < 0) throw BaseObject::ObjIdOverflow();
		
		all_objects[new_obj->get_obj_id()] = new_obj;
	}
	break;

	case __MSG_FLUSH_ALL_OBJECTS:
	{
		flush_all_objects();
	}
	break;

	case __MSG_DELETE_OBJECT:
	{
		auto obj_iterator = all_objects.find(std::stol(msg.get_value("objid")));
		if(obj_iterator != all_objects.end()) {
			obj_iterator->second->on_delete(this);
			all_objects.erase(obj_iterator);
		}
	}
	break;

	case __MSG_FAILURE_RESPONSE:
	{
		failure_response_callback(msg.get_value("response"));
	}
	break;

	default:
	{
		auto obj_iterator = all_objects.find(identifier);
		if(obj_iterator == all_objects.end()) throw BaseObject::NoSuchObject();

		obj_iterator->second->process_message(this, msg);
	}
	break;
	}
}

void RemoteInterface::Client::on_connection_dropped() {
	SATAN_DEBUG("RemoteInterface::Client::on_connection_dropped()\n");
	flush_all_objects();
	disconnect_callback();
}

void RemoteInterface::Client::start_client(const std::string &server_host,
					   std::function<void()> disconnect_callback,
					   std::function<void(const std::string &fresp)> failure_response_callback) {
	std::lock_guard<std::mutex> lock_guard(client_mutex);
	
	try {
		client = std::shared_ptr<Client>(new Client(server_host, disconnect_callback, failure_response_callback));
		
		client->t1 = std::thread([]() {
				client->io_service.run();
				SATAN_DEBUG("client io_service finished.\n");
				}
			);
	} catch (std::exception& e) {
		SATAN_DEBUG("exception caught: %s\n", e.what());
	}
}

void RemoteInterface::Client::disconnect() {
	std::lock_guard<std::mutex> lock_guard(client_mutex);
	if(client) {
		client->io_service.dispatch(
		[]()
		{			
			SATAN_DEBUG("RemoteInterface::Client will disconnect from server...\n");
			try {
				client->my_socket.close();
			} catch(std::exception &exp) {
				SATAN_DEBUG("exception caught when calling create_service_objects(%s).\n", exp.what());
			}
		});

		client->io_service.stop();
		client->t1.join();

		client.reset();
		SATAN_DEBUG("client shared_ptr was reset.\n");
	}
}

void RemoteInterface::Client::register_ri_machine_set_listener(std::weak_ptr<RIMachine::RIMachineSetListener> ri_mset_listener) {
	std::lock_guard<std::mutex> lock_guard(client_mutex);
	if(client) {
		client->io_service.dispatch(
		[ri_mset_listener]()
		{			
			RIMachine::register_ri_machine_set_listener(ri_mset_listener);
		});
	} else {
		// if disconnected, we can go ahead and directly register...
		RIMachine::register_ri_machine_set_listener(ri_mset_listener);		
	}
		
}

void RemoteInterface::Client::distribute_message(std::shared_ptr<Message> &msg) {
	deliver_message(msg);
}

void RemoteInterface::Client::post_action(std::function<void()> f) {
	io_service.post(f);
}

void RemoteInterface::Client::dispatch_action(std::function<void()> f) {
	std::mutex mtx;
	std::condition_variable cv;
	bool ready = false;
	
	io_service.dispatch(
		[&ready, &mtx, &cv, &f]() {
			f();
			std::unique_lock<std::mutex> lck(mtx);
			ready = true;
			cv.notify_all();
		}
		);

	std::unique_lock<std::mutex> lck(mtx);
	while (!ready) cv.wait(lck);
}

auto RemoteInterface::Client::get_object(int32_t objid) -> std::shared_ptr<BaseObject> {
	if(all_objects.find(objid) == all_objects.end()) throw BaseObject::NoSuchObject();

	return all_objects[objid];
}

/***************************
 *
 *  Class RemoteInterface::Server::ClientAgent
 *
 ***************************/

void RemoteInterface::Server::ClientAgent::send_handler_message() {
}

RemoteInterface::Server::ClientAgent::ClientAgent(asio::ip::tcp::socket _socket, Server *_server) :
	MessageHandler(std::move(_socket)), server(_server) {
}

void RemoteInterface::Server::ClientAgent::start() {
	send_handler_message();
	start_receive();
}

void RemoteInterface::Server::ClientAgent::disconnect() {
	my_socket.close();
}

void RemoteInterface::Server::ClientAgent::on_message_received(const Message &msg) {
	std::string resp_msg;
	try {
		server->route_incomming_message(msg);
	} catch(Context::FailureResponse &fresp) {
		resp_msg = fresp.response_message;
	}

	if(resp_msg.size() != 0) {
		std::shared_ptr<Message> response = server->acquire_message();
		response->set_value("id", std::to_string(__MSG_FAILURE_RESPONSE));
		response->set_value("response", resp_msg);
		deliver_message(response);
	}
}

void RemoteInterface::Server::ClientAgent::on_connection_dropped() {
	server->drop_client(std::static_pointer_cast<ClientAgent>(shared_from_this()));
}

/***************************
 *
 *  Class RemoteInterface::Server
 *
 ***************************/

std::shared_ptr<RemoteInterface::Server> RemoteInterface::Server::server;
std::mutex RemoteInterface::Server::server_mutex;

int32_t RemoteInterface::Server::reserve_new_obj_id() {
	int32_t new_obj_id = ++last_obj_id;

	if(new_obj_id < 0) throw BaseObject::ObjIdOverflow();

	return new_obj_id;
}

void RemoteInterface::Server::create_object_from_factory(const std::string &factory_type,
							 std::function<void(std::shared_ptr<BaseObject> nuobj)> new_object_init_callback) {
	int32_t new_obj_id = reserve_new_obj_id();
		
	std::shared_ptr<BaseObject> new_obj = BaseObject::create_object_on_server(new_obj_id, factory_type);
	new_obj->set_context(this);
	all_objects[new_obj_id] = new_obj;

	new_object_init_callback(new_obj);
	
	std::shared_ptr<Message> create_object_message = acquire_message();
	add_create_object_header(create_object_message, new_obj);
	new_obj->serialize(create_object_message);
	distribute_message(create_object_message);
}

void RemoteInterface::Server::delete_object(std::shared_ptr<BaseObject> obj2delete) {
	for(auto obj  = all_objects.begin();
	    obj != all_objects.end();
	    obj++) {
		if((*obj).second == obj2delete) {
			
			std::shared_ptr<Message> destroy_object_message = acquire_message();
			add_destroy_object_header(destroy_object_message, obj2delete);
			distribute_message(destroy_object_message);
			
			all_objects.erase(obj);
			return;
		}
	}
}

void RemoteInterface::Server::project_loaded() {
	io_service.post(

		[this]()
		{
			for(auto m2ri : machine2rimachine) {
				auto x = m2ri.first->get_x_position();
				auto y = m2ri.first->get_y_position();
				m2ri.second->set_position(x, y);
			}
		}
		
		);
}

void RemoteInterface::Server::machine_registered(Machine *m_ptr) {
	SATAN_DEBUG("Machine %p was registered\n", m_ptr);

	io_service.post(

		[this, m_ptr]()
		{
			create_object_from_factory(__FCT_RIMACHINE,
						   [this, m_ptr](std::shared_ptr<BaseObject> nuobj) {
							   auto mch = std::dynamic_pointer_cast<RIMachine>(nuobj);
							   mch->serverside_init_from_machine_ptr(m_ptr);

							   machine2rimachine[m_ptr] = mch;
						   }
				);
		}
		
		);
}

void RemoteInterface::Server::machine_unregistered(Machine *m_ptr) {
	SATAN_DEBUG("Machine %p was unregistered\n", m_ptr);

	io_service.post(

		[this, m_ptr]()
		{
			auto mch = machine2rimachine.find(m_ptr);
			if(mch != machine2rimachine.end()) {
				delete_object(mch->second);
				machine2rimachine.erase(mch);
			}
			Machine::dereference_machine(m_ptr);
		}
		
		);
}

void RemoteInterface::Server::machine_input_attached(Machine *source, Machine *destination,
						     const std::string &output_name,
						     const std::string &input_name) {
	SATAN_DEBUG("Signal attached [%s:%s] -> [%s:%s]\n",
		    source->get_name().c_str(), output_name.c_str(),
		    destination->get_name().c_str(), input_name.c_str());

	io_service.post(
		[this, source, destination, output_name, input_name]()
		{
			auto src_mch = machine2rimachine.find(source);
			auto dst_mch = machine2rimachine.find(destination);
			if(src_mch != machine2rimachine.end() && dst_mch != machine2rimachine.end()) {
				dst_mch->second->attach_input(src_mch->second, output_name, input_name);
			}
		}
		
		);
}

void RemoteInterface::Server::machine_input_detached(Machine *source, Machine *destination,
						     const std::string &output_name,
						     const std::string &input_name) {
	SATAN_DEBUG("Signal detached [%s:%s] -> [%s:%s]\n",
		    source->get_name().c_str(), output_name.c_str(),
		    destination->get_name().c_str(), input_name.c_str());

	io_service.post(
		[this, source, destination, output_name, input_name]()
		{
			auto src_mch = machine2rimachine.find(source);
			auto dst_mch = machine2rimachine.find(destination);
			if(src_mch != machine2rimachine.end() && dst_mch != machine2rimachine.end()) {
				dst_mch->second->detach_input(src_mch->second, output_name, input_name);
			}
		}
		
		);
}

RemoteInterface::Server::Server(const asio::ip::tcp::endpoint& endpoint) : last_obj_id(-1), acceptor(io_service, endpoint),
									   acceptor_socket(io_service) {
	SATAN_DEBUG("RemoteInterface::Server::Server() accepting connections. (%p)\n", this);

	io_service.post(
		[this]()
		{			
			SATAN_DEBUG("RemoteInterface::Server::Server() will create service objects. (%p)\n", this);
			try {
				this->create_service_objects();
			} catch(std::exception &exp) {
				SATAN_DEBUG("exception caught when calling create_service_objects(%s).\n", exp.what());
				throw;
			}
		});
	
	do_accept();
}

void RemoteInterface::Server::do_accept() {
	acceptor.async_accept(
		acceptor_socket,
		[this](std::error_code ec) {
			SATAN_DEBUG("RemoteInterface::Server::do_accept() received a connection.\n");
			
			if (!ec) {
				std::shared_ptr<ClientAgent> new_client_agent = std::make_shared<ClientAgent>(std::move(acceptor_socket), this);
				client_agents.insert(new_client_agent);
				send_all_objects_to_new_client(new_client_agent);
				new_client_agent->start();
			} else {
				SATAN_DEBUG("error in do_accept()\n");
			}
			
			do_accept();
		}
		);
}

void RemoteInterface::Server::drop_client(std::shared_ptr<ClientAgent> client_agent) {
	auto client_iterator = client_agents.find(client_agent);
	if(client_iterator != client_agents.end()) {
		client_agents.erase(client_iterator);
	}
}

void RemoteInterface::Server::add_create_object_header(std::shared_ptr<Message> &target, std::shared_ptr<BaseObject> obj) {
	target->set_value("id", std::to_string(__MSG_CREATE_OBJECT));
	target->set_value("new_objid", std::to_string(obj->get_obj_id()));
	target->set_value("factory", obj->get_type().type_name);
}

void RemoteInterface::Server::add_destroy_object_header(std::shared_ptr<Message> &target, std::shared_ptr<BaseObject> obj) {
	target->set_value("id", std::to_string(__MSG_DELETE_OBJECT));
	target->set_value("objid", std::to_string(obj->get_obj_id()));
}

void RemoteInterface::Server::send_all_objects_to_new_client(std::shared_ptr<MessageHandler> client_agent) {
	for(auto obj : all_objects) {
		std::shared_ptr<Message> create_object_message = acquire_message();
		add_create_object_header(create_object_message, obj.second);
		obj.second->serialize(create_object_message);
		client_agent->deliver_message(create_object_message);
	}
}

void RemoteInterface::Server::route_incomming_message(const Message &msg) {
	int identifier = std::stol(msg.get_value("id"));
		
	auto obj_iterator = all_objects.find(identifier);
	if(obj_iterator == all_objects.end()) throw BaseObject::NoSuchObject();
	
	obj_iterator->second->process_message(this, msg);
}

void RemoteInterface::Server::disconnect_clients() {
	for(auto client_agent : client_agents) {
		client_agent->disconnect();		
	}
}

void RemoteInterface::Server::create_service_objects() {
	{ // create handle list object
		create_object_from_factory(__FCT_HANDLELIST, [](std::shared_ptr<BaseObject> new_obj){});
	}
	{ // register us as a machine set listener
		Machine::register_machine_set_listener(shared_from_this());
	}
}

void RemoteInterface::Server::start_server() {
	std::lock_guard<std::mutex> lock_guard(server_mutex);
	static bool server_created = false;
	
	if(server_created) {
		SATAN_DEBUG("Bug detected - server thread created twice.\n");
		return;
	}
	server_created = true;

	try {
		asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), VUKNOB_SERVER_PORT);
		server = std::shared_ptr<Server>(new Server(endpoint));

		server->t1 = std::thread([]() {
				server->io_service.run();
				SATAN_DEBUG("server io_service ended.\n");
			}
			);
	} catch (std::exception& e) {
		SATAN_DEBUG("Exception caught: %s\n", e.what());
	}
}

void RemoteInterface::Server::stop_server() {
	if(server) {
		server->io_service.dispatch(
		[]()
		{			
			SATAN_DEBUG("RemoteInterface::Server::Server() will disconnect clients.\n");
			try {
				server->disconnect_clients();
			} catch(std::exception &exp) {
				SATAN_DEBUG("exception caught when calling create_service_objects(%s).\n", exp.what());
				throw;
			}
		});

		server->io_service.stop();
		server->t1.join();
		SATAN_DEBUG(" server thread was joined.\n");
	}
}

void RemoteInterface::Server::distribute_message(std::shared_ptr<Message> &msg) {
	for(auto client_agent : client_agents) {
		client_agent->deliver_message(msg);
	}
}

void RemoteInterface::Server::post_action(std::function<void()> f) {
	io_service.post(f);
}

void RemoteInterface::Server::dispatch_action(std::function<void()> f) {
	std::mutex mtx;
	std::condition_variable cv;
	bool ready = false;
	
	io_service.dispatch(
		[&ready, &mtx, &cv, &f]() {
			f();
			std::unique_lock<std::mutex> lck(mtx);
			ready = true;
			cv.notify_all();
		}
		);

	std::unique_lock<std::mutex> lck(mtx);
	while (!ready) cv.wait(lck);
}

auto RemoteInterface::Server::get_object(int32_t objid) -> std::shared_ptr<BaseObject> {
	if(all_objects.find(objid) == all_objects.end()) throw BaseObject::NoSuchObject();

	return all_objects[objid];
}
