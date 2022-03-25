/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdbool.h>

/* Regular colors */
#define ANSI_BLACK   "\x1B[0;30m"
#define ANSI_RED     "\x1B[0;31m"
#define ANSI_GREEN   "\x1B[0;32m"
#define ANSI_YELLOW  "\x1B[0;33m"
#define ANSI_BLUE    "\x1B[0;34m"
#define ANSI_MAGENTA "\x1B[0;35m"
#define ANSI_CYAN    "\x1B[0;36m"
#define ANSI_WHITE   "\x1B[0;37m"

/* Bold/highlighted */
#define ANSI_HIGHLIGHT_BLACK   "\x1B[0;1;30m"
#define ANSI_HIGHLIGHT_RED     "\x1B[0;1;31m"
#define ANSI_HIGHLIGHT_GREEN   "\x1B[0;1;32m"
#define ANSI_HIGHLIGHT_YELLOW  "\x1B[0;1;33m"
#define ANSI_HIGHLIGHT_BLUE    "\x1B[0;1;34m"
#define ANSI_HIGHLIGHT_MAGENTA "\x1B[0;1;35m"
#define ANSI_HIGHLIGHT_CYAN    "\x1B[0;1;36m"
#define ANSI_HIGHLIGHT_WHITE   "\x1B[0;1;37m"

/* Other ANSI codes */
#define ANSI_HIGHLIGHT "\x1B[0;1;39m"

/* Reset/clear ANSI styles */
#define ANSI_NORMAL "\x1B[0m"

bool on_tty(void);
bool terminal_is_dumb(void);
bool colors_enabled(void);

#define DEFINE_ANSI_FUNC(name, NAME)                            \
        static inline const char *ansi_##name(void) {           \
                return colors_enabled() ? ANSI_##NAME : "";     \
        }

DEFINE_ANSI_FUNC(highlight,                  HIGHLIGHT);
DEFINE_ANSI_FUNC(highlight_red,              HIGHLIGHT_RED);
DEFINE_ANSI_FUNC(highlight_green,            HIGHLIGHT_GREEN);
DEFINE_ANSI_FUNC(highlight_yellow,           HIGHLIGHT_YELLOW);
DEFINE_ANSI_FUNC(highlight_blue,             HIGHLIGHT_BLUE);
DEFINE_ANSI_FUNC(highlight_magenta,          HIGHLIGHT_MAGENTA);
DEFINE_ANSI_FUNC(normal,                     NORMAL);

int get_ctty_devnr(pid_t pid, dev_t *d);
int get_ctty(pid_t, dev_t *_devnr, char **r);
