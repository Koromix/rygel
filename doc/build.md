# CMake

This is the main build system, the CMake build scripts can build all projects including
R packages.

macOS is not a supported platform for now, because I don't have a working machine
anymore. Well there's also the fact that it's the worst OS of all, with broken POSIX
support and broken APIs.

## Windows

### MinGW-w64

I recommend the msys2 distribution. The following msys2 packages must be installed
from the msys2 prompt (for x64 builds):

* *mingw-w64-w86_64-gcc*
* *mingw-w64-w86_64-ninja*

cmake, gcc/g++ and ninja binaries must be in PATH when you use the cmake command.

For R packages, you need to install *R*, *Rtools* (>= 3.5) and the following R packages:
*Rcpp* and *data.table*.

```bash
git clone https://github.com/Koromix/rygel.git
cd rygel
mkdir bin\mingw_RelWithDebInfo
cd bin\mingw_RelWithDebInfo
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja ../..
ninja
```

### Visual Studio

Execute commands from a Visual Studio native prompt (x64 or x86).

R packages can only be compiled with GCC. In particular, Visual Studio cannot
build R packages.

```bash
git clone https://github.com/Koromix/rygel.git
cd rygel
mkdir bin\vs_RelWithDebInfo
cd bin\vs_RelWithDebInfo
cmake -G "Visual Studio 15 2017 Win64" ../..
cmake --build . --config RelWithDebInfo
```

## Linux

Ubuntu dependencies:

* cmake
* gcc (>= 6.0)
* g++
* ninja-build
* r-base (to build and use R packages)

You also need the following R packages (only if you want to build and use R packages):
*Rcpp* and *data.table*.

```batch
git clone https://github.com/Koromix/rygel.git
cd rygel
mkdir -p bin/linux_RelWithDebInfo
cd bin/linux_RelWithDebInfo
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja ../..
ninja
```

# Alternative build systems

## Felix

First, you need to build felix. You can use the build scripts in build/felix
to bootstrap it:

* Run _build/felix/build_posix.sh_ on POSIX platforms
* Run _build/felix/build_win32.bat_ on Windows

This will create a felix binary at the root of the source tree. You can then start
it to build all projects defined in *FelixBuild.ini*.

As of now, R packages cannot be built using this method.

## R packages

Some packages provide an Rproject file and can be built by R CMD INSTALL. Open the
project file (e.g. *src/drd/drdR/drdR.Rproj*) in RStudio and use *Install and restart* in the
Build tab.

It should just work!
