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
