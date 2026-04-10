/*
 * Copyright (c) The mldsa-native project authors
 * SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT
 */
#include "../../../common.h"

#if defined(MLD_ARITH_BACKEND_AARCH64) && !defined(MLD_CONFIG_MULTILEVEL_NO_SHARED) && \
           (defined(MLD_CONFIG_MULTILEVEL_WITH_SHARED) || (MLD_CONFIG_PARAMETER_SET == 65 || MLD_CONFIG_PARAMETER_SET == 87))
/* simpasm: header-end */

// a aliased with a0
.macro decompose32 a1, a, temp
        // range: 0 <= a <= Q-1 = 32*GAMMA2

        /* check-magic: 523776 == 2 * intdiv(MLDSA_Q - 1, 32) */
        /* check-magic: 1074791425 == floor(2**49 / 523776) */
        /* check-magic: 575897802350002176 == 1 / (1 / 523776 - 1074791425 / 2^49) */
        // Compute a1 = round-(a / (2*GAMMA2)) = round-(a / 523776) ≈
        // round(a * 1074791425 / 2^49), where round-() denotes "round half
        // down". This is exact for 0 <= a < Q. We'll prove this in the
        // following paragraphs, in which we denote 2*GAMMA2 as B to avoid
        // clutter.
        //
        // Consider the (signed) error a * (1 / B - 1074791425 / 2^49) between
        // a / B and the (under-)approximation a * 1074791425 / 2^49. Because
        // eps := 1 / B - 1074791425 / 2^49 is 1 / 575897802350002176 ≈
        // 2^(-58.99) < 2^(-58), we have 0 <= a * eps < 2^23 * 2^(-58) =
        // 1 / 2^35 < 1 / 2^19 < 1 / B (note that a is non-negative).
        //
        // On the other hand, 1 / B is the spacing between the integral
        // multiples of 1 / B, which includes all rounding boundaries n + 0.5
        // (since B is even). Hence, if a / B is not of the form n + 0.5, then
        // it is at least 1 / B away from the nearest rounding boundary, so
        // moving from a / B to a * 1074791425 / 2^49 does not affect the
        // rounding result, no matter the type of rounding used in either side.
        // In particular, we have round-(a / B) = round(a * 1074791425 / 2^49)
        // as claimed.
        //
        // As for the remaining case where a / B _is_ of the form n + 0.5,
        // because a * 1074791425 / 2^49 is slightly but strictly below a / B =
        // n + 0.5 (note that a and thus the error a * eps cannot be 0 here), it
        // is always rounded down to n. More precisely, we have round-(a / B) =
        // round(a * 1074791425 / 2^49), where the round-down on the LHS is
        // essential, and on the RHS the type of rounding again does not matter.
        // This concludes the proof.
        //
        // See proofs/isabelle/compress for a formalization of this argument.
        sqdmulh \a1\().4s, \a\().4s, barrett_const.4s
        srshr \a1\().4s, \a1\().4s, #18
        // range: 0 <= a1 <= 16

        // If a1 = 16, i.e. a > 31*GAMMA2, proceed as if a' = a - Q was
        // given instead. (For a = 31*GAMMA2 + 1 thus a' = -GAMMA2, we
        // still round it to 0 like other "wrapped around" cases.)

        // Check for wrap-around
        cmgt \temp\().4s, \a\().4s, q_bound.4s

        // Compute remainder a0
        mls \a\().4s, \a1\().4s, gamma2_2x.4s
        // range: -GAMMA2 < a0 <= GAMMA2
        //
        // This holds since a1 = round-(a / (2*GAMMA2)) was computed exactly.

        // If wrap-around is required, set a1 = 0 and a0 -= 1
        bic \a1\().16b, \a1\().16b, \temp\().16b
        add \a\().4s, \a\().4s, \temp\().4s
        // range: 0 <= a1 <= 15, -GAMMA2 <= a0 <= GAMMA2
.endm

        /* Parameters */
        a1_ptr          .req x0     // Output polynomial with coefficients c1
        a0_ptr          .req x1     // Input/Output polynomial with coefficients c0

        count           .req x3

        /* Constant register assignments */
        q               .req v20    // Q = 8380417
        q_bound         .req v21    // 31*GAMMA2 = 8118528
        gamma2_2x       .req v22    // 2*GAMMA2 = 523776
        barrett_const   .req v23    // Barrett constant = 1074791425


.text
.global MLD_ASM_NAMESPACE(poly_decompose_32_asm)
.balign 4
MLD_ASM_FN_SYMBOL(poly_decompose_32_asm)
        // Load constants into SIMD registers
        movz w4, #57345
        movk w4, #127, lsl #16
        dup q.4s, w4

        movz w5, #0xe100
        movk w5, #0x7b, lsl #16
        dup q_bound.4s, w5

        movz w7, #0xfe00
        movk w7, #7, lsl #16
        dup gamma2_2x.4s, w7

        movz w11, #0x0401
        movk w11, #0x4010, lsl #16
        dup barrett_const.4s, w11

        mov count, #(64/4)

poly_decompose_32_loop:
        ldr q0, [a0_ptr, #0*16]
        ldr q1, [a0_ptr, #1*16]
        ldr q2, [a0_ptr, #2*16]
        ldr q3, [a0_ptr, #3*16]

        decompose32 v5, v1, v24
        decompose32 v6, v2, v24
        decompose32 v7, v3, v24
        decompose32 v4, v0, v24

        str q5, [a1_ptr, #1*16]
        str q6, [a1_ptr, #2*16]
        str q7, [a1_ptr, #3*16]
        str q4, [a1_ptr], #4*16
        str q1, [a0_ptr, #1*16]
        str q2, [a0_ptr, #2*16]
        str q3, [a0_ptr, #3*16]
        str q0, [a0_ptr], #4*16

        subs count, count, #1
        bne poly_decompose_32_loop

        ret

        .unreq a1_ptr
        .unreq a0_ptr
        .unreq count
        .unreq q
        .unreq q_bound
        .unreq gamma2_2x
        .unreq barrett_const
/* simpasm: footer-start */
#endif /* MLD_ARITH_BACKEND_AARCH64 && !MLD_CONFIG_MULTILEVEL_NO_SHARED && \
          (MLD_CONFIG_MULTILEVEL_WITH_SHARED || MLD_CONFIG_PARAMETER_SET == 65 \
          || MLD_CONFIG_PARAMETER_SET == 87) */
