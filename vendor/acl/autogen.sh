#!/bin/sh -ex
po/update-potfiles
autopoint --force
am_libdir=$(automake --print-libdir)
cp "${am_libdir}/INSTALL" doc/
exec autoreconf -f -i
