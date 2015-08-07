#
# VuKNOB - Based on SATAN
# Copyright (C) 2013 by Anton Persson
#
# SATAN, Signal Applications To Any Network
# Copyright (C) 2003 by Anton Persson & Johan Thim
# Copyright (C) 2005 by Anton Persson
# Copyright (C) 2006 by Anton Persson & Ted Björling
# Copyright (C) 2011 by Anton Persson
#
# About SATAN:
#   Originally developed as a small subproject in
#   a course about applied signal processing.
# Original Developers:
#   Anton Persson
#   Johan Thim
#
# http://www.733kru.org/
#
# This program is free software; you can redistribute it and/or modify it under the terms of
# the GNU General Public License version 2; see COPYING for the complete License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program;
# if not, write to the
# Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#

LOCAL_PATH := $(call my-dir)

LIBSVGANDROID_DIRECTORY := ../libsvgandroid
LIBKAMOFLAGE_DIRECTORY := ../libkamoflage
LIBVUKNOB_DIRECTORY := ../libvuknob

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cc
LOCAL_MODULE    := pathvariable
LOCAL_SRC_FILES := pathvariable.cc
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libsvgandroid
LOCAL_SRC_FILES := $(LIBSVGANDROID_DIRECTORY)/export/armeabi/libsvgandroid.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libkamoflage
LOCAL_SRC_FILES := $(LIBKAMOFLAGE_DIRECTORY)/export/armeabi/libkamoflage.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvuknob
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_CFLAGS += -D__SATAN_USES_FLOATS -mfloat-abi=softfp -mfpu=neon
	LOCAL_SRC_FILES := $(LIBVUKNOB_DIRECTORY)/export/armeabi-v7a/libvuknob.so
else
	LOCAL_CFLAGS += -D__SATAN_USES_FXP -DFIXED_POINT=32
	LOCAL_SRC_FILES := $(LIBVUKNOB_DIRECTORY)/export/armeabi/libvuknob.so
endif # TARGET_ARCH_ABI == armeabi-v7a

include $(PREBUILT_SHARED_LIBRARY)

include $(LOCAL_PATH)/dynlib_auto_Android.mk

include $(CLEAR_VARS)
LOCAL_MODULE := libjack
LOCAL_SRC_FILES := $(LIBVUKNOB_DIRECTORY)/prereqs/lib/libjack.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := wave
LOCAL_SRC_FILES := ../vuknob/Android/libs/armeabi/libwave.so
include $(PREBUILT_SHARED_LIBRARY)

$(call import-module,cpufeatures)
