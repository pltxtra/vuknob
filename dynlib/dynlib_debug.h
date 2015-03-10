/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2008 by Anton Persson
 * Copyright (C) 2011 by Anton Persson
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

#ifndef __GET_DYNLIB_DEBUG
#define __GET_DYNLIB_DEBUG

#define WHERESTR  "[file %s, line %d]: "
#define WHEREARG  __FILE__, __LINE__

#ifdef __DO_DYNLIB_DEBUG

#ifdef ANDROID

#include <android/log.h>
#define DYNLIB_DEBUG_(...)       __android_log_print(ANDROID_LOG_INFO, "SATAN_NDK", __VA_ARGS__)
#define DYNLIB_DEBUG(...)       __android_log_print(ANDROID_LOG_INFO, "SATAN_NDK", __VA_ARGS__)

#else

#include <stdio.h>
#define DYNLIB_DEBUG_(...)       printf(__VA_ARGS__)
#define DYNLIB_DEBUG(...)       printf(__VA_ARGS__)

#endif

#else
// disable debugging

#define DYNLIB_DEBUG_(...)
#define DYNLIB_DEBUG(_fmt, ...)

#endif

#endif

#ifdef ANDROID

#include <android/log.h>
#define DYNLIB_INFORM_(...)       __android_log_print(ANDROID_LOG_INFO, "SATAN_NDK", __VA_ARGS__)
#define DYNLIB_INFORM(...)       __android_log_print(ANDROID_LOG_INFO, "SATAN_NDK", __VA_ARGS__)

#else

#include <stdio.h>
#define DYNLIB_INFORM_(...)       printf(__VA_ARGS__)
#define DYNLIB_INFORM(...)       printf(__VA_ARGS__)

#endif
