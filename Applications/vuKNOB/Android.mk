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

include ../../../config.mk

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	FLOAT_CFLAGS += -D__SATAN_USES_FLOATS -mfloat-abi=softfp -mfpu=neon
else
	FLOAT_CFLAGS += -D__SATAN_USES_FXP -DFIXED_POINT=32
endif # TARGET_ARCH_ABI == armeabi-v7a

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
LOCAL_CFLAGS += $(FLOAT_CFLAGS)
LOCAL_SRC_FILES := $(LIBVUKNOB_DIRECTORY)/export/$(TARGET_ARCH_ABI)/libvuknob.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvuknob_client
LOCAL_CFLAGS += $(FLOAT_CFLAGS)
LOCAL_SRC_FILES := $(LIBVUKNOB_DIRECTORY)/export/$(TARGET_ARCH_ABI)/libvuknob_client.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvuknob_server
LOCAL_CFLAGS += $(FLOAT_CFLAGS)
LOCAL_SRC_FILES := $(LIBVUKNOB_DIRECTORY)/export/$(TARGET_ARCH_ABI)/libvuknob_server.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvuknob_ui
LOCAL_CFLAGS += $(FLOAT_CFLAGS)
LOCAL_SRC_FILES := $(LIBVUKNOB_DIRECTORY)/export/$(TARGET_ARCH_ABI)/libvuknob_ui.so
include $(PREBUILT_SHARED_LIBRARY)

include $(LOCAL_PATH)/dynlib_auto_Android.mk

include $(CLEAR_VARS)
LOCAL_MODULE := libjack
LOCAL_SRC_FILES := $(LIBVUKNOB_DIRECTORY)/prereqs/lib/libjack.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := wave

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_SRC_FILES := $(LIBVUKNOB_DIRECTORY)/export/armeabi-v7a/libwave.so
else
	LOCAL_SRC_FILES := $(LIBVUKNOB_DIRECTORY)/export/armeabi/libwave.so
endif # TARGET_ARCH_ABI == armeabi-v7a

include $(PREBUILT_SHARED_LIBRARY)

$(call import-module,cpufeatures)
