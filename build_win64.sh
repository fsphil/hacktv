#!/bin/bash

set -e
set -x

HOST=x86_64-w64-mingw32
PREFIX=$(pwd)/build_win64/install_root
export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig

mkdir -p $PREFIX
cd build_win64

if [[ ! -f $PREFIX/lib/libusb-1.0.a ]]; then
	
	if [[ ! -f libusb-1.0.22.tar.bz2 ]]; then
		wget https://github.com/libusb/libusb/releases/download/v1.0.22/libusb-1.0.22.tar.bz2
		tar -xvjf libusb-1.0.22.tar.bz2
	fi
	
	cd libusb-1.0.22
	./configure --host=$HOST --prefix=$PREFIX --enable-static --disable-shared
	make -j4 install
	cd ..
fi

if [[ ! -f $PREFIX/lib/libhackrf.a ]]; then
	
	if [[ ! -f hackrf-2018.01.1.tar.gz ]]; then
		wget https://github.com/mossmann/hackrf/archive/v2018.01.1/hackrf-2018.01.1.tar.gz
		tar -xvzf hackrf-2018.01.1.tar.gz
	fi
	
	rm -rf hackrf-2018.01.1/host/libhackrf/build
	mkdir -p hackrf-2018.01.1/host/libhackrf/build
	cd hackrf-2018.01.1/host/libhackrf/build
	mingw64-cmake \
		-DCMAKE_INSTALL_PREFIX=$PREFIX \
		-DCMAKE_INSTALL_LIBPREFIX=$PREFIX/lib \
		-DLIBUSB_INCLUDE_DIR=$PREFIX/include/libusb-1.0 \
		-DLIBUSB_LIBRARIES=$PREFIX/lib/libusb-1.0.a
	make -j4 install
	cd ../../../..
	mv $PREFIX/bin/*.a $PREFIX/lib/
	find $PREFIX -name libhackrf\*.dll\* -delete
fi

if [[ ! -f $PREFIX/lib/libfdk-aac.a ]]; then
	
	if [[ ! -f fdk-aac ]]; then
		git clone https://github.com/mstorsjo/fdk-aac.git
	fi
	
	cd fdk-aac
	./autogen.sh
	./configure --host=$HOST --prefix=$PREFIX --enable-static --disable-shared
	make -j4 install
	cd ..
fi

if [[ ! -f $PREFIX/lib/libavformat.a ]]; then
	
	if [[ ! -f ffmpeg ]]; then
		git clone https://github.com/FFmpeg/FFmpeg.git ffmpeg
	fi
	
	cd ffmpeg
	./configure \
		--enable-gpl --enable-nonfree --enable-libfdk-aac \
		--enable-static --disable-shared --disable-programs \
		--disable-outdevs --disable-encoders \
		--arch=x86_64 --target-os=mingw64 --cross-prefix=$HOST- \
		--pkg-config=pkg-config --prefix=$PREFIX
	make -j4 install
	cd ..
fi

cd ..
CROSS_HOST=$HOST- make -j4 EXTRA_LDFLAGS="-static" EXTRA_PKGS="libusb-1.0"
mv hacktv hacktv.exe
$HOST-strip hacktv.exe

echo "Done"

