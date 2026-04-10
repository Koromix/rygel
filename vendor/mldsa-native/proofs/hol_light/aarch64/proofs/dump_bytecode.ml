(*
 * Copyright (c) The mldsa-native project authors
 * SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT
 *)

(* Load base theories for AArch64 from s2n-bignum *)
needs "arm/proofs/base.ml";;

print_string "=== bytecode start: aarch64/mldsa/mldsa_ntt.o ===\n";;
print_literal_from_elf "aarch64/mldsa/mldsa_ntt.o";;
print_string "==== bytecode end =====================================\n\n";;

print_string "=== bytecode start: aarch64/mldsa/mldsa_poly_caddq.o ===\n";;
print_literal_from_elf "aarch64/mldsa/mldsa_poly_caddq.o";;
print_string "==== bytecode end =====================================\n\n";;

print_string "=== bytecode start: aarch64/mldsa/mldsa_poly_chknorm.o ===\n";;
print_literal_from_elf "aarch64/mldsa/mldsa_poly_chknorm.o";;
print_string "==== bytecode end =====================================\n\n";;
