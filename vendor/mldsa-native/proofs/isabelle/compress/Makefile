# Copyright (c) The mldsa-native project authors
# SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT

ISABELLE_VERSION ?= Isabelle2025
ISABELLE_HOME ?= /Applications/$(ISABELLE_VERSION).app/bin

all:
	$(ISABELLE_HOME)/isabelle build -D .

clean:
	$(ISABELLE_HOME)/isabelle build -c -D .

jedit:
	$(ISABELLE_HOME)/isabelle jedit -D . ML-DSA_Compress.thy

.PHONY: all clean jedit
