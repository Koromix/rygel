(* Copyright (c) The mldsa-native project authors
   SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT *)

theory "ML-DSA_Compress"
  imports Barrett_Division
begin

section \<open>ML-DSA Decompose Specializations\<close>

text \<open>
  ML-DSA uses two parameter sets with different \<^verbatim>\<open>GAMMA2\<close> values:
  \<^item> Parameter set 65/87 (ML-DSA-65, ML-DSA-87): \<^verbatim>\<open>GAMMA2 = 261888\<close>, divisor \<^verbatim>\<open>2*GAMMA2 = 523776\<close>
  \<^item> Parameter set 44 (ML-DSA-44): \<^verbatim>\<open>GAMMA2 = 95232\<close>, divisor \<^verbatim>\<open>2*GAMMA2 = 190464\<close>

  The implementations use different Barrett division strategies:
  \<^item> C and AVX2: Factor out \<^verbatim>\<open>128\<close> first, then Barrett divide by \<^verbatim>\<open>4092\<close> or \<^verbatim>\<open>1488\<close>
  \<^item> AArch64: Barrett divide directly by \<^verbatim>\<open>523776\<close> or \<^verbatim>\<open>190464\<close>

  \<^bold>\<open>Limitation:\<close> The results below operate on unbounded integers (@{typ int}), not
  fixed-width machine words. Connecting these results to the actual implementations
  requires additional reasoning about word-level operations (e.g., \<^verbatim>\<open>sqdmulh\<close>,
  \<^verbatim>\<open>mulhi\<close>, overflow behavior) which is left for future work.
\<close>

subsection \<open>C and AVX2 implementations\<close>

text \<open>
  The C reference \<^file>\<open>../../../mldsa/src/rounding.h\<close> and AVX2 implementations
  \<^file>\<open>../../../dev/x86_64/src/poly_decompose_32_avx2.c\<close> and
  \<^file>\<open>../../../dev/x86_64/src/poly_decompose_88_avx2.c\<close>
  first compute \<^verbatim>\<open>ceil(f / 128)\<close>, then Barrett divide by \<^verbatim>\<open>B = 2*GAMMA2 / 128\<close>.
\<close>

corollary barrett_decompose_32_c_avx2:
  assumes \<open>a \<ge> 0\<close> and \<open>a < 65473\<close>
  shows \<open>round_half_down (rat_of_int a / 4092) = (a * 1025 + 2^21) div 2^22\<close>
  using barrett_division_correct[where b=4092 and N=22, simplified barrett_division, eval] assms by simp

corollary barrett_decompose_88_c_avx2:
  assumes \<open>a \<ge> 0\<close> and \<open>a < 65473\<close>
  shows \<open>round_half_down (rat_of_int a / 1488) = (a * 11275 + 2^23) div 2^24\<close>
  using barrett_division_correct[where b=1488 and N=24, simplified barrett_division, eval] assms by simp

subsection \<open>AArch64 implementation\<close>

text \<open>
  The AArch64 implementations
  \<^file>\<open>../../../dev/aarch64_clean/src/poly_decompose_32_asm.S\<close> and
  \<^file>\<open>../../../dev/aarch64_clean/src/poly_decompose_88_asm.S\<close>
  Barrett divide directly by \<^verbatim>\<open>2*GAMMA2\<close> using \<^verbatim>\<open>sqdmulh\<close> and \<^verbatim>\<open>srshr\<close>.
\<close>

corollary barrett_decompose_32_aarch64:
  assumes \<open>a \<ge> 0\<close> and \<open>a < 1099511627776\<close>
  shows \<open>round_half_down (rat_of_int a / 523776) = (a * 1074791425 + 2^48) div 2^49\<close>
  using barrett_division_correct[where b=523776 and N=49, simplified barrett_division, eval] assms by simp

corollary barrett_decompose_88_aarch64:
  assumes \<open>a \<ge> 0\<close> and \<open>a < 3926827242\<close>
  shows \<open>round_half_down (rat_of_int a / 190464) = (a * 1477838209 + 2^47) div 2^48\<close>
  using barrett_division_correct[where b=190464 and N=48, simplified barrett_division, eval] assms by simp

end
