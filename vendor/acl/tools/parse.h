/*
  File: parse.h
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

#ifndef __PARSE_H
#define __PARSE_H


#include <stdlib.h>
#include <sys/types.h>
#include "sequence.h"


#ifdef __cplusplus
extern "C" {
#endif


/* parse options */

#define SEQ_PARSE_WITH_PERM	(0x0001)
#define SEQ_PARSE_NO_PERM	(0x0002)
#define SEQ_PARSE_ANY_PERM	(0x0001|0x0002)

#define SEQ_PARSE_MULTI		(0x0010)
#define SEQ_PARSE_DEFAULT	(0x0020)	/* "default:" = default acl */

#define SEQ_PROMOTE_ACL		(0x0040)	/* promote from acl
                                                   to default acl */

cmd_t
parse_acl_cmd(
	const char **text_p,
	int seq_cmd,
	int parse_mode);
int
parse_acl_seq(
	seq_t seq,
	const char *text_p,
	int *which,
	int seq_cmd,
	int parse_mode);
int
read_acl_comments(
	FILE *file,
	int *lineno,
	char **path_p,
	uid_t *uid_p,
	gid_t *gid_p,
	mode_t *flags);
int
read_acl_seq(
	FILE *file,
	seq_t seq,
	int seq_cmd,
	int parse_mode,
	int *lineno,
	int *which);


#ifdef __cplusplus
}
#endif


#endif  /* __PARSE_H */

