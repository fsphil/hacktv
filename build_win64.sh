#!/bin/bash

set -e

HOST=x86_64-w64-mingw32
PREFIX=$(pwd)/build_win64/install_root

rm -rf build_win64
mkdir -p $PREFIX
cd build_win64

git clone https://github.com/libusb/libusb.git
cd libusb
./bootstrap.sh
./configure --host=$HOST --prefix=$PREFIX --enable-static --disable-shared
make -j4 install
cd ..

wget http://www.fftw.org/fftw-3.3.8.tar.gz
tar -xvzf fftw-3.3.8.tar.gz
cd fftw-3.3.8
./configure --host=$HOST --prefix=$PREFIX --enable-static --disable-shared --disable-dependency-tracking --disable-threads
make -j4 install
cd ..

git clone https://github.com/mossmann/hackrf.git
cd hackrf/host
mingw64-cmake -DLIBUSB_INCLUDE_DIR=$PREFIX/include/libusb-1.0 -DFFTW_LIBRARIES=$PREFIX/lib -DCMAKE_INSTALL_PREFIX:PATH=$PREFIX
make -j4 install
# The libs end up in /bin - can I fix this above?
mv $PREFIX/bin/libhackrf.*.a $PREFIX/lib/
rm $PREFIX/bin/libhackrf.dll
cd ../..

git clone https://github.com/mstorsjo/fdk-aac.git
cd fdk-aac
./autogen.sh
./configure --host=$HOST --prefix=$PREFIX --enable-static --disable-shared
make -j4 install
cd ..

git clone https://github.com/FFmpeg/FFmpeg.git ffmpeg
cd ffmpeg
PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig ./configure \
	--enable-gpl --enable-nonfree --enable-libfdk-aac \
	--disable-outdevs --disable-encoders \
	--arch=x86_64 --target-os=mingw64 --cross-prefix=$HOST- \
	--pkg-config=pkg-config --prefix=$PREFIX
make -j4 install
cd ..

cd ..
CROSS_HOST=$HOST- PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig make -j4
$HOST-strip hacktv.exe

echo "Done"

