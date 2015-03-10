#!/bin/bash

SECONDS_SINCE_EPOCH="`date +%s`"
printf "#define BUILD_AT_SECONDS_SINCE \"$SECONDS_SINCE_EPOCH\"\n" > build_at_seconds_since.h

# step into the Android directory
cd Android

# check if we have a Android project that is usable, if not, fix it automatically

if [ ! -f ".ANDROID_PROJECT_CREATED" ]; then
    if [ "$1" != "--update-android-project" ]; then
	echo "To build using this script you must have a properly updated Android project."
	echo " - If you have done that manually, please create the file .ANDROID_PROJECT_CREATED"
	echo " - If you have NOT done that and want this script to do it for you, run this script again like this:"
	echo "./BuildAPK.sh --update-android-project <android build target>"
	echo ""

	echo "To list available targets run:"
	echo "android list targets"
	
	exit 1;
    fi

    if [ "$2" = "" ]; then
	echo "You must supply an Android build target."
	echo "Available targets are:"
	android list targets
	exit 1;
    fi
    
    android update project --name vuknob --target $2 --path ./

    if [ $? -ne 0 ]; then
	echo "Android project NOT properly created!"
	exit -1
    fi
    touch .ANDROID_PROJECT_CREATED

    echo "Project created, re-run script to build."
    
    exit 0;
fi

# check if user wants to just uninstall package from device
if [ "$1" = "-ud" ]; then
    adb -d uninstall com.holidaystudios.vuknob
    if [ $? -ne 0 ]; then
	echo "Failed to uninstall - perhaps already uninstalled?"
	exit 1
    fi
    
    echo "Package uninstalled."
    exit 0
fi

# check if user wants to debug
if [ "$1" = "-gdb-start" ]; then
    $NDK_PATH/ndk-gdb --start
    exit 0
fi
if [ "$1" = "-gdb" ]; then
    $NDK_PATH/ndk-gdb
    exit 0
fi


# first package SVG files, dynlib xml descriptor files, and audio samples.

TMPDIR=`mktemp -d`

mkdir $TMPDIR/dynlib
mkdir $TMPDIR/SVG
mkdir $TMPDIR/Samples
mkdir $TMPDIR/Examples


cp ../dynlib/*.xml $TMPDIR/dynlib
cp ../SVG/*.svg $TMPDIR/SVG
cp -R ../Samples/* $TMPDIR/Samples
cp ../Examples/* $TMPDIR/Examples

pushd $TMPDIR
zip archive.zip dynlib SVG Samples Examples
zip archive.zip dynlib/* SVG/* Samples/* Examples/*
zip archive.zip Samples/*/*
popd

mv $TMPDIR/archive.zip res/raw/nativefiletreearchive
rm -rf $TMPDIR

# create timestamp file in the java tree
# this is used to make sure we unpack
# the package with SVG files, dynlib stuff and samples properly on
# upgrades
rm -rf src/com/holidaystudios/vuknob/AutogenDate.java
echo "package com.holidaystudios.vuknob;" > src/com/holidaystudios/vuknob/AutogenDate.java
echo "" >> src/com/holidaystudios/vuknob/AutogenDate.java
echo "public class AutogenDate" >> src/com/holidaystudios/vuknob/AutogenDate.java
echo "{" >> src/com/holidaystudios/vuknob/AutogenDate.java
echo "    public final static String DATE = \"timestamp$SECONDS_SINCE_EPOCH.file\";" >> src/com/holidaystudios/vuknob/AutogenDate.java
echo "}" >> src/com/holidaystudios/vuknob/AutogenDate.java
echo "" >> src/com/holidaystudios/vuknob/AutogenDate.java

touch src/com/holidaystudios/vuknob/vuKNOBnet.java

# build native libraries, then run ant to build java stuff 'n create the .apk

RELEASE_OR_DEBUG=debug
if [ "$1" = "-release" ]; then
    grep 'debuggable="true"' AndroidManifest.xml > /dev/null
    if [ $? -eq 0 ]; then
	echo ""
	echo "Error - can't build release with when AndroidManifest.xml has debuggable set to true."
	echo ""
	exit 1
    fi
    RELEASE_OR_DEBUG=release
fi

$NDK_PATH/ndk-build -j9 && ant $RELEASE_OR_DEBUG

if [ $? -ne 0 ]; then
    echo "Build failed."
    exit -1
fi

if [ "$1" = "-release" ]; then
    echo "Release build finished."
    exit 0
fi

#if -ie (Install to Emulator == -ie)
if [ "$1" = "-ie" ]; then
    if [ "$2" = "-c" ]; then
	adb -e uninstall com.holidaystudios.vuknob
    fi
    adb -e install -r bin/vuknob-debug.apk
fi

#if -id (Install to Device == -id)
if [ "$1" = "-id" ]; then
    if [ "$2" = "-c" ]; then
	adb -d uninstall com.holidaystudios.vuknob
    fi
    adb -d install -r bin/vuknob-debug.apk
fi

echo "Build: $SECONDS_SINCE_EPOCH"
echo "   at:" `date`
