/*
  File: user_group.h
  (Linux Access Control List Management)

  Copyright (C) 1999 by Andreas Gruenbacher
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

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

const char *
user_name(uid_t uid, int numeric);
const char *
group_name(gid_t uid, int numeric);

