#!/usr/bin/env perl

# curves.pl
#
# Copyright (c) 2014-2016, ARM Limited, All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# This file is part of Mbed TLS (https://tls.mbed.org)
#
# Purpose
#
# To test the code dependencies on individual curves in each test suite. This
# is a verification step to ensure we don't ship test suites that do not work
# for some build options.
#
# The process is:
#       for each possible curve
#           build the library and test suites with the curve disabled
#           execute the test suites
#
# And any test suite with the wrong dependencies will fail.
#
# Usage: tests/scripts/curves.pl
#
# This script should be executed from the root of the project directory.
#
# For best effect, run either with cmake disabled, or cmake enabled in a mode
# that includes -Werror.

use warnings;
use strict;

-d 'library' && -d 'include' && -d 'tests' or die "Must be run from root\n";

my $sed_cmd = 's/^#define \(MBEDTLS_ECP_DP.*_ENABLED\)/\1/p';
my $config_h = 'include/mbedtls/config.h';
my @curves = split( /\s+/, `sed -n -e '$sed_cmd' $config_h` );

system( "cp $config_h $config_h.bak" ) and die;
sub abort {
    system( "mv $config_h.bak $config_h" ) and warn "$config_h not restored\n";
    # use an exit code between 1 and 124 for git bisect (die returns 255)
    warn $_[0];
    exit 1;
}

for my $curve (@curves) {
    system( "cp $config_h.bak $config_h" ) and die "$config_h not restored\n";
    system( "make clean" ) and die;

    # depends on a specific curve. Also, ignore error if it wasn't enabled
    system( "scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED" );

    print "\n******************************************\n";
    print "* Testing without curve: $curve\n";
    print "******************************************\n";
    $ENV{MBEDTLS_TEST_CONFIGURATION} = "-$curve";

    system( "scripts/config.py unset $curve" )
        and abort "Failed to disable $curve\n";

    system( "CFLAGS='-Werror -Wall -Wextra' make lib" )
        and abort "Failed to build lib: $curve\n";
    system( "make" ) and abort "Failed to build tests: $curve\n";
    system( "make test" ) and abort "Failed test suite: $curve\n";

}

system( "mv $config_h.bak $config_h" ) and die "$config_h not restored\n";
system( "make clean" ) and die;
exit 0;
