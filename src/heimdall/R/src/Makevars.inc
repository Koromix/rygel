# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

PKG_CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D__STDC_FORMAT_MACROS \
				-fno-strict-aliasing -fvisibility=hidden -fno-semantic-interposition \
				-I. -I../../../.. -Wstrict-overflow=0
PKG_CFLAGS += -std=gnu99
PKG_CXXFLAGS += -std=gnu++20

all:
	@mkdir -p ../R
	cp ../heimdallR.R ../R/heimdallR.R

override OBJECTS := $(addprefix ./${R_ARCH}_, heimdallR.cc.o database.cc.o Rcc.cc.o base.cc.o sqlite.cc.o sqlite3.c.o)

./${R_ARCH}_heimdallR.cc.o: ../heimdallR.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_database.cc.o: ../../server/database.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_Rcc.cc.o: ../../../core/wrap/Rcc.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_base.cc.o: ../../../core/base/base.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_sqlite.cc.o: ../../../core/sqlite/sqlite.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_sqlite3.c.o: ../../../../vendor/sqlite3mc/sqlite3.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -Wno-pedantic -c -o $@ $<
