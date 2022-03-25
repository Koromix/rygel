/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/* Missing glibc definitions to access certain kernel APIs */

#include <unistd.h>

#if HAVE_AUDIT
#include <libaudit.h>
#endif

#ifdef ARCH_MIPS
#include <asm/sgidefs.h>
#endif

#ifndef F_LINUX_SPECIFIC_BASE
#define F_LINUX_SPECIFIC_BASE 1024
#endif

#ifndef F_ADD_SEALS
#define F_ADD_SEALS (F_LINUX_SPECIFIC_BASE + 9)

#define F_SEAL_SEAL     0x0001  /* prevent further seals from being set */
#define F_SEAL_SHRINK   0x0002  /* prevent file from shrinking */
#define F_SEAL_GROW     0x0004  /* prevent file from growing */
#define F_SEAL_WRITE    0x0008  /* prevent writes */
#endif

#ifndef MS_REC
#define MS_REC 16384
#endif

#ifndef MS_REC
#define MS_REC          (1<<19)
#endif

#ifndef PR_SET_MM_ARG_START
#define PR_SET_MM_ARG_START 8
#endif

#ifndef PR_SET_MM_ARG_END
#define PR_SET_MM_ARG_END 9
#endif

#ifndef TFD_TIMER_CANCEL_ON_SET
#  define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif

#ifndef SO_REUSEPORT
#  define SO_REUSEPORT 15
#endif

#ifndef SO_PEERGROUPS
#  define SO_PEERGROUPS 59
#endif

#ifndef KEYCTL_DESCRIBE
#define KEYCTL_DESCRIBE 6
#endif

#ifndef KEYCTL_READ
#define KEYCTL_READ 11
#endif


/* The maximum thread/process name length including trailing NUL byte. This mimics the kernel definition of the same
 * name, which we need in userspace at various places but is not defined in userspace currently, neither under this
 * name nor any other. */
#ifndef TASK_COMM_LEN
#define TASK_COMM_LEN 16
#endif

#ifdef __FreeBSD__
#define ENOMEDIUM       (INT_MAX - 1)
#define ENOPKG          (INT_MAX - 2)
#define EUNATCH         (INT_MAX - 3)
#define ENODATA         (INT_MAX - 4)
#define ELNRNG          (INT_MAX - 5)
#endif
