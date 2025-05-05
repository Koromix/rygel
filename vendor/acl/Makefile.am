ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	exports

SUBDIRS = po

AM_CPPFLAGS = \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include \
	-DLOCALEDIR=\"$(localedir)\"

pkgconfdir = $(libdir)/pkgconfig
sysincludedir = $(includedir)/sys

pkgconf_DATA = libacl.pc
pkginclude_HEADERS =
sysinclude_HEADERS =
bin_PROGRAMS =
lib_LTLIBRARIES =
noinst_HEADERS =
noinst_LTLIBRARIES =
dist_doc_DATA =
dist_man_MANS =

update-po:
	$(MAKE) -C po $@

include doc/Makemodule.am
include examples/Makemodule.am
include include/Makemodule.am
include libacl/Makemodule.am
include libmisc/Makemodule.am
include man/Makemodule.am
include test/Makemodule.am
include tools/Makemodule.am
