/*
 This file is part of libmicrohttpd
 Copyright (C) 2007 Christian Grothoff

 libmicrohttpd is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published
 by the Free Software Foundation; either version 2, or (at your
 option) any later version.

 libmicrohttpd is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with libmicrohttpd; see the file COPYING.  If not, write to the
 Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

/**
 * @file test_https_time_out.c
 * @brief: daemon TLS alert response test-case
 *
 * @author Sagie Amir
 */

#include "platform.h"
#include "microhttpd.h"
#include "tls_test_common.h"
#include <gcrypt.h>
#include "mhd_sockets.h" /* only macros used */


#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif /* !WIN32_LEAN_AND_MEAN */
#include <windows.h>
#endif

extern const char srv_key_pem[];
extern const char srv_self_signed_cert_pem[];

static const int TIME_OUT = 3;

static int
test_tls_session_time_out (gnutls_session_t session)
{
  int ret;
  MHD_socket sd;
  struct sockaddr_in sa;

  sd = socket (AF_INET, SOCK_STREAM, 0);
  if (sd == -1)
    {
      fprintf (stderr, "Failed to create socket: %s\n", strerror (errno));
      return -1;
    }

  memset (&sa, '\0', sizeof (struct sockaddr_in));
  sa.sin_family = AF_INET;
  sa.sin_port = htons (DEAMON_TEST_PORT);
  sa.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  gnutls_transport_set_ptr (session, (gnutls_transport_ptr_t) (intptr_t) sd);

  ret = connect (sd, (struct sockaddr *) &sa, sizeof (struct sockaddr_in));

  if (ret < 0)
    {
      fprintf (stderr, "Error: %s\n", MHD_E_FAILED_TO_CONNECT);
      MHD_socket_close_chk_ (sd);
      return -1;
    }

  ret = gnutls_handshake (session);
  if (ret < 0)
    {
      fprintf (stderr, "Handshake failed\n");
      MHD_socket_close_chk_ (sd);
      return -1;
    }

  sleep (TIME_OUT + 1);

  /* check that server has closed the connection */
  /* TODO better RST trigger */
  if (send (sd, "", 1, 0) == 0)
    {
      fprintf (stderr, "Connection failed to time-out\n");
      MHD_socket_close_chk_ (sd);
      return -1;
    }

  MHD_socket_close_chk_ (sd);
  return 0;
}


int
main (int argc, char *const *argv)
{
  int errorCount = 0;;
  struct MHD_Daemon *d;
  gnutls_session_t session;
  gnutls_datum_t key;
  gnutls_datum_t cert;
  gnutls_certificate_credentials_t xcred;


  gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0);
#ifdef GCRYCTL_INITIALIZATION_FINISHED
  gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
#endif
  gnutls_global_init ();
  gnutls_global_set_log_level (11);

  d = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_TLS |
                        MHD_USE_ERROR_LOG, DEAMON_TEST_PORT,
                        NULL, NULL, &http_dummy_ahc, NULL,
                        MHD_OPTION_CONNECTION_TIMEOUT, TIME_OUT,
                        MHD_OPTION_HTTPS_MEM_KEY, srv_key_pem,
                        MHD_OPTION_HTTPS_MEM_CERT, srv_self_signed_cert_pem,
                        MHD_OPTION_END);

  if (NULL == d)
    {
      fprintf (stderr, MHD_E_SERVER_INIT);
      return -1;
    }

  if (0 != setup_session (&session, &key, &cert, &xcred))
    {
      fprintf (stderr, "failed to setup session\n");
      return 1;
    }
  errorCount += test_tls_session_time_out (session);
  teardown_session (session, &key, &cert, xcred);

  print_test_result (errorCount, argv[0]);

  MHD_stop_daemon (d);
  gnutls_global_deinit ();

  return errorCount != 0 ? 1 : 0;
}
