# Building Qt apps

## Dynamic Qt

Install Qt and set the environment variable QMAKE_PATH to point to the `qmake` binary.

```sh
export QMAKE_PATH=/path/to/qmake
felix QtApp
````

## Static Qt on Linux

### Build static Qt

```sh
wget https://ftp.nluug.nl/languages/qt/archive/qt/6.5/6.5.2/submodules/qtbase-everywhere-src-6.5.2.tar.xz
tar xvf qtbase-everywhere-src-6.5.2.tar.xz
cd qtbase-everywhere-src-6.5.2
./configure -release -static -prefix $HOME/Qt/Static/6.5.2 -no-icu -no-cups -qt-pcre -qt-zlib -qt-libpng -qt-libjpeg -xcb -fontconfig
cmake --build . --parallel
cmake --install .
```

### Run felix

```sh
export QMAKE_PATH=$HOME/Qt/Static/6.5.2/bin/qmake
felix QtApp
```

## Static Qt on Windows

```bat
wget https://ftp.nluug.nl/languages/qt/archive/qt/6.5/6.5.2/submodules/qtbase-everywhere-src-6.5.2.zip
tar xvf qtbase-everywhere-src-6.5.2.zip
cd qtbase-everywhere-src-6.5.2
configure -platform win32-msvc -prefix C:/Qt/Static/6.5.2 -opensource -confirm-license -static -static-runtime -release -no-opengl -no-harfbuzz -no-icu -no-cups -qt-pcre -qt-zlib -qt-freetype -qt-libpng -qt-libjpeg
cmake --build . --parallel
cmake --install .
```

```sh
export QMAKE_PATH=C:/Qt/Static/6.5.2/bin/qmake
felix QtApp
```