/*
  File: sequence.c
  (Linux Access Control List Management)

  Copyright (C) 1999, 2000
  Andreas Gruenbacher, <andreas.gruenbacher@gmail.com>

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


#include "config.h"
#include <stdlib.h>
#include "sequence.h"


cmd_t
cmd_init(
	void)
{
	cmd_t cmd;

	cmd = malloc(sizeof(struct cmd_obj));
	if (cmd) {
		cmd->c_tag = ACL_UNDEFINED_TAG;
		cmd->c_perm = 0;
	}
	return cmd;
}


void
cmd_free(
	cmd_t cmd)
{
	free(cmd);
}


seq_t
seq_init(
	void)
{
	seq_t seq = (seq_t)malloc(sizeof(struct seq_obj));
	if (seq == NULL)
		return NULL;
	seq->s_first = seq->s_last = NULL;
	return seq;
}


int
seq_free(
	seq_t seq)
{
	cmd_t cmd = seq->s_first;
	while (cmd) {
		seq->s_first = seq->s_first->c_next;
		cmd_free(cmd);
		cmd = seq->s_first;
	}
	free(seq);
	return 0;
}


int
seq_empty(
	seq_t seq)
{
	return (seq->s_first == NULL);
}


int
seq_append(
	seq_t seq,
	cmd_t cmd)
{
	cmd->c_next = NULL;
	if (seq->s_first == NULL) {
		seq->s_first = seq->s_last = cmd;
	} else {
		seq->s_last->c_next = cmd;
		seq->s_last = cmd;
	}
	return 0;
}


int
seq_append_cmd(
	seq_t seq,
	cmd_tag_t cmd,
	acl_type_t type)
{
	cmd_t cmd_d = cmd_init();
	if (cmd_d == NULL)
		return -1;
	cmd_d->c_cmd = cmd;
	cmd_d->c_type = type;
	if (seq_append(seq, cmd_d) != 0) {
		cmd_free(cmd_d);
		return -1;
	}
	return 0;
}


int
seq_get_cmd(
	seq_t seq,
	int which,
	cmd_t *cmd)
{
	if (which == SEQ_FIRST_CMD) {
		if (seq->s_first == NULL)
			return 0;
		if (cmd)
			*cmd = seq->s_first;
		return 1;
	} else if (which == SEQ_NEXT_CMD) {
		if (cmd == NULL)
			return -1;
		if (*cmd) {
			*cmd = (*cmd)->c_next;
			return (*cmd == NULL) ? 0 : 1;
		}
		return 0;
	} else {
		return -1;
	}
}


int
seq_delete_cmd(
	seq_t seq,
	cmd_t cmd)
{
	cmd_t prev = seq->s_first;

	if (cmd == seq->s_first) {
		seq->s_first = seq->s_first->c_next;
		cmd_free(cmd);
		return 0;
	}
	while (prev != NULL && prev->c_next != cmd)
		prev = prev->c_next;
	if (prev == NULL)
		return -1;
	if (cmd == seq->s_last)
		seq->s_last = prev;
	prev->c_next = cmd->c_next;
	cmd_free(cmd);
	return 0;
}

