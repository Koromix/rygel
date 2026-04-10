[//]: # (SPDX-License-Identifier: CC-BY-4.0)

# ML-DSA Compression Proofs

Isabelle/HOL proofs for the Barrett division used in ML-DSA's `Decompose` routine.

## Theory Files

- [Rounding.thy](Rounding.thy) — Round-half-down rounding and its stability properties
- [Barrett_Division.thy](Barrett_Division.thy) — Barrett approximation and correctness theorem
- [ML-DSA_Compress.thy](ML-DSA_Compress.thy) — ML-DSA specific instantiations for C/AVX2 and AArch64

## Building

Assuming the `isabelle` binary is in your PATH, you can build via.

```
isabelle build -D .
```

Tested with Isabelle2025.1 and Isabelle2025.2.

Alternatively, you can use the provided [Makefile](Makefile). Use

```
make jedit
```

to start an interactive proof session in Isabelle/jEdit, and

```
make
```

to build the proofs from the command line.

To use the Makefile, you need to set the `ISABELLE_VERSION` to your
Isabelle version, and `ISABELLE_HOME` to the full path of the directory
containing the `isabelle` binary.

## Limitation

The proofs operate on unbounded integers, not fixed-width machine words.
Connecting to actual implementations requires additional word-level reasoning.
