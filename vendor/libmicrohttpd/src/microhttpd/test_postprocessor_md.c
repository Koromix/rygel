/*
     This file is part of libmicrohttpd
     Copyright (C) 2020 Christian Grothoff

     libmicrohttpd is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
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
 * @file test_postprocessor.c
 * @brief  Testcase for postprocessor, keys with no value
 * @author Markus Doppelbauer
 */
#include "MHD_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <microhttpd.h>

/**
 * Handler for fatal errors.
 */
MHD_PanicCallback mhd_panic;

/**
 * Closure argument for #mhd_panic.
 */
void *mhd_panic_cls;


enum MHD_Result
MHD_lookup_connection_value_n (struct MHD_Connection *connection,
                               enum MHD_ValueKind kind,
                               const char *key,
                               size_t key_size,
                               const char **value_ptr,
                               size_t *value_size_ptr)
{
  return MHD_NO;
}


#include "mhd_str.c"
#include "internal.c"
#include "postprocessor.c"


static unsigned int found;


static enum MHD_Result
post_data_iterator (void *cls,
                    enum MHD_ValueKind kind,
                    const char *key,
                    const char *filename,
                    const char *content_type,
                    const char *transfer_encoding,
                    const char *data,
                    uint64_t off,
                    size_t size)
{
  fprintf (stderr,
           "%s\t%s\n",
           key,
           data);
  if (0 == strcmp (key, "xxxx"))
  {
    if ( (4 != size) ||
         (0 != memcmp (data, "xxxx", 4)) )
      exit (1);
    found |= 1;
  }
  if (0 == strcmp (key, "yyyy"))
  {
    if ( (4 != size) ||
         (0 != memcmp (data, "yyyy", 4)) )
      exit (1);
    found |= 2;
  }
  if (0 == strcmp (key, "zzzz"))
  {
    if (0 != size)
      exit (1);
    found |= 4;
  }
  if (0 == strcmp (key, "aaaa"))
  {
    if (0 != size)
      exit (1);
    found |= 8;
  }
  return MHD_YES;
}


int
main (int argc, char *argv[])
{
  struct MHD_PostProcessor *postprocessor;

  postprocessor = malloc (sizeof (struct MHD_PostProcessor)
                          + 0x1000 + 1);
  if (NULL == postprocessor)
    return 77;
  memset (postprocessor,
          0,
          sizeof (struct MHD_PostProcessor) + 0x1000 + 1);
  postprocessor->ikvi = &post_data_iterator;
  postprocessor->encoding = MHD_HTTP_POST_ENCODING_FORM_URLENCODED;
  postprocessor->buffer_size = 0x1000;
  postprocessor->state = PP_Init;
  postprocessor->skip_rn = RN_Inactive;
  MHD_post_process (postprocessor, "xxxx=xxxx", 9);
  MHD_post_process (postprocessor, "&yyyy=yyyy&zzzz=&aaaa=", 22);
  MHD_post_process (postprocessor, "", 0);
  if (MHD_YES !=
      MHD_destroy_post_processor (postprocessor))
    exit (3);
  if (found != 15)
    exit (2);
  return EXIT_SUCCESS;
}
