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

#include "common.hh"

#define RI_LOOP_NOT_SET -1

namespace RemoteInterface {
	class Server;
	class Client;

	class MessageHandler;
	class Context;
	class BaseObject;

	static inline std::string encode_byte_array(size_t len, const char *data) {
		std::string result;
		size_t offset = 0;
		while(len - offset > 0) {
			char frb[3];

			frb[0] = ((data[offset] & 0xf0) >> 4) + 'a';
			frb[1] = ((data[offset] & 0x0f) >> 0) + 'a';
			frb[2] = '\0';

			offset++;

			result +=  frb;
		}
		return result;
	}

	static inline void decode_byte_array(const std::string &encoded,
					     size_t &len, char **data) {
		*data = (char*)malloc((sizeof(char) * encoded.size()) >> 1);
		if(*data == 0) return;

		len = encoded.size() / 2;
		size_t offset = 0;

		while(len - offset > 0) {
			(*data)[offset] =
				( (encoded[(offset << 1) + 0] - 'a') << 4)
				|
				( (encoded[(offset << 1) + 1] - 'a') << 0);
			offset++;
		}
	}

	class Message {
	public:
		enum { header_length = 8 };

		class NoSuchKey : public std::runtime_error {
		public:
			const char *keyname;

			NoSuchKey(const char *_keyname) : runtime_error("No such key in message."), keyname(_keyname) {}
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

		class CannotReceiveReply : public std::runtime_error {
		public:
			CannotReceiveReply() : runtime_error("Server reply interrupted.") {}
			virtual ~CannotReceiveReply() {}
		};

		Message();
		Message(Context *context);

		void set_reply_handler(std::function<void(const Message *reply_msg)> reply_received_callback);
		void reply_to(const Message *reply_msg);
		bool is_awaiting_reply();

		void set_value(const std::string &key, const std::string &value);
		std::string get_value(const std::string &key) const;

		asio::streambuf::mutable_buffers_type prepare_buffer(std::size_t length);
		void commit_data(std::size_t length);
		asio::streambuf::const_buffers_type get_data();

	private:
		Context *context;

		mutable bool encoded;
		mutable uint32_t body_length;
		mutable int32_t client_id;
		mutable asio::streambuf sbuf;
		mutable std::ostream ostrm;
		mutable int data2send;

		std::function<void(const Message *reply_msg)> reply_received_callback;
		bool awaiting_reply = false;

		std::map<std::string, std::string> key2val;

		void clear_msg_content();

	public:
		static void recycle(Message *msg);

		inline uint32_t get_body_length() { return body_length; }
		inline int32_t get_client_id() { return client_id; }

		bool decode_client_id(); // only for messages arriving via UDP
		bool decode_header();
		bool decode_body();

		void encode() const;

	};

	class Context {
	private:
		std::deque<Message *> available_messages;

	protected:
		std::thread io_thread;
		asio::io_service io_service;

	public:
		class FailureResponse : public std::runtime_error {
		public:
			std::string response_message;
			FailureResponse(const std::string &_response_message) :
				runtime_error("Remote interface command execution failed - response to sender generated."),
				response_message(_response_message) {}
			virtual ~FailureResponse() {}
		};

		~Context();
		virtual void distribute_message(std::shared_ptr<Message> &msg, bool via_udp) = 0;
		virtual std::shared_ptr<BaseObject> get_object(int32_t objid) = 0;

		virtual void post_action(std::function<void()> f, bool do_synch = false);
		std::shared_ptr<Message> acquire_message();
		std::shared_ptr<Message> acquire_reply(const Message &originator);
		void recycle_message(Message *used_message);
	};

	class MessageHandler : public std::enable_shared_from_this<MessageHandler> {
	private:
		Message read_msg;
		std::deque<std::shared_ptr<Message> > write_msgs;

		void do_read_header();
		void do_read_body();
		void do_write();
		void do_write_udp(std::shared_ptr<Message> &msg);

	protected:
		asio::ip::tcp::socket my_socket;

		std::shared_ptr<asio::ip::udp::socket> my_udp_socket;
		asio::ip::udp::endpoint udp_target_endpoint;

	public:
		MessageHandler(asio::io_service &io_service);
		MessageHandler(asio::ip::tcp::socket _socket);

		void start_receive();
		void deliver_message(std::shared_ptr<Message> &msg, bool via_udp = false); // will encode() msg before transfer

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
		std::mutex base_object_mutex;

		void send_object_message(std::function<void(std::shared_ptr<Message> &msg_to_send)> create_msg_callback, bool via_udp = false);
		void send_object_message(std::function<void(std::shared_ptr<Message> &msg_to_send)> create_msg_callback,
					 std::function<void(const Message *reply_message)> reply_received_callback);

		inline bool is_server_side() { return __is_server_side; }
		inline bool is_client_side() { return !__is_server_side; }

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
		virtual void process_message(Server *context, MessageHandler *src, const Message &msg) = 0; // server side processing
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
		virtual void process_message(Server *context, MessageHandler *src, const Message &msg); // server side processing
		virtual void process_message(Client *context, const Message &msg); // client side processing
		virtual void serialize(std::shared_ptr<Message> &target);
		virtual void on_delete(Client *context); // called on client side when it's about to be deleted

	public:
		HandleList(const Factory *factory, const Message &serialized); // create client side HandleList
		HandleList(int32_t new_obj_id, const Factory *factory); // create server side HandleList

		static std::map<std::string, std::string> get_handles_and_hints();
		static void create_instance(const std::string &handle, double xpos, double ypos);
	};

	class GlobalControlObject : public BaseObject {
	private:
		class GlobalControlObjectFactory : public Factory {
		public:
			GlobalControlObjectFactory();

			virtual std::shared_ptr<BaseObject> create(const Message &serialized) override;
			virtual std::shared_ptr<BaseObject> create(int32_t new_obj_id) override;
		};

		virtual void post_constructor_client() override; // called after the constructor has been called
		virtual void process_message(Server *context, MessageHandler *src, const Message &msg) override; // server side processing
		virtual void process_message(Client *context, const Message &msg) override; // client side processing
		virtual void serialize(std::shared_ptr<Message> &target) override;
		virtual void on_delete(Client *context) override; // called on client side when it's about to be deleted

		void parse_serialized_arp_patterns(std::vector<std::string> &retval, const std::string &serialized_arp_patterns);
		void parse_serialized_keys(const std::string &scale_id, const std::string &serialized_keys);
		void serialize_arp_patterns(std::shared_ptr<Message> &target);
		void serialize_keys(std::shared_ptr<Message> &target,
				    const std::string &id,
				    std::vector<int> keys);
	public:
		GlobalControlObject(const Factory *factory, const Message &serialized); // create client side HandleList
		GlobalControlObject(int32_t new_obj_id, const Factory *factory); // create server side HandleList

	public: // client side interface
		class PlaybackStateListener {
		public:
			virtual void playback_state_changed(bool is_playing) = 0;
			virtual void recording_state_changed(bool is_recording) = 0;
			virtual void periodic_playback_update(int current_row) = 0;
		};

		static std::shared_ptr<GlobalControlObject> get_global_control_object(); // get a shared ptr to the current GCO, shared_ptr will be empty if the client is not connected

		std::vector<std::string> get_pad_arpeggio_patterns();
		std::vector<std::string> get_scale_names();
		std::vector<int> get_scale_keys(const std::string &scale_name);
		const char* get_key_text(int key);
		int get_custom_scale_note(int offset);
		void set_custom_scale_note(int offset, int note);

		bool is_it_playing();
		void play();
		void stop();
		void rewind();
		void set_record_state(bool do_record);
		bool get_record_state();
		std::string get_record_file_name();

		int get_lpb();
		int get_bpm();
		void set_lpb(int lpb);
		void set_bpm(int lpb);

		static void register_playback_state_listener(std::shared_ptr<PlaybackStateListener> listener_object);

//		void register_periodic(std::function<void(int line)>, int nr_lines_per_period);

	private:
		static std::weak_ptr<GlobalControlObject> clientside_gco;
		static GlobalControlObjectFactory globalcontrolobject_factory;
		static std::mutex gco_mutex;
		static std::vector<std::weak_ptr<PlaybackStateListener> > playback_state_listeners;

		static void insert_new_playback_state_listener_object(std::shared_ptr<PlaybackStateListener> listener_object);

		int bpm, lpb;
		bool is_playing = false, is_recording = false;

		std::map<std::string, std::vector<int> > scale2keys;
		std::vector<std::string> scale_names;
	};

	class SampleBank : public BaseObject {
	private:
		class SampleBankFactory : public Factory {
		public:
			SampleBankFactory();

			virtual std::shared_ptr<BaseObject> create(const Message &serialized) override;
			virtual std::shared_ptr<BaseObject> create(int32_t new_obj_id) override;
		};
		static SampleBankFactory samplebank_factory;

		std::map<int, std::string> sample_names;
		std::string bank_name;

		static std::mutex clientside_samplebanks_mutex;
		static std::map<std::string, std::weak_ptr<SampleBank> > clientside_samplebanks;

		template <class SerderClassT>
		void serderize_samplebank(SerderClassT &serder); // serder is an either an ItemSerializer or ItemDeserializer object.

	public:
		class NoSampleLoaded : public std::runtime_error {
		public:
			NoSampleLoaded() : runtime_error("No sample loaded at that position in the bank.") {}
			virtual ~NoSampleLoaded() {}
		};

		class NoSuchSampleBank : public std::runtime_error {
		public:
			NoSuchSampleBank() : runtime_error("No sample bank that is matching the queried name exists.") {}
			virtual ~NoSuchSampleBank() {}
		};

		SampleBank(const Factory *factory, const Message &serialized); // create client side HandleList
		SampleBank(int32_t new_obj_id, const Factory *factory); // create server side HandleList

		std::string get_name();
		std::string get_sample_name(int bank_index);

		void load_sample(int bank_index, const std::string &serverside_file_path);

		virtual void post_constructor_client() override;
		virtual void process_message(Server *context, MessageHandler *src, const Message &msg) override;
		virtual void process_message(Client *context, const Message &msg) override;
		virtual void serialize(std::shared_ptr<Message> &target) override;
		virtual void on_delete(Client *context) override;

		static std::shared_ptr<SampleBank> get_bank(const std::string name); // empty string or "<global>" => global SampleBank
	};

	class RIMachine : public BaseObject {
	public:
		enum PadEvent_t {
			ms_pad_press = 0,
			ms_pad_slide = 1,
			ms_pad_release = 2,
			ms_pad_no_event = 3
		};
		enum PadMode_t {
			pad_normal = 0,
			pad_arpeggiator = 1
		};

		enum ChordMode_t {
			chord_off = 0,
			chord_triad = 1
		};

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

		class RIController {
		public:
			class NoSuchEnumValue : public std::runtime_error {
			public:
				NoSuchEnumValue() : runtime_error("No such enum value in RIController.") {}
				virtual ~NoSuchEnumValue() {}
			};

			enum Type {
				ric_int = 0,
				ric_float = 1,
				ric_bool = 2,
				ric_string = 3,
				ric_enum = 4, // integer values map to a name
				ric_sigid = 5 // integer values map to sample bank index
			};

			RIController(int ctrl_id, Machine::Controller *ctrl);
			RIController(std::function<
					     void(std::function<void(std::shared_ptr<Message> &msg_to_send)> )
					     >  _send_obj_message,
				     const std::string &serialized);

			std::string get_name(); // name of the control
			std::string get_title(); // user displayable title

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

			std::string get_value_name(int val);

			void set_value(int val);
			void set_value(float val);
			void set_value(bool val);
			void set_value(const std::string &val);

			bool has_midi_controller(int &coarse_controller, int &fine_controller);

			std::string get_serialized_controller();
		private:
			std::function<
			void(std::function<void(std::shared_ptr<Message> &msg_to_send)> )
			>  send_obj_message;

			template <class SerderClassT>
			void serderize_controller(SerderClassT &serder); // serder is an either an ItemSerializer or ItemDeserializer object.

			int ctrl_id = -1;

			int ct_type = ric_int; //using enum Type values, but need to be explicitly stored as an integer
			std::string name, title;

			struct data_f {
				float min, max, step, value;
			};
			struct data_i {
				int min, max, step, value;
			};
			union {
				data_f f;
				data_i i;
			} data;

			std::string str_data = "";
			bool bl_data = false;

			std::map<int, std::string> enum_names;

			int coarse_controller = -1, fine_controller = -1;
		};

		/// get a hint about what this machine is (for example, "effect" or "generator")
		std::string get_hint();

		/// Returns a set of controller groups
		std::vector<std::string> get_controller_groups();
		/// Returns the set of all controller names in a given group
		std::vector<std::string> get_controller_names(const std::string &group_name);
		/// Return a RIController object
		std::shared_ptr<RIController> get_controller(const std::string &controller_name);

		std::string get_name();
		std::string get_sibling_name();
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
		std::set<std::string> available_midi_controllers();
		int get_nr_of_loops();
		void pad_export_to_loop(int loop_id = RI_LOOP_NOT_SET);
		void pad_set_octave(int octave);
		void pad_set_scale(int scale_index);
		void pad_set_record(bool record);
		void pad_set_quantize(bool do_quantize);
		void pad_assign_midi_controller(const std::string &controller);
		void pad_set_chord_mode(ChordMode_t chord_mode);
		void pad_set_arpeggio_pattern(const std::string &arp_pattern);
		void pad_clear();
		void pad_enqueue_event(int finger, PadEvent_t event_type, float ev_x, float ev_y);
		void enqueue_midi_data(size_t len, const char* data);

	public:
		virtual void post_constructor_client(); // called after the constructor has been called
		virtual void process_message(Server *context, MessageHandler *src, const Message &msg); // server side processing
		virtual void process_message(Client *context, const Message &msg); // client side processing
		virtual void serialize(std::shared_ptr<Message> &target);
		virtual void on_delete(Client *context); // called on client side when it's about to be deleted

	private:
		class ServerSideControllerContainer : public IDAllocator {
		public:
			std::map<int32_t, Machine::Controller *> id2ctrl; // map Controller id to Machine::Controller

			~ServerSideControllerContainer() {
				for(auto i2ct : id2ctrl) {
					delete i2ct.second;
				}
			}

		};

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
		std::vector<std::string> controller_groups;

		// map a weak ptr representing the ClientAgent to a ServerSideControllerContainer
		std::map<std::weak_ptr<MessageHandler>,
			 std::shared_ptr<ServerSideControllerContainer>,
			 std::owner_less<std::weak_ptr<MessageHandler> >
			 > client2ctrl_container;

		std::string name, sibling;
		std::string type;
		Machine *real_machine_ptr = nullptr;
		double xpos, ypos;
		std::set<std::string> midi_controllers;

		// clean up Controller objects for disconnected clients
		void cleanup_stray_controllers();

		// serialize a Controller into a std::string
		std::string serialize_controller(Machine::Controller *ctrl);

		// returns a serialized controller representation
 		std::string process_get_ctrl_message(const std::string &ctrl_name, MessageHandler *src);

		void process_setctrl_val_message(MessageHandler *src, const Message &msg);

		void process_attach_message(Context *context, const Message &msg);
		void process_detach_message(Context *context, const Message &msg);

		void parse_serialized_midi_ctrl_list(std::string serialized);
		void parse_serialized_connections_data(std::string serialized);
		void call_listeners(std::function<void(std::shared_ptr<RIMachineStateListener> listener)> callback);

		// used on client side when somethings changed server side
		std::set<std::weak_ptr<RIMachineStateListener>,
			 std::owner_less<std::weak_ptr<RIMachineStateListener> > >state_listeners;

		friend class Client;
		static void register_ri_machine_set_listener(std::weak_ptr<RIMachineSetListener> listener);
	};

	class Client : public Context, public MessageHandler {
	private:
		std::map<int32_t, std::shared_ptr<BaseObject> > all_objects;

		int32_t client_id = -1;

		// code for handling messages waiting for a reply
		int32_t next_reply_id = 0;
		std::map<int32_t, std::shared_ptr<Message> > msg_waiting_for_reply;

		std::map<std::string, std::string> handle2hint;

		asio::ip::tcp::resolver resolver;
		asio::ip::udp::resolver udp_resolver;
		std::function<void()> disconnect_callback;
		std::function<void(const std::string &fresp)> failure_response_callback;

		Client(const std::string &server_host,
		       int server_port,
		       std::function<void()> disconnect_callback,
		       std::function<void(const std::string &failure_response)> failure_response_callback);

		void flush_all_objects();

		static std::shared_ptr<Client> client;
		static std::mutex client_mutex;

	public: // public singleton interface
		static void start_client(const std::string &server_host, int server_port,
					 std::function<void()> disconnect_callback,
					 std::function<void(const std::string &failure_response)> failure_response_callback);
		static void disconnect();

		static void register_ri_machine_set_listener(std::weak_ptr<RIMachine::RIMachineSetListener> ri_mset_listener);

	public:
		virtual void on_message_received(const Message &msg) override;
		virtual void on_connection_dropped() override;

		virtual void distribute_message(std::shared_ptr<Message> &msg, bool via_udp) override;
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

		int32_t last_obj_id; // I am making an assumption here that last_obj_id will not be counted up more than 1/sec. This gives that time until overflow for a session will be more than 20000 days. If this assumption does not hold, an error state will be communicated to the user.

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
			int32_t id;
			Server *server;

			void send_handler_message();
		public:
			ClientAgent(int32_t id, asio::ip::tcp::socket _socket, Server *server);
			void start();

			void disconnect();

			int32_t get_id() { return id; }

			virtual void on_message_received(const Message &msg) override;
			virtual void on_connection_dropped() override;
		};
		friend class ClientAgent;

		typedef std::shared_ptr<ClientAgent> ClientAgent_ptr;
		std::map<int32_t, ClientAgent_ptr> client_agents;
		int32_t next_client_agent_id = 0;

		asio::ip::tcp::acceptor acceptor;
		asio::ip::tcp::socket acceptor_socket;
		int current_port;

		std::shared_ptr<asio::ip::udp::socket> udp_socket;
		Message udp_read_msg;
		asio::ip::udp::endpoint udp_endpoint;

		void do_accept();
		void drop_client(std::shared_ptr<ClientAgent> client_agent);
		void do_udp_receive();

		void disconnect_clients();
		void create_service_objects();
		void add_create_object_header(std::shared_ptr<Message> &target, std::shared_ptr<BaseObject> obj);
		void add_destroy_object_header(std::shared_ptr<Message> &target, std::shared_ptr<BaseObject> obj);
		void send_protocol_version_to_new_client(std::shared_ptr<MessageHandler> client_agent);
		void send_client_id_to_new_client(std::shared_ptr<ClientAgent> client_agent);
		void send_all_objects_to_new_client(std::shared_ptr<MessageHandler> client_agent);

		int get_port();

		Server(const asio::ip::tcp::endpoint& endpoint);

		void route_incomming_message(ClientAgent *src, const Message &msg);

		static std::shared_ptr<Server> server;
		static std::mutex server_mutex;

	public:
		static int start_server(); // will start a server and return the port number. If the server is already started, it will just return the port number.
		static bool is_running();
		static void stop_server();

		virtual void distribute_message(std::shared_ptr<Message> &msg, bool via_udp = false) override;
		virtual std::shared_ptr<BaseObject> get_object(int32_t objid) override;
	};

};

#endif
