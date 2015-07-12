/*
 * VuKNOB
 * (C) 2014 by Anton Persson
 *
 * http://www.vuknob.com/
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

#ifndef COMMON_HH
#define COMMON_HH

#include <set>
#include <stdint.h>
#include <stdexcept>

/***************************
 *
 * Useful macros and constants
 *
 ***************************/
#define QUALIFIED_DELETE(a) {if(a) { delete a; a = NULL; }}
#define INCHES_PER_FINGER 0.31f
#define FLING_DEACCELERATION 2000.0f
#define TRANSITION_TIME 0.125

#ifdef ANDROID
extern const char *KAMOFLAGE_ANDROID_ROOT_DIRECTORY;
#endif

class IDAllocator {
private:
	std::set<uint32_t> available_ids;

	uint32_t total_amount;
public:
	class IDFreedTwice : public std::runtime_error {
	public:
		IDFreedTwice() : runtime_error("Tried to free an ID twice.") {}
		virtual ~IDFreedTwice() {}
	};

	IDAllocator(uint32_t initial_size = 32) : total_amount(initial_size) {
		for(uint32_t x = 0; x < initial_size; x++) {
			available_ids.insert(x);
		}
	}

	uint32_t get_id() {
		uint32_t retval = total_amount;

		if(available_ids.size() > 0) {
			auto iter = available_ids.begin();
			retval = *iter;
			available_ids.erase(iter);
		} else total_amount++;

		return retval;
	}

	void free_id(uint32_t id) {
		if(available_ids.find(id) != available_ids.end())
			throw IDFreedTwice();

		available_ids.insert(id);
	}
};

class FingerScaler {
public:
	// stretch source_width/height_pixels up to width/height_pixels
	// however - limit the scaling to fit_width/height_fingers
	static double fit_to_fingers(double width_inches, double height_inches,
				     double width_pixels, double height_pixels,
				     int fit_width_fingers, int fit_height_fingers,
				     double source_width_pixels, double source_height_pixels) {
		double tmp;

		// calculate the canvas size in "fingers"
		tmp = width_inches / INCHES_PER_FINGER;
		int width_fingers = (int)tmp;
		tmp = height_inches / INCHES_PER_FINGER;
		int height_fingers = (int)tmp;

		// calculate the size of a finger in pixels
		tmp = width_pixels / ((double)width_fingers);
		double finger_width = tmp;
		tmp = height_pixels / ((double)height_fingers);
		double finger_height = tmp;

		// force scaling to fit into 5 by 7 "fingers"
		if(width_fingers > fit_width_fingers) width_fingers = fit_width_fingers;
		if(height_fingers > fit_height_fingers) height_fingers = fit_height_fingers;

		// calculate scaling factor
		double target_w = (double)width_fingers  * finger_width;
		double target_h = (double)height_fingers  * finger_height;

		double scale_w = target_w / (double)source_width_pixels;
		double scale_h = target_h / (double)source_height_pixels;

		return scale_w < scale_h ? scale_w : scale_h;
	}
};

/**************************
 *
 * Replace missing C++11 functionality
 * in the Android NDK (r10d latest checked)
 *
 */

#ifdef ANDROID

#include <string>
#include <sstream>
#include <stdlib.h>

namespace std {

	template <typename T>
	inline string to_string(T value) {
		ostringstream os ;
		os << value ;
		return os.str() ;
	}

	inline long stol(const string &str) {
		return atol(str.c_str());
	}

	inline int stoi(const string &str) {
		return atoi(str.c_str());
	}

	inline float stof(const string &str) {
		return strtof(str.c_str(), 0);
	}

	inline double stod(const string &str) {
		return strtod(str.c_str(), 0);
	}

	inline unsigned long stoul(const string &str) {
		return strtoul(str.c_str(), 0, 0);
	}
};

#endif

#endif // COMMON_HH
