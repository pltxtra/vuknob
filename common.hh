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
};

#endif
