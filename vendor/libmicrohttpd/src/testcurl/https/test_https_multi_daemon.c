/*
 This file is part of libmicrohttpd
 Copyright (C) 2007 Christian Grothoff
 Copyright (C) 2016-2022 Evgeny Grin (Karlson2k)

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
 * @file test_https_multi_daemon.c
 * @brief  Testcase for libmicrohttpd multiple HTTPS daemon scenario
 * @author Sagie Amir
 * @author Karlson2k (Evgeny Grin)
 */

#include "platform.h"
#include "microhttpd.h"
#include <curl/curl.h>
#include <limits.h>
#include <sys/stat.h>
#ifdef MHD_HTTPS_REQUIRE_GCRYPT
#include <gcrypt.h>
#endif /* MHD_HTTPS_REQUIRE_GCRYPT */
#include "tls_test_common.h"
#include "tls_test_keys.h"

/*
 * assert initiating two separate daemons and having one shut down
 * doesn't affect the other
 */
static unsigned int
test_concurent_daemon_pair (void *cls,
                            const char *cipher_suite,
                            int proto_version)
{
  unsigned int ret;
  enum test_get_result res;
  struct MHD_Daemon *d1;
  struct MHD_Daemon *d2;
  uint16_t port1, port2;
  (void) cls;    /* Unused. Silent compiler warning. */

  if (MHD_NO != MHD_is_feature_supported (MHD_FEATURE_AUTODETECT_BIND_PORT))
    port1 = port2 = 0;
  else
  {
    port1 = 3050;
    port2 = 3051;
  }

  d1 = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION
                         | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_TLS
                         | MHD_USE_ERROR_LOG, port1,
                         NULL, NULL, &http_ahc, NULL,
                         MHD_OPTION_HTTPS_MEM_KEY, srv_self_signed_key_pem,
                         MHD_OPTION_HTTPS_MEM_CERT, srv_self_signed_cert_pem,
                         MHD_OPTION_END);

  if (d1 == NULL)
  {
    fprintf (stderr, MHD_E_SERVER_INIT);
    return 1;
  }
  if (0 == port1)
  {
    const union MHD_DaemonInfo *dinfo;
    dinfo = MHD_get_daemon_info (d1, MHD_DAEMON_INFO_BIND_PORT);
    if ((NULL == dinfo) || (0 == dinfo->port) )
    {
      fprintf (stderr, "Cannot detect daemon bind port.\n");
      MHD_stop_daemon (d1);
      return 1;
    }
    port1 = dinfo->port;
  }

  d2 = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION
                         | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_TLS
                         | MHD_USE_ERROR_LOG, port2,
                         NULL, NULL, &http_ahc, NULL,
                         MHD_OPTION_HTTPS_MEM_KEY, srv_self_signed_key_pem,
                         MHD_OPTION_HTTPS_MEM_CERT, srv_self_signed_cert_pem,
                         MHD_OPTION_END);

  if (d2 == NULL)
  {
    MHD_stop_daemon (d1);
    fprintf (stderr, MHD_E_SERVER_INIT);
    return 1;
  }
  if (0 == port2)
  {
    const union MHD_DaemonInfo *dinfo;
    dinfo = MHD_get_daemon_info (d2, MHD_DAEMON_INFO_BIND_PORT);
    if ((NULL == dinfo) || (0 == dinfo->port) )
    {
      fprintf (stderr, "Cannot detect daemon bind port.\n");
      MHD_stop_daemon (d1);
      MHD_stop_daemon (d2);
      return 1;
    }
    port2 = dinfo->port;
  }

  res =
    test_daemon_get (NULL, cipher_suite, proto_version, port1, 0);
  ret = (unsigned int) res;
  if ((TEST_GET_HARD_ERROR == res) ||
      (TEST_GET_CURL_GEN_ERROR == res))
  {
    fprintf (stderr, "libcurl error.\nTest aborted.\n");
    MHD_stop_daemon (d2);
    MHD_stop_daemon (d1);
    return 99;
  }

  res =
    test_daemon_get (NULL, cipher_suite, proto_version,
                     port2, 0);
  ret += (unsigned int) res;
  if ((TEST_GET_HARD_ERROR == res) ||
      (TEST_GET_CURL_GEN_ERROR == res))
  {
    fprintf (stderr, "libcurl error.\nTest aborted.\n");
    MHD_stop_daemon (d2);
    MHD_stop_daemon (d1);
    return 99;
  }

  MHD_stop_daemon (d2);
  res =
    test_daemon_get (NULL, cipher_suite, proto_version, port1, 0);
  ret += (unsigned int) res;
  if ((TEST_GET_HARD_ERROR == res) ||
      (TEST_GET_CURL_GEN_ERROR == res))
  {
    fprintf (stderr, "libcurl error.\nTest aborted.\n");
    MHD_stop_daemon (d1);
    return 99;
  }
  MHD_stop_daemon (d1);
  return ret;
}


int
main (int argc, char *const *argv)
{
  unsigned int errorCount;
  (void) argc; (void) argv;       /* Unused. Silent compiler warning. */

#ifdef MHD_HTTPS_REQUIRE_GCRYPT
  gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0);
#ifdef GCRYCTL_INITIALIZATION_FINISHED
  gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
#endif
#endif /* MHD_HTTPS_REQUIRE_GCRYPT */
  if (! testsuite_curl_global_init ())
    return 99;
  if (NULL == curl_version_info (CURLVERSION_NOW)->ssl_version)
  {
    fprintf (stderr, "Curl does not support SSL.  Cannot run the test.\n");
    curl_global_cleanup ();
    return 77;
  }

  errorCount =
    test_concurent_daemon_pair (NULL, NULL, CURL_SSLVERSION_DEFAULT);

  print_test_result (errorCount, "concurent_daemon_pair");

  curl_global_cleanup ();
  if (99 == errorCount)
    return 99;

  return errorCount != 0 ? 1 : 0;
}
