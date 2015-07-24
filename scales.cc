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

#include "satan_project_entry.hh"
#define SCALES_PROJECT_INTERFACE_LEVEL 2

#include "scales.hh"

static bool scales_library_initialized = false;

struct ScaleEntry {
	int offset;
	const char *name;
	int keys[21];
};

static struct ScaleEntry scales_library[] = {
	{ 0x0, "C- ", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x0, "C-m", {0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
		       0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x0, "C#m", {0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
		       0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x1, "D- ", {0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
		       0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x1, "D-m", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x1, "Db ", {0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
		       0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x1, "D#m", {0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
		       0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x2, "E- ", {0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
		       0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x2, "E-m", {0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x2, "Eb ", {0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
		       0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x3, "F- ", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x3, "F-m", {0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
		       0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x3, "F# ", {0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
		       0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x3, "F#m", {0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
		       0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x4, "G- ", {0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x4, "G-m", {0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
		       0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x4, "G#m", {0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
		       0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x5, "A- ", {0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
		       0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x5, "A-m", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x5, "Ab ", {0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
		       0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x6, "B- ", {0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
		       0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x6, "B-m", {0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
		       0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x6, "Bb ", {0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
		       0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
	{ 0x6, "Bbm", {0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
		       0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
		       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
		} },
};

static void initialize_scale(ScaleEntry *s) {
	for(int x = 0; x < 7; x++) {
		s->keys[x +  7] = s->keys[x] + 12;
		s->keys[x + 14] = s->keys[x] + 24;
	}
}

static int max_scales = -1;

static inline void initialize_scales_library() {
	if(scales_library_initialized) return;

	scales_library_initialized = true;

	int k;

	max_scales = sizeof(scales_library) / sizeof(ScaleEntry);

	for(k = 0; k < max_scales; k++) {
		initialize_scale(&(scales_library[k]));
	}
}

static const char *key_text[] = {
	"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
};

void Scales::initialize_scales() {
	if(scales_initialized) return;
	scales_initialized = true;

	initialize_scales_library();

	scales.clear();

	// insert standard scales
	for(auto k = 0; k < max_scales; k++) {
		auto scl = std::make_shared<Scale>();
		scl->name = scales_library[k].name;
		for(auto l = 0; l < 7; l++) {
			scl->keys[l] = scales_library[k].keys[l + scales_library[k].offset];
		}
		scales.push_back(scl);
	}

	// insert custom scale
	{
		auto scl = std::make_shared<Scale>();

		scl->name = "CS1";
		scl->keys[0] =  0;
		scl->keys[1] =  2;
		scl->keys[2] =  4;
		scl->keys[3] =  5;
		scl->keys[4] =  7;
		scl->keys[5] =  9;
		scl->keys[6] = 11;

		custom_scale = scl;
		scales.push_back(scl);
	}
}

Scales::Scales(const Factory *factory, const RemoteInterface::Message &serialized) : SimpleBaseObject(factory, serialized) {
	register_handlers();
}

Scales::Scales(int32_t new_obj_id, const Factory *factory) : SimpleBaseObject(new_obj_id, factory) {
	register_handlers();
	initialize_scales();
}

void Scales::handle_get_nr_scales(
	RemoteInterface::Context *context, RemoteInterface::MessageHandler *src, const RemoteInterface::Message& msg)
{
	if(is_server_side()) {
		initialize_scales();

		std::shared_ptr<RemoteInterface::Message> reply = context->acquire_reply(msg);
		reply->set_value("nrscl", std::to_string(scales.size()));
		src->deliver_message(reply);
	}
}

void Scales::handle_get_scale_names(
	RemoteInterface::Context *context, RemoteInterface::MessageHandler *src, const RemoteInterface::Message& msg)
{
	if(is_server_side()) {
		initialize_scales();

		Serialize::ItemSerializer iser;
		iser.process(scales);

		std::shared_ptr<RemoteInterface::Message> reply = context->acquire_reply(msg);
		reply->set_value("scales", iser.result());
		src->deliver_message(reply);
	}
}

void Scales::handle_get_scale_keys(
	RemoteInterface::Context *context, RemoteInterface::MessageHandler *src, const RemoteInterface::Message& msg)
{
	if(is_server_side()) {
		initialize_scales();

		auto scale_name = msg.get_value("name");
		int keys[] = {0, 2, 4, 5, 7, 9, 11};

		for(auto scale : scales) {
			if(scale->name == scale_name) {
				for(auto k = 0; k < 7; k++)
					keys[k] = scale->keys[k];
			}
		}

		Serialize::ItemSerializer iser;
		iser.process(keys);

		std::shared_ptr<RemoteInterface::Message> reply = context->acquire_reply(msg);
		reply->set_value("keys", iser.result());
		src->deliver_message(reply);
	}
}

void Scales::handle_get_scale_keysn(
	RemoteInterface::Context *context, RemoteInterface::MessageHandler *src, const RemoteInterface::Message& msg)
{
	if(is_server_side()) {
		initialize_scales();

		int scale_index = std::stoi(msg.get_value("index"));
		int keys[] = {0, 2, 4, 5, 7, 9, 11};

		for(auto k = 0; k < 7; k++)
			keys[k] = scales[scale_index]->keys[k];

		Serialize::ItemSerializer iser;
		iser.process(keys);

		std::shared_ptr<RemoteInterface::Message> reply = context->acquire_reply(msg);
		reply->set_value("keys", iser.result());
		src->deliver_message(reply);
	}
}

void Scales::handle_get_custom_scale_key(
	RemoteInterface::Context *context, RemoteInterface::MessageHandler *src, const RemoteInterface::Message& msg)
{
	if(is_server_side()) {
		initialize_scales();

		int offset = std::stoi(msg.get_value("offset"));

		std::shared_ptr<RemoteInterface::Message> reply = context->acquire_reply(msg);
		reply->set_value("key", std::to_string(custom_scale->keys[offset]));
		src->deliver_message(reply);
	}
}

void Scales::handle_set_custom_scale_key(
	RemoteInterface::Context *context, RemoteInterface::MessageHandler *src, const RemoteInterface::Message& msg)
{
	if(is_server_side()) {
		initialize_scales();

		int offset = std::stoi(msg.get_value("offset"));
		int key = std::stoi(msg.get_value("key"));

		custom_scale->keys[offset] = key;
	}
}

const char* Scales::get_key_text(int key) {
	return key_text[key % 12];
}

int Scales::get_number_of_scales() {
	int retval = 0;

	send_message_to_server(
		CMD_GET_NR_SCALES,

		[](std::shared_ptr<RemoteInterface::Message> &msg2send) {},

		[this, &retval](const RemoteInterface::Message *reply_message) {
			if(reply_message) {
				retval = std::stoi(reply_message->get_value("nrscl"));
			}
		}
		);

	return retval;
}

std::vector<std::string> Scales::get_scale_names() {
	std::vector<std::string> retval;

	send_message_to_server(
		CMD_GET_SCALE_NAMES,

		[](std::shared_ptr<RemoteInterface::Message> &msg2send) {},

		[this, &retval](const RemoteInterface::Message *reply_message) {
			if(reply_message) {
				Serialize::ItemDeserializer serder(reply_message->get_value("scales"));
				serder.process(scales);

				for(auto scale : scales) {
					retval.push_back(scale->name);
				}
			}
		}
		);

	return retval;
}

std::vector<int> Scales::get_scale_keys(const std::string &scale_name) {
	std::vector<int> retval = {0, 2, 4, 5, 7, 9, 11};

	send_message_to_server(
		CMD_GET_SCALE_KEYS,

		[&scale_name](std::shared_ptr<RemoteInterface::Message> &msg2send) {
			msg2send->set_value("name", scale_name);
		},

		[this, &retval](const RemoteInterface::Message *reply_message) {
			if(reply_message) {
				Serialize::ItemDeserializer serder(reply_message->get_value("keys"));
				serder.process(retval);
			}
		}
		);

	return retval;
}

std::vector<int> Scales::get_scale_keys(int index) {
	std::vector<int> retval = {0, 2, 4, 5, 7, 9, 11};

	send_message_to_server(
		CMD_GET_SCALE_KEYSN,

		[index](std::shared_ptr<RemoteInterface::Message> &msg2send) {
			msg2send->set_value("index", std::to_string(index));
		},

		[this, &retval](const RemoteInterface::Message *reply_message) {
			if(reply_message) {
				int keys[7];

				Serialize::ItemDeserializer serder(reply_message->get_value("keys"));
				serder.process(7, keys);
				retval.clear();
				for(auto k = 0; k < 7; k++)
					retval.push_back(keys[k]);
			}
		}
		);

	return retval;
}

void Scales::get_scale_keys(int index, int* result) {
	if(is_client_side())
		throw ServersideOnlyFunction();

	auto scale = scales[index];

	for(auto k = 0; k < 7; k++) {
		result[k] = scale->keys[k];
	}
}

int Scales::get_custom_scale_key(int offset) {
	int retval = 0;

	send_message_to_server(
		CMD_GET_CUSTOM_SCALE_KEY,

		[offset](std::shared_ptr<RemoteInterface::Message> &msg2send) {
			msg2send->set_value("offset", std::to_string(offset));
		},

		[this, &retval](const RemoteInterface::Message *reply_message) {
			if(reply_message) {
				retval = std::stoi(reply_message->get_value("key"));
			}
		}
		);

	return retval;
}

void Scales::set_custom_scale_key(int offset, int key) {
	send_message_to_server(
		CMD_SET_CUSTOM_SCALE_KEY,

		[offset, key](std::shared_ptr<RemoteInterface::Message> &msg2send) {
			msg2send->set_value("offset", std::to_string(offset));
			msg2send->set_value("key", std::to_string(key));
		}

		);
}

class ScalesProjectEntry : public SatanProjectEntry {
private:
	void parse_scale(KXMLDoc &scl) {
		if(auto scalo = Scales::get_scales_object()) {
			unsigned int k, k_max = 0;

			try {
				k_max = scl["k"].get_count();
			} catch(jException e) { k_max = 0;}

			for(k = 0; k < k_max; k++) {
				auto v = scl["k"][k];
				int offset, key;

				KXML_GET_NUMBER(v, "o", offset, 0);
				KXML_GET_NUMBER(v, "n", key, 0);

				scalo->set_custom_scale_key(offset, key);
			}
		}
	}

public:
	ScalesProjectEntry() : SatanProjectEntry("scalesentry", 1, SCALES_PROJECT_INTERFACE_LEVEL) {}

	virtual std::string get_xml_attributes() override {
		return "";
	}

	virtual void generate_xml(std::ostream &output) override {
		if(auto scalo = Scales::get_scales_object()) {
			output << "\n";

			output << "<scale name=\""
			       << "CS1"
			       << "\" >\n";

			for(int k = 0; k < 7; k++) {
				output << "<k o=\""
				       << k
				       << "\" n=\""
				       << scalo->get_custom_scale_key(k)
				       << "\" />\n";
			}
			output << "</scale>\n";
		}
	}

	virtual void parse_xml(int project_interface_level, KXMLDoc &xml_node) override {
		unsigned int k, k_max = 0;

		try {
			k_max = xml_node["scale"].get_count();
		} catch(jException e) { k_max = 0;}

		for(k = 0; k < k_max; k++) {
			auto scale_node = xml_node["scale"][k];
			parse_scale(scale_node);
		}
	}

	virtual void set_defaults() override {
		if(auto scalo = Scales::get_scales_object()) {
			scalo->set_custom_scale_key(0,  0);
			scalo->set_custom_scale_key(1,  2);
			scalo->set_custom_scale_key(2,  4);
			scalo->set_custom_scale_key(3,  5);
			scalo->set_custom_scale_key(4,  7);
			scalo->set_custom_scale_key(5,  9);
			scalo->set_custom_scale_key(6, 11);
		}
	}
};

static ScalesProjectEntry this_will_register_us_as_a_project_entry;
static Scales::ScalesFactory this_will_register_us_as_a_factory;

std::shared_ptr<Scales>		Scales::ScalesFactory::clientside_scales_object;
std::mutex			Scales::ScalesFactory::clientside_mtx;
volatile bool			Scales::ScalesFactory::clientside_obj_created = false;
std::shared_ptr<Scales>		Scales::ScalesFactory::serverside_scales_object;
std::mutex			Scales::ScalesFactory::serverside_mtx;
volatile bool			Scales::ScalesFactory::serverside_obj_created = false;
