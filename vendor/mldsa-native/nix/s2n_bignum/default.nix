# Copyright (c) The mlkem-native project authors
# Copyright (c) The mldsa-native project authors
# SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT
{ stdenv, fetchFromGitHub, writeText, ... }:
stdenv.mkDerivation rec {
  pname = "s2n_bignum";
  version = "3bfe68deb9fbdaf23be0a927c362a87a799adc28";
  src = fetchFromGitHub {
    owner = "awslabs";
    repo = "s2n-bignum";
    rev = "${version}";
    hash = "sha256-C3SdYZ/PhfTmHefz3klO3l/lbEgtA8Hq9wjDmjd+Fek=";
  };
  setupHook = writeText "setup-hook.sh" ''
    export S2N_BIGNUM_DIR="$1"
  '';
  patches = [ ];
  dontBuild = true;
  installPhase = ''
    mkdir -p $out
    cp -a . $out/
  '';
}
