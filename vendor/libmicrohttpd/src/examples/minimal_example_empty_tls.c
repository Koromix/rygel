/*
     This file is part of libmicrohttpd
     Copyright (C) 2007 Christian Grothoff (and other contributing authors)

     This library is free software; you can redistribute it and/or
     modify it under the terms of the GNU Lesser General Public
     License as published by the Free Software Foundation; either
     version 2.1 of the License, or (at your option) any later version.

     This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     Lesser General Public License for more details.

     You should have received a copy of the GNU Lesser General Public
     License along with this library; if not, write to the Free Software
     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
/**
 * @file minimal_example_empty_ssl.c
 * @brief minimal example for how to use libmicrohttpd
 * @author Christian Grothoff
 */

#include "platform.h"
#include <microhttpd.h>


static enum MHD_Result
ahc_echo (void *cls,
          struct MHD_Connection *connection,
          const char *url,
          const char *method,
          const char *version,
          const char *upload_data,
          size_t *upload_data_size,
          void **ptr)
{
  static int aptr;
  struct MHD_Response *response;
  enum MHD_Result ret;

  (void) cls;               /* Unused. Silent compiler warning. */
  (void) url;               /* Unused. Silent compiler warning. */
  (void) version;           /* Unused. Silent compiler warning. */
  (void) upload_data;       /* Unused. Silent compiler warning. */
  (void) upload_data_size;  /* Unused. Silent compiler warning. */

  if (0 != strcmp (method, "GET"))
    return MHD_NO;              /* unexpected method */
  if (&aptr != *ptr)
  {
    /* do never respond on first call */
    *ptr = &aptr;
    return MHD_YES;
  }
  *ptr = NULL;                  /* reset when done */
  response = MHD_create_response_from_buffer (0,
                                              NULL,
                                              MHD_RESPMEM_PERSISTENT);
  ret = MHD_queue_response (connection,
                            MHD_HTTP_NO_CONTENT,
                            response);
  MHD_destroy_response (response);
  return ret;
}


/* test server key */
const char srv_signed_key_pem[] = "-----BEGIN RSA PRIVATE KEY-----\n"
                                  "MIIEowIBAAKCAQEAvfTdv+3fgvVTKRnP/HVNG81cr8TrUP/iiyuve/THMzvFXhCW\n"
                                  "+K03KwEku55QvnUndwBfU/ROzLlv+5hotgiDRNFT3HxurmhouySBrJNJv7qWp8IL\n"
                                  "q4sw32vo0fbMu5BZF49bUXK9L3kW2PdhTtSQPWHEzNrCxO+YgCilKHkY3vQNfdJ0\n"
                                  "20Q5EAAEseD1YtWCIpRvJzYlZMpjYB1ubTl24kwrgOKUJYKqM4jmF4DVQp4oOK/6\n"
                                  "QYGGh1QmHRPAy3CBII6sbb+sZT9cAqU6GYQVB35lm4XAgibXV6KgmpVxVQQ69U6x\n"
                                  "yoOl204xuekZOaG9RUPId74Rtmwfi1TLbBzo2wIDAQABAoIBADu09WSICNq5cMe4\n"
                                  "+NKCLlgAT1NiQpLls1gKRbDhKiHU9j8QWNvWWkJWrCya4QdUfLCfeddCMeiQmv3K\n"
                                  "lJMvDs+5OjJSHFoOsGiuW2Ias7IjnIojaJalfBml6frhJ84G27IXmdz6gzOiTIer\n"
                                  "DjeAgcwBaKH5WwIay2TxIaScl7AwHBauQkrLcyb4hTmZuQh6ArVIN6+pzoVuORXM\n"
                                  "bpeNWl2l/HSN3VtUN6aCAKbN/X3o0GavCCMn5Fa85uJFsab4ss/uP+2PusU71+zP\n"
                                  "sBm6p/2IbGvF5k3VPDA7X5YX61sukRjRBihY8xSnNYx1UcoOsX6AiPnbhifD8+xQ\n"
                                  "Tlf8oJUCgYEA0BTfzqNpr9Wxw5/QXaSdw7S/0eP5a0C/nwURvmfSzuTD4equzbEN\n"
                                  "d+dI/s2JMxrdj/I4uoAfUXRGaabevQIjFzC9uyE3LaOyR2zhuvAzX+vVcs6bSXeU\n"
                                  "pKpCAcN+3Z3evMaX2f+z/nfSUAl2i4J2R+/LQAWJW4KwRky/m+cxpfUCgYEA6bN1\n"
                                  "b73bMgM8wpNt6+fcmS+5n0iZihygQ2U2DEud8nZJL4Nrm1dwTnfZfJBnkGj6+0Q0\n"
                                  "cOwj2KS0/wcEdJBP0jucU4v60VMhp75AQeHqidIde0bTViSRo3HWKXHBIFGYoU3T\n"
                                  "LyPyKndbqsOObnsFXHn56Nwhr2HLf6nw4taGQY8CgYBoSW36FLCNbd6QGvLFXBGt\n"
                                  "2lMhEM8az/K58kJ4WXSwOLtr6MD/WjNT2tkcy0puEJLm6BFCd6A6pLn9jaKou/92\n"
                                  "SfltZjJPb3GUlp9zn5tAAeSSi7YMViBrfuFiHObij5LorefBXISLjuYbMwL03MgH\n"
                                  "Ocl2JtA2ywMp2KFXs8GQWQKBgFyIVv5ogQrbZ0pvj31xr9HjqK6d01VxIi+tOmpB\n"
                                  "4ocnOLEcaxX12BzprW55ytfOCVpF1jHD/imAhb3YrHXu0fwe6DXYXfZV4SSG2vB7\n"
                                  "IB9z14KBN5qLHjNGFpMQXHSMek+b/ftTU0ZnPh9uEM5D3YqRLVd7GcdUhHvG8P8Q\n"
                                  "C9aXAoGBAJtID6h8wOGMP0XYX5YYnhlC7dOLfk8UYrzlp3xhqVkzKthTQTj6wx9R\n"
                                  "GtC4k7U1ki8oJsfcIlBNXd768fqDVWjYju5rzShMpo8OCTS6ipAblKjCxPPVhIpv\n"
                                  "tWPlbSn1qj6wylstJ5/3Z+ZW5H4wIKp5jmLiioDhcP0L/Ex3Zx8O\n"
                                  "-----END RSA PRIVATE KEY-----\n";

/* test server CA signed certificates */
const char srv_signed_cert_pem[] = "-----BEGIN CERTIFICATE-----\n"
                                   "MIIDGzCCAgWgAwIBAgIES0KCvTALBgkqhkiG9w0BAQUwFzEVMBMGA1UEAxMMdGVz\n"
                                   "dF9jYV9jZXJ0MB4XDTEwMDEwNTAwMDcyNVoXDTQ1MDMxMjAwMDcyNVowFzEVMBMG\n"
                                   "A1UEAxMMdGVzdF9jYV9jZXJ0MIIBHzALBgkqhkiG9w0BAQEDggEOADCCAQkCggEA\n"
                                   "vfTdv+3fgvVTKRnP/HVNG81cr8TrUP/iiyuve/THMzvFXhCW+K03KwEku55QvnUn\n"
                                   "dwBfU/ROzLlv+5hotgiDRNFT3HxurmhouySBrJNJv7qWp8ILq4sw32vo0fbMu5BZ\n"
                                   "F49bUXK9L3kW2PdhTtSQPWHEzNrCxO+YgCilKHkY3vQNfdJ020Q5EAAEseD1YtWC\n"
                                   "IpRvJzYlZMpjYB1ubTl24kwrgOKUJYKqM4jmF4DVQp4oOK/6QYGGh1QmHRPAy3CB\n"
                                   "II6sbb+sZT9cAqU6GYQVB35lm4XAgibXV6KgmpVxVQQ69U6xyoOl204xuekZOaG9\n"
                                   "RUPId74Rtmwfi1TLbBzo2wIDAQABo3YwdDAMBgNVHRMBAf8EAjAAMBMGA1UdJQQM\n"
                                   "MAoGCCsGAQUFBwMBMA8GA1UdDwEB/wQFAwMHIAAwHQYDVR0OBBYEFOFi4ilKOP1d\n"
                                   "XHlWCMwmVKr7mgy8MB8GA1UdIwQYMBaAFP2olB4s2T/xuoQ5pT2RKojFwZo2MAsG\n"
                                   "CSqGSIb3DQEBBQOCAQEAHVWPxazupbOkG7Did+dY9z2z6RjTzYvurTtEKQgzM2Vz\n"
                                   "GQBA+3pZ3c5mS97fPIs9hZXfnQeelMeZ2XP1a+9vp35bJjZBBhVH+pqxjCgiUflg\n"
                                   "A3Zqy0XwwVCgQLE2HyaU3DLUD/aeIFK5gJaOSdNTXZLv43K8kl4cqDbMeRpVTbkt\n"
                                   "YmG4AyEOYRNKGTqMEJXJoxD5E3rBUNrVI/XyTjYrulxbNPcMWEHKNeeqWpKDYTFo\n"
                                   "Bb01PCthGXiq/4A2RLAFosadzRa8SBpoSjPPfZ0b2w4MJpReHqKbR5+T2t6hzml6\n"
                                   "4ToyOKPDmamiTuN5KzLN3cw7DQlvWMvqSOChPLnA3Q==\n"
                                   "-----END CERTIFICATE-----\n";


int
main (int argc,
      char *const *argv)
{
  struct MHD_Daemon *d;

  if (argc != 2)
  {
    printf ("%s PORT\n", argv[0]);
    return 1;
  }
  d = MHD_start_daemon (/* MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG, */
    MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG
    | MHD_USE_TLS,
    /* MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG | MHD_USE_POLL, */
    /* MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG | MHD_USE_POLL, */
    /* MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG, */
    atoi (argv[1]),
    NULL, NULL, &ahc_echo, NULL,
    MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120,
    MHD_OPTION_STRICT_FOR_CLIENT, (int) 1,
    /* Optionally, the gnutls_load_file() can be used to
       load the key and the certificate from file. */
    MHD_OPTION_HTTPS_MEM_KEY, srv_signed_key_pem,
    MHD_OPTION_HTTPS_MEM_CERT, srv_signed_cert_pem,
    MHD_OPTION_END);
  if (d == NULL)
    return 1;
  (void) getc (stdin);
  MHD_stop_daemon (d);
  return 0;
}
