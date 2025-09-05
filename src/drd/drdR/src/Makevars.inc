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

PKG_CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D__STDC_FORMAT_MACROS -DLIBDRD_NO_WREN \
				-fno-strict-aliasing -fvisibility=hidden -fno-semantic-interposition \
				-I . -I ../../../.. -Wstrict-overflow=0
PKG_CFLAGS += -std=gnu99
PKG_CXXFLAGS += -std=gnu++20

all:
	@mkdir -p ../R
	cp ../drdR_mco.R ../R/drdR_mco.R

override OBJECTS = $(addprefix ./${R_ARCH}_, drdR_mco.cc.o Rcc.cc.o base.cc.o miniz.cc.o mco_authorization.cc.o mco_classifier.cc.o\
                                             mco_mapper.cc.o mco_pricing.cc.o mco_stay.cc.o mco_table.cc.o miniz.c.o)

./${R_ARCH}_drdR_mco.cc.o: ../drdR_mco.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_Rcc.cc.o: ../../../core/wrap/Rcc.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_base.cc.o: ../../../core/base/base.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_miniz.cc.o: ../../../core/compress/miniz.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_mco_authorization.cc.o: ../../libdrd/mco_authorization.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_mco_classifier.cc.o: ../../libdrd/mco_classifier.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_mco_mapper.cc.o: ../../libdrd/mco_mapper.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_mco_pricing.cc.o: ../../libdrd/mco_pricing.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_mco_stay.cc.o: ../../libdrd/mco_stay.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_mco_table.cc.o: ../../libdrd/mco_table.cc
	$(CXX) $(ALL_CPPFLAGS) $(ALL_CXXFLAGS) -Wno-pedantic -c -o $@ $<
./${R_ARCH}_miniz.c.o: ../../../../vendor/miniz/miniz.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -Wno-pedantic -c -o $@ $<
