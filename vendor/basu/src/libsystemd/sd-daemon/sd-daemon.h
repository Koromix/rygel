/* SPDX-License-Identifier: LGPL-2.1+ */
#ifndef foosddaemonhfoo
#define foosddaemonhfoo
/*
  Helper call for identifying a passed file descriptor. Returns 1 if
  the file descriptor is a socket of the specified family (AF_INET,
  ...) and type (SOCK_DGRAM, SOCK_STREAM, ...), 0 otherwise. If
  family is 0 a socket family check will not be done. If type is 0 a
  socket type check will not be done and the call only verifies if
  the file descriptor refers to a socket. If listening is > 0 it is
  verified that the socket is in listening mode. (i.e. listen() has
  been called) If listening is == 0 it is verified that the socket is
  not in listening mode. If listening is < 0 no listening mode check
  is done. Returns a negative errno style error code on failure.

  See sd_is_socket(3) for more information.
*/
int sd_is_socket(int fd, int family, int type, int listening);

#endif
