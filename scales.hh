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

#ifndef SCALES_HH
#define SCALES_HH

#include "remote_interface.hh"
#include "serialize.hh"

#include "satan_error.hh"

class Scales : public RemoteInterface::SimpleBaseObject {
public:
	static constexpr const char* FACTORY_NAME		= "Scales";

	class ServersideOnlyFunction : public std::runtime_error {
	public:
		ServersideOnlyFunction() : runtime_error("Tried to call serverside function from clientside.") {}
		virtual ~ServersideOnlyFunction() {}
	};

	class ScalesFactory : public Factory {
	private:
		static std::shared_ptr<Scales> clientside_scales_object;
		static std::mutex clientside_mtx;
		volatile static bool clientside_obj_created;

		static std::shared_ptr<Scales> serverside_scales_object;
		static std::mutex serverside_mtx;
		volatile static bool serverside_obj_created;

	public:

		ScalesFactory() : Factory(FACTORY_NAME, true) {}

		virtual std::shared_ptr<BaseObject> create(const RemoteInterface::Message &serialized) override {
			std::lock_guard<std::mutex> lck(clientside_mtx);
			if(clientside_obj_created) throw StaticSingleObjectAlreadyCreated();
			clientside_obj_created = true;
			clientside_scales_object = std::make_shared<Scales>(this, serialized);
			return clientside_scales_object;
		}

		virtual std::shared_ptr<BaseObject> create(int32_t new_obj_id) override {
			std::lock_guard<std::mutex> lck(serverside_mtx);
			if(serverside_obj_created) throw StaticSingleObjectAlreadyCreated();
			serverside_obj_created = true;
			serverside_scales_object = std::make_shared<Scales>(new_obj_id, this);
			return serverside_scales_object;
		}

		static std::shared_ptr<Scales> get_clientside_scales_object() {
			if(clientside_obj_created)
				return clientside_scales_object;

			std::shared_ptr<Scales> empty;
			return empty;
		}

		static std::shared_ptr<Scales> get_serverside_scales_object() {
			if(serverside_obj_created)
				return serverside_scales_object;

			std::shared_ptr<Scales> empty;
			return empty;
		}
	};

private:

	static constexpr const char* CMD_GET_NR_SCALES		= "getnrs";
	static constexpr const char* CMD_GET_SCALE_NAMES	= "getnames";
	static constexpr const char* CMD_GET_SCALE_KEYS		= "getskeys";
	static constexpr const char* CMD_GET_SCALE_KEYSN       	= "getskeysn";
	static constexpr const char* CMD_GET_CUSTOM_SCALE_KEY	= "getcsk";
	static constexpr const char* CMD_SET_CUSTOM_SCALE_KEY	= "setcsk";

	void handle_get_nr_scales(RemoteInterface::Context *context, RemoteInterface::MessageHandler *src,
				  const RemoteInterface::Message& msg);
	void handle_get_scale_names(RemoteInterface::Context *context, RemoteInterface::MessageHandler *src,
				    const RemoteInterface::Message& msg);
	void handle_get_scale_keys(RemoteInterface::Context *context, RemoteInterface::MessageHandler *src,
				   const RemoteInterface::Message& msg);
	void handle_get_scale_keysn(RemoteInterface::Context *context, RemoteInterface::MessageHandler *src,
				    const RemoteInterface::Message& msg);
	void handle_get_custom_scale_key(
		RemoteInterface::Context *context, RemoteInterface::MessageHandler *src, const RemoteInterface::Message& msg);
	void handle_set_custom_scale_key(
		RemoteInterface::Context *context, RemoteInterface::MessageHandler *src, const RemoteInterface::Message& msg);

	void register_handlers() {
		register_handler(CMD_GET_NR_SCALES,
				 std::bind(&Scales::handle_get_nr_scales, this,
					   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		register_handler(CMD_GET_SCALE_NAMES,
				 std::bind(&Scales::handle_get_scale_names, this,
					   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		register_handler(CMD_GET_SCALE_KEYS,
				 std::bind(&Scales::handle_get_scale_keys, this,
					   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		register_handler(CMD_GET_SCALE_KEYSN,
				 std::bind(&Scales::handle_get_scale_keysn, this,
					   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		register_handler(CMD_GET_CUSTOM_SCALE_KEY,
				 std::bind(&Scales::handle_get_custom_scale_key, this,
					   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		register_handler(CMD_SET_CUSTOM_SCALE_KEY,
				 std::bind(&Scales::handle_set_custom_scale_key, this,
					   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}

	/* serverside data */
	struct Scale {
		static constexpr const char* serialize_identifier = "Scales::Scale";

		std::string name;
		int keys[7];

		template <class SerderClassT>
		void serderize(SerderClassT& iserder) {
			iserder.process(name);
			iserder.process(7, keys);
		}
	};

	std::shared_ptr<Scale> custom_scale;

	std::vector<std::shared_ptr<Scale> > scales;
	bool scales_initialized = false;
	void initialize_scales();

public:

	Scales(const Factory *factory, const RemoteInterface::Message &serialized);
	Scales(int32_t new_obj_id, const Factory *factory);

	const char* get_key_text(int key);

	int get_number_of_scales();
	std::vector<std::string> get_scale_names();

	std::vector<int> get_scale_keys(const std::string &scale_name);
	std::vector<int> get_scale_keys(int index);
	void get_scale_keys(int index, int* result); // serverside only

	int get_custom_scale_key(int offset);
	void set_custom_scale_key(int offset, int note);

	static std::shared_ptr<Scales> get_scales_object() {
		return ScalesFactory::get_clientside_scales_object();
	}

	static std::shared_ptr<Scales> get_scales_object_serverside() {
		return ScalesFactory::get_serverside_scales_object();
	}
};

#endif
