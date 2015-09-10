#!/bin/bash

BASEDIR=$PWD

if [ -z "$NDK_PATH" ]; then
    echo "NDK_PATH not set."
    exit 1
fi

if [ -z "`which android`" ]; then
    echo "Android tools not in your current path."
    exit 1
fi

function usage {
    echo "Usage:"
    echo
    echo "   $0 [options] <application>"
    echo
    echo "Options:"
    echo "    -h, --help"
    echo "                  Prints this help information."
    echo "    -i <d/e> [-c]"
    echo "                  install to either <d>evice or <e>mulator, "
    echo "                  [-c] clears previous install"
    echo "    -u <d/e>"
    echo "                  do uninstall on either <d>evice or <e>mulator"
    echo "    --update-android-project <android target>"
    echo "                  Update underlying android project."
    echo "    --gdb <d/e>"
    echo "                  Attach GDB to running instance on"
    echo "                  <d>evice or <e>mulator"
    echo "    --start-gdb <d/e>"
    echo "                  Start app with GDB started on"
    echo "                  <d>evice or <e>mulator"
    echo "    --release"
    echo "                  Build release package."
    echo "    <application>"
    echo "                  One of the following:"
    for APP in $(ls Applications) ; do
	echo "                     $APP"
    done
    echo
    echo
}

if [ $# -lt 1 ]; then
    echo
    echo "Wrong number of options"
    echo
    usage
    exit 1
fi

MODE="NONE"
TARGET=""
DO_CLEAR="false"
APPLICATION=""
DEBUGGABLE="true"
DORELEASE="false"

while [[ $# > 0 ]]
do
key="$1"

case $key in
    -h|--help)
	usage
	exit 0
	;;
    --clean)
	MODE="cleanup"
	;;
    -i)
	MODE="install"
	TARGET="$2"
	shift # skip val
	;;
    -u)
	MODE="uninstall"
	TARGET="$2"
	shift # skip val
	;;
    -c)
	DO_CLEAR="true"
	;;
    --update-android-project)
	MODE="update"
	TARGET="$2"
	shift # skip val
	;;
    --gdb)
	MODE="gdb"
	TARGET="$2"
	shift # skip val
	;;
    --start-gdb)
	MODE="sgdb"
	TARGET="$2"
	shift # skip val
	;;
    --release)
	DORELEASE="true"
	DEBUGGABLE="false"
	;;
    -*)
	echo
	echo "Unknown option $key."
	echo
	usage
	exit 1
            # unknown option
	;;
    *)
	APPLICATION="$key"
	;;
esac

shift # skip arg or val

done

if [ -z "$APPLICATION" ]; then
    echo "You must select an application from the following: `ls Applications`"
    echo
    usage
    exit 2
fi

if [ -a ./config.mk ]; then
    TMP_CONF=`mktemp`
    cat ./config.mk | sed 's/ := /=/' > $TMP_CONF
    . $TMP_CONF
else
    echo "No config.mk available"
    exit 1
fi

if [ -a ./Applications/$APPLICATION/app_config ]; then
    . ./Applications/$APPLICATION/app_config
else
    echo "Applications/$APPLICATION/app_config does not exist."
    exit 1;
fi

if [ -z "$APP_NAMESPACE" ]; then
    echo "./Applications/$APPLICATION/app_config contains incorrect values."
    exit 1;
fi
if [ -z "$APK_PREFIX" ]; then
    echo "./Applications/$APPLICATION/app_config contains incorrect values."
    exit 1;
fi

SECONDS_SINCE_EPOCH="`date +%s`"

# step into the Android directory

if [ "$MODE" = "cleanup" ]; then
    rm -rf Applications/$APPLICATION/Android/bin/*
    rm -rf Applications/$APPLICATION/Android/bin/*
    rm -rf Applications/$APPLICATION/Android/libs/armeab*/*
    rm -rf Applications/$APPLICATION/Android/obj/*

    echo
    echo "Cleanup completed."
    echo

    exit 0
fi

cd Applications/$APPLICATION/Android

rm AndroidManifest.xml
cat $BASEDIR/AndroidManifest.xml.template | sed "s/APP_NAMESPACE/$APP_NAMESPACE/" | sed "s/APP_TARGET_SDK/$APP_TARGET_SDK/" | sed "s/APP_MIN_SDK/$APP_MIN_SDK/" | sed "s/DEBUGGABLE/$DEBUGGABLE/" | sed "s/APP_ACTIVITY/$ACTIVITY/" > AndroidManifest.xml

# check if we have a Android project that is usable, if not, fix it automatically

if [ ! -f ".ANDROID_PROJECT_CREATED" ]; then
    if [ "$MODE" != "update" ]; then
	echo "To build using this script you must have a properly updated Android project."
	echo " - If you have done that manually, please create the file .ANDROID_PROJECT_CREATED"
	echo " - If you have NOT done that and want this script to do it for you, run this script again like this:"
	echo "./BuildAPK.sh --update-android-project <android build target>"
	echo ""

	echo "To list available targets run:"
	echo "android list targets"

	exit 1;
    fi

    if [ "$TARGET" = "" ]; then
	echo "You must supply an Android build target."
	echo "Available targets are:"
	android list targets
	exit 1;
    fi

    android update project --name $APK_PREFIX --target $TARGET --path ./

    if [ $? -ne 0 ]; then
	echo "Android project NOT properly created!"
	exit -1
    fi
    touch .ANDROID_PROJECT_CREATED

    echo "Project created, re-run script to build."

    exit 0;
fi

# check if user wants to just uninstall package from device
if [ "$MODE" = "uninstall" ]; then
    adb -d uninstall $APP_NAMESPACE
    if [ $? -ne 0 ]; then
	echo "Failed to uninstall - perhaps already uninstalled?"
	exit 1
    fi

    echo "Package uninstalled."
    exit 0
fi

# check if user wants to debug
if [ "$MODE" = "sgdb" ]; then
    $NDK_PATH/ndk-gdb --start
    exit 0
fi
if [ "$MODE" = "gdb" ]; then
    $NDK_PATH/ndk-gdb
    exit 0
fi

# first package SVG files, dynlib xml descriptor files, and audio samples.

TMPDIR=`mktemp -d`

mkdir $TMPDIR/dynlib
mkdir $TMPDIR/SVG
mkdir $TMPDIR/Samples
mkdir $TMPDIR/Examples

cp $LIBVUKNOB_DIRECTORY/src_jni/dynlib/*.xml $TMPDIR/dynlib
cp $BASEDIR/Resources/SVG/*.svg $TMPDIR/SVG
cp -R $BASEDIR/Resources/Samples/* $TMPDIR/Samples
cp $BASEDIR/Resources/Examples/* $TMPDIR/Examples

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
rm -rf src/com/holidaystudios/$APK_PREFIX/AutogenDate.java
echo "package $APP_NAMESPACE;" > src/com/holidaystudios/$APK_PREFIX/AutogenDate.java
echo "" >> src/com/holidaystudios/$APK_PREFIX/AutogenDate.java
echo "public class AutogenDate" >> src/com/holidaystudios/$APK_PREFIX/AutogenDate.java
echo "{" >> src/com/holidaystudios/$APK_PREFIX/AutogenDate.java
echo "    public final static String DATE = \"timestamp$SECONDS_SINCE_EPOCH.file\";" >> src/com/holidaystudios/$APK_PREFIX/AutogenDate.java
echo "}" >> src/com/holidaystudios/$APK_PREFIX/AutogenDate.java
echo "" >> src/com/holidaystudios/$APK_PREFIX/AutogenDate.java

touch src/com/holidaystudios/$APK_PREFIX/${ACTIVITY}.java

# build native libraries, then run ant to build java stuff 'n create the .apk

APK_SUFIX="-debug"
RELEASE_OR_DEBUG=debug
if [ "$DORELEASE" = "true" ]; then
    APK_SUFIX="-release"
    grep 'debuggable="true"' AndroidManifest.xml > /dev/null
    if [ $? -eq 0 ]; then
	echo ""
	echo "Error - can't build release with when AndroidManifest.xml has debuggable set to true."
	echo ""
	exit 1
    fi
    RELEASE_OR_DEBUG=release
fi

# copy current dynlib modules from vuknob
CURRENTMODS=`ls -1 ${LIBVUKNOB_DIRECTORY}/src_jni/dynlib/*.xml | awk '{print "basename " $0 ";"}' | bash | cut -d '.' -f 1-1`
echo "#autogenerated from BuildAPK.sh" > ../dynlib_auto_Android.mk
echo "" >> ../dynlib_auto_Android.mk
for MOD in $CURRENTMODS; do
    echo 'include $(CLEAR_VARS)' >> ../dynlib_auto_Android.mk
    echo "LOCAL_MODULE := $MOD" >> ../dynlib_auto_Android.mk
    echo 'ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)' >> ../dynlib_auto_Android.mk
    echo "LOCAL_SRC_FILES  := ${LIBVUKNOB_DIRECTORY}/export/armeabi-v7a/lib$MOD.so" >> ../dynlib_auto_Android.mk
    echo "else" >> ../dynlib_auto_Android.mk
    echo "LOCAL_SRC_FILES  := ${LIBVUKNOB_DIRECTORY}/export/armeabi/lib$MOD.so" >> ../dynlib_auto_Android.mk
    echo "endif" >> ../dynlib_auto_Android.mk
    echo 'include $(PREBUILT_SHARED_LIBRARY)' >> ../dynlib_auto_Android.mk
    echo  >> ../dynlib_auto_Android.mk
done

# build it
$NDK_PATH/ndk-build -j9 && ant $RELEASE_OR_DEBUG

if [ $? -ne 0 ]; then
    echo "Build failed."
    exit -1
fi

if [ "$DORELEASE" = "true" ]; then
    echo
    echo "Release build finished."
    echo
fi

if [ "$MODE" = "install" ]; then

    if [ "$DO_CLEAR" = "true" ]; then
	adb -$TARGET uninstall $APP_NAMESPACE
    fi
    adb -$TARGET install -r bin/${APK_PREFIX}${APK_SUFIX}.apk

    echo "Build: $SECONDS_SINCE_EPOCH"
    echo "   at:" `date`
fi
