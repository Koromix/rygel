/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

int getenv_bool(const char *p);

#if !HAVE_SECURE_GETENV
char *secure_getenv(const char *name);
#endif
