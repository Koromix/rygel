#!/bin/sh

set -e
cd $(dirname $0)

clang++ -O2 -Wall -std=gnu++20 -I ../../.. -o crc64_gen crc64_gen.cc ../base/base.cc

./crc64_gen > crc64.inc

rm crc64_gen
