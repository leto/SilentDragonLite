SilentDragonLite is a GUI wallet for the Hush cryptocoin, which does not require you to download the full blockchain!

## Compiling from source
* silentdragon is written in C++ 14, and can be compiled with g++/clang++/visual c++. 
* It also depends on Qt5, which you can get from [here](https://www.qt.io/download). 
* You'll need Rust v1.37 +

### Building on Linux

```
git clone https://github.com/DenioD/SilenDragonLite.git
cd silentdragonlite
/path/to/qt5/bin/qmake silentdragon-lite.pro CONFIG+=debug
make -j$(nproc)

./silentdragonlite
```

