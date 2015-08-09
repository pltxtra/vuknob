# vuknob
The Digital Audio Workstation.

This repository contains the build script for making release APKs of vuKNOB and vuKNOBnet for Android. It requires the following:

* A GNU/Linux based development host
* The Android SDK
* The Android NDK
* libsvgandroid
* libkamoflage
* libvuknob
* The Samsung Professional Audio SDK

# config.mk

Make sure you create a config.mk file, with the following content:

LIBSVGANDROID_DIRECTORY := <absolute path to libsvgandroid>
LIBKAMOFLAGE_DIRECTORY := <absolute path to libkamoflage>
LIBVUKNOB_DIRECTORY := <absolute path to libvuknob>
