/*
  File: do_set.h
  (Linux Access Control List Management)

  Copyright (C) 2009 by Andreas Gruenbacher
  <a.gruenbacher@computer.org>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <https://www.gnu.org/licenses/>.
*/

#ifndef __DO_SET_H
#define __DO_SET_H

#include "sequence.h"

struct do_set_args {
	seq_t seq;
	mode_t mode;
};

extern int do_set(const char *path_p, const struct stat *stat_p, int flags,
		  void *arg);

/* We need these exported to us. */
extern const char *progname;
extern int opt_recalculate;
extern int opt_test;

#endif  /* __DO_SET_H */
