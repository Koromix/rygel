/*
  File: sequence.h
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


#ifndef __SEQUENCE_H
#define __SEQUENCE_H


#include <sys/acl.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef unsigned int cmd_tag_t;

struct cmd_obj {
	cmd_tag_t		c_cmd;
	acl_type_t		c_type;
	acl_tag_t		c_tag;
	uid_t			c_id;
	mode_t			c_perm;
	struct cmd_obj		*c_next;
};

typedef struct cmd_obj *cmd_t;

struct seq_obj {
	cmd_t			s_first;
	cmd_t			s_last;
};

typedef struct seq_obj *seq_t;

/* command types */
#define CMD_ENTRY_REPLACE	(0)
#define CMD_REMOVE_ENTRY	(3)
#define CMD_REMOVE_EXTENDED_ACL	(4)
#define CMD_REMOVE_ACL		(5)

/* constants for permission specifiers */
#define CMD_PERM_READ		(4)
#define CMD_PERM_WRITE		(2)
#define CMD_PERM_EXECUTE	(1)
#define CMD_PERM_COND_EXECUTE	(8)

/* iteration over command sequence */
#define SEQ_FIRST_CMD		(0)
#define SEQ_NEXT_CMD		(1)

/* command sequence manipulation */

cmd_t
cmd_init(
	void);
void
cmd_free(
	cmd_t cmd);
seq_t
seq_init(
	void);
int
seq_free(
	seq_t seq);
int
seq_empty(
	seq_t seq);
int
seq_append(
	seq_t seq,
	cmd_t cmd);
int
seq_append_cmd(
	seq_t seq,
	cmd_tag_t cmd,
	acl_type_t type);
int
seq_get_cmd(
	seq_t seq,
	int which,
	cmd_t *cmd);
int
seq_delete_cmd(
	seq_t seq,
	cmd_t cmd);


#ifdef __cplusplus
}
#endif


#endif  /* __SEQUENCE_H */

