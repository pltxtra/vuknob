# vuknob
Digital Audio Workstation

# Developers

libvorbis.tar.gz contains precompiled android port of libvorbis and libogg... it is needed to link the Android version of satan.


libogg and libvorbis can be compiled, if you properly setup a stand-alone toolchain, according to the NDK documentation:

export SYSROOT=$NDK/platforms/android-L/arch-arm/
export CC="$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86/bin/arm-linux-androideabi-gcc --sysroot=$SYSROOT"
export CXX="$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86/bin/arm-linux-androideabi-g++ --sysroot=$SYSROOT"

Then libogg should be compiled first, then libvorbis.

For example compile libvorbis-1.3.4.tar.gz, for Android by first running configure like this:

$ ./configure --with-sysroot=/home/apersson/MyWork/Source/Android/android-ndk-r10b/platforms/android-8/arch-arm --enable-shared=no --prefix=/home/apersson/MyWork/Source/Android/libvorbis_new --host=arm-linux --with-ogg-includes=../include/

then run:
make
make install

the resulting files will be in .../libvorbis_new/libs
