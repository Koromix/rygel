# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see https://www.gnu.org/licenses/.

PKG_CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D__STDC_FORMAT_MACROS -DLIBDRD_NO_WREN -DLIBCC_NO_BROTLI\
                -Wstrict-overflow=0 -fno-strict-aliasing -I../../../..
PKG_CFLAGS += -std=gnu99
PKG_CXXFLAGS += -std=gnu++17

all:
	@mkdir -p ../R
	cp ../drdR_mco.R ../R/drdR_mco.R

override OBJECTS = $(addprefix ./${R_ARCH}_, drdR_mco.cc.o Rcc.cc.o libcc.cc.o libdrd.cc.o miniz.c.o)

./${R_ARCH}_drdR_mco.cc.o: ../drdR_mco.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_Rcc.cc.o: ../../../core/libwrap/Rcc.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_libcc.cc.o: ../../../core/libcc/libcc.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_libdrd.cc.o: ../../libdrd/libdrd.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_miniz.c.o: ../../../../vendor/miniz/miniz.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -Wno-pedantic -c -o $@ $<
