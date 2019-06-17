# Felix

First, you need to build felix. You can use the build scripts in build/felix
to bootstrap it:

* Run _build/felix/build_posix.sh_ on Linux (and maybe other POSIX platforms)
* Run _build/felix/build_win32.bat_ on Windows

This will create a felix binary at the root of the source tree. You can then start
it to build all projects defined in *FelixBuild.ini*.

Use `felix --help` for other options.

As of now, R packages cannot be built using this method.

# R packages

Some packages provide an Rproject file and can be built by R CMD INSTALL. Open the
project file (e.g. *src/drd/drdR/drdR.Rproj*) in RStudio and use *Install and restart* in the
Build tab.

It should just work!
