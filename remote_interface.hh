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

#ifndef VUKNOB_REMOTE_INTERFACE
#define VUKNOB_REMOTE_INTERFACE

#include "machine.hh"

#include <string>
#include <vector>
#include <deque>
#include <set>
#include <map>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <asio.hpp>
#include <thread>

#define RI_LOOP_NOT_SET -1

class RemoteInterface {
public:
	class Server;
	class Client;

protected:
	class MessageHandler;
	class Context;
	class BaseObject;

	class Message {
	public:		
		enum { header_length = 8 };

		class NoSuchKey : public std::runtime_error {
		public:
			NoSuchKey() : runtime_error("No such key in message.") {}
			virtual ~NoSuchKey() {}
		};
		
		class IllegalChar : public std::runtime_error {
		public:
			IllegalChar() : runtime_error("Key or value contains illegal character.") {}
			virtual ~IllegalChar() {}
		};

		class MessageTooLarge : public std::runtime_error {
		public:
			MessageTooLarge() : runtime_error("Encoded message too large.") {}
			virtual ~MessageTooLarge() {}
		};
		
		friend class MessageHandler;
		friend class Context;
		
		Message();
		Message(Context *context);
		
		void set_value(const std::string &key, const std::string &value);
		std::string get_value(const std::string &key) const;		
		
	private:
		static void recycle(Message *msg);

		bool decode_header();
		bool decode_body();

		void encode() const;

		Context *context;
		
		mutable bool encoded;		
		mutable uint32_t body_length;
		mutable asio::streambuf sbuf;
		mutable std::ostream ostrm;
		mutable int data2send;
		
		std::map<std::string, std::string> key2val;

	};

	class Context {
	private:
		std::deque<Message *> available_messages;
		
	public:
		class FailureResponse : public std::runtime_error {
		public:
			std::string response_message;
			FailureResponse(const std::string &_response_message) : runtime_error("Remote interface command execution failed - response to sender generated."), response_message(_response_message) {}
			virtual ~FailureResponse() {}
		};

		~Context();
		virtual void distribute_message(std::shared_ptr<Message> &msg) = 0;
		virtual void post_action(std::function<void()> f) = 0;
		virtual void dispatch_action(std::function<void()> f) = 0;
		virtual std::shared_ptr<BaseObject> get_object(int32_t objid) = 0;

		std::shared_ptr<Message> acquire_message();
		void recycle_message(Message *used_message);
	};
	
	class MessageHandler : public std::enable_shared_from_this<MessageHandler> {
	private:
		Message read_msg;
		std::deque<std::shared_ptr<Message> > write_msgs;

		void do_read_header();
		void do_read_body();
		void do_write();
		
	protected:
		asio::ip::tcp::socket my_socket;
		
	public:
		MessageHandler(asio::io_service &io_service);
		MessageHandler(asio::ip::tcp::socket _socket);

		void start_receive();
		void deliver_message(std::shared_ptr<Message> &msg); // will encode() msg before transfer

		virtual void on_message_received(const Message &msg) = 0;
		virtual void on_connection_dropped() = 0;
	};
	
	class BaseObject : public std::enable_shared_from_this<BaseObject> {
	private:
		bool __is_server_side = false;
		
	protected:
		class Factory {
		private:
			const char* type;
		public:
			class FactoryAlreadyCreated : public std::runtime_error {
			public:
				FactoryAlreadyCreated() : runtime_error("Tried to create multiple RemoteInterface::BaseObject::Factory for the same type.") {}
				virtual ~FactoryAlreadyCreated() {}
			};
			
			Factory(const char* type);
			~Factory();

			const char* get_type() const;
			
			virtual std::shared_ptr<BaseObject> create(const Message &serialized) = 0;
			virtual std::shared_ptr<BaseObject> create(int32_t new_obj_id) = 0;
		};

		BaseObject(const Factory *factory, const Message &serialized);
		
		// used to create server side objects - which will then be serialized into messages and sent to all the clients.
		BaseObject(int32_t new_obj_id, const Factory *factory);

		Context *context;

		void send_object_message(std::function<void(std::shared_ptr<Message> &msg_to_send)>);

		inline bool is_server_side() { return __is_server_side; }
		
	public:
		class ObjectType {
		public:
			const char *type_name;
		};
		
		class NoSuchFactory : public std::runtime_error {
		public:
			NoSuchFactory() : runtime_error("Tried to create object in unknown RemoteInterface::BaseObject::Factory.") {}
			virtual ~NoSuchFactory() {}
		};
		class DuplicateObjectId : public std::runtime_error {
		public:
			DuplicateObjectId() : runtime_error("Tried to create an RemoteInterface::BaseObject with an already used id.") {}
			virtual ~DuplicateObjectId() {}
		};
		class NoSuchObject : public std::runtime_error {
		public:
			NoSuchObject() : runtime_error("Tried to route message to an unknown RemoteInterface::BaseObject.") {}
			virtual ~NoSuchObject() {}
		};
		class ObjIdOverflow : public std::runtime_error {
		public:
			ObjIdOverflow() : runtime_error("This session ran out of possible values RemoteInterface::BaseObject::obj_id.") {}
			virtual ~ObjIdOverflow() {}
		};

		int32_t get_obj_id();
		auto get_type() -> ObjectType;

		void set_context(Context *context);

		virtual void post_constructor_client() = 0; // called after the constructor has been called
		virtual void process_message(Server *context, const Message &msg) = 0; // server side processing
		virtual void process_message(Client *context, const Message &msg) = 0; // client side processing
		virtual void serialize(std::shared_ptr<Message> &target) = 0;
		virtual void on_delete(Client *context) = 0; // called on client side when it's about to be deleted
		
		static std::shared_ptr<BaseObject> create_object_from_message(const Message &msg);
		static std::shared_ptr<BaseObject> create_object_on_server(int32_t new_obj_id, const std::string &type);

	private:
		static std::map<std::string, Factory *> factories;

		int32_t obj_id;

		const Factory *my_factory;
	};

public:

	class HandleList : public BaseObject {
	private:
		std::map<std::string, std::string> handle2hint;
		static std::weak_ptr<HandleList> clientside_handle_list;
		
		class HandleListFactory : public Factory {
		public:
			HandleListFactory();
						
			virtual std::shared_ptr<BaseObject> create(const Message &serialized) override;
			virtual std::shared_ptr<BaseObject> create(int32_t new_obj_id) override;
		};

		static HandleListFactory handlelist_factory;

		virtual void post_constructor_client(); // called after the constructor has been called
		virtual void process_message(Server *context, const Message &msg); // server side processing
		virtual void process_message(Client *context, const Message &msg); // client side processing
		virtual void serialize(std::shared_ptr<Message> &target);
		virtual void on_delete(Client *context); // called on client side when it's about to be deleted
		
	public:
		HandleList(const Factory *factory, const Message &serialized); // create client side HandleList
		HandleList(int32_t new_obj_id, const Factory *factory); // create server side HandleList

		static std::map<std::string, std::string> get_handles_and_hints();
		static void create_instance(const std::string &handle, double xpos, double ypos);
	};
	
	class RIMachine : public BaseObject {
	public:
		/* server side API */
		RIMachine(const Factory *factory, const Message &serialized);
		RIMachine(int32_t new_obj_id, const Factory *factory);

		void serverside_init_from_machine_ptr(Machine *m_ptr);

	public:
		/* mixed server/client side API */
		void attach_input(std::shared_ptr<RIMachine> source_machine,
				  const std::string &source_output_name,
				  const std::string &destination_input_name);
		void detach_input(std::shared_ptr<RIMachine> source_machine,
				  const std::string &source_output_name,
				  const std::string &destination_input_name);

	public: 	/* client side base API */	
		class RIMachineSetListener {
		public:
			virtual void ri_machine_registered(std::shared_ptr<RIMachine> ri_machine) = 0;
			virtual void ri_machine_unregistered(std::shared_ptr<RIMachine> ri_machine) = 0;
		};

		class RIMachineStateListener {
		public:
			virtual void on_move() = 0;
			virtual void on_attach(std::shared_ptr<RIMachine> src_machine,
					       const std::string src_output,
					       const std::string dst_input) = 0;
			virtual void on_detach(std::shared_ptr<RIMachine> src_machine,
					       const std::string src_output,
					       const std::string dst_input) = 0;
		};
		
		std::string get_name();
		std::string get_machine_type();

		double get_x_position();
		double get_y_position();
		void set_position(double xpos, double ypos);
		
		void delete_machine(); // called on client - asks server to delete the machine.
		
		std::vector<std::string> get_input_names();
		std::vector<std::string> get_output_names();

		// if the client side app wants to be notified when things in this machine changes
		void set_state_change_listener(std::weak_ptr<RIMachineStateListener> state_listener);

	public:        /* client side Machine Sequencer/Pad API */
		std::vector<std::string> mseq_available_midi_controllers();
		void pad_export_to_loop(int loop_id = RI_LOOP_NOT_SET);
		void pad_set_octave(int octave);
		void pad_set_scale(int scale_index);
		void pad_set_record(bool record);
		void pad_set_quantize(bool do_quantize);
		void pad_assign_midi_controller(int controller);
		void pad_set_chord_mode(int chord_mode);
		void pad_set_arpeggio_pattern(int arp_pattern);
		void pad_clear();
		void pad_enqueue_event(int finger, int event_type, float ev_x, float ev_y);
		
	public:
		virtual void post_constructor_client(); // called after the constructor has been called
		virtual void process_message(Server *context, const Message &msg); // server side processing
		virtual void process_message(Client *context, const Message &msg); // client side processing
		virtual void serialize(std::shared_ptr<Message> &target);
		virtual void on_delete(Client *context); // called on client side when it's about to be deleted

	private:
		class RIMachineFactory : public Factory {
		public:
			RIMachineFactory();

			virtual std::shared_ptr<BaseObject> create(const Message &serialized);
			virtual std::shared_ptr<BaseObject> create(int32_t new_obj_id);
			
		};

		static RIMachineFactory rimachine_factory;
		static std::set<std::weak_ptr<RIMachineSetListener>,
				std::owner_less<std::weak_ptr<RIMachineSetListener> > > listeners;	
		
		std::multimap<int32_t, std::pair<std::string, std::string> > connection_data; // [source machine objid]->(output name, input name)
		std::vector<std::string> inputs;
		std::vector<std::string> outputs;
		
		std::mutex ri_machine_mutex;
		std::string name;
		std::string type;
		Machine *real_machine_ptr = nullptr;
		double xpos, ypos;

		void process_attach_message(Context *context, const Message &msg);
		void process_detach_message(Context *context, const Message &msg);
		
		void parse_serialized_connections_data(std::string serialized);
		void call_listeners(std::function<void(std::shared_ptr<RIMachineStateListener> listener)> callback);
		
		// used on client side when somethings changed server side
		std::set<std::weak_ptr<RIMachineStateListener>,
			 std::owner_less<std::weak_ptr<RIMachineStateListener> > >state_listeners;

		friend class Client;		
		static void register_ri_machine_set_listener(std::weak_ptr<RIMachineSetListener> listener);
	};

	class Client : public MessageHandler, public Context {
	private:
		std::map<int32_t, std::shared_ptr<BaseObject> > all_objects;		

		std::map<std::string, std::string> handle2hint;
	       		
		std::thread t1;
		asio::ip::tcp::resolver resolver;
		std::function<void()> disconnect_callback;
		std::function<void(const std::string &fresp)> failure_response_callback;
		
		Client(const std::string &server_host,
		       std::function<void()> disconnect_callback,
		       std::function<void(const std::string &failure_response)> failure_response_callback);

		void flush_all_objects();
		
		static std::shared_ptr<Client> client;
		static asio::io_service io_service;
		static std::mutex client_mutex;
	       
	public: // public singleton interface
		static void start_client(const std::string &server_host,
					 std::function<void()> disconnect_callback,
					 std::function<void(const std::string &failure_response)> failure_response_callback);
		static void disconnect();
		
		static void register_ri_machine_set_listener(std::weak_ptr<RIMachine::RIMachineSetListener> ri_mset_listener);
		
	public:
		virtual void on_message_received(const Message &msg) override;
		virtual void on_connection_dropped() override;

		virtual void distribute_message(std::shared_ptr<Message> &msg) override;
		virtual void post_action(std::function<void()> f) override;
		virtual void dispatch_action(std::function<void()> f) override;
		virtual std::shared_ptr<BaseObject> get_object(int32_t objid) override;

		class ClientNotConnected : public std::runtime_error {
		public:
			ClientNotConnected() : runtime_error("RemoteInterface::Client not connected to server.") {}
			virtual ~ClientNotConnected() {}
		};

	};

	class Server : public Context, public Machine::MachineSetListener, public std::enable_shared_from_this<Server> {
	private:
		/**** begin service objects data and logic ****/

		std::map<int32_t, std::shared_ptr<BaseObject> > all_objects;
		int32_t last_obj_id; // I am making an assumption here that last_obj_id will not be counted up more than 1/sec. This gives that time until overflow for a session will be more than 20000 days. I am making a bet here that the app will crash or the users will run tired before these 20000 days are over.

		std::map<Machine *, std::shared_ptr<RIMachine> > machine2rimachine;
		
		int32_t reserve_new_obj_id();
		void create_object_from_factory(const std::string &factory_type,
						std::function<void(std::shared_ptr<BaseObject> nuobj)> new_object_init_callback);
		void delete_object(std::shared_ptr<BaseObject> obj2delete);

		virtual void project_loaded() override; // MachineSetListener interface
		virtual void machine_registered(Machine *m_ptr) override; // MachineSetListener interface
		virtual void machine_unregistered(Machine *m_ptr) override; // MachineSetListener interface
		virtual void machine_input_attached(Machine *source, Machine *destination,
						    const std::string &output_name,
						    const std::string &input_name) override; // MachineSetListener interface
		virtual void machine_input_detached(Machine *source, Machine *destination,
						    const std::string &output_name,
						    const std::string &input_name) override; // MachineSetListener interface
		
		std::shared_ptr<HandleList> handle_list;
		
		/**** end service objects data and logic ****/
		
		class ClientAgent : public MessageHandler {
		private:
			Server *server;
			
			void send_handler_message();
		public:
			ClientAgent(asio::ip::tcp::socket _socket, Server *server);
			void start();

			void disconnect();
			
			virtual void on_message_received(const Message &msg) override;
			virtual void on_connection_dropped() override;
		};
		friend class ClientAgent;

		typedef std::shared_ptr<ClientAgent> ClientAgent_ptr;
		std::set<ClientAgent_ptr> client_agents;		
		
		std::thread t1;
		asio::io_service io_service;
		asio::ip::tcp::acceptor acceptor;
		asio::ip::tcp::socket acceptor_socket;		

		void do_accept();
		void drop_client(std::shared_ptr<ClientAgent> client_agent);

		void disconnect_clients();
		void create_service_objects();
		void add_create_object_header(std::shared_ptr<Message> &target, std::shared_ptr<BaseObject> obj);
		void add_destroy_object_header(std::shared_ptr<Message> &target, std::shared_ptr<BaseObject> obj);
		void send_protocol_version_to_new_client(std::shared_ptr<MessageHandler> client_agent);
		void send_all_objects_to_new_client(std::shared_ptr<MessageHandler> client_agent);
		
		Server(const asio::ip::tcp::endpoint& endpoint);

		void route_incomming_message(const Message &msg);
		
		static std::shared_ptr<Server> server;
		static std::mutex server_mutex;
		
	public:
		static void start_server();
		static void stop_server();
		
		virtual void distribute_message(std::shared_ptr<Message> &msg) override;
		virtual void post_action(std::function<void()> f) override;
		virtual void dispatch_action(std::function<void()> f) override;		
		virtual std::shared_ptr<BaseObject> get_object(int32_t objid) override;
	};

};

#endif
