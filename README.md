# vuknob
Digital Audio Workstation

# Pre-reqs, 1 of 3

libogg and libvorbis need to be compiled. Reference the NDK documentation for full documentation on how to setup and use a stand-alone toolchain.

Here's a quick example:

$NDK_PATH/build/tools/make-standalone-toolchain.sh --platform=android-8 --toolchain=arm-linux-androideabi-4.9 --install-dir=$PWD/standalone-toolchain-4.9
export PATH=$PWD/standalone-toolchain-4.9/bin:$PATH
export CC=arm-linux-androideabi-gcc
export CXX=arm-linux-androideabi-g++
mkdir -p vuknob_dependencies

cd vuknob_dependencies
wget http://downloads.xiph.org/releases/ogg/libogg-1.3.2.tar.gz
wget http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.5.tar.gz
tar -xf libogg-1.3.2.tar.gz
tar -xf libvorbis-1.3.5.tar.gz

export TARGET_DIR=$PWD

cd libogg-1.3.2
./configure --enable-shared=no --prefix=$TARGET_DIR --host=arm-linux
make
make install

cd $TARGET_DIR/libvorbis-1.3.5
./configure --enable-shared=no --prefix=$TARGET_DIR --host=arm-linux
make
make install

# Pre-reqs, 2 of 3

To build vuKNOB you will also need to make a branch of kamoflage and libsvg-android:

cd <vuknob path>
export VUKNOB_ROOT_PATH=$pwd
export TMP_DIR=`mktemp`

cd $TMP_DIR
bzr branch lp:~pltxtra/kamoflage/android
mv android $VUKNOB_ROOT_PATH/../kamoflage_bzr

cd $TMP_DIR
bzr branch lp:libsvg-android
mv libsvg-android $VUKNOB_ROOT_PATH/../

rm -rf $TMP_DIR

# Pre-reqs, 3 of 3

You need to have libasio to build vuknob. Unpack the latest stable (tested with 1.10.2) in the parent directory to where you have vuknob.

cd <vuknob path>
cd ..
tar -xf <path-to-asio-archive>
ln -s <asio directory> ./asio
