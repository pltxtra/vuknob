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

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

LOCAL_MODULE    := kamoflage

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
#        LOCAL_ARM_NEON  := true

	LOCAL_CFLAGS += -D__SATAN_USES_FLOATS -mfloat-abi=softfp -mfpu=neon

else
	LOCAL_CFLAGS += -D__SATAN_USES_FXP -DFIXED_POINT=32

endif # TARGET_ARCH_ABI == armeabi-v7a

LOCAL_CFLAGS += -DLIBSVG_EXPAT -DCONFIG_DIR=\"/\" -Ijni/libexpat/ -Ijni/libexpat/expat/ -Ijni/libpng/ -Ijni/libjpeg/ -Ijni/libsvg/ -I../../asio-1.10.2/include -DHAVE_CONFIG_H -DHAVE_EXPAT_CONFIG_H -DKAMOFLAGE_ANDROID_ROOT_DIRECTORY=\"/data/data/com.holidaystudios.satan\" -Ijni/libkamoflage/ -Wall -I../../libvorbis_new/include/ -DDEFAULT_PROJECT_SAVE_PATH=\"/mnt/sdcard/Satan/Projects\" -DDEFAULT_SATAN_ROOT=\"/mnt/sdcard/Satan\" -DDEFAULT_EXPORT_PATH=\"/mnt/sdcard/Satan/Export\"  #-D__DO_TIME_MEASURE
LOCAL_CPPFLAGS += -DASIO_STANDALONE -std=c++11 

# libjpeg stuff
LIBJPEG_SOURCES = libjpeg/jaricom.c libjpeg/jcapimin.c libjpeg/jcapistd.c libjpeg/jcarith.c \
	libjpeg/jccoefct.c libjpeg/jccolor.c \
        libjpeg/jcdctmgr.c libjpeg/jchuff.c libjpeg/jcinit.c libjpeg/jcmainct.c \
	libjpeg/jcmarker.c libjpeg/jcmaster.c \
        libjpeg/jcomapi.c libjpeg/jcparam.c libjpeg/jcprepct.c libjpeg/jcsample.c \
	libjpeg/jctrans.c libjpeg/jdapimin.c \
        libjpeg/jdapistd.c libjpeg/jdarith.c libjpeg/jdatadst.c libjpeg/jdatasrc.c \
	libjpeg/jdcoefct.c libjpeg/jdcolor.c \
        libjpeg/jddctmgr.c libjpeg/jdhuff.c libjpeg/jdinput.c libjpeg/jdmainct.c \
	libjpeg/jdmarker.c libjpeg/jdmaster.c \
        libjpeg/jdmerge.c libjpeg/jdpostct.c libjpeg/jdsample.c libjpeg/jdtrans.c \
	libjpeg/jerror.c libjpeg/jfdctflt.c \
        libjpeg/jfdctfst.c libjpeg/jfdctint.c libjpeg/jidctflt.c libjpeg/jidctfst.c \
	libjpeg/jidctint.c libjpeg/jquant1.c \
        libjpeg/jquant2.c libjpeg/jutils.c libjpeg/jmemmgr.c libjpeg/jmemnobs.c

# zlib stuff
ZLIB_SOURCES = zlib/adler32.c zlib/compress.c zlib/crc32.c zlib/deflate.c zlib/gzclose.c zlib/gzlib.c zlib/gzread.c \
        zlib/gzwrite.c zlib/infback.c zlib/inffast.c zlib/inflate.c zlib/inftrees.c zlib/trees.c zlib/uncompr.c zlib/zutil.c

# libpng stuff
LIBPNG_SOURCES = libpng/png.c libpng/pngset.c libpng/pngget.c libpng/pngrutil.c \
        libpng/pngtrans.c libpng/pngwutil.c \
        libpng/pngread.c libpng/pngrio.c libpng/pngwio.c libpng/pngwrite.c libpng/pngrtran.c \
        libpng/pngwtran.c libpng/pngmem.c libpng/pngerror.c libpng/pngpread.c \
        libpng/png.h libpng/pngconf.h libpng/pngpriv.h

# libexpat stuff
LIBEXPAT_SOURCES = libexpat/expat/xmlparse.c libexpat/expat/xmlrole.c libexpat/expat/xmltok.c libexpat/expat/xmltok_impl.c libexpat/expat/xmltok_ns.c

# libsvg stuff
LIBSVG_EXTRA_SOURCES = libsvg/svg_parser_expat.c libsvg/strhmap_cc.cc
LIBSVG_SOURCES = \
	libsvg/svg.c \
	libsvg/svg.h \
	libsvg/svgint.h \
	libsvg/svg_ascii.h \
	libsvg/svg_ascii.c \
	libsvg/svg_attribute.c \
	libsvg/svg_color.c \
	libsvg/svg_element.c \
	libsvg/svg_gradient.c \
	libsvg/svg_group.c \
	libsvg/svg_length.c \
	libsvg/svg_paint.c \
	libsvg/svg_parser.c \
	libsvg/svg_pattern.c \
	libsvg/svg_image.c \
	libsvg/svg_path.c \
	libsvg/svg_str.c \
	libsvg/svg_style.c \
	libsvg/svg_text.c \
	libsvg/svg_transform.c \
	libsvg/svg_version.h \
	$(LIBSVG_EXTRA_SOURCES)

# libsvg-android stuff
LIBSVG_ANDROID_SOURCES = \
	libsvg-android/svg_android.c \
	libsvg-android/svg_android_render.c \
	libsvg-android/svg_android_render_helper.c \
	libsvg-android/svg_android_state.c \
	libsvg-android/svg-android.h \
	libsvg-android/svg-android-internal.h

# libkamoflage stuff
LIBKAMOFLAGE_SOURCES = \
libkamoflage/kamogui.cc libkamoflage/kamogui.hh \
libkamoflage/kamogui_svg_canvas.cc \
libkamoflage/kamo_xml.cc libkamoflage/kamo_xml.hh \
libkamoflage/kamogui_scale_detector.cc libkamoflage/kamogui_scale_detector.hh \
libkamoflage/kamogui_fling_detector.cc libkamoflage/kamogui_fling_detector.hh

# jngldrum stuff
JNGLDRUM_SOURCES = \
jngldrum/jexception.cc jngldrum/jexception.hh \
jngldrum/jthread.cc jngldrum/jthread.hh \
jngldrum/jinformer.cc jngldrum/jinformer.hh

# kiss fft
KISS_FFT_SOURCES =\
kiss_fft.c kiss_fftr.c

#  kamoflage/satan stuff
LOCAL_STATIC_LIBRARIES := cpufeatures
LOCAL_LDLIBS += -ldl -llog ../../libvorbis_new/lib/libvorbis.a ../../libvorbis_new/lib/libogg.a ../../libvorbis_new/lib/libvorbisenc.a
LOCAL_SRC_FILES := \
$(LIBJPEG_SOURCES) $(LIBPNG_SOURCES) $(ZLIB_SOURCES) $(LIBEXPAT_SOURCES) $(LIBSVG_SOURCES) $(LIBSVG_ANDROID_SOURCES)  $(LIBKAMOFLAGE_SOURCES) $(JNGLDRUM_SOURCES) $(KISS_FFT_SOURCES) \
satan.cc \
android_java_interface.cc android_java_interface.hh \
static_signal_preview.cc static_signal_preview.hh \
wavloader.cc wavloader.hh \
signal.cc signal.hh \
machine.cc machine.hh machine_project_entry.cc \
dynamic_machine.cc dynamic_machine.hh \
load_ui.cc save_ui.cc \
new_project_ui.cc \
project_info_entry.cc \
controller_handler.cc \
controller_envelope.cc controller_envelope.hh \
general_tools.cc \
canvas_widget.cc canvas_widget.hh \
logo_screen.cc logo_screen.hh \
numeric_keyboard.cc numeric_keyboard.hh \
top_menu.cc top_menu.hh \
livepad2.cc livepad2.hh \
pncsequencer.cc pncsequencer.hh \
machine_sequencer.cc machine_sequencer.hh \
midi_export.cc midi_export.hh \
midi_export_gui.cc \
file_request_ui.cc \
advanced_file_request_ui.cc \
android_audio.cc android_audio.hh \
fixedpointmathlib.cc \
satan_project_entry.cc satan_project_entry.hh \
graph_project_entry.cc graph_project_entry.hh \
license_view.cc \
information_catcher.cc information_catcher.hh \
vorbis_export_ui.cc \
vorbis_encoder.cc vorbis_encoder.hh \
share_ui.cc share_ui.hh \
samples_editor_ng.cc \
machine_selector_ui.cc \
whistle_ui.cc \
whistle_analyzer.cc \
async_operations.cc \
svg_loader.cc svg_loader.hh \
tracker.cc tracker.hh \
scale_slider.cc scale_slider.hh \
listview.cc listview.hh \
connector.cc connector.hh \
remote_interface.cc remote_interface.hh \
corner_button.cc corner_button.hh \
connection_list.cc connection_list.hh \
time_measure.cc 

include $(BUILD_SHARED_LIBRARY)

include $(LOCAL_PATH)/dynlib/Android.mk

$(call import-module,cpufeatures)
