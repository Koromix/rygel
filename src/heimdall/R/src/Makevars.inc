# Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

PKG_CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D__STDC_FORMAT_MACROS -DLIBCC_NO_BROTLI\
                -Wstrict-overflow=0 -fno-strict-aliasing -I../../../..
PKG_CFLAGS += -std=gnu99
PKG_CXXFLAGS += -std=gnu++17

all:
	@mkdir -p ../R
	cp ../heimdallR.R ../R/heimdallR.R

override OBJECTS := $(addprefix ./${R_ARCH}_, heimdallR.cc.o Rcc.cc.o base.cc.o gui.cc.o opengl.cc.o libheimdall.cc.o\
                                              imgui.cpp.o imgui_demo.cpp.o imgui_draw.cpp.o imgui_tables.cpp.o imgui_widgets.cpp.o\
                                              miniz.c.o)

./${R_ARCH}_heimdallR.cc.o: ../heimdallR.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_Rcc.cc.o: ../../../core/wrap/Rcc.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_base.cc.o: ../../../core/base/base.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_gui.cc.o: ../gui.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_opengl.cc.o: ../../../core/wrap/opengl.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_libheimdall.cc.o: ../../lib/libheimdall.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<

./${R_ARCH}_imgui.cpp.o: ../../../../vendor/imgui/imgui.cpp
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_imgui_demo.cpp.o: ../../../../vendor/imgui/imgui_demo.cpp
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_imgui_draw.cpp.o: ../../../../vendor/imgui/imgui_draw.cpp
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_imgui_tables.cpp.o: ../../../../vendor/imgui/imgui_tables.cpp
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_imgui_widgets.cpp.o: ../../../../vendor/imgui/imgui_widgets.cpp
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_miniz.c.o: ../../../../vendor/miniz/miniz.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -Wno-pedantic -c -o $@ $<
