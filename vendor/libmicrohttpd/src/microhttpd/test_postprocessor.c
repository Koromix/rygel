/*
     This file is part of libmicrohttpd
     Copyright (C) 2007,2013,2019 Christian Grothoff

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
 * @brief  Testcase for postprocessor
 * @author Christian Grothoff
 */
#include "platform.h"
#include "microhttpd.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mhd_compat.h"

#ifndef WINDOWS
#include <unistd.h>
#endif

/**
 * Array of values that the value checker "wants".
 * Each series of checks should be terminated by
 * five NULL-entries.
 */
const char *want[] = {
#define URL_NOVALUE1_DATA "abc&x=5"
#define URL_NOVALUE1_START 0
  "abc", NULL, NULL, NULL, NULL,
  "x", NULL, NULL, NULL, "5",
#define URL_NOVALUE1_END (URL_NOVALUE1_START + 10)
#define URL_NOVALUE2_DATA "abc=&x=5"
#define URL_NOVALUE2_START URL_NOVALUE1_END
  "abc", NULL, NULL, NULL, "",
  "x", NULL, NULL, NULL, "5",
#define URL_NOVALUE2_END (URL_NOVALUE2_START + 10)
#define URL_DATA "abc=def&x=5"
#define URL_START URL_NOVALUE2_END
  "abc", NULL, NULL, NULL, "def",
  "x", NULL, NULL, NULL, "5",
#define URL_END (URL_START + 10)
  NULL, NULL, NULL, NULL, NULL,
#define FORM_DATA \
  "--AaB03x\r\ncontent-disposition: form-data; name=\"field1\"\r\n\r\nJoe Blow\r\n--AaB03x\r\ncontent-disposition: form-data; name=\"pics\"; filename=\"file1.txt\"\r\nContent-Type: text/plain\r\nContent-Transfer-Encoding: binary\r\n\r\nfiledata\r\n--AaB03x--\r\n"
#define FORM_START (URL_END + 5)
  "field1", NULL, NULL, NULL, "Joe Blow",
  "pics", "file1.txt", "text/plain", "binary", "filedata",
#define FORM_END (FORM_START + 10)
  NULL, NULL, NULL, NULL, NULL,
#define FORM_NESTED_DATA \
  "--AaB03x\r\ncontent-disposition: form-data; name=\"field1\"\r\n\r\nJane Blow\r\n--AaB03x\r\ncontent-disposition: form-data; name=\"pics\"\r\nContent-type: multipart/mixed, boundary=BbC04y\r\n\r\n--BbC04y\r\nContent-disposition: attachment; filename=\"file1.txt\"\r\nContent-Type: text/plain\r\n\r\nfiledata1\r\n--BbC04y\r\nContent-disposition: attachment; filename=\"file2.gif\"\r\nContent-type: image/gif\r\nContent-Transfer-Encoding: binary\r\n\r\nfiledata2\r\n--BbC04y--\r\n--AaB03x--"
#define FORM_NESTED_START (FORM_END + 5)
  "field1", NULL, NULL, NULL, "Jane Blow",
  "pics", "file1.txt", "text/plain", NULL, "filedata1",
  "pics", "file2.gif", "image/gif", "binary", "filedata2",
#define FORM_NESTED_END (FORM_NESTED_START + 15)
  NULL, NULL, NULL, NULL, NULL,
#define URL_EMPTY_VALUE_DATA "key1=value1&key2=&key3="
#define URL_EMPTY_VALUE_START (FORM_NESTED_END + 5)
  "key1", NULL, NULL, NULL, "value1",
  "key2", NULL, NULL, NULL, "",
  "key3", NULL, NULL, NULL, "",
#define URL_EMPTY_VALUE_END (URL_EMPTY_VALUE_START + 15)
  NULL, NULL, NULL, NULL, NULL
};


static int
mismatch (const char *a, const char *b)
{
  if (a == b)
    return 0;
  if ((a == NULL) || (b == NULL))
    return 1;
  return 0 != strcmp (a, b);
}


static enum MHD_Result
value_checker (void *cls,
               enum MHD_ValueKind kind,
               const char *key,
               const char *filename,
               const char *content_type,
               const char *transfer_encoding,
               const char *data,
               uint64_t off,
               size_t size)
{
  int *want_off = cls;
  int idx = *want_off;
  (void) kind;  /* Unused. Silent compiler warning. */

#if 0
  fprintf (stderr,
           "VC: `%s' `%s' `%s' `%s' `%.*s' (%d)\n",
           key, filename, content_type, transfer_encoding,
           (int) size,
           data,
           (int) size);
#endif
  if ( (0 != off) && (0 == size) )
  {
    if (NULL == want[idx + 4])
      *want_off = idx + 5;
    return MHD_YES;
  }
  if ((idx < 0) ||
      (want[idx] == NULL) ||
      (0 != strcmp (key, want[idx])) ||
      (mismatch (filename, want[idx + 1])) ||
      (mismatch (content_type, want[idx + 2])) ||
      (mismatch (transfer_encoding, want[idx + 3])) ||
      (0 != memcmp (data,
                    &want[idx + 4][off],
                    size)))
  {
    *want_off = -1;
    fprintf (stderr,
             "Failed with: `%s' `%s' `%s' `%s' `%.*s'\n",
             key, filename, content_type, transfer_encoding,
             (int) size,
             data);
    fprintf (stderr,
             "Wanted: `%s' `%s' `%s' `%s' `%s'\n",
             want[idx],
             want[idx + 1],
             want[idx + 2],
             want[idx + 3],
             want[idx + 4]);
    fprintf (stderr,
             "Unexpected result: %d/%d/%d/%d/%d/%d/%d\n",
             (idx < 0),
             (want[idx] == NULL),
             (NULL != want[idx]) && (0 != strcmp (key, want[idx])),
             (mismatch (filename, want[idx + 1])),
             (mismatch (content_type, want[idx + 2])),
             (mismatch (transfer_encoding, want[idx + 3])),
             (0 != memcmp (data, &want[idx + 4][off], size)));
    return MHD_NO;
  }
  if ( ( (NULL == want[idx + 4]) &&
         (0 == off + size) ) ||
       (off + size == strlen (want[idx + 4])) )
    *want_off = idx + 5;
  return MHD_YES;
}


static int
test_urlencoding_case (unsigned int want_start,
                       unsigned int want_end,
                       const char *url_data)
{
  struct MHD_Connection connection;
  struct MHD_HTTP_Header header;
  struct MHD_PostProcessor *pp;
  unsigned int want_off = want_start;
  size_t i;
  size_t delta;
  size_t size;

  memset (&connection, 0, sizeof (struct MHD_Connection));
  memset (&header, 0, sizeof (struct MHD_HTTP_Header));
  connection.headers_received = &header;
  header.header = MHD_HTTP_HEADER_CONTENT_TYPE;
  header.value = MHD_HTTP_POST_ENCODING_FORM_URLENCODED;
  header.header_size = MHD_STATICSTR_LEN_ (MHD_HTTP_HEADER_CONTENT_TYPE);
  header.value_size = MHD_STATICSTR_LEN_ (
    MHD_HTTP_POST_ENCODING_FORM_URLENCODED);
  header.kind = MHD_HEADER_KIND;
  pp = MHD_create_post_processor (&connection,
                                  1024,
                                  &value_checker,
                                  &want_off);
  i = 0;
  size = strlen (url_data);
  while (i < size)
  {
    delta = 1 + MHD_random_ () % (size - i);
    MHD_post_process (pp,
                      &url_data[i],
                      delta);
    i += delta;
  }
  MHD_destroy_post_processor (pp);
  if (want_off != want_end)
  {
    fprintf (stderr,
             "Test failed in line %u: %u != %u\n",
             (unsigned int) __LINE__,
             want_off,
             want_end);
    return 1;
  }
  return 0;
}


static int
test_urlencoding (void)
{
  unsigned int errorCount = 0;

  errorCount += test_urlencoding_case (URL_START,
                                       URL_END,
                                       URL_DATA);
  errorCount += test_urlencoding_case (URL_NOVALUE1_START,
                                       URL_NOVALUE1_END,
                                       URL_NOVALUE1_DATA);
  errorCount += test_urlencoding_case (URL_NOVALUE2_START,
                                       URL_NOVALUE2_END,
                                       URL_NOVALUE2_DATA);
  if (0 != errorCount)
    fprintf (stderr,
             "Test failed in line %u with %u errors\n",
             (unsigned int) __LINE__,
             errorCount);
  return errorCount;
}


static int
test_multipart_garbage (void)
{
  struct MHD_Connection connection;
  struct MHD_HTTP_Header header;
  struct MHD_PostProcessor *pp;
  unsigned int want_off;
  size_t size = strlen (FORM_DATA);
  size_t splitpoint;
  char xdata[size + 3];

  /* fill in evil garbage at the beginning */
  xdata[0] = '-';
  xdata[1] = 'x';
  xdata[2] = '\r';
  memcpy (&xdata[3], FORM_DATA, size);
  size += 3;
  for (splitpoint = 1; splitpoint < size; splitpoint++)
  {
    want_off = FORM_START;
    memset (&connection, 0, sizeof (struct MHD_Connection));
    memset (&header, 0, sizeof (struct MHD_HTTP_Header));
    connection.headers_received = &header;
    header.header = MHD_HTTP_HEADER_CONTENT_TYPE;
    header.value =
      MHD_HTTP_POST_ENCODING_MULTIPART_FORMDATA ", boundary=AaB03x";
    header.header_size = MHD_STATICSTR_LEN_ (MHD_HTTP_HEADER_CONTENT_TYPE);
    header.value_size = MHD_STATICSTR_LEN_ (
      MHD_HTTP_POST_ENCODING_MULTIPART_FORMDATA ", boundary=AaB03x");
    header.kind = MHD_HEADER_KIND;
    pp = MHD_create_post_processor (&connection,
                                    1024, &value_checker, &want_off);
    MHD_post_process (pp, xdata, splitpoint);
    MHD_post_process (pp, &xdata[splitpoint], size - splitpoint);
    MHD_destroy_post_processor (pp);
    if (want_off != FORM_END)
    {
      fprintf (stderr,
               "Test failed in line %u at point %d\n",
               (unsigned int) __LINE__,
               (int) splitpoint);
      return (int) splitpoint;
    }
  }
  return 0;
}


static int
test_multipart_splits (void)
{
  struct MHD_Connection connection;
  struct MHD_HTTP_Header header;
  struct MHD_PostProcessor *pp;
  unsigned int want_off;
  size_t size;
  size_t splitpoint;

  size = strlen (FORM_DATA);
  for (splitpoint = 1; splitpoint < size; splitpoint++)
  {
    want_off = FORM_START;
    memset (&connection, 0, sizeof (struct MHD_Connection));
    memset (&header, 0, sizeof (struct MHD_HTTP_Header));
    connection.headers_received = &header;
    header.header = MHD_HTTP_HEADER_CONTENT_TYPE;
    header.value =
      MHD_HTTP_POST_ENCODING_MULTIPART_FORMDATA ", boundary=AaB03x";
    header.header_size = strlen (header.header);
    header.value_size = strlen (header.value);
    header.kind = MHD_HEADER_KIND;
    pp = MHD_create_post_processor (&connection,
                                    1024, &value_checker, &want_off);
    MHD_post_process (pp, FORM_DATA, splitpoint);
    MHD_post_process (pp, &FORM_DATA[splitpoint], size - splitpoint);
    MHD_destroy_post_processor (pp);
    if (want_off != FORM_END)
    {
      fprintf (stderr,
               "Test failed in line %u at point %d\n",
               (unsigned int) __LINE__,
               (int) splitpoint);
      return (int) splitpoint;
    }
  }
  return 0;
}


static int
test_multipart (void)
{
  struct MHD_Connection connection;
  struct MHD_HTTP_Header header;
  struct MHD_PostProcessor *pp;
  unsigned int want_off = FORM_START;
  size_t i;
  size_t delta;
  size_t size;

  memset (&connection, 0, sizeof (struct MHD_Connection));
  memset (&header, 0, sizeof (struct MHD_HTTP_Header));
  connection.headers_received = &header;
  header.header = MHD_HTTP_HEADER_CONTENT_TYPE;
  header.value =
    MHD_HTTP_POST_ENCODING_MULTIPART_FORMDATA ", boundary=AaB03x";
  header.kind = MHD_HEADER_KIND;
  header.header_size = strlen (header.header);
  header.value_size = strlen (header.value);
  pp = MHD_create_post_processor (&connection,
                                  1024, &value_checker, &want_off);
  i = 0;
  size = strlen (FORM_DATA);
  while (i < size)
  {
    delta = 1 + MHD_random_ () % (size - i);
    MHD_post_process (pp, &FORM_DATA[i], delta);
    i += delta;
  }
  MHD_destroy_post_processor (pp);
  if (want_off != FORM_END)
  {
    fprintf (stderr,
             "Test failed in line %u\n",
             (unsigned int) __LINE__);
    return 2;
  }
  return 0;
}


static int
test_nested_multipart (void)
{
  struct MHD_Connection connection;
  struct MHD_HTTP_Header header;
  struct MHD_PostProcessor *pp;
  unsigned int want_off = FORM_NESTED_START;
  size_t i;
  size_t delta;
  size_t size;

  memset (&connection, 0, sizeof (struct MHD_Connection));
  memset (&header, 0, sizeof (struct MHD_HTTP_Header));
  connection.headers_received = &header;
  header.header = MHD_HTTP_HEADER_CONTENT_TYPE;
  header.value =
    MHD_HTTP_POST_ENCODING_MULTIPART_FORMDATA ", boundary=AaB03x";
  header.kind = MHD_HEADER_KIND;
  header.header_size = strlen (header.header);
  header.value_size = strlen (header.value);
  pp = MHD_create_post_processor (&connection,
                                  1024, &value_checker, &want_off);
  i = 0;
  size = strlen (FORM_NESTED_DATA);
  while (i < size)
  {
    delta = 1 + MHD_random_ () % (size - i);
    MHD_post_process (pp, &FORM_NESTED_DATA[i], delta);
    i += delta;
  }
  MHD_destroy_post_processor (pp);
  if (want_off != FORM_NESTED_END)
  {
    fprintf (stderr,
             "Test failed in line %u\n",
             (unsigned int) __LINE__);
    return 4;
  }
  return 0;
}


static int
test_empty_value (void)
{
  struct MHD_Connection connection;
  struct MHD_HTTP_Header header;
  struct MHD_PostProcessor *pp;
  unsigned int want_off = URL_EMPTY_VALUE_START;
  size_t i;
  size_t delta;
  size_t size;

  memset (&connection, 0, sizeof (struct MHD_Connection));
  memset (&header, 0, sizeof (struct MHD_HTTP_Header));
  connection.headers_received = &header;
  header.header = MHD_HTTP_HEADER_CONTENT_TYPE;
  header.value = MHD_HTTP_POST_ENCODING_FORM_URLENCODED;
  header.header_size = strlen (header.header);
  header.value_size = strlen (header.value);
  header.kind = MHD_HEADER_KIND;
  pp = MHD_create_post_processor (&connection,
                                  1024, &value_checker, &want_off);
  i = 0;
  size = strlen (URL_EMPTY_VALUE_DATA);
  while (i < size)
  {
    delta = 1 + MHD_random_ () % (size - i);
    MHD_post_process (pp, &URL_EMPTY_VALUE_DATA[i], delta);
    i += delta;
  }
  MHD_destroy_post_processor (pp);
  if (want_off != URL_EMPTY_VALUE_END)
  {
    fprintf (stderr,
             "Test failed in line %u at offset %d\n",
             (unsigned int) __LINE__,
             (int) want_off);
    return 8;
  }
  return 0;
}


static enum MHD_Result
value_checker2 (void *cls,
                enum MHD_ValueKind kind,
                const char *key,
                const char *filename,
                const char *content_type,
                const char *transfer_encoding,
                const char *data,
                uint64_t off,
                size_t size)
{
  return MHD_YES;
}


static int
test_overflow ()
{
  struct MHD_Connection connection;
  struct MHD_HTTP_Header header;
  struct MHD_PostProcessor *pp;
  size_t i;
  size_t j;
  size_t delta;
  char *buf;

  memset (&connection, 0, sizeof (struct MHD_Connection));
  memset (&header, 0, sizeof (struct MHD_HTTP_Header));
  connection.headers_received = &header;
  header.header = MHD_HTTP_HEADER_CONTENT_TYPE;
  header.value = MHD_HTTP_POST_ENCODING_FORM_URLENCODED;
  header.header_size = strlen (header.header);
  header.value_size = strlen (header.value);
  header.kind = MHD_HEADER_KIND;
  for (i = 128; i < 1024 * 1024; i += 1024)
  {
    pp = MHD_create_post_processor (&connection,
                                    1024,
                                    &value_checker2,
                                    NULL);
    buf = malloc (i);
    if (NULL == buf)
      return 1;
    memset (buf, 'A', i);
    buf[i / 2] = '=';
    delta = 1 + (MHD_random_ () % (i - 1));
    j = 0;
    while (j < i)
    {
      if (j + delta > i)
        delta = i - j;
      if (MHD_NO ==
          MHD_post_process (pp,
                            &buf[j],
                            delta))
        break;
      j += delta;
    }
    free (buf);
    MHD_destroy_post_processor (pp);
  }
  return 0;
}


int
main (int argc, char *const *argv)
{
  unsigned int errorCount = 0;
  (void) argc; (void) argv;  /* Unused. Silent compiler warning. */

  errorCount += test_multipart_splits ();
  errorCount += test_multipart_garbage ();
  errorCount += test_urlencoding ();
  errorCount += test_multipart ();
  errorCount += test_nested_multipart ();
  errorCount += test_empty_value ();
  errorCount += test_overflow ();
  if (errorCount != 0)
    fprintf (stderr, "Error (code: %u)\n", errorCount);
  return errorCount != 0;       /* 0 == pass */
}
