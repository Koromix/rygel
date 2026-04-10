[//]: # (SPDX-License-Identifier: CC-BY-4.0)

# HOL Light functional correctness proofs

This directory contains functional correctness proofs for AArch64 and x86_64 assembly routines
used in mldsa-native. The proofs are written in the [HOL Light](https://hol-light.github.io/) theorem
prover, utilizing the assembly verification infrastructure from [s2n-bignum](https://github.com/awslabs/s2n-bignum).

Each function is proved in a separate `.ml` file in [aarch64/proofs/](aarch64/proofs) and [x86_64/proofs/](x86_64/proofs). Each file
contains the byte code being verified, as well as the specification that is being proved.

## Reproducing the proofs

To reproduce the proofs, enter the nix shell via

```bash
nix develop .#hol_light --experimental-features 'nix-command flakes'
```

from mldsa-native's base directory. Then

```bash
make -C proofs/hol_light/aarch64
```
or

```bash
make -C proofs/hol_light/x86_64
```

will build and run the proofs. Note that this may take hours even on powerful machines.

For convenience, you can also use `tests hol_light` which wraps the `make` invocation above; see `tests hol_light --help`.

## Interactive proof development

For interactive proof development, start the HOL Light server:

```bash
hol-server [port]  # default port is 2012
```

Then use the [HOL Light extension for VS Code](https://marketplace.visualstudio.com/items?itemName=monadius.hol-light-simple)
to connect and send commands interactively.

Alternatively, send commands using netcat:

```bash
echo '1+1;;' | nc -w 5 127.0.0.1 2012
```

## What is covered?

- AArch64 poly_caddq: [mldsa_poly_caddq.S](aarch64/mldsa/mldsa_poly_caddq.S)
- AArch64 poly_chknorm: [mldsa_poly_chknorm.S](aarch64/mldsa/mldsa_poly_chknorm.S)
- x86_64 forward NTT: [mldsa_ntt.S](x86_64/mldsa/mldsa_ntt.S)
- x86_64 inverse NTT: [mldsa_intt.S](x86_64/mldsa/mldsa_intt.S)
