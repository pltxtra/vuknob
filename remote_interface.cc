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

#define VUKNOB_MAX_UDP_SIZE 512

#define __MSG_CREATE_OBJECT -1
#define __MSG_FLUSH_ALL_OBJECTS -2
#define __MSG_DELETE_OBJECT -3
#define __MSG_FAILURE_RESPONSE -4
#define __MSG_PROTOCOL_VERSION -5
#define __MSG_REPLY -6
#define __MSG_CLIENT_ID -7

// Factory names
#define __FCT_HANDLELIST		"HandleList"
#define __FCT_GLOBALCONTROLOBJECT	"GloCtrlObj"
#define __FCT_RIMACHINE			"RIMachine"
#define __FCT_SAMPLEBANK	        "SampleBank"

#define __VUKNOB_PROTOCOL_VERSION__ 4

//#define VUKNOB_UDP_SUPPORT
//#define VUKNOB_UDP_USE

/***************************
 *
 *  Decoder/Encoder functions
 *
 ***************************/

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
			v[0] = (v[0] << 4) | v[1];
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

	return output;
}

static std::string encode_string(const std::string &uncoded) {
	char inp[uncoded.size() + 1];
	(void) strncpy(inp, uncoded.c_str(), sizeof(inp));

	char *tok = inp;
	std::string output = "";

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

	return output;
}

class ItemSerializer {
private:
	std::stringstream result_stream;

public:

	void process(const std::string &v) {
		result_stream << "string;" << encode_string(v) << ";";
	}

	void process(bool v) {
		result_stream << "bool;" << (v ? "true" : "false") << ";";
	}

	void process(int v) {
		result_stream << "int;" << v << ";";
	}

	void process(unsigned int v) {
		result_stream << "uint;" << v << ";";
	}

	void process(float v) {
		result_stream << "float;" << v << ";";
	}

	void process(double v) {
		result_stream << "double;" << v << ";";
	}

	template <typename U, typename V>
	void process(const std::pair<U,V> &pair);

	template <class ContainerT>
	void process(const ContainerT &elements);

	std::string result() {
		return encode_string(result_stream.str());
	}
};

template <typename U, typename V>
void ItemSerializer::process(const std::pair<U,V> &pair) {
	ItemSerializer subser;

	subser.process(pair.first);
	subser.process(pair.second);

	result_stream << "pair;" << encode_string(subser.result()) << ";";
}

template <class ContainerT>
void ItemSerializer::process(const ContainerT &elements) {
	ItemSerializer subser;

	for(auto element : elements) {
		subser.process(element);
	}

	result_stream << "container;" << encode_string(subser.result()) << ";";
}

class ItemDeserializer {
public:
	class UnexpectedTypeWhenDeserializing : public std::runtime_error {
	public:
		UnexpectedTypeWhenDeserializing() : runtime_error("Found unexpected type when trying to deserialize object.") {}
		virtual ~UnexpectedTypeWhenDeserializing() {}
	};

	class UnexpectedEndWhenDeserializing : public std::runtime_error {
	public:
		UnexpectedEndWhenDeserializing() : runtime_error("Found unexpected end of string input when trying to deserialize object.") {}
		virtual ~UnexpectedEndWhenDeserializing() {}
	};

private:
	std::istringstream is;

	inline void verify_type(const std::string &type_identifier) {

		std::string type;
		std::getline(is, type, ';');
		if(is.eof()) throw UnexpectedEndWhenDeserializing();

		SATAN_DEBUG("verify_type(%s) (eof: %s) -> %s\n", type_identifier.c_str(), is.eof() ? "true" : "false", type.c_str());

		if(type == type_identifier) return;

		SATAN_DEBUG(" expected type [%s] but found [%s]\n", type_identifier.c_str(), type.c_str());

		throw UnexpectedTypeWhenDeserializing();
	}

	inline void get_string(std::string &val) {

		std::getline(is, val, ';');
		if(is.eof()) throw UnexpectedEndWhenDeserializing();

		SATAN_DEBUG("get_string() (eof: %s)-> %s\n", is.eof() ? "true" : "false", val.c_str());
	}

#define __ITD_GET_STRING(name) std::string name; get_string(name);
public:
	ItemDeserializer(const std::string input) : is(decode_string(input)) {
		SATAN_DEBUG("ItemDeserializer() -> %s\n", decode_string(input).c_str());
	}

	void process(std::string &v) {
		verify_type("string");

		__ITD_GET_STRING(val_s);
		v = decode_string(val_s);
	}

	void process(bool &v) {
		verify_type("bool");

		__ITD_GET_STRING(val_s);
		v = (val_s == "true") ? true : false;
	}

	void process(int &v) {
		verify_type("int");

		__ITD_GET_STRING(val_s);
		v = std::stoi(val_s);
	}

	void process(unsigned int &v) {
		verify_type("uint");

		__ITD_GET_STRING(val_s);
		v = (unsigned int)std::stoul(val_s);
	}

	void process(float &v) {
		verify_type("float");

		__ITD_GET_STRING(val_s);
		v = std::stof(val_s);
	}

	void process(double &v) {
		verify_type("double");

		__ITD_GET_STRING(val_s);
		v = std::stod(val_s);
	}

	template <typename U, typename V>
	void process(std::pair<U,V> &pair);

	template <typename U, typename V>
	void process(std::map<U,V> &pair);

	template <class ContainerT>
	void process(ContainerT &elements);

	bool eof() {
		return is.eof();
	}
};

template <typename U, typename V>
void ItemDeserializer::process(std::pair<U,V> &pair) {
	verify_type("pair");

	__ITD_GET_STRING(subserialized);

	SATAN_DEBUG("Will deserialize pair [%s]\n", decode_string(subserialized).c_str());

	ItemDeserializer subdeser(decode_string(subserialized));

	subdeser.process(pair.first);
	subdeser.process(pair.second);
}

template <typename U, typename V>
void ItemDeserializer::process(std::map<U,V> &elements) {
	verify_type("container");

	__ITD_GET_STRING(subserialized);

	SATAN_DEBUG("Will deserialize container [%s]\n", decode_string(subserialized).c_str());

	ItemDeserializer subdeser(decode_string(subserialized));

	std::pair<U,V> element;
	while(!subdeser.eof()) {
		try {
			subdeser.process(element);
			(void) elements.insert(elements.end(), element);
		} catch(UnexpectedEndWhenDeserializing& ignored) {
			/* ignore - this indicates end of contained data */
		}
	}
}

template <class ContainerT>
void ItemDeserializer::process(ContainerT &elements) {
	typedef typename ContainerT::value_type ElementT;

	verify_type("container");

	__ITD_GET_STRING(subserialized);

	SATAN_DEBUG("Will deserialize container [%s]\n", decode_string(subserialized).c_str());

	ItemDeserializer subdeser(decode_string(subserialized));

	ElementT element;
	while(!subdeser.eof()) {
		try {
			subdeser.process(element);
			(void) elements.insert(elements.end(), element);
		} catch(UnexpectedEndWhenDeserializing& ignored) {
			/* ignore - this indicates end of contained data */
		}
	}
}

/***************************
 *
 *  Class RemoteInterface::Message
 *
 ***************************/

RemoteInterface::Message::Message() : context(), ostrm(&sbuf), data2send(0) {}

RemoteInterface::Message::Message(RemoteInterface::Context *_context) : context(_context), ostrm(&sbuf), data2send(0) {}

void RemoteInterface::Message::set_reply_handler(std::function<void(const Message *reply_msg)> __reply_received_callback) {
	reply_received_callback = __reply_received_callback;
	awaiting_reply = true;
}

void RemoteInterface::Message::reply_to(const Message *reply_msg) {
	reply_received_callback(reply_msg);
}

bool RemoteInterface::Message::is_awaiting_reply() {
	return awaiting_reply;
}

void RemoteInterface::Message::set_value(const std::string &key, const std::string &value) {
	if(key.find(';') != std::string::npos) throw IllegalChar();
	if(key.find('=') != std::string::npos) throw IllegalChar();
	if(value.find(';') != std::string::npos) throw IllegalChar();
	if(value.find('=') != std::string::npos) throw IllegalChar();

	auto enc_key = encode_string(key);
	auto enc_val = encode_string(value);

	key2val[enc_key] = enc_val;

	data2send += enc_key.size() + enc_val.size() + 2; // +2 for the '=' and ';' that encode will add later

	encoded = false;
}

std::string RemoteInterface::Message::get_value(const std::string &key) const {
	auto result = key2val.find(key);
	if(result == key2val.end()) {
		for(auto k2v : key2val) {
			SATAN_DEBUG("[%s] does not match [%s] -> %s.\n", key.c_str(), k2v.first.c_str(), k2v.second.c_str());
		}
		throw NoSuchKey(key.c_str());
	}
	return result->second;
}

asio::streambuf::mutable_buffers_type RemoteInterface::Message::prepare_buffer(std::size_t length) {
	clear_msg_content();
	return sbuf.prepare(length);
}

void RemoteInterface::Message::commit_data(std::size_t length) {
	sbuf.commit(length);
}

asio::streambuf::const_buffers_type RemoteInterface::Message::get_data() {
	return sbuf.data();
}

bool RemoteInterface::Message::decode_client_id() {
	std::istream is(&sbuf);
	std::string header;
	is >> header;
	sscanf(header.c_str(), "%08x", &client_id);

	return true;
}

bool RemoteInterface::Message::decode_header() {
	std::istream is(&sbuf);
	std::string header;
	is >> header;
	sscanf(header.c_str(), "%08x", &body_length);

	SATAN_DEBUG("decode_header()-> [%s]\n", header.c_str());

	return true;
}

bool RemoteInterface::Message::decode_body() {
	std::istream is(&sbuf);

	std::string key, val;

	std::getline(is, key, '=');
	SATAN_DEBUG("decode_body()\n");
	while(!is.eof() && key != "") {
		SATAN_DEBUG("   key: %s\n", key.c_str());
		std::getline(is, val, ';');
		SATAN_DEBUG("     -> val: %s\n", val.c_str());
		if(key != "") {
			key2val[decode_string(key)] = decode_string(val);
		}
		std::getline(is, key, '=');
	}

	return true;
}

void RemoteInterface::Message::encode() const {
	if(encoded) return;
	encoded = true;

	// add header
	char header[9];
	snprintf(header, 9, "%08x", data2send);
	ostrm << header;

	int checksum = 0;
	SATAN_DEBUG("encode() header -> [%s]\n", header);
	// add body
	for(auto k2v : key2val) {
		auto enc_key = k2v.first;
		auto enc_val = k2v.second;

		ostrm << enc_key << "=" << enc_val << ";";

		SATAN_DEBUG("   encode: [%s] -> [%s]\n", enc_key.c_str(), enc_val.c_str());
		checksum += 2 + enc_key.size() + enc_val.size();
	}
	SATAN_DEBUG("data2send[%d] =? checksum[%d] ?\n", data2send, checksum);

	data2send += header_length; // add header length to the total data size
}

void RemoteInterface::Message::clear_msg_content() {
	data2send = 0;
	awaiting_reply = false;
	reply_received_callback = [](const Message *reply_msg){};
	key2val.clear();
}

void RemoteInterface::Message::recycle(Message *msg) {
	if(msg->context) {
		msg->sbuf.consume(msg->data2send);
		msg->clear_msg_content();
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

void RemoteInterface::Context::post_action(std::function<void()> f, bool do_synch) {
	if(do_synch) {
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
	} else {
		io_service.dispatch(f);
	}
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

std::shared_ptr<RemoteInterface::Message> RemoteInterface::Context::acquire_reply(const Message &originator) {
	std::shared_ptr<Message> reply = acquire_message();
	reply->set_value("id", std::to_string(__MSG_REPLY));
	reply->set_value("repid", std::to_string(originator.get_value("repid")));
	return reply;
}

void RemoteInterface::Context::recycle_message(RemoteInterface::Message* used_message) {
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
		read_msg.prepare_buffer(Message::header_length),
		[this, self](std::error_code ec, std::size_t length) {
			if(!ec) read_msg.commit_data(length);

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
		read_msg.prepare_buffer(read_msg.get_body_length()),
		[this, self](std::error_code ec, std::size_t length) {
			if(!ec) read_msg.commit_data(length);

			if(!ec && read_msg.decode_body()) {

				try {
					on_message_received(read_msg);
				} catch (Message::NoSuchKey& e) {
					SATAN_ERROR("do_read_body() caught an NoSuchKey exception: %s\n", e.keyname);
					on_connection_dropped();
				} catch (std::exception& e) {
					SATAN_ERROR("do_read_body() caught an exception: %s\n", e.what());
					on_connection_dropped();
				}

				do_read_header();
			} else {
				on_connection_dropped();
			}
		}
		);
}

void RemoteInterface::MessageHandler::do_write() {
	auto self(shared_from_this());
	asio::async_write(
		my_socket,
		write_msgs.front()->get_data(),
		[this, self](std::error_code ec, std::size_t length) {
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

void RemoteInterface::MessageHandler::do_write_udp(std::shared_ptr<Message> &msg) {
	try {
		my_udp_socket->send_to(msg->get_data(), udp_target_endpoint);
	} catch(std::exception &exp) {
		SATAN_ERROR("RemoteInterface::MessageHandler::do_write_udp() failed to deliver message: %s\n",
			    exp.what());
	}
}

RemoteInterface::MessageHandler::MessageHandler(asio::io_service &io_service) : my_socket(io_service) {
	my_udp_socket = std::make_shared<asio::ip::udp::socket>(io_service, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
}

RemoteInterface::MessageHandler::MessageHandler(asio::ip::tcp::socket _socket) :
	my_socket(std::move(_socket)) {
}

void RemoteInterface::MessageHandler::start_receive() {
	do_read_header();
}

void RemoteInterface::MessageHandler::deliver_message(std::shared_ptr<Message> &msg, bool via_udp) {
	msg->encode();

	if(via_udp && my_udp_socket &&
	   (msg->get_body_length() <= VUKNOB_MAX_UDP_SIZE)) {
		do_write_udp(msg);
	} else {
		bool write_in_progress = !write_msgs.empty();
		write_msgs.push_back(msg);
		if (!write_in_progress){
			do_write();
		}
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

void RemoteInterface::BaseObject::send_object_message(std::function<void(std::shared_ptr<Message> &msg_to_send)> complete_message, bool via_udp) {
	context->post_action(
		[this, via_udp, complete_message]() {
			std::shared_ptr<Message> msg2send = context->acquire_message();

			{
				// fill in object id header
				msg2send->set_value("id", std::to_string(obj_id));
			}
			{
				// call complete message
				complete_message(msg2send);
			}

			try {
				context->distribute_message(msg2send, via_udp);
			} catch(std::exception& e) {
				SATAN_ERROR("RemoteInterface::BaseObject::send_object_message() caught an exception: %s\n", e.what());
				throw;
			} catch(...) {
				SATAN_ERROR("RemoteInterface::BaseObject::send_object_message() caught an unknown exception.\n");
				throw;
			}
		}, true
		);
}

void RemoteInterface::BaseObject::send_object_message(std::function<void(std::shared_ptr<Message> &msg_to_send)> complete_message,
						      std::function<void(const Message *reply_msg)> reply_received_callback) {
	std::mutex mtx;
	std::condition_variable cv;
	bool ready = false;

	context->post_action(
		[this, complete_message, reply_received_callback, &ready, &mtx, &cv]() {
			std::shared_ptr<Message> msg2send = context->acquire_message();

			{
				// fill in object id header
				msg2send->set_value("id", std::to_string(obj_id));
				msg2send->set_reply_handler(
					[reply_received_callback, &ready, &mtx, &cv](const Message *reply_msg) {
						try {
							reply_received_callback(reply_msg);
						} catch(const std::exception &e) {
							SATAN_ERROR("RemoteInterface::BaseObject::send_object_message() - failed to process server reply: %s\n",
								    e.what());
							throw;
						} catch(...) {
							SATAN_ERROR("RemoteInterface::Client::send_object_message() - failed to process server reply, unknown exception.\n");
							throw;
						}

						std::unique_lock<std::mutex> lck(mtx);
						ready = true;
						cv.notify_all();
					}
					);
			}
			{
				// call complete message
				complete_message(msg2send);
			}

			context->distribute_message(msg2send, false);
		}
		);

	std::unique_lock<std::mutex> lck(mtx);
	while (!ready) cv.wait(lck);
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
	if(factory_iterator == factories.end()) {
		throw NoSuchFactory();
	}

	std::shared_ptr<RemoteInterface::BaseObject> o = factory_iterator->second->create(msg);

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
	std::stringstream handles(serialized.get_value("handles"));
	std::string handle;

	std::getline(handles, handle, ':');
	while(!handles.eof() && handle != "") {
		std::stringstream hint_key;
		hint_key << "hint_" << handle;
		std::string hint = serialized.get_value(hint_key.str());

		handle2hint[handle] = hint;

		std::getline(handles, handle, ':');
	}
}

RemoteInterface::HandleList::HandleList(int32_t new_obj_id, const Factory *factory) : BaseObject(new_obj_id, factory) {}

void RemoteInterface::HandleList::post_constructor_client() {}

void RemoteInterface::HandleList::process_message(Server *context, MessageHandler *src, const Message &msg) {
	std::string command = msg.get_value("command");

	if(command == "instance") {
		std::string new_m_handle = msg.get_value("handle");
		double xpos = atof(msg.get_value("xpos").c_str());
		double ypos = atof(msg.get_value("ypos").c_str());
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
		hlst->context->post_action(
			[hlst, &retval]() {
				retval = hlst->handle2hint;
			}, true
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
 *  Class RemoteInterface::GlobalControlObject::GlobalControlObjectFactory
 *
 ***************************/

RemoteInterface::GlobalControlObject::GlobalControlObjectFactory::GlobalControlObjectFactory() : Factory(__FCT_GLOBALCONTROLOBJECT) {}

std::shared_ptr<RemoteInterface::BaseObject> RemoteInterface::GlobalControlObject::GlobalControlObjectFactory::create(const Message &serialized) {
	std::shared_ptr<GlobalControlObject> gco = std::make_shared<GlobalControlObject>(this, serialized);
	clientside_gco = gco;
	return gco;
}

std::shared_ptr<RemoteInterface::BaseObject> RemoteInterface::GlobalControlObject::GlobalControlObjectFactory::create(int32_t new_obj_id) {
	return std::make_shared<GlobalControlObject>(new_obj_id, this);
}

/***************************
 *
 *  Class RemoteInterface::GlobalControlObject
 *
 ***************************/

void RemoteInterface::GlobalControlObject::parse_serialized_arp_patterns(std::vector<std::string> &result,
									 const std::string &serialized_arp_patterns) {
	// parse scale list
	std::stringstream ptrns_s(serialized_arp_patterns);
	std::string ptrn_name;

	std::getline(ptrns_s, ptrn_name, ':');
	while(!ptrns_s.eof() && ptrn_name != "") {
		result.push_back(ptrn_name);

		std::getline(ptrns_s, ptrn_name, ':');
	}
}

void RemoteInterface::GlobalControlObject::parse_serialized_keys(const std::string &scale_id, const std::string &serialized_keys) {
	// parse scale list
	std::stringstream keys_s(serialized_keys);
	std::string key;

	std::vector<int> result;

	std::getline(keys_s, key, ':');
	while(!keys_s.eof() && key != "") {
		result.push_back(std::stoi(key));

		std::getline(keys_s, key, ':');
	}

	scale2keys[scale_id] = result;
}

RemoteInterface::GlobalControlObject::GlobalControlObject(const Factory *factory, const Message &serialized) : BaseObject(factory, serialized) {
	// get basics
	bpm = std::stoi(serialized.get_value("bpm"));
	lpb = std::stoi(serialized.get_value("lpb"));
	is_playing = serialized.get_value("is_playing") == "true" ? true : false;
	is_recording = serialized.get_value("is_recording") == "true" ? true : false;

	// parse scale list
	std::stringstream scales_s(serialized.get_value("scales"));
	std::string scale;

	// get first scale name
	std::getline(scales_s, scale, ':');
	while(!scales_s.eof() && scale != "") {
		// insert scale name
		scale_names.push_back(scale);

		// get keys for the scale
		std::stringstream keys4scale_id;
		keys4scale_id << "keys4scale_" << scale;
		std::string serialized_keys = serialized.get_value(keys4scale_id.str());

		parse_serialized_keys(scale, serialized_keys);

		// get next scale name
		std::getline(scales_s, scale, ':');
	}
}

RemoteInterface::GlobalControlObject::GlobalControlObject(int32_t new_obj_id, const Factory *factory) : BaseObject(new_obj_id, factory) {
	Machine::register_periodic(

		[this](int row) {
			send_object_message(
				[this, row](std::shared_ptr<Message> &msg2send) {
					msg2send->set_value("command", "nrplaying");
					msg2send->set_value("nrow", std::to_string(row));
				}
				);
		}
		);
}

void RemoteInterface::GlobalControlObject::post_constructor_client() {}

void RemoteInterface::GlobalControlObject::process_message(Server *context, MessageHandler *src, const Message &msg) {
	std::string command = msg.get_value("command");

	if(command == "get_arp_patterns") {
		SATAN_DEBUG("Will send get_arp_patterns reply...\n");
		std::shared_ptr<Message> reply = context->acquire_reply(msg);
		serialize_arp_patterns(reply);
		src->deliver_message(reply);
		SATAN_DEBUG("Reply delivered...\n");
	} else if(command == "is_it_playing") {
		SATAN_DEBUG("Will send is_it_playing reply...\n");
		std::shared_ptr<Message> reply = context->acquire_reply(msg);
		reply->set_value("playing", Machine::is_it_playing() ? "true" : "false");
		src->deliver_message(reply);
		SATAN_DEBUG("Reply delivered...\n");
	} else if(command == "play") {
		SATAN_DEBUG("play command received...\n");
		Machine::play();

		// update state of clients
		send_object_message(
			[](std::shared_ptr<Message> &msg2send) {
				msg2send->set_value("command", "play");
			}
			);

	} else if(command == "stop") {
		SATAN_DEBUG("stop command received...\n");
		Machine::stop();

		// update state of clients
		send_object_message(
			[](std::shared_ptr<Message> &msg2send) {
				msg2send->set_value("command", "stop");
			}
			);

	} else if(command == "rewind") {
		SATAN_DEBUG("rewind command received...\n");
		Machine::rewind();
	} else if(command == "set_rec_state") {
		SATAN_DEBUG("set rec_state command received...\n");
		bool isrec = msg.get_value("state") == "true";
		Machine::set_record_state(isrec);

		// update state of clients
		send_object_message(
			[isrec](std::shared_ptr<Message> &msg2send) {
				msg2send->set_value("command", isrec ? "dorecord" : "dontrecord");
			}
			);

	} else if(command == "is_it_recording") {
		SATAN_DEBUG("Will send is_it_recording reply...\n");
		std::shared_ptr<Message> reply = context->acquire_reply(msg);
		reply->set_value("recording", Machine::get_record_state() ? "true" : "false");
		src->deliver_message(reply);
		SATAN_DEBUG("Reply delivered...\n");
	} else if(command == "get_rec_fname") {
		SATAN_DEBUG("Will send get_rec_fname reply...\n");
		std::shared_ptr<Message> reply = context->acquire_reply(msg);
		reply->set_value("fname", Machine::get_record_file_name());
		src->deliver_message(reply);
		SATAN_DEBUG("Reply delivered...\n");
	} else if(command == "set_bpm") {
		SATAN_DEBUG("set_bpm command received...\n");
		int new_bpm = std::stoi(msg.get_value("bpm"));
		Machine::set_bpm(new_bpm);
		new_bpm = Machine::get_bpm();

		// update state of clients
		send_object_message(
			[new_bpm](std::shared_ptr<Message> &msg2send) {
				msg2send->set_value("command", "bpmset");
				msg2send->set_value("bpm", std::to_string(new_bpm));
			}
			);

		// send reply to synchronize
		std::shared_ptr<Message> reply = context->acquire_reply(msg);
		reply->set_value("bpmsync", std::to_string(new_bpm));
		src->deliver_message(reply);

	} else if(command == "set_lpb") {
		SATAN_DEBUG("set_lpb command received...\n");
		int new_lpb = std::stoi(msg.get_value("lpb"));
		Machine::set_lpb(new_lpb);
		new_lpb = Machine::get_lpb();

		// update state of clients
		send_object_message(
			[new_lpb](std::shared_ptr<Message> &msg2send) {
				msg2send->set_value("command", "lpbset");
				msg2send->set_value("lpb", std::to_string(new_lpb));
			}
			);

		// send reply to synchronize
		std::shared_ptr<Message> reply = context->acquire_reply(msg);
		reply->set_value("lpbsync", std::to_string(new_lpb));
		src->deliver_message(reply);

	}

}

void RemoteInterface::GlobalControlObject::process_message(Client *context, const Message &msg) {
	std::string command = msg.get_value("command");

	if(command == "nrplaying") {
		auto new_row = std::stoi(msg.get_value("nrow"));
		for(auto w_clb : playback_state_listeners) if(auto clb = w_clb.lock()) clb->periodic_playback_update(new_row);
	} else if(command == "play") {
		SATAN_DEBUG("Client: Playing is now true.\n");
		is_playing = true;
		for(auto w_clb : playback_state_listeners) if(auto clb = w_clb.lock()) clb->playback_state_changed(is_playing);
	} else if(command == "stop") {
		SATAN_DEBUG("Client: Playing is now false.\n");
		is_playing = false;
		for(auto w_clb : playback_state_listeners) if(auto clb = w_clb.lock()) clb->playback_state_changed(is_playing);
	} else if(command == "dorecord") {
		SATAN_DEBUG("Client: Recording is now true.\n");
		is_recording = true;
		for(auto w_clb : playback_state_listeners) if(auto clb = w_clb.lock()) clb->recording_state_changed(is_recording);
	} else if(command == "dontrecord") {
		SATAN_DEBUG("Client: Recording is now false.\n");
		is_recording = false;
		for(auto w_clb : playback_state_listeners) if(auto clb = w_clb.lock()) clb->recording_state_changed(is_recording);
	} else if(command == "bpmset") {
		std::lock_guard<std::mutex> lock_guard(base_object_mutex);
		bpm = std::stoi(msg.get_value("bpm"));
		SATAN_DEBUG("Client: bpm updated: %d.\n", bpm);
	} else if(command == "lpbset") {
		std::lock_guard<std::mutex> lock_guard(base_object_mutex);
		lpb = std::stoi(msg.get_value("lpb"));
		SATAN_DEBUG("Client: lpb updated: %d.\n", lpb);
	}
}

void RemoteInterface::GlobalControlObject::serialize_keys(std::shared_ptr<Message> &target,
							  const std::string &id,
							  std::vector<int> keys) {
	std::stringstream keys_serialized;

	for(auto key : keys) {
		keys_serialized << std::to_string(key) << ":";
	}
	target->set_value(id, keys_serialized.str());
}

void RemoteInterface::GlobalControlObject::serialize_arp_patterns(std::shared_ptr<Message> &target) {
	std::stringstream ptrns_serialized;

	for(auto ptrn :  MachineSequencer::get_pad_arpeggio_patterns()) {
		ptrns_serialized << ptrn << ":";
	}
	target->set_value("arp_patterns", ptrns_serialized.str());
}

void RemoteInterface::GlobalControlObject::serialize(std::shared_ptr<Message> &target) {
	std::vector<std::string> scale_names = MachineSequencer::PadConfiguration::get_scale_names();
	std::stringstream scales_serialized;

	for(auto scale_name : scale_names) {
		scales_serialized << scale_name << ":";

		std::stringstream keys4scale_id;
		keys4scale_id << "keys4scale_" << scale_name;

		serialize_keys(target, keys4scale_id.str(), MachineSequencer::PadConfiguration::get_scale_keys(scale_name));
	}
	target->set_value("scales", scales_serialized.str());

	target->set_value("bpm", std::to_string(Machine::get_bpm()));
	target->set_value("lpb", std::to_string(Machine::get_lpb()));
	target->set_value("is_playing", Machine::is_it_playing() ? "true" : "false");
	target->set_value("is_recording", Machine::get_record_state() ? "true" : "false");
}

void RemoteInterface::GlobalControlObject::on_delete(Client *context) { /* noop */ }

std::vector<std::string> RemoteInterface::GlobalControlObject::get_pad_arpeggio_patterns() {
	std::vector<std::string> retval;
	bool failed = true;

	send_object_message(
		[](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "get_arp_patterns");
		},
		[this, &retval, &failed](const Message *reply_message) {
			if(reply_message) {
				failed = false;
				parse_serialized_arp_patterns(retval, reply_message->get_value("arp_patterns"));
			}
		}
		);

	if(failed) throw Message::CannotReceiveReply();
	return retval;
}

std::vector<std::string> RemoteInterface::GlobalControlObject::get_scale_names() {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	return scale_names;
}

std::vector<int> RemoteInterface::GlobalControlObject::get_scale_keys(const std::string &scale_name) {

	{
		std::lock_guard<std::mutex> lock_guard(base_object_mutex);
		auto scl = scale2keys.find(scale_name);
		if(scl != scale2keys.end()) {
			return (*scl).second;
		}
	}

	std::vector<int> empty_retval;
	return empty_retval;
}

bool RemoteInterface::GlobalControlObject::is_it_playing() {
	bool is_it_playing = false;

	send_object_message(
		[](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "is_it_playing");
		},
		[this, &is_it_playing](const Message *reply_message) {
			if(reply_message) {
				if(reply_message->get_value("playing") == "true")
					is_it_playing = true;
			}
		}
		);

	return is_it_playing;
}

void RemoteInterface::GlobalControlObject::play() {
	send_object_message(
		[](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "play");
		}
		);
}

void RemoteInterface::GlobalControlObject::stop() {
	send_object_message(
		[](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "stop");
		}
		);
}

void RemoteInterface::GlobalControlObject::rewind() {
	send_object_message(
		[](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "rewind");
		}
		);
}

void RemoteInterface::GlobalControlObject::set_record_state(bool do_record) {
	send_object_message(
		[do_record](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "set_rec_state");
			msg2send->set_value("state", do_record ? "true" : "false");
		}
		);
}

bool RemoteInterface::GlobalControlObject::get_record_state() {
	bool is_it_recording = false;

	send_object_message(
		[](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "is_it_recording");
		},
		[this, &is_it_recording](const Message *reply_message) {
			if(reply_message) {
				if(reply_message->get_value("recording") == "true")
					is_it_recording = true;
			}
		}
		);

	return is_it_recording;
}

std::string RemoteInterface::GlobalControlObject::get_record_file_name() {
	std::string rec_fname = "";

	send_object_message(
		[](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "get_rec_fname");
		},
		[this, &rec_fname](const Message *reply_message) {
			if(reply_message) {
				rec_fname = reply_message->get_value("fname");
			}
		}
		);

	return rec_fname;
}

int RemoteInterface::GlobalControlObject::get_bpm() {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	return bpm;
}

int RemoteInterface::GlobalControlObject::get_lpb() {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	return lpb;
}

void RemoteInterface::GlobalControlObject::set_bpm(int _bpm) {
	send_object_message(
		[_bpm](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "set_bpm");
			msg2send->set_value("bpm", std::to_string(_bpm));
		},
		[this, &_bpm](const Message *reply_message) {
			if(reply_message) {
				// this is just for synchronization - not really used
				_bpm = std::stoi(reply_message->get_value("bpmsync"));
			}
		}

		);
}

void RemoteInterface::GlobalControlObject::set_lpb(int _lpb) {
	send_object_message(
		[_lpb](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "set_lpb");
			msg2send->set_value("lpb", std::to_string(_lpb));
		},
		[this, &_lpb](const Message *reply_message) {
			if(reply_message) {
				// this is just for synchronization - not really used
				_lpb = std::stoi(reply_message->get_value("lpbsync"));
			}
		}

		);
}

void RemoteInterface::GlobalControlObject::insert_new_playback_state_listener_object(std::shared_ptr<PlaybackStateListener> listener_object) {
	// insert listener into our vector
	playback_state_listeners.push_back(listener_object);

	// cleanup non valid listeners
	auto i = playback_state_listeners.begin();
	while(i != playback_state_listeners.end()) {
		auto locked = (*i).lock();
		if(!locked) {
			i = playback_state_listeners.erase(i);
		} else {
			i++;
		}
	}
}

void RemoteInterface::GlobalControlObject::register_playback_state_listener(std::shared_ptr<PlaybackStateListener> listener_object) {
	std::lock_guard<std::mutex> lock_guard(gco_mutex);
	auto gco = clientside_gco.lock();

	if(gco) {
		gco->context->post_action(
			[gco, listener_object]() {
				insert_new_playback_state_listener_object(listener_object);

				// call the callback with the current state
				listener_object->playback_state_changed(gco->is_playing);
				listener_object->playback_state_changed(gco->is_recording);
			}
			);
	} else {
		insert_new_playback_state_listener_object(listener_object);

		// call the callback with the current state
		listener_object->playback_state_changed(false);
		listener_object->playback_state_changed(false);
	}
}

auto RemoteInterface::GlobalControlObject::get_global_control_object() -> std::shared_ptr<GlobalControlObject> {
	return clientside_gco.lock();
}

std::weak_ptr<RemoteInterface::GlobalControlObject> RemoteInterface::GlobalControlObject::clientside_gco;
RemoteInterface::GlobalControlObject::GlobalControlObjectFactory RemoteInterface::GlobalControlObject::globalcontrolobject_factory;
std::mutex RemoteInterface::GlobalControlObject::gco_mutex;
std::vector<std::weak_ptr<RemoteInterface::GlobalControlObject::PlaybackStateListener> > RemoteInterface::GlobalControlObject::playback_state_listeners;

/***************************
 *
 *  Class RemoteInterface::SampleBank::SampleBankFactory
 *
 ***************************/

RemoteInterface::SampleBank::SampleBankFactory::SampleBankFactory() : Factory(__FCT_SAMPLEBANK) {}

std::shared_ptr<RemoteInterface::BaseObject> RemoteInterface::SampleBank::SampleBankFactory::create(const Message &serialized) {
	std::lock_guard<std::mutex> lock_guard(clientside_samplebanks_mutex);

	std::shared_ptr<SampleBank> sb = std::make_shared<SampleBank>(this, serialized);
	clientside_samplebanks[sb->get_name()] = sb;

	SATAN_DEBUG("All clientside samplebanks:\n");
	for(auto cssb : clientside_samplebanks) {
		SATAN_DEBUG(" sbank: %s", cssb.first.c_str());
	}

	return sb;
}

std::shared_ptr<RemoteInterface::BaseObject> RemoteInterface::SampleBank::SampleBankFactory::create(int32_t new_obj_id) {
	return std::make_shared<SampleBank>(new_obj_id, this);
}

/***************************
 *
 *  Class RemoteInterface::SampleBank
 *
 ***************************/

// client side
RemoteInterface::SampleBank::SampleBank(const Factory *factory, const Message &serialized) : BaseObject(factory, serialized) {
	{
		ItemDeserializer ides(serialized.get_value("content"));
		serderize_samplebank(ides);
	}
	SATAN_DEBUG("SampleBank - client side - [%s] created.\n", bank_name.c_str());
}

// server side
RemoteInterface::SampleBank::SampleBank(int32_t new_obj_id, const Factory *factory) : BaseObject(new_obj_id, factory) {
	bank_name = "<global>";
	// currently we only support the global sample bank (that's the only one availble at this time)
	sample_names = Machine::StaticSignalLoader::get_all_signal_names();

	SATAN_DEBUG("SampleBank - server side - created.\n");
}

template <class SerderClassT>
void RemoteInterface::SampleBank::serderize_samplebank(SerderClassT& iserder) {
	iserder.process(bank_name);
	iserder.process(sample_names);
}

std::string RemoteInterface::SampleBank::get_name() {
	return bank_name;
}

std::string RemoteInterface::SampleBank::get_sample_name(int bank_index) {
	auto itr = sample_names.find(bank_index);

	if(itr == sample_names.end()) {
		throw NoSampleLoaded();
	}

	return (*itr).second;
}

void RemoteInterface::SampleBank::load_sample(int bank_index, const std::string &serverside_file_path) {
	if(is_client_side()) {
		// transmit the attach request to the server
		std::string new_name = "";
		bool failed = true;

		send_object_message(
		[bank_index, serverside_file_path](std::shared_ptr<Message> &msg2send) {
				msg2send->set_value("command", "loads");
				msg2send->set_value("index", std::to_string(bank_index));
				msg2send->set_value("path", serverside_file_path);
			},
		[&failed, &new_name](const Message *reply_message) {
				if(reply_message) {
					failed = false;
					new_name = reply_message->get_value("name");
				}
			}
			);
		if(!failed) {
			std::lock_guard<std::mutex> lock_guard(base_object_mutex);
			sample_names[bank_index] = new_name;
		}
	}
}

void RemoteInterface::SampleBank::post_constructor_client() {}

void RemoteInterface::SampleBank::process_message(Server *context, MessageHandler *src, const Message &msg) {
	auto command = msg.get_value("command");
	if(command == "loads") {

		int bank_index = std::stoi(msg.get_value("index"));
		auto path = msg.get_value("path");

		Machine::StaticSignalLoader::load_signal(bank_index, path);

		auto new_name = Machine::StaticSignalLoader::get_signal_name_for_slot(bank_index);

		// we also need to update all the clients of this change
		auto thiz = std::dynamic_pointer_cast<SampleBank>(shared_from_this());
		send_object_message(
			[this, thiz, new_name, bank_index](std::shared_ptr<Message> &msg2send) {
				msg2send->set_value("command", "setname");
				msg2send->set_value("ignored", thiz->bank_name); // make sure thiz is not optimized away
				msg2send->set_value("name", new_name);
				msg2send->set_value("index", std::to_string(bank_index));
			}
			);

		// then we reply, to synchronize
		{
			std::shared_ptr<Message> reply = context->acquire_reply(msg);
			reply->set_value("name", new_name);
			src->deliver_message(reply);
		}

	}
}

void RemoteInterface::SampleBank::process_message(Client *context, const Message &msg) {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);

	auto command = msg.get_value("command");
	if(command == "setname") {
		auto new_name = msg.get_value("name");
		int bank_index = std::stoi(msg.get_value("index"));

		sample_names[bank_index] = new_name;
	}
}

void RemoteInterface::SampleBank::serialize(std::shared_ptr<Message> &target) {
	ItemSerializer iser;
	serderize_samplebank(iser);
	target->set_value("content", iser.result());
}

void RemoteInterface::SampleBank::on_delete(Client *context) {
	std::lock_guard<std::mutex> lock_guard(clientside_samplebanks_mutex);

	auto itr = clientside_samplebanks.begin();
	while(itr != clientside_samplebanks.end()) {
		auto sb_shr = std::dynamic_pointer_cast<SampleBank>(shared_from_this());
		if((*itr).second.lock() == sb_shr) {
			// just delete us from the map and return
			(void) clientside_samplebanks.erase(itr);
			return;
		}
	}
}

auto RemoteInterface::SampleBank::get_bank(const std::string _name) -> std::shared_ptr<SampleBank> {
	std::lock_guard<std::mutex> lock_guard(clientside_samplebanks_mutex);

	std::string name = "<global>"; // default to <global>
	if(_name != "") name = _name; // but if _name is not empty then we set name to _name

	auto itr = clientside_samplebanks.find(name);
	if(itr != clientside_samplebanks.end()) {
		auto sb_locked = (*itr).second.lock();
		if(sb_locked)
			return sb_locked;
	}

	throw NoSuchSampleBank();
}

std::map<std::string, std::weak_ptr<RemoteInterface::SampleBank> >RemoteInterface::SampleBank::clientside_samplebanks;
std::mutex RemoteInterface::SampleBank::clientside_samplebanks_mutex;
RemoteInterface::SampleBank::SampleBankFactory  RemoteInterface::SampleBank::samplebank_factory;

/***************************
 *
 *  Class RemoteInterface::RIMachine::RIController
 *
 ***************************/

RemoteInterface::RIMachine::RIController::RIController(int _ctrl_id, Machine::Controller *ctrl) : ctrl_id(_ctrl_id) {
	name = ctrl->get_name();
	title = ctrl->get_title();

	{
		switch(ctrl->get_type()) {
		case Machine::Controller::c_int:
			ct_type = RIController::ric_int;
			break;
		case Machine::Controller::c_float:
			ct_type = RIController::ric_float;
			break;
		case Machine::Controller::c_bool:
			ct_type = RIController::ric_bool;
			break;
		case Machine::Controller::c_string:
			ct_type = RIController::ric_string;
			break;
		case Machine::Controller::c_enum:
			ct_type = RIController::ric_enum;
			break;
		case Machine::Controller::c_sigid:
			ct_type = RIController::ric_sigid;
			break;
		}
	}

        if(ct_type ==  RIController::ric_float) {
		ctrl->get_min(data.f.min);
		ctrl->get_max(data.f.max);
		ctrl->get_step(data.f.step);
		ctrl->get_value(data.f.value);
	} else {
		ctrl->get_min(data.i.min);
		ctrl->get_max(data.i.max);
		ctrl->get_step(data.i.step);
		ctrl->get_value(data.i.value);

		if(ct_type == RIController::ric_enum) {
			for(auto k = data.i.min; k <= data.i.max; k += data.i.step) {
				enum_names[k] = ctrl->get_value_name(k);
			}
		}
	}

	ctrl->get_value(str_data);
	ctrl->get_value(bl_data);

	(void) /*ignore return value */ ctrl->has_midi_controller(coarse_controller, fine_controller);
}

RemoteInterface::RIMachine::RIController::RIController(std::function<
							       void(std::function<void(std::shared_ptr<Message> &msg_to_send)> )
							       >  _send_obj_message,
						       const std::string &serialized) : send_obj_message(_send_obj_message) {
	ItemDeserializer ides(serialized);
	serderize_controller(ides);
}

std::string RemoteInterface::RIMachine::RIController::get_serialized_controller() {
	ItemSerializer iser;
	serderize_controller(iser);
	return iser.result();
}

template <class SerderClassT>
void RemoteInterface::RIMachine::RIController::serderize_controller(SerderClassT& iserder) {
	iserder.process(ctrl_id);
	iserder.process(name);
	iserder.process(title);

	iserder.process(ct_type);

	switch(ct_type) {
	case RIController::ric_float: {
		iserder.process(data.f.min);
		iserder.process(data.f.max);
		iserder.process(data.f.step);
		iserder.process(data.f.value);
	} break;

	case RIController::ric_int:
	case RIController::ric_enum:
	case RIController::ric_sigid: {
		iserder.process(data.i.min);
		iserder.process(data.i.max);
		iserder.process(data.i.step);
		iserder.process(data.i.value);

		if(ct_type == RIController::ric_enum) {
			iserder.process(enum_names);
		}
	} break;

	case RIController::ric_bool: {
		iserder.process(bl_data);
	} break;

	case RIController::ric_string: {
		iserder.process(str_data);
	} break;

	}

	iserder.process(coarse_controller);
	iserder.process(fine_controller);
}

std::string RemoteInterface::RIMachine::RIController::get_name() {
	return name;
}

std::string RemoteInterface::RIMachine::RIController::get_title() {
	return title;
}

auto RemoteInterface::RIMachine::RIController::get_type() -> Type {
	return (Type)ct_type;
}

void RemoteInterface::RIMachine::RIController::get_min(float &val) {
	val = data.f.min;
}

void RemoteInterface::RIMachine::RIController::get_max(float &val) {
	val = data.f.max;
}

void RemoteInterface::RIMachine::RIController::get_step(float &val) {
	val = data.f.step;
}

void RemoteInterface::RIMachine::RIController::get_min(int &val) {
	val = data.i.min;
}

void RemoteInterface::RIMachine::RIController::get_max(int &val) {
	val = data.i.max;
}

void RemoteInterface::RIMachine::RIController::get_step(int &val) {
	val = data.i.step;
}

void RemoteInterface::RIMachine::RIController::get_value(int &val) {
	val = data.i.value;
}

void RemoteInterface::RIMachine::RIController::get_value(float &val) {
	val = data.f.value;
}

void RemoteInterface::RIMachine::RIController::get_value(bool &val) {
	val = bl_data;
}

void RemoteInterface::RIMachine::RIController::get_value(std::string &val) {
	val = str_data;
}

std::string RemoteInterface::RIMachine::RIController::get_value_name(int val) {
	if(ct_type == RIController::ric_sigid) {
		std::string retval = "no file loaded";

		auto global_sb = SampleBank::get_bank("");
		if(global_sb) {
			try {
				retval = global_sb->get_sample_name(val);
			} catch(SampleBank::NoSampleLoaded &e) { /* ignore */ }
		}

		return retval;
	}

	auto enam = enum_names.find(val);

	if(enam == enum_names.end())
		throw NoSuchEnumValue();

	return (*enam).second;
}

void RemoteInterface::RIMachine::RIController::set_value(int val) {
	data.i.value = val;
	auto crid = ctrl_id;
	send_obj_message(
		[crid, val](std::shared_ptr<Message> &msg_to_send) {
			msg_to_send->set_value("command", "setctrval");
			msg_to_send->set_value("ctrl_id", std::to_string(crid));
			msg_to_send->set_value("value", std::to_string(val));
		}
		);
}

void RemoteInterface::RIMachine::RIController::set_value(float val) {
	data.f.value = val;
	auto crid = ctrl_id;
	send_obj_message(
		[crid, val](std::shared_ptr<Message> &msg_to_send) {
			msg_to_send->set_value("command", "setctrval");
			msg_to_send->set_value("ctrl_id", std::to_string(crid));
			msg_to_send->set_value("value", std::to_string(val));
		}
		);
}

void RemoteInterface::RIMachine::RIController::set_value(bool val) {
	bl_data = val;
	auto crid = ctrl_id;
	send_obj_message(
		[crid, val](std::shared_ptr<Message> &msg_to_send) {
			msg_to_send->set_value("command", "setctrval");
			msg_to_send->set_value("ctrl_id", std::to_string(crid));
			msg_to_send->set_value("value", val ? "true" : "false");
		}
		);
}

void RemoteInterface::RIMachine::RIController::set_value(const std::string &val) {
	str_data = val;
	auto crid = ctrl_id;
	send_obj_message(
		[crid, val](std::shared_ptr<Message> &msg_to_send) {
			msg_to_send->set_value("command", "setctrval");
			msg_to_send->set_value("ctrl_id", std::to_string(crid));
			msg_to_send->set_value("value", val);
		}
		);
}

bool RemoteInterface::RIMachine::RIController::has_midi_controller(int &__coarse_controller, int &__fine_controller) {
	if(coarse_controller == -1 && fine_controller == -1) return false;

	__coarse_controller = coarse_controller;
	__fine_controller = fine_controller;

	return true;
}

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

	if(type == "MachineSequencer") {
		sibling = serialized.get_value("sibling");
	}

	parse_serialized_connections_data(serialized.get_value("connections"));
	parse_serialized_midi_ctrl_list(serialized.get_value("mctrls"));
	try {
		parse_io(inputs, serialized.get_value("inputs"));
	} catch(Message::NoSuchKey &e) { /* ignore empty inputs */ }
	try {
		parse_io(outputs, serialized.get_value("outputs"));
	} catch(Message::NoSuchKey &e) { /* ignore empty inputs */ }

	ItemDeserializer deserializor(serialized.get_value("ctrgroups"));
	deserializor.process(controller_groups);
	SATAN_DEBUG("Client, RIMachine::RIMachine(%s) - controller_groups [%s]:\n", name.c_str(), serialized.get_value("ctrgroups").c_str());
	for(auto ctgr : controller_groups) {
		SATAN_DEBUG("  ctr group: %s\n", ctgr.c_str());
	}
}

RemoteInterface::RIMachine::RIMachine(int32_t new_obj_id, const Factory *factory) : BaseObject(new_obj_id, factory) {}

void RemoteInterface::RIMachine::parse_serialized_midi_ctrl_list(std::string serialized) {
	std::stringstream is(serialized);

	std::string ctrl;

	// get first midi controller
	std::getline(is, ctrl, '$');
	while(!is.eof() && ctrl != "") {
		midi_controllers.insert(ctrl);

		// find next
		std::getline(is, ctrl, '$');
	}
}

void RemoteInterface::RIMachine::parse_serialized_connections_data(std::string serialized) {
	std::stringstream is(serialized);

	std::string srcid, src_name, dst_name;

	// get first source objid
	std::getline(is, srcid, '@');
	while(!is.eof() && srcid != "") {
		// get source output name
		std::getline(is, src_name, '{');

		// get destination input name
		std::getline(is, dst_name, '}');

		// store connection
		if(srcid != "" && src_name != "" && dst_name != "") {
			connection_data.insert({(int32_t)std::stol(srcid), {src_name, dst_name}});
		}

		// find start of next
		std::getline(is, srcid, '@');
	}
}

void RemoteInterface::RIMachine::call_listeners(std::function<void(std::shared_ptr<RIMachineStateListener> listener)> callback) {
	auto weak_listener = state_listeners.begin();
	while(weak_listener != state_listeners.end()) {
		if(auto listener = (*weak_listener).lock()) {
			base_object_mutex.unlock();
			callback(listener);
			base_object_mutex.lock();
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
		if(mseq != NULL) {
			type = "MachineSequencer";
			midi_controllers = mseq->available_midi_controllers();

			// set pad defaults
			mseq->get_pad()->set_pad_resolution(1024, 1024);
			mseq->set_pad_arpeggio_pattern("no arpeggio");
		}
	}
	{ // see if it's a DynamicMachine
		DynamicMachine *dmch = dynamic_cast<DynamicMachine *>((m_ptr));
		if(dmch != NULL) {
			type = "DynamicMachine";
		}
	}

	real_machine_ptr = m_ptr;

	xpos = m_ptr->get_x_position();
	ypos = m_ptr->get_y_position();

	try {
		inputs = m_ptr->get_input_names();
	} catch(...) {
		throw;
	}
	try {
		outputs = m_ptr->get_output_names();
	} catch(...) {
		throw;
	}

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

std::vector<std::string> RemoteInterface::RIMachine::get_controller_groups() {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	return controller_groups;
}

std::vector<std::string> RemoteInterface::RIMachine::get_controller_names(const std::string &group_name) {
	std::vector<std::string> retval;

	send_object_message(
		[this, &group_name](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "get_ctrl_names");
			msg2send->set_value("grname", group_name);
		},

		[this, &retval](const Message *reply_message) {
			if(reply_message) {
				ItemDeserializer deserializor(reply_message->get_value("ctrlnames"));
				deserializor.process(retval);
			}
		}
		);

	return retval;
}

auto RemoteInterface::RIMachine::get_controller(const std::string &controller_name) -> std::shared_ptr<RIController> {
	std::shared_ptr<RIController> retval;

	send_object_message(
		[this, &controller_name](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "get_ctrl");
			msg2send->set_value("ctrlname", controller_name);
		},

		[this, &retval](const Message *reply_message) {
			if(reply_message) {
				retval = std::make_shared<RIController>(
					[this](std::function<void(std::shared_ptr<Message> &)> fill_in_msg) {
						send_object_message(fill_in_msg);
					}
					,
					reply_message->get_value("ctrl"));
			}
		}
		);

	return retval;
}

std::string RemoteInterface::RIMachine::get_name() {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	return name;
}

std::string RemoteInterface::RIMachine::get_sibling_name() {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	return sibling;
}

std::string RemoteInterface::RIMachine::get_machine_type() {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	return type;
}

double RemoteInterface::RIMachine::get_x_position() {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	return xpos;
}

double RemoteInterface::RIMachine::get_y_position() {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
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
		auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
		send_object_message(
			[this, xp, yp, thiz](std::shared_ptr<Message> &msg2send) {
				std::lock_guard<std::mutex> lock_guard(base_object_mutex);

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
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	return inputs;
}

std::vector<std::string> RemoteInterface::RIMachine::get_output_names() {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	return outputs;
}

void RemoteInterface::RIMachine::set_state_change_listener(std::weak_ptr<RIMachineStateListener> state_listener) {
	{
		std::lock_guard<std::mutex> lock_guard(base_object_mutex);
		state_listeners.insert(state_listener);
	}

	context->post_action(
		[this, state_listener]() {
			// call all state listener callbacks
			if(auto listener = state_listener.lock()) {
				listener->on_move();

				base_object_mutex.lock();
				for(auto connection : connection_data) {
					auto mch = std::dynamic_pointer_cast<RIMachine>(context->get_object(connection.first));

					base_object_mutex.unlock();
					listener->on_attach(mch, connection.second.first, connection.second.second);
					base_object_mutex.lock();
				}
				base_object_mutex.unlock();
			}
		}
		);
}

std::set<std::string> RemoteInterface::RIMachine::available_midi_controllers() {
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	return midi_controllers;
}

int RemoteInterface::RIMachine::get_nr_of_loops() {
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	int reply;
	bool failed = false;

	send_object_message(
		[this, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "getnrloops");
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away
		},
		[this, thiz, &reply, &failed](const Message *reply_message) {
			if(reply_message)
				reply = std::stol(reply_message->get_value("nrloops"));
			else
				failed = true;
		}
		);

	if(failed) throw Message::CannotReceiveReply();

	return reply;
}

void RemoteInterface::RIMachine::pad_export_to_loop(int loop_id) {
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[this, loop_id, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away

			msg2send->set_value("command", "export2loop");
			msg2send->set_value("loopid", std::to_string(loop_id));
		}
		);
}

void RemoteInterface::RIMachine::pad_set_octave(int octave) {
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[this, octave, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away

			msg2send->set_value("command", "setoct");
			msg2send->set_value("octave", std::to_string(octave));
		}
		);
}

void RemoteInterface::RIMachine::pad_set_scale(int scale_index) {
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[this, scale_index, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away

			msg2send->set_value("command", "setscl");
			msg2send->set_value("scale", std::to_string(scale_index));
		}
		);
}

void RemoteInterface::RIMachine::pad_set_record(bool record) {
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[this, record, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away

			msg2send->set_value("command", "setrec");
			msg2send->set_value("record", record ? "true" : "false");
		}
		);
}

void RemoteInterface::RIMachine::pad_set_quantize(bool do_quantize) {
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[this, do_quantize, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away

			msg2send->set_value("command", "setqnt");
			msg2send->set_value("doquantize", do_quantize ? "true" : "false");
		}
		);
}

void RemoteInterface::RIMachine::pad_assign_midi_controller(const std::string &controller) {
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[this, controller, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away

			msg2send->set_value("command", "setctrl");
			msg2send->set_value("midictrl", controller);
		}
		);
}

void RemoteInterface::RIMachine::pad_set_chord_mode(ChordMode_t chord_mode) {
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[this, chord_mode, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away

			msg2send->set_value("command", "setchord");
			msg2send->set_value("chordmode", std::to_string((int)chord_mode));
		}
		);
}

void RemoteInterface::RIMachine::pad_set_arpeggio_pattern(const std::string &arp_pattern) {
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[this, arp_pattern, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away

			msg2send->set_value("command", "setarp");
			msg2send->set_value("arppattern", arp_pattern);
		}
		);
}

void RemoteInterface::RIMachine::pad_clear() {
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[this, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away

			msg2send->set_value("command", "padclear");
		}
		);
}

void RemoteInterface::RIMachine::pad_enqueue_event(int finger, PadEvent_t event_type, float ev_x, float ev_y) {
	ev_x *= 1024.0f;
	ev_y *= 1024.0f;
	int xp = ev_x;
	int yp = ev_y;

	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[this, xp, yp, finger, event_type, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "padevt");
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away

			msg2send->set_value("evt", std::to_string(((int)event_type)));
			msg2send->set_value("fgr", std::to_string(finger));
			msg2send->set_value("xp", std::to_string(xp));
			msg2send->set_value("yp", std::to_string(yp));
		},

#if defined(VUKNOB_UDP_SUPPORT) && defined(VUKNOB_UDP_USE)
		true  // send via UDP
#else
		false  // don't send via UDP
#endif
		);
}

void RemoteInterface::RIMachine::enqueue_midi_data(size_t len, const char* data) {
	std::string encoded = encode_byte_array(len, data);
	auto thiz = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
	send_object_message(
		[encoded, thiz](std::shared_ptr<Message> &msg2send) {
			msg2send->set_value("command", "midi");
			msg2send->set_value("ignored", thiz->name); // make sure thiz is not optimized away

			msg2send->set_value("data", encoded);

			SATAN_DEBUG("RIMachine::enqueue_midi_data() - data: %s\n", encoded.c_str());
		}
		);
}

void RemoteInterface::RIMachine::post_constructor_client() {
	for(auto weak_listener : listeners) {
		if(auto listener = weak_listener.lock()) {
			auto mch = std::dynamic_pointer_cast<RIMachine>(shared_from_this());
			listener->ri_machine_registered(mch);
		}
	}
}

void RemoteInterface::RIMachine::process_message(Server *context, MessageHandler *src, const Message &msg) {
	std::string command = msg.get_value("command");

	if(command == "padevt") {
		PadEvent_t event_type = (PadEvent_t)(std::stol(msg.get_value("evt")));
		int finger = std::stol(msg.get_value("fgr"));
		int xp = std::stol(msg.get_value("xp"));
		int yp = std::stol(msg.get_value("yp"));

		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));
		if(mseq != NULL) {
			MachineSequencer::PadEvent_t pevt = MachineSequencer::ms_pad_no_event;
			switch(event_type) {
			case RIMachine::ms_pad_press:
				pevt = MachineSequencer::ms_pad_press;
				break;
			case RIMachine::ms_pad_slide:
				pevt = MachineSequencer::ms_pad_slide;
				break;
			case RIMachine::ms_pad_release:
				pevt = MachineSequencer::ms_pad_release;
				break;
			case RIMachine::ms_pad_no_event:
				pevt = MachineSequencer::ms_pad_no_event;
				break;
			}
			mseq->get_pad()->enqueue_event(finger, pevt, xp, yp);
		}
	} else if(command == "midi") {
		size_t len;
		char *data;
		decode_byte_array(msg.get_value("data"), len, &data);
		SATAN_DEBUG("RIMachine::process_message() - midi data received. (%d -> %p)\n",
			    len, data);
		if(data) {
			try {
				MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));
				if(mseq != NULL) {
					mseq->enqueue_midi_data(len, data);
				}
				free(data);
			} catch(...) {
				free(data);
				throw;
			}
		}
	} else if(command == "setctrval") {
		SATAN_DEBUG("Server will now process a setctrlval message.\n");
		process_setctrl_val_message(src, msg);
	} else if(command == "deleteme") {
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
	} else if(command == "getnrloops") {
		SATAN_DEBUG("Received request for getnrloops.\n");
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));

		if(mseq != NULL) { // see if it's a MachineSequencer
			SATAN_DEBUG("Will send getnrloops reply...\n");
			std::shared_ptr<Message> reply = context->acquire_reply(msg);
			reply->set_value("nrloops", std::to_string(mseq->get_nr_of_loops()));
			src->deliver_message(reply);
			SATAN_DEBUG("Reply delivered...\n");
		} else {
			// not a MachineSequencer
			throw Context::FailureResponse("Not a MachineSequencer object.");
		}
	} else if(command == "export2loop") {
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));

		if(mseq != NULL) { // see if it's a MachineSequencer
			int loop_id = std::stoi(msg.get_value("loopid"));
			mseq->export_pad_to_loop(loop_id);
		} else {
			// not a MachineSequencer
			throw Context::FailureResponse("Not a MachineSequencer object.");
		}
	} else if(command == "setoct") {
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));

		if(mseq != NULL) { // see if it's a MachineSequencer
			int octave = std::stoi(msg.get_value("octave"));
			mseq->get_pad()->set_octave(octave);
		} else {
			// not a MachineSequencer
			throw Context::FailureResponse("Not a MachineSequencer object.");
		}
	} else if(command == "setscl") {
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));

		if(mseq != NULL) { // see if it's a MachineSequencer
			int scale = std::stoi(msg.get_value("scale"));
			mseq->get_pad()->set_scale(scale);
		} else {
			// not a MachineSequencer
			throw Context::FailureResponse("Not a MachineSequencer object.");
		}
	} else if(command == "setrec") {
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));

		if(mseq != NULL) { // see if it's a MachineSequencer
			bool rec = msg.get_value("record") == "true";
			mseq->get_pad()->set_record(rec);
		} else {
			// not a MachineSequencer
			throw Context::FailureResponse("Not a MachineSequencer object.");
		}
	} else if(command == "setqnt") {
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));

		if(mseq != NULL) { // see if it's a MachineSequencer
			bool do_quant = msg.get_value("doquantize") == "true";
			mseq->get_pad()->set_quantize(do_quant);
		} else {
			// not a MachineSequencer
			throw Context::FailureResponse("Not a MachineSequencer object.");
		}
	} else if(command == "setctrl") {
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));

		if(mseq != NULL) { // see if it's a MachineSequencer
			std::string ctrl = msg.get_value("midictrl");
			mseq->assign_pad_to_midi_controller(ctrl);
		} else {
			// not a MachineSequencer
			throw Context::FailureResponse("Not a MachineSequencer object.");
		}
	} else if(command == "setchord") {
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));

		if(mseq != NULL) { // see if it's a MachineSequencer
			ChordMode_t chord_mode = (ChordMode_t)std::stoi(msg.get_value("chordmode"));
			MachineSequencer::PadConfiguration::ChordMode cm = MachineSequencer::PadConfiguration::chord_off;
			switch(chord_mode) {
			case chord_off:
				cm = MachineSequencer::PadConfiguration::chord_off;
				break;
			case chord_triad:
				cm = MachineSequencer::PadConfiguration::chord_triad;
				break;
			}
			mseq->get_pad()->set_chord_mode(cm);
		} else {
			// not a MachineSequencer
			throw Context::FailureResponse("Not a MachineSequencer object.");
		}
	} else if(command == "setarp") {
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));

		if(mseq != NULL) { // see if it's a MachineSequencer
			std::string arp_pattern = msg.get_value("arppattern");
			mseq->set_pad_arpeggio_pattern(arp_pattern);
		} else {
			// not a MachineSequencer
			throw Context::FailureResponse("Not a MachineSequencer object.");
		}
	} else if(command == "padclear") {
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));

		if(mseq != NULL) { // see if it's a MachineSequencer
			mseq->get_pad()->clear_pad();
		} else {
			// not a MachineSequencer
			throw Context::FailureResponse("Not a MachineSequencer object.");
		}
	} else if(command == "get_ctrl_names") {
		SATAN_DEBUG("Will send get_ctrl_names reply...\n");

		auto group_name = msg.get_value("grname");

		std::shared_ptr<Message> reply = context->acquire_reply(msg);
		ItemSerializer serializor;

		if(group_name == "") {
			serializor.process(real_machine_ptr->get_controller_names());
		} else {
			serializor.process(real_machine_ptr->get_controller_names(group_name));
		}
		reply->set_value("ctrlnames", serializor.result());

		src->deliver_message(reply);
		SATAN_DEBUG("Reply delivered...\n");
	} else if(command == "get_ctrl") {
		SATAN_DEBUG("Will send get_ctrl reply...\n");

		std::shared_ptr<Message> reply = context->acquire_reply(msg);
		reply->set_value("ctrl", process_get_ctrl_message(msg.get_value("ctrlname"), src));
		src->deliver_message(reply);

		SATAN_DEBUG("Reply delivered...\n");
	}
}

void RemoteInterface::RIMachine::cleanup_stray_controllers() {
	auto c2cc = client2ctrl_container.begin();
	while(c2cc != client2ctrl_container.end()) {
		if(auto c = (*c2cc).first.lock()) { // if the c is still valid, JUST continue
			c2cc++;
		} else { // otherwise ERASE the container, THEN continue
			c2cc = client2ctrl_container.erase(c2cc);
		}
	}
}

std::string RemoteInterface::RIMachine::process_get_ctrl_message(const std::string &ctrl_name, MessageHandler *src) {
	std::shared_ptr<ServerSideControllerContainer> sscc;
	std::string retval;

	auto src_sp = src->shared_from_this();

	// first try to find the proper ServerSideControllerContainer, if none - create it
	auto sscc_i = client2ctrl_container.find(src_sp);
	if(sscc_i == client2ctrl_container.end()) {
		sscc = std::make_shared<ServerSideControllerContainer>();
		client2ctrl_container[src_sp] = sscc;
	} else sscc = (*sscc_i).second;

	// then we locate a free id
	if(sscc) {
		auto new_id = sscc->get_id();
		auto ctrl = real_machine_ptr->get_controller(ctrl_name);
		SATAN_DEBUG("ServerSide created Machine::Controller with ptr %p\n", ctrl);
		sscc->id2ctrl[new_id] = ctrl;

		RIController rico(new_id, ctrl);
		retval = rico.get_serialized_controller();
	}

	cleanup_stray_controllers();

	return retval;
}

void RemoteInterface::RIMachine::process_setctrl_val_message(MessageHandler *src, const Message &msg) {
	auto c2cc = client2ctrl_container.find(src->shared_from_this());
	if(c2cc != client2ctrl_container.end()) {
		auto sscc = (*c2cc).second;
		int ctrl_id = std::stoi(msg.get_value("ctrl_id"));

		auto ctrl_iterator = sscc->id2ctrl.find(ctrl_id);
		if(ctrl_iterator != sscc->id2ctrl.end()) {
			auto ctrl = (*ctrl_iterator).second;
			SATAN_DEBUG("ServerSide, setctrl val, found previously created Machine::Control with ptr %p\n", ctrl);
			switch(ctrl->get_type()) {
			case Machine::Controller::c_float:
			{
				float val = std::stof(msg.get_value("value"));
				ctrl->set_value(val);
			}
				break;
			case Machine::Controller::c_bool:
			{
				bool val = (msg.get_value("value") == "true") ? true : false;
				ctrl->set_value(val);
			}
				break;
			case Machine::Controller::c_string:
			{
				std::string val = msg.get_value("value");
				ctrl->set_value(val);
			}
				break;
			case Machine::Controller::c_int:
			case Machine::Controller::c_enum:
			case Machine::Controller::c_sigid:
			{
				int val = std::stoi(msg.get_value("value"));
				ctrl->set_value(val);
			}
				break;
			}
		}
	}
}

void RemoteInterface::RIMachine::process_attach_message(Context *context, const Message &msg) {
	int32_t identifier = std::stol(msg.get_value("s_objid"));
	std::string src_output = msg.get_value("source");
	std::string dst_input = msg.get_value("destination");

	if(is_server_side()) {
		try {
			auto mch = std::dynamic_pointer_cast<RIMachine>(context->get_object(identifier));
			real_machine_ptr->attach_input(mch->real_machine_ptr, src_output, dst_input);
		} catch(NoSuchObject exp) {
			/* ignored for now */
		} catch(jException exp) {
			throw Context::FailureResponse(exp.message);
		}

	} else {
		try {
			auto mch = std::dynamic_pointer_cast<RIMachine>(context->get_object(identifier));

			call_listeners(
				[this, &mch, &src_output, &dst_input](std::shared_ptr<RIMachineStateListener> listener) {
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
		try {
			auto mch = std::dynamic_pointer_cast<RIMachine>(context->get_object(identifier));
			real_machine_ptr->detach_input(mch->real_machine_ptr, src_output, dst_input);
		} catch(NoSuchObject exp) {
			/* ignored for now */
		} catch(jException exp) {
			throw Context::FailureResponse(exp.message);
		}
	} else {
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
	std::lock_guard<std::mutex> lock_guard(base_object_mutex);
	std::string command = msg.get_value("command");

	if(command == "setpos") {
		xpos = atof(msg.get_value("xpos").c_str());
		ypos = atof(msg.get_value("ypos").c_str());

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
		std::stringstream mctrl_string;
		for(auto mctrl : midi_controllers) {
			mctrl_string << mctrl << "$";
		}
		if(mctrl_string.str() == "") {
			target->set_value("mctrls", "$"); // no connections
		} else {
			target->set_value("mctrls", mctrl_string.str());
		}
	}

	{
		MachineSequencer *mseq = dynamic_cast<MachineSequencer *>((real_machine_ptr));
		if(mseq != NULL) {
			target->set_value("sibling", mseq->get_machine()->get_name());
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

	{
		ItemSerializer serializor;
		serializor.process(real_machine_ptr->get_controller_groups());
		target->set_value("ctrgroups", serializor.result());
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

RemoteInterface::Client::Client(const std::string &server_host,
				int server_port,
				std::function<void()> _disconnect_callback,
				std::function<void(const std::string &failure_response)> _failure_response_callback) :
	MessageHandler(io_service), resolver(io_service), udp_resolver(io_service), disconnect_callback(_disconnect_callback)
{
	failure_response_callback = _failure_response_callback;
	auto endpoint_iterator = resolver.resolve({server_host, std::to_string(server_port) });

	asio::async_connect(my_socket, endpoint_iterator,
			    [this](std::error_code ec, asio::ip::tcp::resolver::iterator)
			    {
				    if (!ec)
				    {
					    start_receive();
				    } else {
					    SATAN_ERROR("Failed to connect to server.\n");
				    }
			    }
		);

	udp_target_endpoint = *udp_resolver.resolve({asio::ip::udp::v4(), server_host, std::to_string(server_port) });
}

void RemoteInterface::Client::flush_all_objects() {
	for(auto objid2obj : all_objects) {
		objid2obj.second->on_delete(this);
	}
	all_objects.clear();

}

void RemoteInterface::Client::on_message_received(const Message &msg) {
	int identifier = std::stol(msg.get_value("id"));

	SATAN_DEBUG("Client() received message: %d\n", identifier);

	switch(identifier) {
	case __MSG_PROTOCOL_VERSION:
	{
		auto protocol_version = std::stol(msg.get_value("pversion"));
		if(protocol_version > __VUKNOB_PROTOCOL_VERSION__) {
			failure_response_callback("Server is to new - you must upgrade before you can connect.");
			disconnect();
		}
	}
	break;

	case __MSG_CLIENT_ID:
	{
		client_id = std::stol(msg.get_value("clid"));
	}
	break;

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

	case __MSG_REPLY:
	{
		SATAN_DEBUG("Will get repid...\n");

		int32_t reply_identifier = std::stol(msg.get_value("repid"));
		SATAN_DEBUG("Reply received: %d\n", reply_identifier);
		auto repmsg = msg_waiting_for_reply.find(reply_identifier);
		if(repmsg != msg_waiting_for_reply.end()) {
			SATAN_DEBUG("Matching outstanding request.\n");
			repmsg->second->reply_to(&msg);
			SATAN_DEBUG("Reply processed.\n");
			msg_waiting_for_reply.erase(repmsg);
		}
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
	// inform all waiting messages that their action failed
	for(auto msg : msg_waiting_for_reply) {
		msg.second->reply_to(NULL);
	}
	msg_waiting_for_reply.clear();

	flush_all_objects();
	disconnect_callback();

}

void RemoteInterface::Client::start_client(const std::string &server_host,
					   int server_port,
					   std::function<void()> disconnect_callback,
					   std::function<void(const std::string &fresp)> failure_response_callback) {
	std::lock_guard<std::mutex> lock_guard(client_mutex);

	try {
		client = std::shared_ptr<Client>(new Client(server_host, server_port, disconnect_callback, failure_response_callback));

		client->io_thread = std::thread([]() {
				client->io_service.run();
			}
			);
	} catch (std::exception& e) {
		SATAN_ERROR("exception caught: %s\n", e.what());
	}
}

void RemoteInterface::Client::disconnect() {
	std::lock_guard<std::mutex> lock_guard(client_mutex);
	if(client) {
		client->io_service.post(
			[]()
			{
				try {
					client->my_socket.close();
				} catch(std::exception &exp) {
				}
				client->io_service.stop();
			});

		client->io_thread.join();
		client.reset();
	}
}

void RemoteInterface::Client::register_ri_machine_set_listener(std::weak_ptr<RIMachine::RIMachineSetListener> ri_mset_listener) {
	std::lock_guard<std::mutex> lock_guard(client_mutex);
	if(client) {
		client->io_service.post(
		[ri_mset_listener]()
		{
			RIMachine::register_ri_machine_set_listener(ri_mset_listener);
		});
	} else {
		// if disconnected, we can go ahead and directly register...
		RIMachine::register_ri_machine_set_listener(ri_mset_listener);
	}

}

void RemoteInterface::Client::distribute_message(std::shared_ptr<Message> &msg, bool via_udp) {
	if(msg->is_awaiting_reply()) {
		if(msg_waiting_for_reply.find(next_reply_id) != msg_waiting_for_reply.end()) {
			// no available reply id - reply with empty message to signal failure
			msg->reply_to(NULL);
		} else {
			// fill in object reply id header
			msg->set_value("repid", std::to_string(next_reply_id));
			// add the msg to our set of messages waiting for a reply
			msg_waiting_for_reply[next_reply_id++] = msg;
			// deliver it
			deliver_message(msg, via_udp);
		}
	} else {
		deliver_message(msg, via_udp);
	}
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

RemoteInterface::Server::ClientAgent::ClientAgent(int32_t _id, asio::ip::tcp::socket _socket, Server *_server) :
	MessageHandler(std::move(_socket)), id(_id), server(_server) {
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
		server->route_incomming_message(this, msg);
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
	distribute_message(create_object_message, false);
}

void RemoteInterface::Server::delete_object(std::shared_ptr<BaseObject> obj2delete) {
	for(auto obj  = all_objects.begin();
	    obj != all_objects.end();
	    obj++) {
		if((*obj).second == obj2delete) {

			std::shared_ptr<Message> destroy_object_message = acquire_message();
			add_destroy_object_header(destroy_object_message, obj2delete);
			distribute_message(destroy_object_message, false);

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
	io_service.post(

		[this, m_ptr]()
		{
			SATAN_DEBUG("!!! Machine %s was registered\n", m_ptr->get_name().c_str());
			std::string resp_msg;

			try {
				create_object_from_factory(__FCT_RIMACHINE,
							   [this, m_ptr](std::shared_ptr<BaseObject> nuobj) {
								   auto mch = std::dynamic_pointer_cast<RIMachine>(nuobj);
								   mch->serverside_init_from_machine_ptr(m_ptr);

								   machine2rimachine[m_ptr] = mch;
							   }
					);
			} catch(BaseObject::ObjIdOverflow &e) {
				resp_msg = "Internal server error - Object ID overflow. Please restart session.";
			}
			if(resp_msg != "") {
				std::shared_ptr<Message> response = server->acquire_message();
				response->set_value("id", std::to_string(__MSG_FAILURE_RESPONSE));
				response->set_value("response", resp_msg);
				distribute_message(response);
			}
		}

		);
}

void RemoteInterface::Server::machine_unregistered(Machine *m_ptr) {
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
	io_service.post(
		[this, source, destination, output_name, input_name]()
		{
			SATAN_DEBUG("!!! Signal attached [%s:%s] -> [%s:%s]\n",
				    source->get_name().c_str(), output_name.c_str(),
				    destination->get_name().c_str(), input_name.c_str());

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
	io_service.post(
		[this, source, destination, output_name, input_name]()
		{
			SATAN_DEBUG("!!! Signal detached [%s:%s] -> [%s:%s]\n",
				    source->get_name().c_str(), output_name.c_str(),
				    destination->get_name().c_str(), input_name.c_str());

			auto src_mch = machine2rimachine.find(source);
			auto dst_mch = machine2rimachine.find(destination);
			if(src_mch != machine2rimachine.end() && dst_mch != machine2rimachine.end()) {
				dst_mch->second->detach_input(src_mch->second, output_name, input_name);
			}
		}

		);
}

RemoteInterface::Server::Server(const asio::ip::tcp::endpoint& endpoint) : last_obj_id(-1), acceptor(io_service, endpoint),
									   acceptor_socket(io_service)
{
	acceptor.listen();

	current_port = acceptor.local_endpoint().port();

	SATAN_DEBUG("RemoteInterface::Server::Server() current ip: %s, port: %d\n",
		    acceptor.local_endpoint().address().to_string().c_str(),
		    current_port);

#ifdef VUKNOB_UDP_SUPPORT
	udp_socket = std::make_shared<asio::ip::udp::socket>(io_service,
							     asio::ip::udp::endpoint(asio::ip::udp::v4(), current_port));
#endif

	io_service.post(
		[this]()
		{
			try {
				this->create_service_objects();
			} catch(std::exception &exp) {
				throw;
			}
		});

	do_accept();
#ifdef VUKNOB_UDP_SUPPORT
	do_udp_receive();
#endif
}

int RemoteInterface::Server::get_port() {
	return current_port;
}

void RemoteInterface::Server::do_accept() {
	acceptor.async_accept(
		acceptor_socket,
		[this](std::error_code ec) {

			if (!ec) {
				auto new_id = next_client_agent_id++;

				std::shared_ptr<ClientAgent> new_client_agent = std::make_shared<ClientAgent>(new_id,
													      std::move(acceptor_socket), this);

				client_agents[new_id] = new_client_agent;

				send_protocol_version_to_new_client(new_client_agent);
				send_client_id_to_new_client(new_client_agent);
				send_all_objects_to_new_client(new_client_agent);

				new_client_agent->start();
			}

			do_accept();
		}
		);
}

void RemoteInterface::Server::do_udp_receive() {
	udp_socket->async_receive_from(
		//asio::buffer(udp_buffer),
		udp_read_msg.prepare_buffer(VUKNOB_MAX_UDP_SIZE),
		udp_endpoint,
		[this](std::error_code ec,
		       std::size_t bytes_transferred) {
//			if(!ec) udp_read_msg.commit_data(bytes_transferred);

			if((!ec) && udp_read_msg.decode_client_id()) {
				if(udp_read_msg.decode_header()) {
					if(udp_read_msg.decode_body()) {
						// make sure body length matches what we received
						if(bytes_transferred == 16 + udp_read_msg.get_body_length()) {
							try {
								auto udp_client_agent = client_agents.find(udp_read_msg.get_client_id());
								if(udp_client_agent != client_agents.end()) {
									udp_client_agent->second->on_message_received(udp_read_msg);
								} else {
									SATAN_ERROR("RemoteInterface::Server::do_udp_receive()"
										    " received an udp message from %s with an"
										    " unkown client id %d.\n",
										    udp_endpoint.address().to_string().c_str(),
										    udp_read_msg.get_client_id());
								}
							} catch (std::exception& e) {
								SATAN_ERROR("RemoteInterface::Server::do_udp_receive() caught an exception (%s)"
									    " when processing an incomming message.\n",
									    e.what());
							}
						} else {
							SATAN_ERROR("RemoteInterface::Server::do_udp_receive() received an"
								    " udp message from %s of the wrong size (%d != %d).\n",
								    udp_endpoint.address().to_string().c_str(),
								    bytes_transferred - 16, udp_read_msg.get_body_length());
						}
					} else {
						SATAN_ERROR("RemoteInterface::Server::do_udp_receive() received an"
							    " udp message from %s with a malformed body.\n",
							    udp_endpoint.address().to_string().c_str());
					}
				} else {
					SATAN_ERROR("RemoteInterface::Server::do_udp_receive() received an"
						    " udp message from %s with a malformed header.\n",
						    udp_endpoint.address().to_string().c_str());
				}
			} else {
				SATAN_ERROR("RemoteInterface::Server::do_udp_receive() received a malformed udp message from %s.\n",
					    udp_endpoint.address().to_string().c_str());
			}

			do_udp_receive();
		}
		);
}

void RemoteInterface::Server::drop_client(std::shared_ptr<ClientAgent> client_agent) {
	auto client_iterator = client_agents.find(client_agent->get_id());
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

void RemoteInterface::Server::send_protocol_version_to_new_client(std::shared_ptr<MessageHandler> client_agent) {
	std::shared_ptr<Message> pv_message = acquire_message();
	pv_message->set_value("id", std::to_string(__MSG_PROTOCOL_VERSION));
	pv_message->set_value("pversion", std::to_string(__VUKNOB_PROTOCOL_VERSION__));
	client_agent->deliver_message(pv_message);
}

void RemoteInterface::Server::send_client_id_to_new_client(std::shared_ptr<ClientAgent> client_agent) {
	std::shared_ptr<Message> pv_message = acquire_message();
	pv_message->set_value("id", std::to_string(__MSG_CLIENT_ID));
	pv_message->set_value("clid", std::to_string(client_agent->get_id()));
	client_agent->deliver_message(pv_message);
}

void RemoteInterface::Server::send_all_objects_to_new_client(std::shared_ptr<MessageHandler> client_agent) {
	for(auto obj : all_objects) {
		std::shared_ptr<Message> create_object_message = acquire_message();
		add_create_object_header(create_object_message, obj.second);
		obj.second->serialize(create_object_message);
		client_agent->deliver_message(create_object_message);
	}
}

void RemoteInterface::Server::route_incomming_message(ClientAgent *src, const Message &msg) {
	int identifier = std::stol(msg.get_value("id"));

	auto obj_iterator = all_objects.find(identifier);
	if(obj_iterator == all_objects.end()) throw BaseObject::NoSuchObject();

	obj_iterator->second->process_message(this, src, msg);
}

void RemoteInterface::Server::disconnect_clients() {
	for(auto client_agent : client_agents) {
		client_agent.second->disconnect();
	}
}

void RemoteInterface::Server::create_service_objects() {
	{ // create handle list object
		create_object_from_factory(__FCT_HANDLELIST, [](std::shared_ptr<BaseObject> new_obj){});
	}
	{ // create global control object
		create_object_from_factory(__FCT_GLOBALCONTROLOBJECT, [](std::shared_ptr<BaseObject> new_obj){});
	}
	{ // create global sample bank
		create_object_from_factory(__FCT_SAMPLEBANK, [](std::shared_ptr<BaseObject> new_obj){});
	}
	{ // register us as a machine set listener
		Machine::register_machine_set_listener(shared_from_this());
	}
}

int RemoteInterface::Server::start_server() {
	std::lock_guard<std::mutex> lock_guard(server_mutex);

	if(server) {
		return server->get_port();
	}

	int portval = -1;

	try {
		asio::ip::tcp::endpoint endpoint;//(asio::ip::tcp::v4(), 0); // 0 => select a random available port
		server = std::shared_ptr<Server>(new Server(endpoint));

		server->io_thread = std::thread([]() {
				try {
					server->io_service.run();
				} catch(std::exception const& e) {
					SATAN_ERROR("RemoteInterface::Server::start_server() - std::exception caught %s\n", e.what());
				} catch(...) {
					SATAN_ERROR("RemoteInterface::Server::start_server() - unknown exception caught\n");
				}
			}
			);

		portval = server->get_port();
	} catch (std::exception& e) {
		SATAN_ERROR("Exception caught: %s\n", e.what());
	}

	return portval;
}

bool RemoteInterface::Server::is_running() {
	std::lock_guard<std::mutex> lock_guard(server_mutex);

	if(server) {
		return true;
	}
	return false;
}

void RemoteInterface::Server::stop_server() {
	std::lock_guard<std::mutex> lock_guard(server_mutex);

	if(server) {
		server->io_service.post(
		[]()
		{
			try {
				SATAN_DEBUG("will disconnect clients...\n");
				server->disconnect_clients();
				SATAN_DEBUG("clients disconnected!");
			} catch(std::exception &exp) {
				throw;
			}
			SATAN_DEBUG("stop IO service...!");
			server->io_service.stop();
		});

		SATAN_DEBUG("waiting for thread...!");
		server->io_thread.join();
		SATAN_DEBUG("reseting server object...!");
		server.reset();
		SATAN_DEBUG("done!");
	}
}

void RemoteInterface::Server::distribute_message(std::shared_ptr<Message> &msg, bool via_udp) {
	for(auto client_agent : client_agents) {
		client_agent.second->deliver_message(msg, via_udp);
	}
}

auto RemoteInterface::Server::get_object(int32_t objid) -> std::shared_ptr<BaseObject> {
	if(all_objects.find(objid) == all_objects.end()) throw BaseObject::NoSuchObject();

	return all_objects[objid];
}
