# Copyright (C) 2009  Andreas Gruenbacher <agruen@suse.de>
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

AC_PREREQ([2.69])
AC_INIT([acl], [2.3.2], [acl-devel@nongnu.org])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([include/config.h])
AC_PREFIX_DEFAULT(/usr)

AM_INIT_AUTOMAKE([-Wall foreign 1.15 dist-xz subdir-objects])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O
AC_USE_SYSTEM_EXTENSIONS
AC_FUNC_GCC_VISIBILITY
AC_C_BIGENDIAN
AC_SYS_LARGEFILE

AM_PROG_AR
LT_INIT

dnl Minimal version supporting AM_GNU_GETTEXT_REQUIRE_VERSION.
AM_GNU_GETTEXT_VERSION([0.19.6])
dnl Require at least the following version, but use the latest available one.
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--enable-debug], [Enable extra debugging])])
AS_IF([test "x$enable_debug" = "xyes"],
	[CPPFLAGS="$CPPFLAGS -DDEBUG"],
	[CPPFLAGS="$CPPFLAGS -DNDEBUG"])

dnl Automatically increment the revision for every release.
set -- $(IFS=.; echo ${PACKAGE_VERSION})
AS_IF([test $# -ne 3 -o ${#1} -gt 1 -o ${#2} -gt 1 -o ${#3} -gt 2],
	[AC_MSG_ERROR([This PACKAGE_VERSION breaks LT_REVISION])])
LT_REVISION=$(printf "%d%d%02d" "$1" "$2" "$3")
AC_SUBST(LT_REVISION)

AC_PACKAGE_NEED_ATTR_ERROR_H
AC_PACKAGE_NEED_GETXATTR_LIBATTR

AC_CONFIG_COMMANDS([include/acl],
	[dnl
	rm -rf include/acl
	$as_ln_s "$ac_abs_top_srcdir/include" include/acl
	])
AC_CONFIG_COMMANDS([include/sys],
	[dnl
	rm -rf include/sys
	$as_ln_s "$ac_abs_top_srcdir/include" include/sys
	])
AC_CONFIG_FILES([
	libacl.pc
	Makefile
	po/Makefile.in
])
AC_OUTPUT
