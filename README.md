# SilentDragonLite

SilentDragonLite is a lightwallet for HUSH ($HUSH) runs on Linux and Windows which does not require you to download the full blockchain. This is experimental software under active development!

## PRIVACY NOTICE

SilentDragonLite contacts a few different external websites to get various bits of data. * coingecko.com for price data API * explorer.myhush.org for explorer links * dexstats.info for address utilities, hush-lightwallet.de to get Data.

This means your IP address is known to these servers. Enable Tor setting in SilentDragon to prevent this, or better yet, use TAILS: https://tails.boum.org/

## Installation

Go to the releases page and grab the latest installers or binary. https://github.com/MyHush/SilentDragonLite/releases

## Compiling from source
* SilentDragonLite is written in C++ 14, and can be compiled with g++/clang++/visual c++. 
* It also depends on Qt5, which you can get from [here](https://www.qt.io/download). 
* You'll need Rust v1.37 +

## Building on Linux

```
git clone https://github.com/MyHush/SilenDragonLite.git
cd silentdragonlite
./build.sh
./Silentdragonlite

```

## Crosscompile for Windows (only tested for Ubuntu 18.04)

```
# build dependencies
sudo apt install clang g++ build-essential make mingw-w64 git pkg-config libc6-dev m4 g++-multilib autoconf libtool-bin ncurses-dev unzip python python-zmq zlib1g-dev wget curl bsdmainutils automake libgl1-mesa-dev libglu1-mesa-dev libfontconfig1-dev autopoint libssl-dev

# MXE dependencies
sudo apt install bash bison bzip2 flex gettext git gperf intltool libc6-dev-i386 libgdk-pixbuf2.0-dev libltdl-dev libtool-bin libxml-parser-perl make openssl p7zip-full patch perl pkg-config python ruby sed unzip wget xz-utils

```
# Compile OpenSSL

```
# Download openssl 1.0.2
https://www.openssl.org/source/
cd openssl
./Configure linux-x86_64
make
```
## Static build of Qt5

# Download Qt5 sources
```
mkdir -p ~/Qt/5.11.2 && cd ~/Qt/5.11.2

wget https://download.qt.io/archive/qt/5.11/5.11.2/single/qt-everywhere-src-5.11.2.tar.xz
tar xvf qt-everywhere-src-5.11.2.tar.xz && cd qt-everywhere-src-5.11.2

```
# Configure and build Qt5 statically.
```
OPENSSL_LIBS='-L/PATH/TO/openssl-1.0.2q -lssl -lcrypto' ./configure -static -prefix ~/Qt/5.11.2/static  -skip qtlocation -skip qtmacextras -skip qtpurchasing -skip qtscript -skip qtsensors -skip qtserialbus -skip qtserialport -skip qtspeech -skip qtdatavis3d -skip qtdoc -skip qtcharts -skip qtdeclarative -skip qt3d -skip qtwebengine -skip qtandroidextras -skip qtwebview -skip qtgamepad -skip qtquickcontrols -skip qtquickcontrols2 -skip qtremoteobjects -skip qtwebview -skip qtwebchannel -skip qtwebglplugin  -nomake examples -nomake tests -qt-zlib -qt-libpng -qt-xcb -qt-xkbcommon -feature-fontconfig -no-feature-getentropy  -release -openssl-linked -opensource

make -j$(nproc)
make -j$(nproc) install

```
# Build MXE (Cross-compiled Qt5 for Windows in Linux)
```
mkdir ~/github && cd ~/github
git clone https://github.com/mxe/mxe.git
cd mxe

make -j$(nproc) MXE_TARGETS=x86_64-w64-mingw32.static qtbase qtwebsockets

```
# Build SilentDragonLite .exe
cd SilentDragonLite
./win-static-build.sh
