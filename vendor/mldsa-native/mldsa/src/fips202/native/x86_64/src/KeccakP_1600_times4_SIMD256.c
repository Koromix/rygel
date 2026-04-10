/*
 * Copyright (c) The mlkem-native project authors
 * Copyright (c) The mldsa-native project authors
 * SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT
 */

/*
Implementation by the Keccak, Keyak and Ketje Teams, namely, Guido Bertoni,
Joan Daemen, Michaël Peeters, Gilles Van Assche and Ronny Van Keer, hereby
denoted as "the implementer".

For more information, feedback or questions, please refer to our websites:
http://keccak.noekeon.org/
http://keyak.noekeon.org/
http://ketje.noekeon.org/

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

/*
 * Changes for mlkem-native/mldsa-native:
 * - MLD_COPY_FROM_STATE and MLD_COPY_TO_STATE operate on uninterleaved
 *   Keccak states in memory.
 */

#include "../../../../common.h"
#if defined(MLD_FIPS202_X86_64_XKCP) && \
    !defined(MLD_CONFIG_MULTILEVEL_NO_SHARED)

#include <immintrin.h>

#include "KeccakP_1600_times4_SIMD256.h"

#ifndef MLD_SYS_LITTLE_ENDIAN
#error Expecting a little-endian platform
#endif

#define MLD_ANDNU256(a, b) _mm256_andnot_si256(a, b)
#define MLD_CONST256(a) _mm256_load_si256((const __m256i *)&(a))
#define MLD_CONST256_64(a) (__m256i) _mm256_broadcast_sd((const double *)(&a))
#define MLD_ROL64IN256(d, a, o) \
  d = _mm256_or_si256(_mm256_slli_epi64(a, o), _mm256_srli_epi64(a, 64 - (o)))
#define MLD_ROL64IN256_8(d, a) \
  d = _mm256_shuffle_epi8(a, MLD_CONST256(mld_rho8))
#define MLD_ROL64IN256_56(d, a) \
  d = _mm256_shuffle_epi8(a, MLD_CONST256(mld_rho56))
static const uint64_t mld_rho8[4] = {0x0605040302010007, 0x0E0D0C0B0A09080F,
                                     0x1615141312111017, 0x1E1D1C1B1A19181F};
static const uint64_t mld_rho56[4] = {0x0007060504030201, 0x080F0E0D0C0B0A09,
                                      0x1017161514131211, 0x181F1E1D1C1B1A19};
#define MLD_STORE256(a, b) _mm256_store_si256((__m256i *)&(a), b)
#define MLD_XOR256(a, b) _mm256_xor_si256(a, b)
#define MLD_XOREQ256(a, b) a = _mm256_xor_si256(a, b)

#define MLD_SNP_LANELENGTHINBYTES 8

#define MLD_DECLARE_ABCDE          \
  __m256i Aba, Abe, Abi, Abo, Abu; \
  __m256i Aga, Age, Agi, Ago, Agu; \
  __m256i Aka, Ake, Aki, Ako, Aku; \
  __m256i Ama, Ame, Ami, Amo, Amu; \
  __m256i Asa, Ase, Asi, Aso, Asu; \
  __m256i Bba, Bbe, Bbi, Bbo, Bbu; \
  __m256i Bga, Bge, Bgi, Bgo, Bgu; \
  __m256i Bka, Bke, Bki, Bko, Bku; \
  __m256i Bma, Bme, Bmi, Bmo, Bmu; \
  __m256i Bsa, Bse, Bsi, Bso, Bsu; \
  __m256i Ca, Ce, Ci, Co, Cu;      \
  __m256i Ca1, Ce1, Ci1, Co1, Cu1; \
  __m256i Da, De, Di, Do, Du;      \
  __m256i Eba, Ebe, Ebi, Ebo, Ebu; \
  __m256i Ega, Ege, Egi, Ego, Egu; \
  __m256i Eka, Eke, Eki, Eko, Eku; \
  __m256i Ema, Eme, Emi, Emo, Emu; \
  __m256i Esa, Ese, Esi, Eso, Esu;

#define MLD_prepareTheta                                                       \
  Ca =                                                                         \
      MLD_XOR256(Aba, MLD_XOR256(Aga, MLD_XOR256(Aka, MLD_XOR256(Ama, Asa)))); \
  Ce =                                                                         \
      MLD_XOR256(Abe, MLD_XOR256(Age, MLD_XOR256(Ake, MLD_XOR256(Ame, Ase)))); \
  Ci =                                                                         \
      MLD_XOR256(Abi, MLD_XOR256(Agi, MLD_XOR256(Aki, MLD_XOR256(Ami, Asi)))); \
  Co =                                                                         \
      MLD_XOR256(Abo, MLD_XOR256(Ago, MLD_XOR256(Ako, MLD_XOR256(Amo, Aso)))); \
  Cu = MLD_XOR256(Abu, MLD_XOR256(Agu, MLD_XOR256(Aku, MLD_XOR256(Amu, Asu))));

/*
 * --- Theta Rho Pi Chi Iota Prepare-theta
 * --- 64-bit lanes mapped to 64-bit words
 */
#define MLD_thetaRhoPiChiIotaPrepareTheta(i, A, E)                        \
  MLD_ROL64IN256(Ce1, Ce, 1);                                             \
  Da = MLD_XOR256(Cu, Ce1);                                               \
  MLD_ROL64IN256(Ci1, Ci, 1);                                             \
  De = MLD_XOR256(Ca, Ci1);                                               \
  MLD_ROL64IN256(Co1, Co, 1);                                             \
  Di = MLD_XOR256(Ce, Co1);                                               \
  MLD_ROL64IN256(Cu1, Cu, 1);                                             \
  Do = MLD_XOR256(Ci, Cu1);                                               \
  MLD_ROL64IN256(Ca1, Ca, 1);                                             \
  Du = MLD_XOR256(Co, Ca1);                                               \
                                                                          \
  MLD_XOREQ256(A##ba, Da);                                                \
  Bba = A##ba;                                                            \
  MLD_XOREQ256(A##ge, De);                                                \
  MLD_ROL64IN256(Bbe, A##ge, 44);                                         \
  MLD_XOREQ256(A##ki, Di);                                                \
  MLD_ROL64IN256(Bbi, A##ki, 43);                                         \
  E##ba = MLD_XOR256(Bba, MLD_ANDNU256(Bbe, Bbi));                        \
  MLD_XOREQ256(E##ba, MLD_CONST256_64(mld_keccakf1600RoundConstants[i])); \
  Ca = E##ba;                                                             \
  MLD_XOREQ256(A##mo, Do);                                                \
  MLD_ROL64IN256(Bbo, A##mo, 21);                                         \
  E##be = MLD_XOR256(Bbe, MLD_ANDNU256(Bbi, Bbo));                        \
  Ce = E##be;                                                             \
  MLD_XOREQ256(A##su, Du);                                                \
  MLD_ROL64IN256(Bbu, A##su, 14);                                         \
  E##bi = MLD_XOR256(Bbi, MLD_ANDNU256(Bbo, Bbu));                        \
  Ci = E##bi;                                                             \
  E##bo = MLD_XOR256(Bbo, MLD_ANDNU256(Bbu, Bba));                        \
  Co = E##bo;                                                             \
  E##bu = MLD_XOR256(Bbu, MLD_ANDNU256(Bba, Bbe));                        \
  Cu = E##bu;                                                             \
                                                                          \
  MLD_XOREQ256(A##bo, Do);                                                \
  MLD_ROL64IN256(Bga, A##bo, 28);                                         \
  MLD_XOREQ256(A##gu, Du);                                                \
  MLD_ROL64IN256(Bge, A##gu, 20);                                         \
  MLD_XOREQ256(A##ka, Da);                                                \
  MLD_ROL64IN256(Bgi, A##ka, 3);                                          \
  E##ga = MLD_XOR256(Bga, MLD_ANDNU256(Bge, Bgi));                        \
  MLD_XOREQ256(Ca, E##ga);                                                \
  MLD_XOREQ256(A##me, De);                                                \
  MLD_ROL64IN256(Bgo, A##me, 45);                                         \
  E##ge = MLD_XOR256(Bge, MLD_ANDNU256(Bgi, Bgo));                        \
  MLD_XOREQ256(Ce, E##ge);                                                \
  MLD_XOREQ256(A##si, Di);                                                \
  MLD_ROL64IN256(Bgu, A##si, 61);                                         \
  E##gi = MLD_XOR256(Bgi, MLD_ANDNU256(Bgo, Bgu));                        \
  MLD_XOREQ256(Ci, E##gi);                                                \
  E##go = MLD_XOR256(Bgo, MLD_ANDNU256(Bgu, Bga));                        \
  MLD_XOREQ256(Co, E##go);                                                \
  E##gu = MLD_XOR256(Bgu, MLD_ANDNU256(Bga, Bge));                        \
  MLD_XOREQ256(Cu, E##gu);                                                \
                                                                          \
  MLD_XOREQ256(A##be, De);                                                \
  MLD_ROL64IN256(Bka, A##be, 1);                                          \
  MLD_XOREQ256(A##gi, Di);                                                \
  MLD_ROL64IN256(Bke, A##gi, 6);                                          \
  MLD_XOREQ256(A##ko, Do);                                                \
  MLD_ROL64IN256(Bki, A##ko, 25);                                         \
  E##ka = MLD_XOR256(Bka, MLD_ANDNU256(Bke, Bki));                        \
  MLD_XOREQ256(Ca, E##ka);                                                \
  MLD_XOREQ256(A##mu, Du);                                                \
  MLD_ROL64IN256_8(Bko, A##mu);                                           \
  E##ke = MLD_XOR256(Bke, MLD_ANDNU256(Bki, Bko));                        \
  MLD_XOREQ256(Ce, E##ke);                                                \
  MLD_XOREQ256(A##sa, Da);                                                \
  MLD_ROL64IN256(Bku, A##sa, 18);                                         \
  E##ki = MLD_XOR256(Bki, MLD_ANDNU256(Bko, Bku));                        \
  MLD_XOREQ256(Ci, E##ki);                                                \
  E##ko = MLD_XOR256(Bko, MLD_ANDNU256(Bku, Bka));                        \
  MLD_XOREQ256(Co, E##ko);                                                \
  E##ku = MLD_XOR256(Bku, MLD_ANDNU256(Bka, Bke));                        \
  MLD_XOREQ256(Cu, E##ku);                                                \
                                                                          \
  MLD_XOREQ256(A##bu, Du);                                                \
  MLD_ROL64IN256(Bma, A##bu, 27);                                         \
  MLD_XOREQ256(A##ga, Da);                                                \
  MLD_ROL64IN256(Bme, A##ga, 36);                                         \
  MLD_XOREQ256(A##ke, De);                                                \
  MLD_ROL64IN256(Bmi, A##ke, 10);                                         \
  E##ma = MLD_XOR256(Bma, MLD_ANDNU256(Bme, Bmi));                        \
  MLD_XOREQ256(Ca, E##ma);                                                \
  MLD_XOREQ256(A##mi, Di);                                                \
  MLD_ROL64IN256(Bmo, A##mi, 15);                                         \
  E##me = MLD_XOR256(Bme, MLD_ANDNU256(Bmi, Bmo));                        \
  MLD_XOREQ256(Ce, E##me);                                                \
  MLD_XOREQ256(A##so, Do);                                                \
  MLD_ROL64IN256_56(Bmu, A##so);                                          \
  E##mi = MLD_XOR256(Bmi, MLD_ANDNU256(Bmo, Bmu));                        \
  MLD_XOREQ256(Ci, E##mi);                                                \
  E##mo = MLD_XOR256(Bmo, MLD_ANDNU256(Bmu, Bma));                        \
  MLD_XOREQ256(Co, E##mo);                                                \
  E##mu = MLD_XOR256(Bmu, MLD_ANDNU256(Bma, Bme));                        \
  MLD_XOREQ256(Cu, E##mu);                                                \
                                                                          \
  MLD_XOREQ256(A##bi, Di);                                                \
  MLD_ROL64IN256(Bsa, A##bi, 62);                                         \
  MLD_XOREQ256(A##go, Do);                                                \
  MLD_ROL64IN256(Bse, A##go, 55);                                         \
  MLD_XOREQ256(A##ku, Du);                                                \
  MLD_ROL64IN256(Bsi, A##ku, 39);                                         \
  E##sa = MLD_XOR256(Bsa, MLD_ANDNU256(Bse, Bsi));                        \
  MLD_XOREQ256(Ca, E##sa);                                                \
  MLD_XOREQ256(A##ma, Da);                                                \
  MLD_ROL64IN256(Bso, A##ma, 41);                                         \
  E##se = MLD_XOR256(Bse, MLD_ANDNU256(Bsi, Bso));                        \
  MLD_XOREQ256(Ce, E##se);                                                \
  MLD_XOREQ256(A##se, De);                                                \
  MLD_ROL64IN256(Bsu, A##se, 2);                                          \
  E##si = MLD_XOR256(Bsi, MLD_ANDNU256(Bso, Bsu));                        \
  MLD_XOREQ256(Ci, E##si);                                                \
  E##so = MLD_XOR256(Bso, MLD_ANDNU256(Bsu, Bsa));                        \
  MLD_XOREQ256(Co, E##so);                                                \
  E##su = MLD_XOR256(Bsu, MLD_ANDNU256(Bsa, Bse));                        \
  MLD_XOREQ256(Cu, E##su);


/*
 * --- Theta Rho Pi Chi Iota
 * --- 64-bit lanes mapped to 64-bit words
 */
#define MLD_thetaRhoPiChiIota(i, A, E)                                    \
  MLD_ROL64IN256(Ce1, Ce, 1);                                             \
  Da = MLD_XOR256(Cu, Ce1);                                               \
  MLD_ROL64IN256(Ci1, Ci, 1);                                             \
  De = MLD_XOR256(Ca, Ci1);                                               \
  MLD_ROL64IN256(Co1, Co, 1);                                             \
  Di = MLD_XOR256(Ce, Co1);                                               \
  MLD_ROL64IN256(Cu1, Cu, 1);                                             \
  Do = MLD_XOR256(Ci, Cu1);                                               \
  MLD_ROL64IN256(Ca1, Ca, 1);                                             \
  Du = MLD_XOR256(Co, Ca1);                                               \
                                                                          \
  MLD_XOREQ256(A##ba, Da);                                                \
  Bba = A##ba;                                                            \
  MLD_XOREQ256(A##ge, De);                                                \
  MLD_ROL64IN256(Bbe, A##ge, 44);                                         \
  MLD_XOREQ256(A##ki, Di);                                                \
  MLD_ROL64IN256(Bbi, A##ki, 43);                                         \
  E##ba = MLD_XOR256(Bba, MLD_ANDNU256(Bbe, Bbi));                        \
  MLD_XOREQ256(E##ba, MLD_CONST256_64(mld_keccakf1600RoundConstants[i])); \
  MLD_XOREQ256(A##mo, Do);                                                \
  MLD_ROL64IN256(Bbo, A##mo, 21);                                         \
  E##be = MLD_XOR256(Bbe, MLD_ANDNU256(Bbi, Bbo));                        \
  MLD_XOREQ256(A##su, Du);                                                \
  MLD_ROL64IN256(Bbu, A##su, 14);                                         \
  E##bi = MLD_XOR256(Bbi, MLD_ANDNU256(Bbo, Bbu));                        \
  E##bo = MLD_XOR256(Bbo, MLD_ANDNU256(Bbu, Bba));                        \
  E##bu = MLD_XOR256(Bbu, MLD_ANDNU256(Bba, Bbe));                        \
                                                                          \
  MLD_XOREQ256(A##bo, Do);                                                \
  MLD_ROL64IN256(Bga, A##bo, 28);                                         \
  MLD_XOREQ256(A##gu, Du);                                                \
  MLD_ROL64IN256(Bge, A##gu, 20);                                         \
  MLD_XOREQ256(A##ka, Da);                                                \
  MLD_ROL64IN256(Bgi, A##ka, 3);                                          \
  E##ga = MLD_XOR256(Bga, MLD_ANDNU256(Bge, Bgi));                        \
  MLD_XOREQ256(A##me, De);                                                \
  MLD_ROL64IN256(Bgo, A##me, 45);                                         \
  E##ge = MLD_XOR256(Bge, MLD_ANDNU256(Bgi, Bgo));                        \
  MLD_XOREQ256(A##si, Di);                                                \
  MLD_ROL64IN256(Bgu, A##si, 61);                                         \
  E##gi = MLD_XOR256(Bgi, MLD_ANDNU256(Bgo, Bgu));                        \
  E##go = MLD_XOR256(Bgo, MLD_ANDNU256(Bgu, Bga));                        \
  E##gu = MLD_XOR256(Bgu, MLD_ANDNU256(Bga, Bge));                        \
                                                                          \
  MLD_XOREQ256(A##be, De);                                                \
  MLD_ROL64IN256(Bka, A##be, 1);                                          \
  MLD_XOREQ256(A##gi, Di);                                                \
  MLD_ROL64IN256(Bke, A##gi, 6);                                          \
  MLD_XOREQ256(A##ko, Do);                                                \
  MLD_ROL64IN256(Bki, A##ko, 25);                                         \
  E##ka = MLD_XOR256(Bka, MLD_ANDNU256(Bke, Bki));                        \
  MLD_XOREQ256(A##mu, Du);                                                \
  MLD_ROL64IN256_8(Bko, A##mu);                                           \
  E##ke = MLD_XOR256(Bke, MLD_ANDNU256(Bki, Bko));                        \
  MLD_XOREQ256(A##sa, Da);                                                \
  MLD_ROL64IN256(Bku, A##sa, 18);                                         \
  E##ki = MLD_XOR256(Bki, MLD_ANDNU256(Bko, Bku));                        \
  E##ko = MLD_XOR256(Bko, MLD_ANDNU256(Bku, Bka));                        \
  E##ku = MLD_XOR256(Bku, MLD_ANDNU256(Bka, Bke));                        \
                                                                          \
  MLD_XOREQ256(A##bu, Du);                                                \
  MLD_ROL64IN256(Bma, A##bu, 27);                                         \
  MLD_XOREQ256(A##ga, Da);                                                \
  MLD_ROL64IN256(Bme, A##ga, 36);                                         \
  MLD_XOREQ256(A##ke, De);                                                \
  MLD_ROL64IN256(Bmi, A##ke, 10);                                         \
  E##ma = MLD_XOR256(Bma, MLD_ANDNU256(Bme, Bmi));                        \
  MLD_XOREQ256(A##mi, Di);                                                \
  MLD_ROL64IN256(Bmo, A##mi, 15);                                         \
  E##me = MLD_XOR256(Bme, MLD_ANDNU256(Bmi, Bmo));                        \
  MLD_XOREQ256(A##so, Do);                                                \
  MLD_ROL64IN256_56(Bmu, A##so);                                          \
  E##mi = MLD_XOR256(Bmi, MLD_ANDNU256(Bmo, Bmu));                        \
  E##mo = MLD_XOR256(Bmo, MLD_ANDNU256(Bmu, Bma));                        \
  E##mu = MLD_XOR256(Bmu, MLD_ANDNU256(Bma, Bme));                        \
                                                                          \
  MLD_XOREQ256(A##bi, Di);                                                \
  MLD_ROL64IN256(Bsa, A##bi, 62);                                         \
  MLD_XOREQ256(A##go, Do);                                                \
  MLD_ROL64IN256(Bse, A##go, 55);                                         \
  MLD_XOREQ256(A##ku, Du);                                                \
  MLD_ROL64IN256(Bsi, A##ku, 39);                                         \
  E##sa = MLD_XOR256(Bsa, MLD_ANDNU256(Bse, Bsi));                        \
  MLD_XOREQ256(A##ma, Da);                                                \
  MLD_ROL64IN256(Bso, A##ma, 41);                                         \
  E##se = MLD_XOR256(Bse, MLD_ANDNU256(Bsi, Bso));                        \
  MLD_XOREQ256(A##se, De);                                                \
  MLD_ROL64IN256(Bsu, A##se, 2);                                          \
  E##si = MLD_XOR256(Bsi, MLD_ANDNU256(Bso, Bsu));                        \
  E##so = MLD_XOR256(Bso, MLD_ANDNU256(Bsu, Bsa));                        \
  E##su = MLD_XOR256(Bsu, MLD_ANDNU256(Bsa, Bse));


static MLD_ALIGN const uint64_t mld_keccakf1600RoundConstants[24] = {
    (uint64_t)0x0000000000000001ULL, (uint64_t)0x0000000000008082ULL,
    (uint64_t)0x800000000000808aULL, (uint64_t)0x8000000080008000ULL,
    (uint64_t)0x000000000000808bULL, (uint64_t)0x0000000080000001ULL,
    (uint64_t)0x8000000080008081ULL, (uint64_t)0x8000000000008009ULL,
    (uint64_t)0x000000000000008aULL, (uint64_t)0x0000000000000088ULL,
    (uint64_t)0x0000000080008009ULL, (uint64_t)0x000000008000000aULL,
    (uint64_t)0x000000008000808bULL, (uint64_t)0x800000000000008bULL,
    (uint64_t)0x8000000000008089ULL, (uint64_t)0x8000000000008003ULL,
    (uint64_t)0x8000000000008002ULL, (uint64_t)0x8000000000000080ULL,
    (uint64_t)0x000000000000800aULL, (uint64_t)0x800000008000000aULL,
    (uint64_t)0x8000000080008081ULL, (uint64_t)0x8000000000008080ULL,
    (uint64_t)0x0000000080000001ULL, (uint64_t)0x8000000080008008ULL};


#define MLD_COPY_FROM_STATE(X, state)                                       \
  do                                                                        \
  {                                                                         \
    const uint64_t *state64 = (const uint64_t *)(state);                    \
    __m256i _idx =                                                          \
        _mm256_set_epi64x((long long)&state64[75], (long long)&state64[50], \
                          (long long)&state64[25], (long long)&state64[0]); \
    X##ba = _mm256_i64gather_epi64((long long *)(0 * 8), _idx, 1);          \
    X##be = _mm256_i64gather_epi64((long long *)(1 * 8), _idx, 1);          \
    X##bi = _mm256_i64gather_epi64((long long *)(2 * 8), _idx, 1);          \
    X##bo = _mm256_i64gather_epi64((long long *)(3 * 8), _idx, 1);          \
    X##bu = _mm256_i64gather_epi64((long long *)(4 * 8), _idx, 1);          \
    X##ga = _mm256_i64gather_epi64((long long *)(5 * 8), _idx, 1);          \
    X##ge = _mm256_i64gather_epi64((long long *)(6 * 8), _idx, 1);          \
    X##gi = _mm256_i64gather_epi64((long long *)(7 * 8), _idx, 1);          \
    X##go = _mm256_i64gather_epi64((long long *)(8 * 8), _idx, 1);          \
    X##gu = _mm256_i64gather_epi64((long long *)(9 * 8), _idx, 1);          \
    X##ka = _mm256_i64gather_epi64((long long *)(10 * 8), _idx, 1);         \
    X##ke = _mm256_i64gather_epi64((long long *)(11 * 8), _idx, 1);         \
    X##ki = _mm256_i64gather_epi64((long long *)(12 * 8), _idx, 1);         \
    X##ko = _mm256_i64gather_epi64((long long *)(13 * 8), _idx, 1);         \
    X##ku = _mm256_i64gather_epi64((long long *)(14 * 8), _idx, 1);         \
    X##ma = _mm256_i64gather_epi64((long long *)(15 * 8), _idx, 1);         \
    X##me = _mm256_i64gather_epi64((long long *)(16 * 8), _idx, 1);         \
    X##mi = _mm256_i64gather_epi64((long long *)(17 * 8), _idx, 1);         \
    X##mo = _mm256_i64gather_epi64((long long *)(18 * 8), _idx, 1);         \
    X##mu = _mm256_i64gather_epi64((long long *)(19 * 8), _idx, 1);         \
    X##sa = _mm256_i64gather_epi64((long long *)(20 * 8), _idx, 1);         \
    X##se = _mm256_i64gather_epi64((long long *)(21 * 8), _idx, 1);         \
    X##si = _mm256_i64gather_epi64((long long *)(22 * 8), _idx, 1);         \
    X##so = _mm256_i64gather_epi64((long long *)(23 * 8), _idx, 1);         \
    X##su = _mm256_i64gather_epi64((long long *)(24 * 8), _idx, 1);         \
  } while (0);

#define MLD_SCATTER_STORE256(state, idx, v)                    \
  do                                                           \
  {                                                            \
    const uint64_t *state64 = (const uint64_t *)(state);       \
    __m128d t = _mm_castsi128_pd(_mm256_castsi256_si128((v))); \
    _mm_storel_pd((double *)&state64[0 + (idx)], t);           \
    _mm_storeh_pd((double *)&state64[25 + (idx)], t);          \
    t = _mm_castsi128_pd(_mm256_extracti128_si256((v), 1));    \
    _mm_storel_pd((double *)&state64[50 + (idx)], t);          \
    _mm_storeh_pd((double *)&state64[75 + (idx)], t);          \
  } while (0)

#define MLD_COPY_TO_STATE(state, X)       \
  MLD_SCATTER_STORE256(state, 0, X##ba);  \
  MLD_SCATTER_STORE256(state, 1, X##be);  \
  MLD_SCATTER_STORE256(state, 2, X##bi);  \
  MLD_SCATTER_STORE256(state, 3, X##bo);  \
  MLD_SCATTER_STORE256(state, 4, X##bu);  \
  MLD_SCATTER_STORE256(state, 5, X##ga);  \
  MLD_SCATTER_STORE256(state, 6, X##ge);  \
  MLD_SCATTER_STORE256(state, 7, X##gi);  \
  MLD_SCATTER_STORE256(state, 8, X##go);  \
  MLD_SCATTER_STORE256(state, 9, X##gu);  \
  MLD_SCATTER_STORE256(state, 10, X##ka); \
  MLD_SCATTER_STORE256(state, 11, X##ke); \
  MLD_SCATTER_STORE256(state, 12, X##ki); \
  MLD_SCATTER_STORE256(state, 13, X##ko); \
  MLD_SCATTER_STORE256(state, 14, X##ku); \
  MLD_SCATTER_STORE256(state, 15, X##ma); \
  MLD_SCATTER_STORE256(state, 16, X##me); \
  MLD_SCATTER_STORE256(state, 17, X##mi); \
  MLD_SCATTER_STORE256(state, 18, X##mo); \
  MLD_SCATTER_STORE256(state, 19, X##mu); \
  MLD_SCATTER_STORE256(state, 20, X##sa); \
  MLD_SCATTER_STORE256(state, 21, X##se); \
  MLD_SCATTER_STORE256(state, 22, X##si); \
  MLD_SCATTER_STORE256(state, 23, X##so); \
  MLD_SCATTER_STORE256(state, 24, X##su);

#define MLD_COPY_STATE_VARIABLES(X, Y) \
  X##ba = Y##ba;                       \
  X##be = Y##be;                       \
  X##bi = Y##bi;                       \
  X##bo = Y##bo;                       \
  X##bu = Y##bu;                       \
  X##ga = Y##ga;                       \
  X##ge = Y##ge;                       \
  X##gi = Y##gi;                       \
  X##go = Y##go;                       \
  X##gu = Y##gu;                       \
  X##ka = Y##ka;                       \
  X##ke = Y##ke;                       \
  X##ki = Y##ki;                       \
  X##ko = Y##ko;                       \
  X##ku = Y##ku;                       \
  X##ma = Y##ma;                       \
  X##me = Y##me;                       \
  X##mi = Y##mi;                       \
  X##mo = Y##mo;                       \
  X##mu = Y##mu;                       \
  X##sa = Y##sa;                       \
  X##se = Y##se;                       \
  X##si = Y##si;                       \
  X##so = Y##so;                       \
  X##su = Y##su;

/* clang-format off */
#define MLD_ROUNDS24 \
    MLD_prepareTheta \
    MLD_thetaRhoPiChiIotaPrepareTheta( 0, A, E) \
    MLD_thetaRhoPiChiIotaPrepareTheta( 1, E, A) \
    MLD_thetaRhoPiChiIotaPrepareTheta( 2, A, E) \
    MLD_thetaRhoPiChiIotaPrepareTheta( 3, E, A) \
    MLD_thetaRhoPiChiIotaPrepareTheta( 4, A, E) \
    MLD_thetaRhoPiChiIotaPrepareTheta( 5, E, A) \
    MLD_thetaRhoPiChiIotaPrepareTheta( 6, A, E) \
    MLD_thetaRhoPiChiIotaPrepareTheta( 7, E, A) \
    MLD_thetaRhoPiChiIotaPrepareTheta( 8, A, E) \
    MLD_thetaRhoPiChiIotaPrepareTheta( 9, E, A) \
    MLD_thetaRhoPiChiIotaPrepareTheta(10, A, E) \
    MLD_thetaRhoPiChiIotaPrepareTheta(11, E, A) \
    MLD_thetaRhoPiChiIotaPrepareTheta(12, A, E) \
    MLD_thetaRhoPiChiIotaPrepareTheta(13, E, A) \
    MLD_thetaRhoPiChiIotaPrepareTheta(14, A, E) \
    MLD_thetaRhoPiChiIotaPrepareTheta(15, E, A) \
    MLD_thetaRhoPiChiIotaPrepareTheta(16, A, E) \
    MLD_thetaRhoPiChiIotaPrepareTheta(17, E, A) \
    MLD_thetaRhoPiChiIotaPrepareTheta(18, A, E) \
    MLD_thetaRhoPiChiIotaPrepareTheta(19, E, A) \
    MLD_thetaRhoPiChiIotaPrepareTheta(20, A, E) \
    MLD_thetaRhoPiChiIotaPrepareTheta(21, E, A) \
    MLD_thetaRhoPiChiIotaPrepareTheta(22, A, E) \
    MLD_thetaRhoPiChiIota(23, E, A)
/* clang-format on */

void mld_keccakf1600x4_permute24(void *states)
{
  __m256i *statesAsLanes = (__m256i *)states;
  MLD_DECLARE_ABCDE MLD_COPY_FROM_STATE(A, statesAsLanes)
      MLD_ROUNDS24 MLD_COPY_TO_STATE(statesAsLanes, A)
}

#else /* MLD_FIPS202_X86_64_XKCP && !MLD_CONFIG_MULTILEVEL_NO_SHARED */

MLD_EMPTY_CU(fips202_avx2_keccakx4)

#endif /* !(MLD_FIPS202_X86_64_XKCP && !MLD_CONFIG_MULTILEVEL_NO_SHARED) */

/* To facilitate single-compilation-unit (SCU) builds, undefine all macros.
 * Don't modify by hand -- this is auto-generated by scripts/autogen. */
#undef MLD_ANDNU256
#undef MLD_CONST256
#undef MLD_CONST256_64
#undef MLD_ROL64IN256
#undef MLD_ROL64IN256_8
#undef MLD_ROL64IN256_56
#undef MLD_STORE256
#undef MLD_XOR256
#undef MLD_XOREQ256
#undef MLD_SNP_LANELENGTHINBYTES
#undef MLD_DECLARE_ABCDE
#undef MLD_prepareTheta
#undef MLD_thetaRhoPiChiIotaPrepareTheta
#undef MLD_thetaRhoPiChiIota
#undef MLD_COPY_FROM_STATE
#undef MLD_SCATTER_STORE256
#undef MLD_COPY_TO_STATE
#undef MLD_COPY_STATE_VARIABLES
#undef MLD_ROUNDS24
