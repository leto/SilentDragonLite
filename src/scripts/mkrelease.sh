#!/bin/bash
if [ -z $QT_STATIC ]; then 
    echo "QT_STATIC is not set. Please set it to the base directory of a statically compiled Qt"; 
    exit 1; 
fi

if [ -z $APP_VERSION ]; then echo "APP_VERSION is not set"; exit 1; fi
if [ -z $PREV_VERSION ]; then echo "PREV_VERSION is not set"; exit 1; fi

echo -n "Version files.........."
# Replace the version number in the .pro file so it gets picked up everywhere
sed -i "s/${PREV_VERSION}/${APP_VERSION}/g" silentdragon-lite.pro > /dev/null

# Also update it in the README.md
sed -i "s/${PREV_VERSION}/${APP_VERSION}/g" README.md > /dev/null
echo "[OK]"

echo -n "Cleaning..............."
rm -rf bin/*
rm -rf artifacts/*
make distclean >/dev/null 2>&1
echo "[OK]"

echo ""
echo "[Building on" `lsb_release -r`"]"

echo -n "Configuring............"
QT_STATIC=$QT_STATIC bash src/scripts/dotranslations.sh >/dev/null
$QT_STATIC/bin/qmake silentdragon-lite.pro -spec linux-clang CONFIG+=release > /dev/null
echo "[OK]"


echo -n "Building..............."
rm -rf bin/silentdragonlite* > /dev/null
# Build the lib first
cd lib && make release && cd ..
make -j$(nproc) > /dev/null
echo "[OK]"


# Test for Qt
echo -n "Static link............"
if [[ $(ldd silentdragonlite | grep -i "Qt") ]]; then
    echo "FOUND QT; ABORT"; 
    exit 1
fi
echo "[OK]"


echo -n "Packaging.............."
mkdir bin/silentdragonlite-v$APP_VERSION > /dev/null
strip silentdragonlite

cp silentdragonlite                 bin/silentdragonlite-v$APP_VERSION > /dev/null
cp README.md                      bin/silentdragonlite-v$APP_VERSION > /dev/null
cp LICENSE                        bin/silentdragonlite-v$APP_VERSION > /dev/null

cd bin && tar czf linux-silentdragonlite-v$APP_VERSION.tar.gz silentdragonlite-v$APP_VERSION/ > /dev/null
cd .. 

mkdir artifacts >/dev/null 2>&1
cp bin/linux-silentdragonlite-v$APP_VERSION.tar.gz ./artifacts/linux-binaries-silentdragonlite-v$APP_VERSION.tar.gz
echo "[OK]"


if [ -f artifacts/linux-binaries-silentdragonlite-v$APP_VERSION.tar.gz ] ; then
    echo -n "Package contents......."
    # Test if the package is built OK
    if tar tf "artifacts/linux-binaries-silentdragonlite-v$APP_VERSION.tar.gz" | wc -l | grep -q "4"; then 
        echo "[OK]"
    else
        echo "[ERROR]"
        exit 1
    fi    
else
    echo "[ERROR]"
    exit 1
fi

echo -n "Building deb..........."
debdir=bin/deb/silentdragonlite-v$APP_VERSION
mkdir -p $debdir > /dev/null
mkdir    $debdir/DEBIAN
mkdir -p $debdir/usr/local/bin

cat src/scripts/control | sed "s/RELEASE_VERSION/$APP_VERSION/g" > $debdir/DEBIAN/control

cp silentdragonlite                   $debdir/usr/local/bin/

mkdir -p $debdir/usr/share/pixmaps/
cp res/silentdragonlite.xpm           $debdir/usr/share/pixmaps/

mkdir -p $debdir/usr/share/applications
cp src/scripts/desktopentry    $debdir/usr/share/applications/silentdragonlite.desktop

dpkg-deb --build $debdir >/dev/null
cp $debdir.deb                 artifacts/linux-deb-silentdragonlite-v$APP_VERSION.deb
echo "[OK]"



echo ""
echo "[Windows]"

if [ -z $MXE_PATH ]; then 
    echo "MXE_PATH is not set. Set it to ~/github/mxe/usr/bin if you want to build Windows"
    echo "Not building Windows"
    exit 0; 
fi

export PATH=$MXE_PATH:$PATH

echo -n "Configuring............"
make clean  > /dev/null
#rm -f silentdragonlite-mingw.pro
rm -rf release/
#Mingw seems to have trouble with precompiled headers, so strip that option from the .pro file
#cat silentdragon-lite.pro | sed "s/precompile_header/release/g" | sed "s/PRECOMPILED_HEADER.*//g" > silentdragonlite-mingw.pro
echo "[OK]"


echo -n "Building..............."
# Build the lib first
cd lib && make winrelease && cd ..
x86_64-w64-mingw32.static-qmake-qt5 silentdragonlite.pro CONFIG+=release > /dev/null
make -j32 > /dev/null
echo "[OK]"


echo -n "Packaging.............."
mkdir release/silentdragonlite-v$APP_VERSION  
cp release/silentdragonlite.exe          release/silentdragonlite-v$APP_VERSION 
cp README.md                          release/silentdragonlite-v$APP_VERSION 
cp LICENSE                            release/silentdragonlite-v$APP_VERSION 
cd release && zip -r Windows-binaries-silentdragonlite-v$APP_VERSION.zip silentdragonlite-v$APP_VERSION/ > /dev/null
cd ..

mkdir artifacts >/dev/null 2>&1
cp release/Windows-binaries-silentdragonlite-v$APP_VERSION.zip ./artifacts/
echo "[OK]"

if [ -f artifacts/Windows-binaries-silentdragonlite-v$APP_VERSION.zip ] ; then
    echo -n "Package contents......."
    if unzip -l "artifacts/Windows-binaries-silentdragonlite-v$APP_VERSION.zip" | wc -l | grep -q "9"; then 
        echo "[OK]"
    else
        echo "[ERROR]"
        exit 1
    fi
else
    echo "[ERROR]"
    exit 1
fi
