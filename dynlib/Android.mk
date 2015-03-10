# SATAN, Signal Applications To Any Network
# Copyright (C) 2010 by Anton Persson
#
# This program is free software; you can redistribute it and/or modify it under the terms of
# the GNU General Public License as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
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

LOCAL_CPP_EXTENSION := .cc

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_CFLAGS += -O3 -D__SATAN_USES_FLOATS -DHAVE_CONFIG_H -Wall -I../ -mfloat-abi=softfp -mfpu=neon
else
LOCAL_CFLAGS += -O3 -D__SATAN_USES_FXP -DHAVE_CONFIG_H -Wall -I../
endif

#LOCAL_CFLAGS += -fstack-protector -fno-omit-frame-pointer -fno-optimize-sibling-calls -O0 -g

#LOCAL_CFLAGS += -DHEXTER_USE_FLOATING_POINT
LOCAL_LDLIBS += -ldl -llog

FRAMEWORK_SOURCES :=
#dynlib.h ../fixedpointmathcode.h  ../fixedpointmath.h  ../fixedpointmathlut.h

LOCAL_MODULE    := drumsampler
LOCAL_MODULE_FILENAME    := libdrumsampler
LOCAL_SRC_FILES := drumsampler.c  $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := grooveiator
LOCAL_MODULE_FILENAME    := libgrooveiator
LOCAL_SRC_FILES := grooveiator.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := filterbox
LOCAL_MODULE_FILENAME    := libfilterbox
LOCAL_SRC_FILES := filterbox.c  $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := mono2stereo
LOCAL_MODULE_FILENAME    := libmono2stereo
LOCAL_SRC_FILES := mono2stereo.c  $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := reverboz
LOCAL_MODULE_FILENAME    := libreverboz
LOCAL_SRC_FILES := reverboz.c  $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := sampler
LOCAL_MODULE_FILENAME    := libsampler
LOCAL_SRC_FILES := sampler.c  $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := ssynth
LOCAL_MODULE_FILENAME    := libssynth
LOCAL_SRC_FILES := ssynth.c  $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := stereospread
LOCAL_MODULE_FILENAME    := libstereospread
LOCAL_SRC_FILES := stereospread.c  $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := xecho
LOCAL_MODULE_FILENAME    := libxecho
LOCAL_SRC_FILES := xecho.c  $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := comprezza
LOCAL_MODULE_FILENAME    := libcomprezza
LOCAL_SRC_FILES := comprezza.c  $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := tcutter
LOCAL_MODULE_FILENAME    := libtcutter
LOCAL_SRC_FILES := tcutter.c  $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := eq10
LOCAL_MODULE_FILENAME    := libeq10
LOCAL_SRC_FILES := eq10.c  $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := rewerb
LOCAL_MODULE_FILENAME    := librewerb
LOCAL_SRC_FILES := rewerb.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := flanger
LOCAL_MODULE_FILENAME    := libflanger
LOCAL_SRC_FILES := flanger.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := chorus
LOCAL_MODULE_FILENAME    := libchorus
LOCAL_SRC_FILES := chorus.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := sima_overdrive
LOCAL_MODULE_FILENAME    := libsima_overdrive
LOCAL_SRC_FILES := sima_overdrive.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := europa4
LOCAL_MODULE_FILENAME    := libeuropa4
LOCAL_SRC_FILES := europa4.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := silverbox
LOCAL_MODULE_FILENAME    := libsilverbox
LOCAL_SRC_FILES := silverbox.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := subastard
LOCAL_MODULE_FILENAME    := libsubastard
LOCAL_SRC_FILES := subastard.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := digitar
LOCAL_MODULE_FILENAME    := libdigitar
LOCAL_SRC_FILES := digitar.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := delay
LOCAL_MODULE_FILENAME    := libdelay
LOCAL_SRC_FILES := delay.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := dx7
LOCAL_MODULE_FILENAME    := libdx7
LOCAL_SRC_FILES := \
hexter_src/dx7_voice.c \
hexter_src/dx7_voice_data.c \
hexter_src/dx7_voice_patches.c \
hexter_src/dx7_voice_tables.c \
hexter_src/dx7_voice_render.c \
hexter_src/hexter_synth.c \
dx7.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

LOCAL_MODULE    := vocoder
LOCAL_MODULE_FILENAME    := libvocoder
LOCAL_SRC_FILES := vocoder.c $(FRAMEWORK_SOURCES)
include $(BUILD_SHARED_LIBRARY)

#  only build OpenSL/ES if we have support for floating point - also build the fallback version...
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)

LOCAL_MODULE    := liveout_fallback
LOCAL_MODULE_FILENAME    := libliveout_fallback
LOCAL_SRC_FILES := riff_wave_output.c liveout.c  $(FRAMEWORK_SOURCES)
LOCAL_LDLIBS += -ldl -llog
include $(BUILD_SHARED_LIBRARY)

LOCAL_CFLAGS += -DUSE_OPEN_SL_ES -DHAVE_CONFIG_H -Wall -I../
LOCAL_MODULE    := liveout
LOCAL_MODULE_FILENAME    := libliveout
LOCAL_SRC_FILES := riff_wave_output.c liveout.c  $(FRAMEWORK_SOURCES)
LOCAL_LDLIBS += -ldl -llog -lOpenSLES
include $(BUILD_SHARED_LIBRARY)

else
# if we don't have armeabi-v7a support, just build the version without OpenSL/ES

LOCAL_MODULE    := liveout
LOCAL_MODULE_FILENAME    := libliveout
LOCAL_SRC_FILES := riff_wave_output.c liveout.c  $(FRAMEWORK_SOURCES)
LOCAL_LDLIBS += -ldl -llog
include $(BUILD_SHARED_LIBRARY)

endif