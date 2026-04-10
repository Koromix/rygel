(*
 * Copyright (c) The mldsa-native project authors
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT-0
 *)

(* ========================================================================= *)
(* Forward number theoretic transform.                                       *)
(* ========================================================================= *)

needs "arm/proofs/base.ml";;
needs "common/mldsa_specs.ml";;
needs "aarch64/proofs/aarch64_utils.ml";;
needs "aarch64/proofs/mldsa_zetas.ml";;

(**** print_literal_from_elf "aarch64/mldsa/mldsa_ntt.o";;
 ****)

let mldsa_ntt_mc = define_assert_from_elf
 "mldsa_ntt_mc" "aarch64/mldsa/mldsa_ntt.o"
(*** BYTECODE START ***)
[
  0xd10103ff;       (* arm_SUB SP SP (rvalue (word 64)) *)
  0x6d0027e8;       (* arm_STP D8 D9 SP (Immediate_Offset (iword (&0))) *)
  0x6d012fea;       (* arm_STP D10 D11 SP (Immediate_Offset (iword (&16))) *)
  0x6d0237ec;       (* arm_STP D12 D13 SP (Immediate_Offset (iword (&32))) *)
  0x6d033fee;       (* arm_STP D14 D15 SP (Immediate_Offset (iword (&48))) *)
  0x529c0025;       (* arm_MOV W5 (rvalue (word 57345)) *)
  0x72a00fe5;       (* arm_MOVK W5 (word 127) 16 *)
  0x4e040ca7;       (* arm_DUP_GEN Q7 X5 32 128 *)
  0xaa0003e3;       (* arm_MOV X3 X0 *)
  0xd2800104;       (* arm_MOV X4 (rvalue (word 8)) *)
  0x3cc40420;       (* arm_LDR Q0 X1 (Postimmediate_Offset (word 64)) *)
  0x3cdd0021;       (* arm_LDR Q1 X1 (Immediate_Offset (word 18446744073709551568)) *)
  0x3cde0022;       (* arm_LDR Q2 X1 (Immediate_Offset (word 18446744073709551584)) *)
  0x3cdf0023;       (* arm_LDR Q3 X1 (Immediate_Offset (word 18446744073709551600)) *)
  0x3dc0e417;       (* arm_LDR Q23 X0 (Immediate_Offset (word 912)) *)
  0x3dc0e00d;       (* arm_LDR Q13 X0 (Immediate_Offset (word 896)) *)
  0x3dc02016;       (* arm_LDR Q22 X0 (Immediate_Offset (word 128)) *)
  0x3dc0641a;       (* arm_LDR Q26 X0 (Immediate_Offset (word 400)) *)
  0x3dc0a008;       (* arm_LDR Q8 X0 (Immediate_Offset (word 640)) *)
  0x3dc08406;       (* arm_LDR Q6 X0 (Immediate_Offset (word 528)) *)
  0x4f8081aa;       (* arm_MUL_VEC Q10 Q13 (Q0 :> LANE_S 0) 32 128 *)
  0x4fa0d1ad;       (* arm_SQRDMULH_VEC Q13 Q13 (Q0 :> LANE_S 1) 32 128 *)
  0x4f80810c;       (* arm_MUL_VEC Q12 Q8 (Q0 :> LANE_S 0) 32 128 *)
  0x4fa0d11b;       (* arm_SQRDMULH_VEC Q27 Q8 (Q0 :> LANE_S 1) 32 128 *)
  0x4f8080c4;       (* arm_MUL_VEC Q4 Q6 (Q0 :> LANE_S 0) 32 128 *)
  0x6f8741aa;       (* arm_MLS_VEC Q10 Q13 (Q7 :> LANE_S 0) 32 128 *)
  0x3dc0600d;       (* arm_LDR Q13 X0 (Immediate_Offset (word 384)) *)
  0x4fa0d2ee;       (* arm_SQRDMULH_VEC Q14 Q23 (Q0 :> LANE_S 1) 32 128 *)
  0x6f87436c;       (* arm_MLS_VEC Q12 Q27 (Q7 :> LANE_S 0) 32 128 *)
  0x4eaa85bf;       (* arm_ADD_VEC Q31 Q13 Q10 32 128 *)
  0x6eaa85ad;       (* arm_SUB_VEC Q13 Q13 Q10 32 128 *)
  0x4f8082ea;       (* arm_MUL_VEC Q10 Q23 (Q0 :> LANE_S 0) 32 128 *)
  0x4fa1d1a8;       (* arm_SQRDMULH_VEC Q8 Q13 (Q1 :> LANE_S 1) 32 128 *)
  0x6eac86d2;       (* arm_SUB_VEC Q18 Q22 Q12 32 128 *)
  0x6f8741ca;       (* arm_MLS_VEC Q10 Q14 (Q7 :> LANE_S 0) 32 128 *)
  0x4f8181ad;       (* arm_MUL_VEC Q13 Q13 (Q1 :> LANE_S 0) 32 128 *)
  0x6f87410d;       (* arm_MLS_VEC Q13 Q8 (Q7 :> LANE_S 0) 32 128 *)
  0x6eaa875d;       (* arm_SUB_VEC Q29 Q26 Q10 32 128 *)
  0x4eaa8759;       (* arm_ADD_VEC Q25 Q26 Q10 32 128 *)
  0x4f808bea;       (* arm_MUL_VEC Q10 Q31 (Q0 :> LANE_S 2) 32 128 *)
  0x4f808b2e;       (* arm_MUL_VEC Q14 Q25 (Q0 :> LANE_S 2) 32 128 *)
  0x4ead8651;       (* arm_ADD_VEC Q17 Q18 Q13 32 128 *)
  0x6ead864f;       (* arm_SUB_VEC Q15 Q18 Q13 32 128 *)
  0x4fa0dbed;       (* arm_SQRDMULH_VEC Q13 Q31 (Q0 :> LANE_S 3) 32 128 *)
  0x4fa3d1f4;       (* arm_SQRDMULH_VEC Q20 Q15 (Q3 :> LANE_S 1) 32 128 *)
  0x4fa2da25;       (* arm_SQRDMULH_VEC Q5 Q17 (Q2 :> LANE_S 3) 32 128 *)
  0x6f8741aa;       (* arm_MLS_VEC Q10 Q13 (Q7 :> LANE_S 0) 32 128 *)
  0x3dc0c00d;       (* arm_LDR Q13 X0 (Immediate_Offset (word 768)) *)
  0x4f828a32;       (* arm_MUL_VEC Q18 Q17 (Q2 :> LANE_S 2) 32 128 *)
  0x4eac86df;       (* arm_ADD_VEC Q31 Q22 Q12 32 128 *)
  0x4f8381f7;       (* arm_MUL_VEC Q23 Q15 (Q3 :> LANE_S 0) 32 128 *)
  0x3dc02411;       (* arm_LDR Q17 X0 (Immediate_Offset (word 144)) *)
  0x4eaa87f3;       (* arm_ADD_VEC Q19 Q31 Q10 32 128 *)
  0x6eaa87f0;       (* arm_SUB_VEC Q16 Q31 Q10 32 128 *)
  0x4f8081aa;       (* arm_MUL_VEC Q10 Q13 (Q0 :> LANE_S 0) 32 128 *)
  0x4fa0d1ad;       (* arm_SQRDMULH_VEC Q13 Q13 (Q0 :> LANE_S 1) 32 128 *)
  0x4fa2d21b;       (* arm_SQRDMULH_VEC Q27 Q16 (Q2 :> LANE_S 1) 32 128 *)
  0x4f82820b;       (* arm_MUL_VEC Q11 Q16 (Q2 :> LANE_S 0) 32 128 *)
  0x6f8741aa;       (* arm_MLS_VEC Q10 Q13 (Q7 :> LANE_S 0) 32 128 *)
  0x3dc0a40d;       (* arm_LDR Q13 X0 (Immediate_Offset (word 656)) *)
  0x3dc04016;       (* arm_LDR Q22 X0 (Immediate_Offset (word 256)) *)
  0x6f87436b;       (* arm_MLS_VEC Q11 Q27 (Q7 :> LANE_S 0) 32 128 *)
  0x4fa0d1af;       (* arm_SQRDMULH_VEC Q15 Q13 (Q0 :> LANE_S 1) 32 128 *)
  0x6eaa86cc;       (* arm_SUB_VEC Q12 Q22 Q10 32 128 *)
  0x4eaa86de;       (* arm_ADD_VEC Q30 Q22 Q10 32 128 *)
  0x4f8081aa;       (* arm_MUL_VEC Q10 Q13 (Q0 :> LANE_S 0) 32 128 *)
  0x3dc0001c;       (* arm_LDR Q28 X0 (Immediate_Offset (word 0)) *)
  0x4fa0db2d;       (* arm_SQRDMULH_VEC Q13 Q25 (Q0 :> LANE_S 3) 32 128 *)
  0x4fa0dbdb;       (* arm_SQRDMULH_VEC Q27 Q30 (Q0 :> LANE_S 3) 32 128 *)
  0x6f8741ea;       (* arm_MLS_VEC Q10 Q15 (Q7 :> LANE_S 0) 32 128 *)
  0x6f8741ae;       (* arm_MLS_VEC Q14 Q13 (Q7 :> LANE_S 0) 32 128 *)
  0x3dc0800d;       (* arm_LDR Q13 X0 (Immediate_Offset (word 512)) *)
  0x4fa1d199;       (* arm_SQRDMULH_VEC Q25 Q12 (Q1 :> LANE_S 1) 32 128 *)
  0x4eaa8638;       (* arm_ADD_VEC Q24 Q17 Q10 32 128 *)
  0x6eaa8635;       (* arm_SUB_VEC Q21 Q17 Q10 32 128 *)
  0x4fa0d1a8;       (* arm_SQRDMULH_VEC Q8 Q13 (Q0 :> LANE_S 1) 32 128 *)
  0x6eae8709;       (* arm_SUB_VEC Q9 Q24 Q14 32 128 *)
  0x4f81819a;       (* arm_MUL_VEC Q26 Q12 (Q1 :> LANE_S 0) 32 128 *)
  0x4f8081ad;       (* arm_MUL_VEC Q13 Q13 (Q0 :> LANE_S 0) 32 128 *)
  0x6f87410d;       (* arm_MLS_VEC Q13 Q8 (Q7 :> LANE_S 0) 32 128 *)
  0x4f808bc8;       (* arm_MUL_VEC Q8 Q30 (Q0 :> LANE_S 2) 32 128 *)
  0x6f874368;       (* arm_MLS_VEC Q8 Q27 (Q7 :> LANE_S 0) 32 128 *)
  0x4ead8790;       (* arm_ADD_VEC Q16 Q28 Q13 32 128 *)
  0x6ead878a;       (* arm_SUB_VEC Q10 Q28 Q13 32 128 *)
  0x6f87433a;       (* arm_MLS_VEC Q26 Q25 (Q7 :> LANE_S 0) 32 128 *)
  0x4fa1da6c;       (* arm_SQRDMULH_VEC Q12 Q19 (Q1 :> LANE_S 3) 32 128 *)
  0x6ea88619;       (* arm_SUB_VEC Q25 Q16 Q8 32 128 *)
  0x6f874297;       (* arm_MLS_VEC Q23 Q20 (Q7 :> LANE_S 0) 32 128 *)
  0x6eab8736;       (* arm_SUB_VEC Q22 Q25 Q11 32 128 *)
  0x4fa2d134;       (* arm_SQRDMULH_VEC Q20 Q9 (Q2 :> LANE_S 1) 32 128 *)
  0x6eba854f;       (* arm_SUB_VEC Q15 Q10 Q26 32 128 *)
  0xd1000884;       (* arm_SUB X4 X4 (rvalue (word 2)) *)
  0x4eba855f;       (* arm_ADD_VEC Q31 Q10 Q26 32 128 *)
  0x4f818a71;       (* arm_MUL_VEC Q17 Q19 (Q1 :> LANE_S 2) 32 128 *)
  0x4eb785fa;       (* arm_ADD_VEC Q26 Q15 Q23 32 128 *)
  0x3dc0a81e;       (* arm_LDR Q30 X0 (Immediate_Offset (word 672)) *)
  0x6eb785ed;       (* arm_SUB_VEC Q13 Q15 Q23 32 128 *)
  0x4f8183b7;       (* arm_MUL_VEC Q23 Q29 (Q1 :> LANE_S 0) 32 128 *)
  0x4eab8739;       (* arm_ADD_VEC Q25 Q25 Q11 32 128 *)
  0x3d806016;       (* arm_STR Q22 X0 (Immediate_Offset (word 384)) *)
  0x4f82812b;       (* arm_MUL_VEC Q11 Q9 (Q2 :> LANE_S 0) 32 128 *)
  0x3d80e00d;       (* arm_STR Q13 X0 (Immediate_Offset (word 896)) *)
  0x3dc0041c;       (* arm_LDR Q28 X0 (Immediate_Offset (word 16)) *)
  0x4ea8860a;       (* arm_ADD_VEC Q10 Q16 Q8 32 128 *)
  0x6f874191;       (* arm_MLS_VEC Q17 Q12 (Q7 :> LANE_S 0) 32 128 *)
  0x3dc0e80d;       (* arm_LDR Q13 X0 (Immediate_Offset (word 928)) *)
  0x3d80c01a;       (* arm_STR Q26 X0 (Immediate_Offset (word 768)) *)
  0x4fa0d3db;       (* arm_SQRDMULH_VEC Q27 Q30 (Q0 :> LANE_S 1) 32 128 *)
  0x6f8740b2;       (* arm_MLS_VEC Q18 Q5 (Q7 :> LANE_S 0) 32 128 *)
  0x3dc06809;       (* arm_LDR Q9 X0 (Immediate_Offset (word 416)) *)
  0x6eb18550;       (* arm_SUB_VEC Q16 Q10 Q17 32 128 *)
  0x4eb1854f;       (* arm_ADD_VEC Q15 Q10 Q17 32 128 *)
  0x4fa0d0ca;       (* arm_SQRDMULH_VEC Q10 Q6 (Q0 :> LANE_S 1) 32 128 *)
  0x3d802010;       (* arm_STR Q16 X0 (Immediate_Offset (word 128)) *)
  0x3c81040f;       (* arm_STR Q15 X0 (Postimmediate_Offset (word 16)) *)
  0x4fa0d1b3;       (* arm_SQRDMULH_VEC Q19 Q13 (Q0 :> LANE_S 1) 32 128 *)
  0x6eb287ef;       (* arm_SUB_VEC Q15 Q31 Q18 32 128 *)
  0x4f8081a8;       (* arm_MUL_VEC Q8 Q13 (Q0 :> LANE_S 0) 32 128 *)
  0x4eb287fa;       (* arm_ADD_VEC Q26 Q31 Q18 32 128 *)
  0x3d809c0f;       (* arm_STR Q15 X0 (Immediate_Offset (word 624)) *)
  0x4fa1d3ad;       (* arm_SQRDMULH_VEC Q13 Q29 (Q1 :> LANE_S 1) 32 128 *)
  0x3d807c1a;       (* arm_STR Q26 X0 (Immediate_Offset (word 496)) *)
  0x6f874268;       (* arm_MLS_VEC Q8 Q19 (Q7 :> LANE_S 0) 32 128 *)
  0x6f87428b;       (* arm_MLS_VEC Q11 Q20 (Q7 :> LANE_S 0) 32 128 *)
  0x6f8741b7;       (* arm_MLS_VEC Q23 Q13 (Q7 :> LANE_S 0) 32 128 *)
  0x4ea88536;       (* arm_ADD_VEC Q22 Q9 Q8 32 128 *)
  0x3dc08406;       (* arm_LDR Q6 X0 (Immediate_Offset (word 528)) *)
  0x6ea8853d;       (* arm_SUB_VEC Q29 Q9 Q8 32 128 *)
  0x4f8083d1;       (* arm_MUL_VEC Q17 Q30 (Q0 :> LANE_S 0) 32 128 *)
  0x3dc0c009;       (* arm_LDR Q9 X0 (Immediate_Offset (word 768)) *)
  0x4fa0dacd;       (* arm_SQRDMULH_VEC Q13 Q22 (Q0 :> LANE_S 3) 32 128 *)
  0x4eb786b2;       (* arm_ADD_VEC Q18 Q21 Q23 32 128 *)
  0x6f874144;       (* arm_MLS_VEC Q4 Q10 (Q7 :> LANE_S 0) 32 128 *)
  0x6eb786bf;       (* arm_SUB_VEC Q31 Q21 Q23 32 128 *)
  0x4fa3d3f0;       (* arm_SQRDMULH_VEC Q16 Q31 (Q3 :> LANE_S 1) 32 128 *)
  0x4eae8713;       (* arm_ADD_VEC Q19 Q24 Q14 32 128 *)
  0x4f808ace;       (* arm_MUL_VEC Q14 Q22 (Q0 :> LANE_S 2) 32 128 *)
  0x6ea4878a;       (* arm_SUB_VEC Q10 Q28 Q4 32 128 *)
  0x6f8741ae;       (* arm_MLS_VEC Q14 Q13 (Q7 :> LANE_S 0) 32 128 *)
  0x3dc0400d;       (* arm_LDR Q13 X0 (Immediate_Offset (word 256)) *)
  0x4fa0d136;       (* arm_SQRDMULH_VEC Q22 Q9 (Q0 :> LANE_S 1) 32 128 *)
  0x4f808128;       (* arm_MUL_VEC Q8 Q9 (Q0 :> LANE_S 0) 32 128 *)
  0x4f8383f7;       (* arm_MUL_VEC Q23 Q31 (Q3 :> LANE_S 0) 32 128 *)
  0x6f8742c8;       (* arm_MLS_VEC Q8 Q22 (Q7 :> LANE_S 0) 32 128 *)
  0x6f874217;       (* arm_MLS_VEC Q23 Q16 (Q7 :> LANE_S 0) 32 128 *)
  0x4ea48790;       (* arm_ADD_VEC Q16 Q28 Q4 32 128 *)
  0x3dc02416;       (* arm_LDR Q22 X0 (Immediate_Offset (word 144)) *)
  0x4f8080c4;       (* arm_MUL_VEC Q4 Q6 (Q0 :> LANE_S 0) 32 128 *)
  0x6f874371;       (* arm_MLS_VEC Q17 Q27 (Q7 :> LANE_S 0) 32 128 *)
  0x4ea885b5;       (* arm_ADD_VEC Q21 Q13 Q8 32 128 *)
  0x6ea885bb;       (* arm_SUB_VEC Q27 Q13 Q8 32 128 *)
  0x4fa0dabf;       (* arm_SQRDMULH_VEC Q31 Q21 (Q0 :> LANE_S 3) 32 128 *)
  0x3d803c19;       (* arm_STR Q25 X0 (Immediate_Offset (word 240)) *)
  0x4f808aa8;       (* arm_MUL_VEC Q8 Q21 (Q0 :> LANE_S 2) 32 128 *)
  0x4eb186d8;       (* arm_ADD_VEC Q24 Q22 Q17 32 128 *)
  0x6eb186d5;       (* arm_SUB_VEC Q21 Q22 Q17 32 128 *)
  0x4fa2da45;       (* arm_SQRDMULH_VEC Q5 Q18 (Q2 :> LANE_S 3) 32 128 *)
  0x6f8743e8;       (* arm_MLS_VEC Q8 Q31 (Q7 :> LANE_S 0) 32 128 *)
  0x6eae8709;       (* arm_SUB_VEC Q9 Q24 Q14 32 128 *)
  0x4fa1d374;       (* arm_SQRDMULH_VEC Q20 Q27 (Q1 :> LANE_S 1) 32 128 *)
  0x4f81837a;       (* arm_MUL_VEC Q26 Q27 (Q1 :> LANE_S 0) 32 128 *)
  0x6ea88619;       (* arm_SUB_VEC Q25 Q16 Q8 32 128 *)
  0x4f828a52;       (* arm_MUL_VEC Q18 Q18 (Q2 :> LANE_S 2) 32 128 *)
  0x6eab8736;       (* arm_SUB_VEC Q22 Q25 Q11 32 128 *)
  0x6f87429a;       (* arm_MLS_VEC Q26 Q20 (Q7 :> LANE_S 0) 32 128 *)
  0x4fa2d134;       (* arm_SQRDMULH_VEC Q20 Q9 (Q2 :> LANE_S 1) 32 128 *)
  0x4fa1da6c;       (* arm_SQRDMULH_VEC Q12 Q19 (Q1 :> LANE_S 3) 32 128 *)
  0x6eba854f;       (* arm_SUB_VEC Q15 Q10 Q26 32 128 *)
  0xf1000484;       (* arm_SUBS X4 X4 (rvalue (word 1)) *)
  0xb5fff664;       (* arm_CBNZ X4 (word 2096844) *)
  0x4eba854d;       (* arm_ADD_VEC Q13 Q10 Q26 32 128 *)
  0x6f8740b2;       (* arm_MLS_VEC Q18 Q5 (Q7 :> LANE_S 0) 32 128 *)
  0x3d806016;       (* arm_STR Q22 X0 (Immediate_Offset (word 384)) *)
  0x4ea8861b;       (* arm_ADD_VEC Q27 Q16 Q8 32 128 *)
  0x4f818a76;       (* arm_MUL_VEC Q22 Q19 (Q1 :> LANE_S 2) 32 128 *)
  0x4eae871a;       (* arm_ADD_VEC Q26 Q24 Q14 32 128 *)
  0x3dc0441f;       (* arm_LDR Q31 X0 (Immediate_Offset (word 272)) *)
  0x6eb785ee;       (* arm_SUB_VEC Q14 Q15 Q23 32 128 *)
  0x4eb785f1;       (* arm_ADD_VEC Q17 Q15 Q23 32 128 *)
  0x6f874196;       (* arm_MLS_VEC Q22 Q12 (Q7 :> LANE_S 0) 32 128 *)
  0x4eb285bc;       (* arm_ADD_VEC Q28 Q13 Q18 32 128 *)
  0x3d80e00e;       (* arm_STR Q14 X0 (Immediate_Offset (word 896)) *)
  0x4fa0d0d8;       (* arm_SQRDMULH_VEC Q24 Q6 (Q0 :> LANE_S 1) 32 128 *)
  0x4eab8725;       (* arm_ADD_VEC Q5 Q25 Q11 32 128 *)
  0x6eb285b3;       (* arm_SUB_VEC Q19 Q13 Q18 32 128 *)
  0x3d80c011;       (* arm_STR Q17 X0 (Immediate_Offset (word 768)) *)
  0x3d804005;       (* arm_STR Q5 X0 (Immediate_Offset (word 256)) *)
  0x4f828130;       (* arm_MUL_VEC Q16 Q9 (Q2 :> LANE_S 0) 32 128 *)
  0x3dc0c412;       (* arm_LDR Q18 X0 (Immediate_Offset (word 784)) *)
  0x3d80a013;       (* arm_STR Q19 X0 (Immediate_Offset (word 640)) *)
  0x6f874290;       (* arm_MLS_VEC Q16 Q20 (Q7 :> LANE_S 0) 32 128 *)
  0x3d80801c;       (* arm_STR Q28 X0 (Immediate_Offset (word 512)) *)
  0x4eb6876d;       (* arm_ADD_VEC Q13 Q27 Q22 32 128 *)
  0x3dc0040f;       (* arm_LDR Q15 X0 (Immediate_Offset (word 16)) *)
  0x6eb6876a;       (* arm_SUB_VEC Q10 Q27 Q22 32 128 *)
  0x6f874304;       (* arm_MLS_VEC Q4 Q24 (Q7 :> LANE_S 0) 32 128 *)
  0x3c81040d;       (* arm_STR Q13 X0 (Postimmediate_Offset (word 16)) *)
  0x3d801c0a;       (* arm_STR Q10 X0 (Immediate_Offset (word 112)) *)
  0x4fa1d3ac;       (* arm_SQRDMULH_VEC Q12 Q29 (Q1 :> LANE_S 1) 32 128 *)
  0x4f8183b7;       (* arm_MUL_VEC Q23 Q29 (Q1 :> LANE_S 0) 32 128 *)
  0x4f818b48;       (* arm_MUL_VEC Q8 Q26 (Q1 :> LANE_S 2) 32 128 *)
  0x4ea485f4;       (* arm_ADD_VEC Q20 Q15 Q4 32 128 *)
  0x6ea485e6;       (* arm_SUB_VEC Q6 Q15 Q4 32 128 *)
  0x6f874197;       (* arm_MLS_VEC Q23 Q12 (Q7 :> LANE_S 0) 32 128 *)
  0x4fa0d256;       (* arm_SQRDMULH_VEC Q22 Q18 (Q0 :> LANE_S 1) 32 128 *)
  0x4f808245;       (* arm_MUL_VEC Q5 Q18 (Q0 :> LANE_S 0) 32 128 *)
  0x6eb786bc;       (* arm_SUB_VEC Q28 Q21 Q23 32 128 *)
  0x4fa1db4a;       (* arm_SQRDMULH_VEC Q10 Q26 (Q1 :> LANE_S 3) 32 128 *)
  0x6f8742c5;       (* arm_MLS_VEC Q5 Q22 (Q7 :> LANE_S 0) 32 128 *)
  0x4fa3d39e;       (* arm_SQRDMULH_VEC Q30 Q28 (Q3 :> LANE_S 1) 32 128 *)
  0x4eb786a4;       (* arm_ADD_VEC Q4 Q21 Q23 32 128 *)
  0x6f874148;       (* arm_MLS_VEC Q8 Q10 (Q7 :> LANE_S 0) 32 128 *)
  0x4ea587ec;       (* arm_ADD_VEC Q12 Q31 Q5 32 128 *)
  0x6ea587e9;       (* arm_SUB_VEC Q9 Q31 Q5 32 128 *)
  0x4fa2d899;       (* arm_SQRDMULH_VEC Q25 Q4 (Q2 :> LANE_S 3) 32 128 *)
  0x4fa1d12f;       (* arm_SQRDMULH_VEC Q15 Q9 (Q1 :> LANE_S 1) 32 128 *)
  0x4fa0d99f;       (* arm_SQRDMULH_VEC Q31 Q12 (Q0 :> LANE_S 3) 32 128 *)
  0x4f808992;       (* arm_MUL_VEC Q18 Q12 (Q0 :> LANE_S 2) 32 128 *)
  0x4f81812b;       (* arm_MUL_VEC Q11 Q9 (Q1 :> LANE_S 0) 32 128 *)
  0x6f8743f2;       (* arm_MLS_VEC Q18 Q31 (Q7 :> LANE_S 0) 32 128 *)
  0x4f82889d;       (* arm_MUL_VEC Q29 Q4 (Q2 :> LANE_S 2) 32 128 *)
  0x6f87433d;       (* arm_MLS_VEC Q29 Q25 (Q7 :> LANE_S 0) 32 128 *)
  0x4eb28697;       (* arm_ADD_VEC Q23 Q20 Q18 32 128 *)
  0x6f8741eb;       (* arm_MLS_VEC Q11 Q15 (Q7 :> LANE_S 0) 32 128 *)
  0x6eb2869f;       (* arm_SUB_VEC Q31 Q20 Q18 32 128 *)
  0x4ea886f1;       (* arm_ADD_VEC Q17 Q23 Q8 32 128 *)
  0x4eb087e5;       (* arm_ADD_VEC Q5 Q31 Q16 32 128 *)
  0x4f838398;       (* arm_MUL_VEC Q24 Q28 (Q3 :> LANE_S 0) 32 128 *)
  0x3c810411;       (* arm_STR Q17 X0 (Postimmediate_Offset (word 16)) *)
  0x6eb087f3;       (* arm_SUB_VEC Q19 Q31 Q16 32 128 *)
  0x6f8743d8;       (* arm_MLS_VEC Q24 Q30 (Q7 :> LANE_S 0) 32 128 *)
  0x3d803c05;       (* arm_STR Q5 X0 (Immediate_Offset (word 240)) *)
  0x4eab84df;       (* arm_ADD_VEC Q31 Q6 Q11 32 128 *)
  0x6ea886fa;       (* arm_SUB_VEC Q26 Q23 Q8 32 128 *)
  0x3d805c13;       (* arm_STR Q19 X0 (Immediate_Offset (word 368)) *)
  0x4ebd87e4;       (* arm_ADD_VEC Q4 Q31 Q29 32 128 *)
  0x6eab84cd;       (* arm_SUB_VEC Q13 Q6 Q11 32 128 *)
  0x3d801c1a;       (* arm_STR Q26 X0 (Immediate_Offset (word 112)) *)
  0x6ebd87eb;       (* arm_SUB_VEC Q11 Q31 Q29 32 128 *)
  0x6eb885b6;       (* arm_SUB_VEC Q22 Q13 Q24 32 128 *)
  0x4eb885b7;       (* arm_ADD_VEC Q23 Q13 Q24 32 128 *)
  0x3d807c04;       (* arm_STR Q4 X0 (Immediate_Offset (word 496)) *)
  0x3d809c0b;       (* arm_STR Q11 X0 (Immediate_Offset (word 624)) *)
  0x3d80bc17;       (* arm_STR Q23 X0 (Immediate_Offset (word 752)) *)
  0x3d80dc16;       (* arm_STR Q22 X0 (Immediate_Offset (word 880)) *)
  0xaa0303e0;       (* arm_MOV X0 X3 *)
  0xd2800104;       (* arm_MOV X4 (rvalue (word 8)) *)
  0x3dc01009;       (* arm_LDR Q9 X0 (Immediate_Offset (word 64)) *)
  0x3cc40437;       (* arm_LDR Q23 X1 (Postimmediate_Offset (word 64)) *)
  0x3dc01855;       (* arm_LDR Q21 X2 (Immediate_Offset (word 96)) *)
  0x3dc00801;       (* arm_LDR Q1 X0 (Immediate_Offset (word 32)) *)
  0x3cdd002e;       (* arm_LDR Q14 X1 (Immediate_Offset (word 18446744073709551568)) *)
  0x3dc0000d;       (* arm_LDR Q13 X0 (Immediate_Offset (word 0)) *)
  0x3dc0144b;       (* arm_LDR Q11 X2 (Immediate_Offset (word 80)) *)
  0x4fb7d130;       (* arm_SQRDMULH_VEC Q16 Q9 (Q23 :> LANE_S 1) 32 128 *)
  0x3dc01411;       (* arm_LDR Q17 X0 (Immediate_Offset (word 80)) *)
  0x4f97812f;       (* arm_MUL_VEC Q15 Q9 (Q23 :> LANE_S 0) 32 128 *)
  0x3dc01c1e;       (* arm_LDR Q30 X0 (Immediate_Offset (word 112)) *)
  0x3dc0181b;       (* arm_LDR Q27 X0 (Immediate_Offset (word 96)) *)
  0x3dc00c48;       (* arm_LDR Q8 X2 (Immediate_Offset (word 48)) *)
  0x4fb7d22c;       (* arm_SQRDMULH_VEC Q12 Q17 (Q23 :> LANE_S 1) 32 128 *)
  0x3dc00c06;       (* arm_LDR Q6 X0 (Immediate_Offset (word 48)) *)
  0x6f87420f;       (* arm_MLS_VEC Q15 Q16 (Q7 :> LANE_S 0) 32 128 *)
  0x4fb7d372;       (* arm_SQRDMULH_VEC Q18 Q27 (Q23 :> LANE_S 1) 32 128 *)
  0x4fb7d3d3;       (* arm_SQRDMULH_VEC Q19 Q30 (Q23 :> LANE_S 1) 32 128 *)
  0x4eaf85a5;       (* arm_ADD_VEC Q5 Q13 Q15 32 128 *)
  0x4f978379;       (* arm_MUL_VEC Q25 Q27 (Q23 :> LANE_S 0) 32 128 *)
  0x6eaf85ba;       (* arm_SUB_VEC Q26 Q13 Q15 32 128 *)
  0x6f874259;       (* arm_MLS_VEC Q25 Q18 (Q7 :> LANE_S 0) 32 128 *)
  0x4f97822a;       (* arm_MUL_VEC Q10 Q17 (Q23 :> LANE_S 0) 32 128 *)
  0x6f87418a;       (* arm_MLS_VEC Q10 Q12 (Q7 :> LANE_S 0) 32 128 *)
  0x4f9783c4;       (* arm_MUL_VEC Q4 Q30 (Q23 :> LANE_S 0) 32 128 *)
  0x6eb98436;       (* arm_SUB_VEC Q22 Q1 Q25 32 128 *)
  0x6f874264;       (* arm_MLS_VEC Q4 Q19 (Q7 :> LANE_S 0) 32 128 *)
  0x4eb9843c;       (* arm_ADD_VEC Q28 Q1 Q25 32 128 *)
  0x4fb7db93;       (* arm_SQRDMULH_VEC Q19 Q28 (Q23 :> LANE_S 3) 32 128 *)
  0x4faed2c9;       (* arm_SQRDMULH_VEC Q9 Q22 (Q14 :> LANE_S 1) 32 128 *)
  0x4ea484c2;       (* arm_ADD_VEC Q2 Q6 Q4 32 128 *)
  0x4f978b80;       (* arm_MUL_VEC Q0 Q28 (Q23 :> LANE_S 2) 32 128 *)
  0x4fb7d85b;       (* arm_SQRDMULH_VEC Q27 Q2 (Q23 :> LANE_S 3) 32 128 *)
  0x6ea484d1;       (* arm_SUB_VEC Q17 Q6 Q4 32 128 *)
  0x4f978843;       (* arm_MUL_VEC Q3 Q2 (Q23 :> LANE_S 2) 32 128 *)
  0x4faed234;       (* arm_SQRDMULH_VEC Q20 Q17 (Q14 :> LANE_S 1) 32 128 *)
  0x3dc00401;       (* arm_LDR Q1 X0 (Immediate_Offset (word 16)) *)
  0x6f874363;       (* arm_MLS_VEC Q3 Q27 (Q7 :> LANE_S 0) 32 128 *)
  0x6f874260;       (* arm_MLS_VEC Q0 Q19 (Q7 :> LANE_S 0) 32 128 *)
  0x3cde0030;       (* arm_LDR Q16 X1 (Immediate_Offset (word 18446744073709551584)) *)
  0x4eaa843f;       (* arm_ADD_VEC Q31 Q1 Q10 32 128 *)
  0x4f8e823e;       (* arm_MUL_VEC Q30 Q17 (Q14 :> LANE_S 0) 32 128 *)
  0x6f87429e;       (* arm_MLS_VEC Q30 Q20 (Q7 :> LANE_S 0) 32 128 *)
  0x4ea387fb;       (* arm_ADD_VEC Q27 Q31 Q3 32 128 *)
  0x6eaa8437;       (* arm_SUB_VEC Q23 Q1 Q10 32 128 *)
  0x6ea387f8;       (* arm_SUB_VEC Q24 Q31 Q3 32 128 *)
  0x4faedb64;       (* arm_SQRDMULH_VEC Q4 Q27 (Q14 :> LANE_S 3) 32 128 *)
  0x4fb0d30a;       (* arm_SQRDMULH_VEC Q10 Q24 (Q16 :> LANE_S 1) 32 128 *)
  0x4f908312;       (* arm_MUL_VEC Q18 Q24 (Q16 :> LANE_S 0) 32 128 *)
  0x4ebe86ef;       (* arm_ADD_VEC Q15 Q23 Q30 32 128 *)
  0x6ebe86f7;       (* arm_SUB_VEC Q23 Q23 Q30 32 128 *)
  0x4f8e8b7d;       (* arm_MUL_VEC Q29 Q27 (Q14 :> LANE_S 2) 32 128 *)
  0x6ea084a2;       (* arm_SUB_VEC Q2 Q5 Q0 32 128 *)
  0x4ea084ac;       (* arm_ADD_VEC Q12 Q5 Q0 32 128 *)
  0x6f874152;       (* arm_MLS_VEC Q18 Q10 (Q7 :> LANE_S 0) 32 128 *)
  0x3cdf0023;       (* arm_LDR Q3 X1 (Immediate_Offset (word 18446744073709551600)) *)
  0x6f87409d;       (* arm_MLS_VEC Q29 Q4 (Q7 :> LANE_S 0) 32 128 *)
  0x4f8e82c4;       (* arm_MUL_VEC Q4 Q22 (Q14 :> LANE_S 0) 32 128 *)
  0x4eb28441;       (* arm_ADD_VEC Q1 Q2 Q18 32 128 *)
  0x6eb28458;       (* arm_SUB_VEC Q24 Q2 Q18 32 128 *)
  0x6f874124;       (* arm_MLS_VEC Q4 Q9 (Q7 :> LANE_S 0) 32 128 *)
  0x3dc00454;       (* arm_LDR Q20 X2 (Immediate_Offset (word 16)) *)
  0x4ebd8599;       (* arm_ADD_VEC Q25 Q12 Q29 32 128 *)
  0x4f8382e9;       (* arm_MUL_VEC Q9 Q23 (Q3 :> LANE_S 0) 32 128 *)
  0x6ebd8585;       (* arm_SUB_VEC Q5 Q12 Q29 32 128 *)
  0x4fa3d2ff;       (* arm_SQRDMULH_VEC Q31 Q23 (Q3 :> LANE_S 1) 32 128 *)
  0x4e986826;       (* arm_TRN2 Q6 Q1 Q24 32 128 *)
  0x4e856b2a;       (* arm_TRN2 Q10 Q25 Q5 32 128 *)
  0x4fb0d9ed;       (* arm_SQRDMULH_VEC Q13 Q15 (Q16 :> LANE_S 3) 32 128 *)
  0x4ec6695e;       (* arm_TRN2 Q30 Q10 Q6 64 128 *)
  0x3ccc0443;       (* arm_LDR Q3 X2 (Postimmediate_Offset (word 192)) *)
  0x4f9089ec;       (* arm_MUL_VEC Q12 Q15 (Q16 :> LANE_S 2) 32 128 *)
  0x4ec6295b;       (* arm_TRN1 Q27 Q10 Q6 64 128 *)
  0x6f8743e9;       (* arm_MLS_VEC Q9 Q31 (Q7 :> LANE_S 0) 32 128 *)
  0x4e852b36;       (* arm_TRN1 Q22 Q25 Q5 32 128 *)
  0x6ea48746;       (* arm_SUB_VEC Q6 Q26 Q4 32 128 *)
  0x6f8741ac;       (* arm_MLS_VEC Q12 Q13 (Q7 :> LANE_S 0) 32 128 *)
  0x4e982821;       (* arm_TRN1 Q1 Q1 Q24 32 128 *)
  0x4ea4874d;       (* arm_ADD_VEC Q13 Q26 Q4 32 128 *)
  0x4ec16aca;       (* arm_TRN2 Q10 Q22 Q1 64 128 *)
  0x4ea39fdc;       (* arm_MUL_VEC Q28 Q30 Q3 32 128 *)
  0x6ea984df;       (* arm_SUB_VEC Q31 Q6 Q9 32 128 *)
  0xd1000484;       (* arm_SUB X4 X4 (rvalue (word 1)) *)
  0x4eac85a2;       (* arm_ADD_VEC Q2 Q13 Q12 32 128 *)
  0x6eb4b7c5;       (* arm_SQRDMULH_VEC Q5 Q30 Q20 32 128 *)
  0x6eac85b9;       (* arm_SUB_VEC Q25 Q13 Q12 32 128 *)
  0x4ea984d1;       (* arm_ADD_VEC Q17 Q6 Q9 32 128 *)
  0x4ea39d53;       (* arm_MUL_VEC Q19 Q10 Q3 32 128 *)
  0x4e996844;       (* arm_TRN2 Q4 Q2 Q25 32 128 *)
  0x3cdb0058;       (* arm_LDR Q24 X2 (Immediate_Offset (word 18446744073709551536)) *)
  0x4e9f6a3d;       (* arm_TRN2 Q29 Q17 Q31 32 128 *)
  0x6eb4b54f;       (* arm_SQRDMULH_VEC Q15 Q10 Q20 32 128 *)
  0x6f8740bc;       (* arm_MLS_VEC Q28 Q5 (Q7 :> LANE_S 0) 32 128 *)
  0x4edd6883;       (* arm_TRN2 Q3 Q4 Q29 64 128 *)
  0x6eb8b46c;       (* arm_SQRDMULH_VEC Q12 Q3 Q24 32 128 *)
  0x4eb59c70;       (* arm_MUL_VEC Q16 Q3 Q21 32 128 *)
  0x6f8741f3;       (* arm_MLS_VEC Q19 Q15 (Q7 :> LANE_S 0) 32 128 *)
  0x3cd6004a;       (* arm_LDR Q10 X2 (Immediate_Offset (word 18446744073709551456)) *)
  0x4ebc876d;       (* arm_ADD_VEC Q13 Q27 Q28 32 128 *)
  0x6f874190;       (* arm_MLS_VEC Q16 Q12 (Q7 :> LANE_S 0) 32 128 *)
  0x6ea8b5a9;       (* arm_SQRDMULH_VEC Q9 Q13 Q8 32 128 *)
  0x6ebc877e;       (* arm_SUB_VEC Q30 Q27 Q28 32 128 *)
  0x3cc40432;       (* arm_LDR Q18 X1 (Postimmediate_Offset (word 64)) *)
  0x4eaa9da8;       (* arm_MUL_VEC Q8 Q13 Q10 32 128 *)
  0x3dc0340a;       (* arm_LDR Q10 X0 (Immediate_Offset (word 208)) *)
  0x6eabb7ce;       (* arm_SQRDMULH_VEC Q14 Q30 Q11 32 128 *)
  0x3dc03817;       (* arm_LDR Q23 X0 (Immediate_Offset (word 224)) *)
  0x4fb2d14d;       (* arm_SQRDMULH_VEC Q13 Q10 (Q18 :> LANE_S 1) 32 128 *)
  0x4fb2d2ec;       (* arm_SQRDMULH_VEC Q12 Q23 (Q18 :> LANE_S 1) 32 128 *)
  0x3cd80046;       (* arm_LDR Q6 X2 (Immediate_Offset (word 18446744073709551488)) *)
  0x4f928143;       (* arm_MUL_VEC Q3 Q10 (Q18 :> LANE_S 0) 32 128 *)
  0x6f8741a3;       (* arm_MLS_VEC Q3 Q13 (Q7 :> LANE_S 0) 32 128 *)
  0x3dc03c0d;       (* arm_LDR Q13 X0 (Immediate_Offset (word 240)) *)
  0x4e99285b;       (* arm_TRN1 Q27 Q2 Q25 32 128 *)
  0x4ea69fc2;       (* arm_MUL_VEC Q2 Q30 Q6 32 128 *)
  0x4e9f2a34;       (* arm_TRN1 Q20 Q17 Q31 32 128 *)
  0x4edd2899;       (* arm_TRN1 Q25 Q4 Q29 64 128 *)
  0x4fb2d1aa;       (* arm_SQRDMULH_VEC Q10 Q13 (Q18 :> LANE_S 1) 32 128 *)
  0x4ed46b65;       (* arm_TRN2 Q5 Q27 Q20 64 128 *)
  0x3cdf0046;       (* arm_LDR Q6 X2 (Immediate_Offset (word 18446744073709551600)) *)
  0x6f874128;       (* arm_MLS_VEC Q8 Q9 (Q7 :> LANE_S 0) 32 128 *)
  0x6eb0872f;       (* arm_SUB_VEC Q15 Q25 Q16 32 128 *)
  0x6eb8b4bf;       (* arm_SQRDMULH_VEC Q31 Q5 Q24 32 128 *)
  0x6ea6b5fe;       (* arm_SQRDMULH_VEC Q30 Q15 Q6 32 128 *)
  0x3cdd0049;       (* arm_LDR Q9 X2 (Immediate_Offset (word 18446744073709551568)) *)
  0x4eb59ca4;       (* arm_MUL_VEC Q4 Q5 Q21 32 128 *)
  0x3cdc0055;       (* arm_LDR Q21 X2 (Immediate_Offset (word 18446744073709551552)) *)
  0x3cde0046;       (* arm_LDR Q6 X2 (Immediate_Offset (word 18446744073709551584)) *)
  0x4eb08725;       (* arm_ADD_VEC Q5 Q25 Q16 32 128 *)
  0x6f8743e4;       (* arm_MLS_VEC Q4 Q31 (Q7 :> LANE_S 0) 32 128 *)
  0x4eb59ca0;       (* arm_MUL_VEC Q0 Q5 Q21 32 128 *)
  0x4f9281b1;       (* arm_MUL_VEC Q17 Q13 (Q18 :> LANE_S 0) 32 128 *)
  0x6f874151;       (* arm_MLS_VEC Q17 Q10 (Q7 :> LANE_S 0) 32 128 *)
  0x3dc02c1c;       (* arm_LDR Q28 X0 (Immediate_Offset (word 176)) *)
  0x6ea9b4ba;       (* arm_SQRDMULH_VEC Q26 Q5 Q9 32 128 *)
  0x4ea69de9;       (* arm_MUL_VEC Q9 Q15 Q6 32 128 *)
  0x4ec12ac6;       (* arm_TRN1 Q6 Q22 Q1 64 128 *)
  0x6f8743c9;       (* arm_MLS_VEC Q9 Q30 (Q7 :> LANE_S 0) 32 128 *)
  0x4eb18799;       (* arm_ADD_VEC Q25 Q28 Q17 32 128 *)
  0x6f8741c2;       (* arm_MLS_VEC Q2 Q14 (Q7 :> LANE_S 0) 32 128 *)
  0x4ed42b65;       (* arm_TRN1 Q5 Q27 Q20 64 128 *)
  0x3dc00454;       (* arm_LDR Q20 X2 (Immediate_Offset (word 16)) *)
  0x4f928b3d;       (* arm_MUL_VEC Q29 Q25 (Q18 :> LANE_S 2) 32 128 *)
  0x4eb384cf;       (* arm_ADD_VEC Q15 Q6 Q19 32 128 *)
  0x3dc0301e;       (* arm_LDR Q30 X0 (Immediate_Offset (word 192)) *)
  0x6eb384d3;       (* arm_SUB_VEC Q19 Q6 Q19 32 128 *)
  0x4ea885ff;       (* arm_ADD_VEC Q31 Q15 Q8 32 128 *)
  0x6f874340;       (* arm_MLS_VEC Q0 Q26 (Q7 :> LANE_S 0) 32 128 *)
  0x3cdd002e;       (* arm_LDR Q14 X1 (Immediate_Offset (word 18446744073709551568)) *)
  0x4ea28675;       (* arm_ADD_VEC Q21 Q19 Q2 32 128 *)
  0x6ea28678;       (* arm_SUB_VEC Q24 Q19 Q2 32 128 *)
  0x4fb2db3b;       (* arm_SQRDMULH_VEC Q27 Q25 (Q18 :> LANE_S 3) 32 128 *)
  0x6ea885fa;       (* arm_SUB_VEC Q26 Q15 Q8 32 128 *)
  0x3dc02402;       (* arm_LDR Q2 X0 (Immediate_Offset (word 144)) *)
  0x4f9283d0;       (* arm_MUL_VEC Q16 Q30 (Q18 :> LANE_S 0) 32 128 *)
  0x6eb18799;       (* arm_SUB_VEC Q25 Q28 Q17 32 128 *)
  0x4e9a2beb;       (* arm_TRN1 Q11 Q31 Q26 32 128 *)
  0x3dc02801;       (* arm_LDR Q1 X0 (Immediate_Offset (word 160)) *)
  0x4e982aa6;       (* arm_TRN1 Q6 Q21 Q24 32 128 *)
  0x4faed32d;       (* arm_SQRDMULH_VEC Q13 Q25 (Q14 :> LANE_S 1) 32 128 *)
  0x4ea38448;       (* arm_ADD_VEC Q8 Q2 Q3 32 128 *)
  0x4ec6697c;       (* arm_TRN2 Q28 Q11 Q6 64 128 *)
  0x4fb2d3d3;       (* arm_SQRDMULH_VEC Q19 Q30 (Q18 :> LANE_S 1) 32 128 *)
  0x6ea484aa;       (* arm_SUB_VEC Q10 Q5 Q4 32 128 *)
  0x3cde0036;       (* arm_LDR Q22 X1 (Immediate_Offset (word 18446744073709551584)) *)
  0x3d80081c;       (* arm_STR Q28 X0 (Immediate_Offset (word 32)) *)
  0x6f87437d;       (* arm_MLS_VEC Q29 Q27 (Q7 :> LANE_S 0) 32 128 *)
  0x4ea9854f;       (* arm_ADD_VEC Q15 Q10 Q9 32 128 *)
  0x4f8e8339;       (* arm_MUL_VEC Q25 Q25 (Q14 :> LANE_S 0) 32 128 *)
  0x3cdf003b;       (* arm_LDR Q27 X1 (Immediate_Offset (word 18446744073709551600)) *)
  0x4e9a6bf1;       (* arm_TRN2 Q17 Q31 Q26 32 128 *)
  0x4e986ab5;       (* arm_TRN2 Q21 Q21 Q24 32 128 *)
  0x6f874270;       (* arm_MLS_VEC Q16 Q19 (Q7 :> LANE_S 0) 32 128 *)
  0x6ebd8518;       (* arm_SUB_VEC Q24 Q8 Q29 32 128 *)
  0x6ea9854a;       (* arm_SUB_VEC Q10 Q10 Q9 32 128 *)
  0x6f8741b9;       (* arm_MLS_VEC Q25 Q13 (Q7 :> LANE_S 0) 32 128 *)
  0x4ec6296d;       (* arm_TRN1 Q13 Q11 Q6 64 128 *)
  0x3dc0201c;       (* arm_LDR Q28 X0 (Immediate_Offset (word 128)) *)
  0x4fb6d31e;       (* arm_SQRDMULH_VEC Q30 Q24 (Q22 :> LANE_S 1) 32 128 *)
  0x4ed56a33;       (* arm_TRN2 Q19 Q17 Q21 64 128 *)
  0x4ed52a26;       (* arm_TRN1 Q6 Q17 Q21 64 128 *)
  0x4f9282ff;       (* arm_MUL_VEC Q31 Q23 (Q18 :> LANE_S 0) 32 128 *)
  0x3c88040d;       (* arm_STR Q13 X0 (Postimmediate_Offset (word 128)) *)
  0x3c990006;       (* arm_STR Q6 X0 (Immediate_Offset (word 18446744073709551504)) *)
  0x3c9b0013;       (* arm_STR Q19 X0 (Immediate_Offset (word 18446744073709551536)) *)
  0x3dc0144b;       (* arm_LDR Q11 X2 (Immediate_Offset (word 80)) *)
  0x6f87419f;       (* arm_MLS_VEC Q31 Q12 (Q7 :> LANE_S 0) 32 128 *)
  0x3dc01855;       (* arm_LDR Q21 X2 (Immediate_Offset (word 96)) *)
  0x4e8a29e9;       (* arm_TRN1 Q9 Q15 Q10 32 128 *)
  0x4e8a69e6;       (* arm_TRN2 Q6 Q15 Q10 32 128 *)
  0x4f968318;       (* arm_MUL_VEC Q24 Q24 (Q22 :> LANE_S 0) 32 128 *)
  0x6ea3844a;       (* arm_SUB_VEC Q10 Q2 Q3 32 128 *)
  0x3ccc0443;       (* arm_LDR Q3 X2 (Postimmediate_Offset (word 192)) *)
  0x6f8743d8;       (* arm_MLS_VEC Q24 Q30 (Q7 :> LANE_S 0) 32 128 *)
  0x4ebd851a;       (* arm_ADD_VEC Q26 Q8 Q29 32 128 *)
  0x3cd70048;       (* arm_LDR Q8 X2 (Immediate_Offset (word 18446744073709551472)) *)
  0x4ea484b1;       (* arm_ADD_VEC Q17 Q5 Q4 32 128 *)
  0x4faedb42;       (* arm_SQRDMULH_VEC Q2 Q26 (Q14 :> LANE_S 3) 32 128 *)
  0x6ebf842d;       (* arm_SUB_VEC Q13 Q1 Q31 32 128 *)
  0x4ebf843e;       (* arm_ADD_VEC Q30 Q1 Q31 32 128 *)
  0x4eb9854f;       (* arm_ADD_VEC Q15 Q10 Q25 32 128 *)
  0x4faed1b3;       (* arm_SQRDMULH_VEC Q19 Q13 (Q14 :> LANE_S 1) 32 128 *)
  0x6eb98559;       (* arm_SUB_VEC Q25 Q10 Q25 32 128 *)
  0x4f8e81bd;       (* arm_MUL_VEC Q29 Q13 (Q14 :> LANE_S 0) 32 128 *)
  0x6eb08785;       (* arm_SUB_VEC Q5 Q28 Q16 32 128 *)
  0x4fb2dbc4;       (* arm_SQRDMULH_VEC Q4 Q30 (Q18 :> LANE_S 3) 32 128 *)
  0x6ea08637;       (* arm_SUB_VEC Q23 Q17 Q0 32 128 *)
  0x4ea0863f;       (* arm_ADD_VEC Q31 Q17 Q0 32 128 *)
  0x4f928bd2;       (* arm_MUL_VEC Q18 Q30 (Q18 :> LANE_S 2) 32 128 *)
  0x4eb08781;       (* arm_ADD_VEC Q1 Q28 Q16 32 128 *)
  0x4e976bec;       (* arm_TRN2 Q12 Q31 Q23 32 128 *)
  0x6f87427d;       (* arm_MLS_VEC Q29 Q19 (Q7 :> LANE_S 0) 32 128 *)
  0x4e972bed;       (* arm_TRN1 Q13 Q31 Q23 32 128 *)
  0x4ec6699e;       (* arm_TRN2 Q30 Q12 Q6 64 128 *)
  0x6f874092;       (* arm_MLS_VEC Q18 Q4 (Q7 :> LANE_S 0) 32 128 *)
  0x4ec969aa;       (* arm_TRN2 Q10 Q13 Q9 64 128 *)
  0x4ec929bf;       (* arm_TRN1 Q31 Q13 Q9 64 128 *)
  0x4f8e8b53;       (* arm_MUL_VEC Q19 Q26 (Q14 :> LANE_S 2) 32 128 *)
  0x4ec6298c;       (* arm_TRN1 Q12 Q12 Q6 64 128 *)
  0x6ebd84a6;       (* arm_SUB_VEC Q6 Q5 Q29 32 128 *)
  0x6f874053;       (* arm_MLS_VEC Q19 Q2 (Q7 :> LANE_S 0) 32 128 *)
  0x4ebd84ad;       (* arm_ADD_VEC Q13 Q5 Q29 32 128 *)
  0x3c9e000a;       (* arm_STR Q10 X0 (Immediate_Offset (word 18446744073709551584)) *)
  0x6eb2842a;       (* arm_SUB_VEC Q10 Q1 Q18 32 128 *)
  0x4eb2843c;       (* arm_ADD_VEC Q28 Q1 Q18 32 128 *)
  0x4fbbd325;       (* arm_SQRDMULH_VEC Q5 Q25 (Q27 :> LANE_S 1) 32 128 *)
  0x3c9c001f;       (* arm_STR Q31 X0 (Immediate_Offset (word 18446744073709551552)) *)
  0x4eb8855a;       (* arm_ADD_VEC Q26 Q10 Q24 32 128 *)
  0x6eb8855f;       (* arm_SUB_VEC Q31 Q10 Q24 32 128 *)
  0x4f9b8329;       (* arm_MUL_VEC Q9 Q25 (Q27 :> LANE_S 0) 32 128 *)
  0x3c9d000c;       (* arm_STR Q12 X0 (Immediate_Offset (word 18446744073709551568)) *)
  0x6eb38798;       (* arm_SUB_VEC Q24 Q28 Q19 32 128 *)
  0x4fb6d9ea;       (* arm_SQRDMULH_VEC Q10 Q15 (Q22 :> LANE_S 3) 32 128 *)
  0x4e9f2b41;       (* arm_TRN1 Q1 Q26 Q31 32 128 *)
  0x3c9f001e;       (* arm_STR Q30 X0 (Immediate_Offset (word 18446744073709551600)) *)
  0x4eb3879e;       (* arm_ADD_VEC Q30 Q28 Q19 32 128 *)
  0x6f8740a9;       (* arm_MLS_VEC Q9 Q5 (Q7 :> LANE_S 0) 32 128 *)
  0x4e9f6b59;       (* arm_TRN2 Q25 Q26 Q31 32 128 *)
  0x4e986bce;       (* arm_TRN2 Q14 Q30 Q24 32 128 *)
  0x4f9689ec;       (* arm_MUL_VEC Q12 Q15 (Q22 :> LANE_S 2) 32 128 *)
  0x4e982bd6;       (* arm_TRN1 Q22 Q30 Q24 32 128 *)
  0x4ed929db;       (* arm_TRN1 Q27 Q14 Q25 64 128 *)
  0x6f87414c;       (* arm_MLS_VEC Q12 Q10 (Q7 :> LANE_S 0) 32 128 *)
  0x4ed969de;       (* arm_TRN2 Q30 Q14 Q25 64 128 *)
  0x6ea984df;       (* arm_SUB_VEC Q31 Q6 Q9 32 128 *)
  0x4ec16aca;       (* arm_TRN2 Q10 Q22 Q1 64 128 *)
  0x4ea39fdc;       (* arm_MUL_VEC Q28 Q30 Q3 32 128 *)
  0xf1000484;       (* arm_SUBS X4 X4 (rvalue (word 1)) *)
  0xb5ffeb64;       (* arm_CBNZ X4 (word 2096492) *)
  0x4ea984c9;       (* arm_ADD_VEC Q9 Q6 Q9 32 128 *)
  0x6eb4b7c6;       (* arm_SQRDMULH_VEC Q6 Q30 Q20 32 128 *)
  0x3cd60058;       (* arm_LDR Q24 X2 (Immediate_Offset (word 18446744073709551456)) *)
  0x4eac85b9;       (* arm_ADD_VEC Q25 Q13 Q12 32 128 *)
  0x6eac85af;       (* arm_SUB_VEC Q15 Q13 Q12 32 128 *)
  0x4ea39d53;       (* arm_MUL_VEC Q19 Q10 Q3 32 128 *)
  0x4e9f6925;       (* arm_TRN2 Q5 Q9 Q31 32 128 *)
  0x6eb4b543;       (* arm_SQRDMULH_VEC Q3 Q10 Q20 32 128 *)
  0x4e8f6b2a;       (* arm_TRN2 Q10 Q25 Q15 32 128 *)
  0x6f8740dc;       (* arm_MLS_VEC Q28 Q6 (Q7 :> LANE_S 0) 32 128 *)
  0x4ec5694d;       (* arm_TRN2 Q13 Q10 Q5 64 128 *)
  0x3cdb005e;       (* arm_LDR Q30 X2 (Immediate_Offset (word 18446744073709551536)) *)
  0x4eb59dac;       (* arm_MUL_VEC Q12 Q13 Q21 32 128 *)
  0x6f874073;       (* arm_MLS_VEC Q19 Q3 (Q7 :> LANE_S 0) 32 128 *)
  0x4ebc8774;       (* arm_ADD_VEC Q20 Q27 Q28 32 128 *)
  0x6ebeb5ad;       (* arm_SQRDMULH_VEC Q13 Q13 Q30 32 128 *)
  0x6ebc8763;       (* arm_SUB_VEC Q3 Q27 Q28 32 128 *)
  0x4eb89e98;       (* arm_MUL_VEC Q24 Q20 Q24 32 128 *)
  0x6eabb466;       (* arm_SQRDMULH_VEC Q6 Q3 Q11 32 128 *)
  0x3cd8005b;       (* arm_LDR Q27 X2 (Immediate_Offset (word 18446744073709551488)) *)
  0x6f8741ac;       (* arm_MLS_VEC Q12 Q13 (Q7 :> LANE_S 0) 32 128 *)
  0x4e8f2b39;       (* arm_TRN1 Q25 Q25 Q15 32 128 *)
  0x4ebb9c7b;       (* arm_MUL_VEC Q27 Q3 Q27 32 128 *)
  0x4e9f293f;       (* arm_TRN1 Q31 Q9 Q31 32 128 *)
  0x4ec52943;       (* arm_TRN1 Q3 Q10 Q5 64 128 *)
  0x3cdd004d;       (* arm_LDR Q13 X2 (Immediate_Offset (word 18446744073709551568)) *)
  0x3cdc004f;       (* arm_LDR Q15 X2 (Immediate_Offset (word 18446744073709551552)) *)
  0x6ea8b689;       (* arm_SQRDMULH_VEC Q9 Q20 Q8 32 128 *)
  0x4edf6b34;       (* arm_TRN2 Q20 Q25 Q31 64 128 *)
  0x3cdf004a;       (* arm_LDR Q10 X2 (Immediate_Offset (word 18446744073709551600)) *)
  0x6f8740db;       (* arm_MLS_VEC Q27 Q6 (Q7 :> LANE_S 0) 32 128 *)
  0x4eac8465;       (* arm_ADD_VEC Q5 Q3 Q12 32 128 *)
  0x6eac8466;       (* arm_SUB_VEC Q6 Q3 Q12 32 128 *)
  0x6ebeb683;       (* arm_SQRDMULH_VEC Q3 Q20 Q30 32 128 *)
  0x4ec12acc;       (* arm_TRN1 Q12 Q22 Q1 64 128 *)
  0x6eaab4ca;       (* arm_SQRDMULH_VEC Q10 Q6 Q10 32 128 *)
  0x6f874138;       (* arm_MLS_VEC Q24 Q9 (Q7 :> LANE_S 0) 32 128 *)
  0x6eb38589;       (* arm_SUB_VEC Q9 Q12 Q19 32 128 *)
  0x4edf2b39;       (* arm_TRN1 Q25 Q25 Q31 64 128 *)
  0x6eadb4bf;       (* arm_SQRDMULH_VEC Q31 Q5 Q13 32 128 *)
  0x4ebb853e;       (* arm_ADD_VEC Q30 Q9 Q27 32 128 *)
  0x4eb3858d;       (* arm_ADD_VEC Q13 Q12 Q19 32 128 *)
  0x4eb59e81;       (* arm_MUL_VEC Q1 Q20 Q21 32 128 *)
  0x3cde004c;       (* arm_LDR Q12 X2 (Immediate_Offset (word 18446744073709551584)) *)
  0x4eb885b5;       (* arm_ADD_VEC Q21 Q13 Q24 32 128 *)
  0x6eb885ad;       (* arm_SUB_VEC Q13 Q13 Q24 32 128 *)
  0x6f874061;       (* arm_MLS_VEC Q1 Q3 (Q7 :> LANE_S 0) 32 128 *)
  0x6ebb8523;       (* arm_SUB_VEC Q3 Q9 Q27 32 128 *)
  0x4eac9cc9;       (* arm_MUL_VEC Q9 Q6 Q12 32 128 *)
  0x4e8d6aac;       (* arm_TRN2 Q12 Q21 Q13 32 128 *)
  0x4e832bc6;       (* arm_TRN1 Q6 Q30 Q3 32 128 *)
  0x4e836bde;       (* arm_TRN2 Q30 Q30 Q3 32 128 *)
  0x6f874149;       (* arm_MLS_VEC Q9 Q10 (Q7 :> LANE_S 0) 32 128 *)
  0x4e8d2aad;       (* arm_TRN1 Q13 Q21 Q13 32 128 *)
  0x4eaf9caf;       (* arm_MUL_VEC Q15 Q5 Q15 32 128 *)
  0x6ea18723;       (* arm_SUB_VEC Q3 Q25 Q1 32 128 *)
  0x4ea18725;       (* arm_ADD_VEC Q5 Q25 Q1 32 128 *)
  0x6f8743ef;       (* arm_MLS_VEC Q15 Q31 (Q7 :> LANE_S 0) 32 128 *)
  0x4ec629b5;       (* arm_TRN1 Q21 Q13 Q6 64 128 *)
  0x4ec669a6;       (* arm_TRN2 Q6 Q13 Q6 64 128 *)
  0x4ea9846a;       (* arm_ADD_VEC Q10 Q3 Q9 32 128 *)
  0x6ea9846d;       (* arm_SUB_VEC Q13 Q3 Q9 32 128 *)
  0x3c880415;       (* arm_STR Q21 X0 (Postimmediate_Offset (word 128)) *)
  0x4ede2983;       (* arm_TRN1 Q3 Q12 Q30 64 128 *)
  0x4ede699f;       (* arm_TRN2 Q31 Q12 Q30 64 128 *)
  0x4e8d2955;       (* arm_TRN1 Q21 Q10 Q13 32 128 *)
  0x6eaf84be;       (* arm_SUB_VEC Q30 Q5 Q15 32 128 *)
  0x4eaf84ac;       (* arm_ADD_VEC Q12 Q5 Q15 32 128 *)
  0x3c990003;       (* arm_STR Q3 X0 (Immediate_Offset (word 18446744073709551504)) *)
  0x4e8d694d;       (* arm_TRN2 Q13 Q10 Q13 32 128 *)
  0x4e9e2993;       (* arm_TRN1 Q19 Q12 Q30 32 128 *)
  0x4e9e698c;       (* arm_TRN2 Q12 Q12 Q30 32 128 *)
  0x3c9a0006;       (* arm_STR Q6 X0 (Immediate_Offset (word 18446744073709551520)) *)
  0x3c9b001f;       (* arm_STR Q31 X0 (Immediate_Offset (word 18446744073709551536)) *)
  0x4ed52a6a;       (* arm_TRN1 Q10 Q19 Q21 64 128 *)
  0x4ed56a63;       (* arm_TRN2 Q3 Q19 Q21 64 128 *)
  0x4ecd2995;       (* arm_TRN1 Q21 Q12 Q13 64 128 *)
  0x4ecd698d;       (* arm_TRN2 Q13 Q12 Q13 64 128 *)
  0x3c9c000a;       (* arm_STR Q10 X0 (Immediate_Offset (word 18446744073709551552)) *)
  0x3c9e0003;       (* arm_STR Q3 X0 (Immediate_Offset (word 18446744073709551584)) *)
  0x3c9f000d;       (* arm_STR Q13 X0 (Immediate_Offset (word 18446744073709551600)) *)
  0x3c9d0015;       (* arm_STR Q21 X0 (Immediate_Offset (word 18446744073709551568)) *)
  0x6d4027e8;       (* arm_LDP D8 D9 SP (Immediate_Offset (iword (&0))) *)
  0x6d412fea;       (* arm_LDP D10 D11 SP (Immediate_Offset (iword (&16))) *)
  0x6d4237ec;       (* arm_LDP D12 D13 SP (Immediate_Offset (iword (&32))) *)
  0x6d433fee;       (* arm_LDP D14 D15 SP (Immediate_Offset (iword (&48))) *)
  0x910103ff;       (* arm_ADD SP SP (rvalue (word 64)) *)
  0xd65f03c0        (* arm_RET X30 *)
];;
(*** BYTECODE END ***)

let MLDSA_NTT_EXEC = ARM_MK_EXEC_RULE mldsa_ntt_mc;;

let LENGTH_MLDSA_NTT_MC =
  REWRITE_CONV[mldsa_ntt_mc] `LENGTH mldsa_ntt_mc`
  |> CONV_RULE (RAND_CONV LENGTH_CONV);;

let MLDSA_NTT_PREAMBLE_LENGTH = new_definition
  `MLDSA_NTT_PREAMBLE_LENGTH = 20`;;

let MLDSA_NTT_POSTAMBLE_LENGTH = new_definition
  `MLDSA_NTT_POSTAMBLE_LENGTH = 24`;;

let MLDSA_NTT_CORE_START = new_definition
  `MLDSA_NTT_CORE_START = MLDSA_NTT_PREAMBLE_LENGTH`;;

let MLDSA_NTT_CORE_END = new_definition
  `MLDSA_NTT_CORE_END = LENGTH mldsa_ntt_mc - MLDSA_NTT_POSTAMBLE_LENGTH`;;

let LENGTH_NTT_ZETAS_LAYER012345 =
  REWRITE_CONV[ntt_zetas_layer012345] `LENGTH ntt_zetas_layer012345`
  |> CONV_RULE (RAND_CONV LENGTH_CONV);;

let LENGTH_NTT_ZETAS_LAYER67 =
  REWRITE_CONV[ntt_zetas_layer67] `LENGTH ntt_zetas_layer67`
  |> CONV_RULE (RAND_CONV LENGTH_CONV);;

let LENGTH_SIMPLIFY_CONV =
  REWRITE_CONV[LENGTH_MLDSA_NTT_MC;
              LENGTH_NTT_ZETAS_LAYER012345; LENGTH_NTT_ZETAS_LAYER67;
              MLDSA_NTT_CORE_START; MLDSA_NTT_CORE_END;
              MLDSA_NTT_PREAMBLE_LENGTH; MLDSA_NTT_POSTAMBLE_LENGTH] THENC
  NUM_REDUCE_CONV THENC REWRITE_CONV [ADD_0];;

(* ------------------------------------------------------------------------- *)
(* Correctness proof.                                                        *)
(* ------------------------------------------------------------------------- *)

let MLDSA_NTT_CORRECT = prove
 (`!a z_012345 z_67 (zetas_012345:int32 list) (zetas_67:int32 list) x pc.
      ALL (nonoverlapping (a,1024))
          [(word pc,LENGTH mldsa_ntt_mc);
           (z_012345,LENGTH ntt_zetas_layer012345 * 4);
           (z_67,LENGTH ntt_zetas_layer67 * 4)]
      ==> ensures arm
           (\s. aligned_bytes_loaded s (word pc) mldsa_ntt_mc /\
                read PC s = word (pc + MLDSA_NTT_CORE_START) /\
                C_ARGUMENTS [a; z_012345; z_67] s /\
                wordlist_from_memory(z_012345,LENGTH ntt_zetas_layer012345) s = zetas_012345 /\
                wordlist_from_memory(z_67,LENGTH ntt_zetas_layer67) s = zetas_67 /\
                !i. i < 256
                    ==> read(memory :> bytes32(word_add a (word(4 * i)))) s =
                        x i)
           (\s. read PC s = word(pc + MLDSA_NTT_CORE_END) /\
                (zetas_012345 = MAP iword ntt_zetas_layer012345 /\
                 zetas_67 = MAP iword ntt_zetas_layer67 /\
                 (!i. i < 256 ==> abs(ival(x i)) <= &8380416)
                 ==> !i. i < 256
                         ==> let zi =
                        read(memory :> bytes32(word_add a (word(4 * i)))) s in
                        (ival zi == arm_mldsa_forward_ntt (ival o x) i) (mod &8380417) /\
                        abs(ival zi) <= &75423752))
           (MAYCHANGE_REGS_AND_FLAGS_PERMITTED_BY_ABI ,,
            MAYCHANGE [Q8; Q9; Q10; Q11; Q12; Q13; Q14; Q15] ,,
            MAYCHANGE [memory :> bytes(a,1024)])`,
  CONV_TAC LENGTH_SIMPLIFY_CONV THEN
  MAP_EVERY X_GEN_TAC
   [`a:int64`; `z_012345:int64`; `z_67:int64`; `zetas_012345:int32 list`;
    `zetas_67:int32 list`; `x:num->int32`; `pc:num`] THEN
  REWRITE_TAC[MAYCHANGE_REGS_AND_FLAGS_PERMITTED_BY_ABI; C_ARGUMENTS;
              NONOVERLAPPING_CLAUSES; ALL] THEN
  DISCH_THEN(REPEAT_TCL CONJUNCTS_THEN ASSUME_TAC) THEN

  (*** Globalize the assumptions on zeta constants by case splitting ***)

  ASM_CASES_TAC
   `zetas_012345:int32 list = MAP iword ntt_zetas_layer012345 /\
    zetas_67:int32 list = MAP iword ntt_zetas_layer67` THEN
  ASM_REWRITE_TAC[CONJ_ASSOC] THEN REWRITE_TAC[GSYM CONJ_ASSOC] THENL
   [FIRST_X_ASSUM(CONJUNCTS_THEN SUBST1_TAC);
    ARM_QUICKSIM_TAC MLDSA_NTT_EXEC
     [`read X0 s = a`; `read X1 s = z`; `read X2 s = w`;
      `read X3 s = i`; `read X4 s = i`]
     (1--1959)] THEN

  (*** Manually expand the cases in the hypotheses ***)

  CONV_TAC(RATOR_CONV(LAND_CONV(ONCE_DEPTH_CONV
   (EXPAND_CASES_CONV THENC ONCE_DEPTH_CONV NUM_MULT_CONV)))) THEN
  CONV_TAC(ONCE_DEPTH_CONV WORDLIST_FROM_MEMORY_CONV) THEN
  REWRITE_TAC[ntt_zetas_layer012345; ntt_zetas_layer67] THEN
  REWRITE_TAC[MAP; CONS_11] THEN CONV_TAC(ONCE_DEPTH_CONV WORD_IWORD_CONV) THEN
  REWRITE_TAC[WORD_ADD_0] THEN ENSURES_INIT_TAC "s0" THEN

  (*** Manually restructure to match the 128-bit loads. It would be nicer
   *** if the simulation machinery did this automatically.
   ***)

  MEMORY_128_FROM_32_TAC "a" 0 64 THEN
  MEMORY_128_FROM_32_TAC "z_012345" 0 36 THEN
  MEMORY_128_FROM_32_TAC "z_67" 0 96 THEN
  ASM_REWRITE_TAC[WORD_ADD_0] THEN CONV_TAC WORD_REDUCE_CONV THEN
  DISCARD_MATCHING_ASSUMPTIONS [`read (memory :> bytes32 a) s = x`] THEN
  REPEAT STRIP_TAC THEN

  (*** Simulate all the way to the end, in effect unrolling loops ***)

  MAP_EVERY (fun n -> ARM_STEPS_TAC MLDSA_NTT_EXEC [n] THEN
                      SIMD_SIMPLIFY_ABBREV_TAC[arm_mldsa_barmul] [])
            (1--1959) THEN

  ENSURES_FINAL_STATE_TAC THEN ASM_REWRITE_TAC[] THEN

  (*** Reverse the restructuring by splitting the 128-bit words up ***)

  REPEAT(FIRST_X_ASSUM(STRIP_ASSUME_TAC o
    CONV_RULE(SIMD_SIMPLIFY_CONV[]) o
    CONV_RULE(READ_MEMORY_SPLIT_CONV 2) o
    check (can (term_match [] `read qqq s:int128 = xxx`) o concl))) THEN

  (*** Expand and substitute in the conclusion we want to prove ***)

  DISCH_TAC THEN
  CONV_TAC(ONCE_DEPTH_CONV let_CONV) THEN REWRITE_TAC[INT_ABS_BOUNDS] THEN
  GEN_REWRITE_TAC (BINDER_CONV o RAND_CONV) [GSYM I_THM] THEN
  CONV_TAC(EXPAND_CASES_CONV THENC ONCE_DEPTH_CONV NUM_MULT_CONV) THEN
  ASM_REWRITE_TAC[I_THM; WORD_ADD_0] THEN DISCARD_STATE_TAC "s1959" THEN

  W(fun (asl,w) ->
    let lfn = PROCESS_BOUND_ASSUMPTIONS
      (CONJUNCTS(tryfind (CONV_RULE EXPAND_CASES_CONV o snd) asl))
    and asms =
      map snd (filter (is_local_definition [arm_mldsa_barmul] o concl o snd) asl) in
    let lfn' = LOCAL_CONGBOUND_RULE lfn (rev asms) in

    REPEAT(W(fun (asl,w) ->
      if length(conjuncts w) > 3 then CONJ_TAC else NO_TAC)) THEN

    W(MP_TAC o ASM_CONGBOUND_RULE lfn' o
        rand o lhand o rator o lhand o snd) THEN
   (MATCH_MP_TAC MONO_AND THEN CONJ_TAC THENL
     [MATCH_MP_TAC(REWRITE_RULE[IMP_CONJ_ALT] INT_CONG_TRANS) THEN
      CONV_TAC(ONCE_DEPTH_CONV ARM_MLDSA_FORWARD_NTT_CONV) THEN
      REWRITE_TAC[GSYM INT_REM_EQ; o_THM] THEN CONV_TAC INT_REM_DOWN_CONV THEN
      REWRITE_TAC[INT_REM_EQ] THEN
      REWRITE_TAC[REAL_INT_CONGRUENCE; INT_OF_NUM_EQ; ARITH_EQ] THEN
      REWRITE_TAC[GSYM REAL_OF_INT_CLAUSES] THEN
      CONV_TAC(RAND_CONV REAL_POLY_CONV) THEN REAL_INTEGER_TAC;
      MATCH_MP_TAC(INT_ARITH
       `l':int <= l /\ u <= u'
        ==> l <= x /\ x <= u ==> l' <= x /\ x <= u'`) THEN
      CONV_TAC INT_REDUCE_CONV])));;

(*** Subroutine form, somewhat messy elaboration of the usual wrapper ***)

(* NOTE: This must be kept in sync with the CBMC specification
 * in mldsa/src/native/aarch64/src/arith_native_aarch64.h *)

let MLDSA_NTT_SUBROUTINE_CORRECT = prove
 (`!a z_012345 z_67 zetas_012345 zetas_67 x pc stackpointer returnaddress.
      aligned 16 stackpointer /\
      ALLPAIRS nonoverlapping
       [(a,1024); (word_sub stackpointer (word 64),64)]
       [(word pc,LENGTH mldsa_ntt_mc);
        (z_012345,LENGTH ntt_zetas_layer012345 * 4);
        (z_67,LENGTH ntt_zetas_layer67 * 4)] /\
      nonoverlapping (a,1024) (word_sub stackpointer (word 64),64)
      ==> ensures arm
           (\s. aligned_bytes_loaded s (word pc) mldsa_ntt_mc /\
                read PC s = word pc /\
                read SP s = stackpointer /\
                read X30 s = returnaddress /\
                C_ARGUMENTS [a; z_012345; z_67] s /\
                wordlist_from_memory(z_012345,LENGTH ntt_zetas_layer012345) s:int32 list = zetas_012345 /\
                wordlist_from_memory(z_67,LENGTH ntt_zetas_layer67) s:int32 list = zetas_67 /\
                !i. i < 256
                    ==> read(memory :> bytes32(word_add a (word(4 * i)))) s =
                        x i)
           (\s. read PC s = returnaddress /\
                (zetas_012345 = MAP iword ntt_zetas_layer012345 /\
                 zetas_67 = MAP iword ntt_zetas_layer67 /\
                 (!i. i < 256 ==> abs(ival(x i)) <= &8380416)
                 ==> !i. i < 256
                         ==> let zi =
                        read(memory :> bytes32(word_add a (word(4 * i)))) s in
                        (ival zi == arm_mldsa_forward_ntt (ival o x) i) (mod &8380417) /\
                        abs(ival zi) <= &75423752))
           (MAYCHANGE_REGS_AND_FLAGS_PERMITTED_BY_ABI ,,
            MAYCHANGE [memory :> bytes(a,1024);
                       memory :> bytes(word_sub stackpointer (word 64),64)])`,
  CONV_TAC LENGTH_SIMPLIFY_CONV THEN
  let TWEAK_CONV =
    ONCE_DEPTH_CONV let_CONV THENC
    ONCE_DEPTH_CONV WORDLIST_FROM_MEMORY_CONV THENC
    ONCE_DEPTH_CONV EXPAND_CASES_CONV THENC
    ONCE_DEPTH_CONV NUM_MULT_CONV THENC
    PURE_REWRITE_CONV [WORD_ADD_0] in
  REWRITE_TAC[fst MLDSA_NTT_EXEC] THEN
  CONV_TAC TWEAK_CONV THEN
  ARM_ADD_RETURN_STACK_TAC ~pre_post_nsteps:(5,5) MLDSA_NTT_EXEC
   (REWRITE_RULE[fst MLDSA_NTT_EXEC] (CONV_RULE TWEAK_CONV (CONV_RULE LENGTH_SIMPLIFY_CONV MLDSA_NTT_CORRECT)))
    `[D8; D9; D10; D11; D12; D13; D14; D15]` 64);;


(* ------------------------------------------------------------------------- *)
(* Constant-time and memory safety proof.                                    *)
(* ------------------------------------------------------------------------- *)
needs "arm/proofs/consttime.ml";;
needs "aarch64/proofs/subroutine_signatures.ml";;

let full_spec,public_vars = mk_safety_spec
    ~keep_maychanges:false
    (assoc "mldsa_ntt_arm" subroutine_signatures)
    MLDSA_NTT_SUBROUTINE_CORRECT
    MLDSA_NTT_EXEC;;

let MLDSA_NTT_SUBROUTINE_SAFE = time prove
 (`exists f_events.
       forall e a z_012345 z_67 pc stackpointer returnaddress.
           aligned 16 stackpointer /\
           ALLPAIRS nonoverlapping
           [a,1024; word_sub stackpointer (word 64),64]
           [word pc,LENGTH mldsa_ntt_mc;
            z_012345,LENGTH ntt_zetas_layer012345 * 4;
            z_67,LENGTH ntt_zetas_layer67 * 4] /\
           nonoverlapping (a,1024) (word_sub stackpointer (word 64),64)
           ==> ensures arm
               (\s.
                    aligned_bytes_loaded s (word pc) mldsa_ntt_mc /\
                    read PC s = word pc /\
                    read SP s = stackpointer /\
                    read X30 s = returnaddress /\
                    C_ARGUMENTS [a; z_012345; z_67] s /\
                    read events s = e)
               (\s.
                    read PC s = returnaddress /\
                    exists e2.
                        read events s = APPEND e2 e /\
                        e2 =
                        f_events z_012345 z_67 a pc
                        (word_sub stackpointer (word 64))
                        returnaddress /\
                        memaccess_inbounds e2
                        [a,1024; z_012345,576; z_67,1536;
                         word_sub stackpointer (word 64),64]
                        [a,1024; word_sub stackpointer (word 64),64])
               (\s s'. true)`,
  ASSERT_CONCL_TAC full_spec THEN
  PROVE_SAFETY_SPEC_TAC ~public_vars:public_vars MLDSA_NTT_EXEC);;
