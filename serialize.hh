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

#ifndef __SERIALIZE_HH
#define __SERIALIZE_HH

#include <string>
#include <utility>
#include <map>
#include <sstream>
#include <memory>

#include "common.hh"

#include "satan_error.hh"

namespace Serialize {

/***************************
 *
 *  Decoder/Encoder functions
 *
 ***************************/

	std::string decode_string(const std::string &encoded);
	std::string encode_string(const std::string &uncoded);

	class ItemSerializer {
	protected:
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

		template <class SerializableT>
		void process(std::shared_ptr<SerializableT> &t);

		template <class ArrayT>
		void process(size_t array_size, ArrayT* elements);

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

	template <class SerializableT>
	void ItemSerializer::process(std::shared_ptr<SerializableT> &t) {
		ItemSerializer subser;
		t->serderize(subser);

		result_stream << SerializableT::serialize_identifier << ";" << encode_string(subser.result()) << ";";
	}

	template <class ArrayT>
	void ItemSerializer::process(size_t array_size, ArrayT* elements) {
		ItemSerializer subser;

		for(size_t k = 0; k < array_size; k++) {
			subser.process(elements[k]);
		}

		result_stream << "array;" << encode_string(subser.result()) << ";";
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

	protected:
		std::istringstream is;

		inline void verify_type(const std::string &type_identifier) {

			std::string type;
			std::getline(is, type, ';');
			if(is.eof()) throw UnexpectedEndWhenDeserializing();

			if(type == type_identifier) return;

			SATAN_ERROR(" expected type [%s] but found [%s]\n", type_identifier.c_str(), type.c_str());

			throw UnexpectedTypeWhenDeserializing();
		}

		inline void get_string(std::string &val) {

			std::getline(is, val, ';');
			if(is.eof()) throw UnexpectedEndWhenDeserializing();

		}

#define __ITD_GET_STRING(name) std::string name; get_string(name);
	public:
		ItemDeserializer(const std::string &input) : is(decode_string(input)) {
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

		template <class SerializableT>
		void process(std::shared_ptr<SerializableT> &t);

		template <class ArrayT>
		void process(size_t array_size, ArrayT* elements);

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

		ItemDeserializer subdeser(decode_string(subserialized));

		subdeser.process(pair.first);
		subdeser.process(pair.second);
	}

	template <typename U, typename V>
	void ItemDeserializer::process(std::map<U,V> &elements) {
		verify_type("container");

		__ITD_GET_STRING(subserialized);

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

	template <class SerializableT>
	void ItemDeserializer::process(std::shared_ptr<SerializableT> &t) {
		verify_type(SerializableT::serialize_identifier);

		__ITD_GET_STRING(subserialized);

		ItemDeserializer subdeser(Serialize::decode_string(subserialized));

		t = std::make_shared<SerializableT>();
		t->serderize(subdeser);
	}

	template <class ArrayT>
	void ItemDeserializer::process(size_t array_size, ArrayT* elements) {
		verify_type("array");
		__ITD_GET_STRING(subserialized);

		ItemDeserializer subdeser(decode_string(subserialized));
		size_t index = 0;

		while((index < array_size) && (!subdeser.eof())) {
			try {
				subdeser.process(elements[index++]);
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

		ItemDeserializer subdeser(decode_string(subserialized));

		elements.clear();

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
};

#endif
