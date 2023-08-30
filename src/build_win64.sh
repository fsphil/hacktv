#!/bin/bash

set -e
set -x

HOST=x86_64-w64-mingw32
PREFIX=$(pwd)/build_win64/install_root
export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig

mkdir -p $PREFIX
cd build_win64

if [[ ! -f $PREFIX/lib/libusb-1.0.a ]]; then
	
	if [[ ! -f libusb-1.0.26.tar.bz2 ]]; then
		wget https://github.com/libusb/libusb/releases/download/v1.0.26/libusb-1.0.26.tar.bz2
		tar -xvjf libusb-1.0.26.tar.bz2
	fi
	
	cd libusb-1.0.26
	./configure --host=$HOST --prefix=$PREFIX --enable-static --disable-shared
	make -j4 install
	cd ..
fi

if [[ ! -f $PREFIX/lib/libhackrf.a ]]; then
	
	if [[ ! -f hackrf-2023.01.1.tar.xz ]]; then
		wget https://github.com/greatscottgadgets/hackrf/releases/download/v2023.01.1/hackrf-2023.01.1.tar.xz
		tar -xvJf hackrf-2023.01.1.tar.xz
	fi
	
	rm -rf hackrf-2023.01.1/host/libhackrf/build
	mkdir -p hackrf-2023.01.1/host/libhackrf/build
	cd hackrf-2023.01.1/host/libhackrf/build
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

if [[ ! -f $PREFIX/lib/libosmo-fl2k.a ]]; then
	
	if [[ ! -d osmo-fl2k ]]; then
		git clone --depth 1 git://git.osmocom.org/osmo-fl2k
	fi
	
	rm -rf osmo-fl2k/build
	mkdir -p osmo-fl2k/build
	cd osmo-fl2k/build
	mingw64-cmake \
		-DCMAKE_INSTALL_PREFIX=$PREFIX \
		-DCMAKE_INSTALL_LIBPREFIX=$PREFIX \
		-DCMAKE_INSTALL_LIBDIR=$PREFIX/lib \
		-DLIBUSB_INCLUDE_DIR=$PREFIX/include/libusb-1.0 \
		-DLIBUSB_LIBRARIES=$PREFIX/lib/libusb-1.0.a
	make -j4 install
	cd ../..
	mv $PREFIX/lib/liblibosmo-fl2k_static.a $PREFIX/lib/libosmo-fl2k.a
fi

if [[ ! -f $PREFIX/lib/libfdk-aac.a ]]; then
	
	if [[ ! -d fdk-aac ]]; then
		git clone https://github.com/mstorsjo/fdk-aac.git
	fi
	
	cd fdk-aac
	./autogen.sh
	./configure --host=$HOST --prefix=$PREFIX --enable-static --disable-shared
	make -j4 install
	cd ..
fi

if [[ ! -f $PREFIX/lib/libopus.a ]]; then
	
	if [[ ! -f opus-1.4.tar.gz ]]; then
		wget https://downloads.xiph.org/releases/opus/opus-1.4.tar.gz
		tar -xvzf opus-1.4.tar.gz
	fi
	
	cd opus-1.4
	./configure --host=$HOST --prefix=$PREFIX --enable-static --disable-shared --disable-doc --disable-extra-programs
	make -j4 install
	cd ..
fi

if [[ ! -f $PREFIX/lib/libavformat.a ]]; then
	
	if [[ ! -d ffmpeg ]]; then
		git clone --depth 1 https://github.com/FFmpeg/FFmpeg.git ffmpeg
	fi
	
	cd ffmpeg
	./configure \
		--enable-gpl --enable-nonfree --enable-libfdk-aac --enable-libopus \
		--enable-static --disable-shared --disable-programs \
		--disable-outdevs --disable-encoders \
		--arch=x86_64 --target-os=mingw64 --cross-prefix=$HOST- \
		--pkg-config=pkg-config --prefix=$PREFIX
	make -j4 install
	cd ..
fi

cd ..
CROSS_HOST=$HOST- make -j4 EXTRA_LDFLAGS="-static" EXTRA_PKGS="libusb-1.0"
mv -f hacktv hacktv.exe || true
$HOST-strip hacktv.exe

echo "Done"

