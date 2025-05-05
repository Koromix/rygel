/*
  Copyright (C) 2003  Andreas Gruenbacher <agruen@suse.de>

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

/* Features we always have */

#define HAVE_ACL_LIBACL_H 1
#define HAVE_CONFIG_H 1
#define HAVE_SYS_ACL_H 1
#define HAVE_LIBACL_LIBACL_H 1

#define HAVE_ACL_DELETE_DEF_FILE 1
#define HAVE_ACL_ENTRIES 1
#define HAVE_ACL_FREE 1
#define HAVE_ACL_FROM_MODE 1
#define HAVE_ACL_FROM_TEXT 1
#define HAVE_ACL_GET_ENTRY 1
#define HAVE_ACL_GET_FD 1
#define HAVE_ACL_GET_FILE 1
#define HAVE_ACL_GET_PERM 1
#define HAVE_ACL_GET_PERMSET 1
#define HAVE_ACL_GET_TAG_TYPE 1
#define HAVE_ACL_SET_FD 1
#define HAVE_ACL_SET_FILE 1

#include "config.h"
#include "misc.h"
