APP_PROJECT_PATH := $(call my-dir)/Android/
APP_BUILD_SCRIPT := $(call my-dir)/Android.mk
APP_STL := gnustl_static
APP_CPPFLAGS += -fexceptions
APP_CPPFLAGS += -frtti
APP_ABI := armeabi armeabi-v7a
NDK_TOOLCHAIN_VERSION=4.9
