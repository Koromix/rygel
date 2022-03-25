/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

void test_setup_logging(int level);
int log_tests_skipped(const char *message);
int log_tests_skipped_errno(int ret, const char *message);
