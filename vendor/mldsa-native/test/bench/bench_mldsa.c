/*
 * Copyright (c) The mldsa-native project authors
 * Copyright (c) The mlkem-native project authors
 * SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT
 */
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/common.h"

#include "hal.h"
#include "mldsa_native.h"
#include "src/randombytes.h"

/* Additional SUPERCOP-style macros for functions not in the standard set */
#define crypto_sign_keypair_internal MLD_API_NAMESPACE(keypair_internal)
#define crypto_sign_signature_internal MLD_API_NAMESPACE(signature_internal)

#ifndef MLD_BENCHMARK_NWARMUP
#define MLD_BENCHMARK_NWARMUP 3
#endif
#ifndef MLD_BENCHMARK_NITERATIONS
#define MLD_BENCHMARK_NITERATIONS 5
#endif
#ifndef MLD_BENCHMARK_NTESTS
#define MLD_BENCHMARK_NTESTS 1000
#endif
#define MLEN 59
#define CTXLEN 1

#define CHECK(x)                                              \
  do                                                          \
  {                                                           \
    int rc;                                                   \
    rc = (x);                                                 \
    if (!rc)                                                  \
    {                                                         \
      fprintf(stderr, "ERROR (%s,%d)\n", __FILE__, __LINE__); \
      return 1;                                               \
    }                                                         \
  } while (0)

static int cmp_uint64_t(const void *a, const void *b)
{
  return (int)((*((const uint64_t *)a)) - (*((const uint64_t *)b)));
}

static void print_avg(const char *txt, uint64_t cyc[MLD_BENCHMARK_NTESTS])
{
  uint64_t avg = 0;
  int i;
  for (i = 0; i < MLD_BENCHMARK_NTESTS; i++)
  {
    avg += cyc[i];
  }
  avg /= (MLD_BENCHMARK_NTESTS * MLD_BENCHMARK_NITERATIONS);
  printf("%10s cycles (avg) = %" PRIu64 "\n", txt, avg);
}


static int percentiles[] = {1, 10, 20, 30, 40, 50, 60, 70, 80, 90, 99};

static void print_percentile_legend(void)
{
  unsigned i;
  printf("%21s", "percentile");
  for (i = 0; i < sizeof(percentiles) / sizeof(percentiles[0]); i++)
  {
    printf("%9d", percentiles[i]);
  }
  printf("\n");
}

static void print_percentiles(const char *txt,
                              uint64_t cyc[MLD_BENCHMARK_NTESTS])
{
  unsigned i;
  printf("%10s percentiles:", txt);
  for (i = 0; i < sizeof(percentiles) / sizeof(percentiles[0]); i++)
  {
    printf("%9" PRIu64, (cyc)[MLD_BENCHMARK_NTESTS * percentiles[i] / 100] /
                            MLD_BENCHMARK_NITERATIONS);
  }
  printf("\n");
}

static int bench(void)
{
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t sig[CRYPTO_BYTES];
  uint8_t m[MLEN];
  uint8_t ctx[CTXLEN];
  unsigned char kg_rand[MLDSA_SEEDBYTES], sig_rand[MLDSA_SEEDBYTES];
  size_t siglen;

  unsigned i, j;
  uint64_t t0, t1;

  uint64_t cycles_kg[MLD_BENCHMARK_NTESTS], cycles_sign[MLD_BENCHMARK_NTESTS],
      cycles_verify[MLD_BENCHMARK_NTESTS];
  unsigned char pre[CTXLEN + 2];

  for (i = 0; i < MLD_BENCHMARK_NTESTS; i++)
  {
    int ret = 0;
    CHECK(mld_randombytes(kg_rand, sizeof(kg_rand)) == 0);
    CHECK(mld_randombytes(sig_rand, sizeof(sig_rand)) == 0);


    /* Key-pair generation */
    for (j = 0; j < MLD_BENCHMARK_NWARMUP; j++)
    {
      ret |= crypto_sign_keypair_internal(pk, sk, kg_rand);
    }

    t0 = get_cyclecounter();
    for (j = 0; j < MLD_BENCHMARK_NITERATIONS; j++)
    {
      ret |= crypto_sign_keypair_internal(pk, sk, kg_rand);
    }
    t1 = get_cyclecounter();
    cycles_kg[i] = t1 - t0;


    /* Signing */
    CHECK(mld_randombytes(ctx, CTXLEN) == 0);
    CHECK(mld_randombytes(m, MLEN) == 0);

    pre[0] = 0;
    pre[1] = CTXLEN;
    memcpy(pre + 2, ctx, CTXLEN);


    for (j = 0; j < MLD_BENCHMARK_NWARMUP; j++)
    {
      ret |= crypto_sign_signature_internal(sig, &siglen, m, MLEN, pre,
                                            CTXLEN + 2, sig_rand, sk, 0);
    }
    t0 = get_cyclecounter();
    for (j = 0; j < MLD_BENCHMARK_NITERATIONS; j++)
    {
      ret |= crypto_sign_signature_internal(sig, &siglen, m, MLEN, pre,
                                            CTXLEN + 2, sig_rand, sk, 0);
    }
    t1 = get_cyclecounter();
    cycles_sign[i] = t1 - t0;

    /* Verification */
    for (j = 0; j < MLD_BENCHMARK_NWARMUP; j++)
    {
      ret |= crypto_sign_verify(sig, siglen, m, MLEN, ctx, CTXLEN, pk);
    }
    t0 = get_cyclecounter();
    for (j = 0; j < MLD_BENCHMARK_NITERATIONS; j++)
    {
      ret |= crypto_sign_verify(sig, siglen, m, MLEN, ctx, CTXLEN, pk);
    }
    t1 = get_cyclecounter();
    cycles_verify[i] = t1 - t0;

    CHECK(ret == 0);
  }

  print_avg("keypair", cycles_kg);
  print_avg("sign", cycles_sign);
  print_avg("verify", cycles_verify);

  printf("\n");

  qsort(cycles_kg, MLD_BENCHMARK_NTESTS, sizeof(uint64_t), cmp_uint64_t);
  qsort(cycles_sign, MLD_BENCHMARK_NTESTS, sizeof(uint64_t), cmp_uint64_t);
  qsort(cycles_verify, MLD_BENCHMARK_NTESTS, sizeof(uint64_t), cmp_uint64_t);


  print_percentile_legend();

  print_percentiles("keypair", cycles_kg);
  print_percentiles("sign", cycles_sign);
  print_percentiles("verify", cycles_verify);

  return 0;
}

int main(void)
{
  enable_cyclecounter();
  bench();
  disable_cyclecounter();

  return 0;
}
