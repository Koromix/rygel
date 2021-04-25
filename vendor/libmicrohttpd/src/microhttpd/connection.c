/*
     This file is part of libmicrohttpd
     Copyright (C) 2007-2020 Daniel Pittman and Christian Grothoff

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
 * @file connection.c
 * @brief  Methods for managing connections
 * @author Daniel Pittman
 * @author Christian Grothoff
 * @author Karlson2k (Evgeny Grin)
 */
#include "internal.h"
#include "mhd_limits.h"
#include "connection.h"
#include "memorypool.h"
#include "response.h"
#include "mhd_mono_clock.h"
#include "mhd_str.h"
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
#include "mhd_locks.h"
#endif
#include "mhd_sockets.h"
#include "mhd_compat.h"
#include "mhd_itc.h"
#ifdef MHD_LINUX_SOLARIS_SENDFILE
#include <sys/sendfile.h>
#endif /* MHD_LINUX_SOLARIS_SENDFILE */
#if defined(HAVE_FREEBSD_SENDFILE) || defined(HAVE_DARWIN_SENDFILE)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#endif /* HAVE_FREEBSD_SENDFILE || HAVE_DARWIN_SENDFILE */
#ifdef HTTPS_SUPPORT
#include "connection_https.h"
#endif /* HTTPS_SUPPORT */
#ifdef HAVE_SYS_PARAM_H
/* For FreeBSD version identification */
#include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#include "mhd_send.h"

/**
 * Message to transmit when http 1.1 request is received
 */
#define HTTP_100_CONTINUE "HTTP/1.1 100 Continue\r\n\r\n"

/**
 * Response text used when the request (http header) is too big to
 * be processed.
 *
 * Intentionally empty here to keep our memory footprint
 * minimal.
 */
#ifdef HAVE_MESSAGES
#define REQUEST_TOO_BIG \
  "<html><head><title>Request too big</title></head><body>Your HTTP header was too big for the memory constraints of this webserver.</body></html>"
#else
#define REQUEST_TOO_BIG ""
#endif

/**
 * Response text used when the request (http header) does not
 * contain a "Host:" header and still claims to be HTTP 1.1.
 *
 * Intentionally empty here to keep our memory footprint
 * minimal.
 */
#ifdef HAVE_MESSAGES
#define REQUEST_LACKS_HOST \
  "<html><head><title>&quot;Host:&quot; header required</title></head><body>In HTTP 1.1, requests must include a &quot;Host:&quot; header, and your HTTP 1.1 request lacked such a header.</body></html>"
#else
#define REQUEST_LACKS_HOST ""
#endif

/**
 * Response text used when the request (http header) is
 * malformed.
 *
 * Intentionally empty here to keep our memory footprint
 * minimal.
 */
#ifdef HAVE_MESSAGES
#define REQUEST_MALFORMED \
  "<html><head><title>Request malformed</title></head><body>Your HTTP request was syntactically incorrect.</body></html>"
#else
#define REQUEST_MALFORMED ""
#endif

/**
 * Response text used when there is an internal server error.
 *
 * Intentionally empty here to keep our memory footprint
 * minimal.
 */
#ifdef HAVE_MESSAGES
#define INTERNAL_ERROR \
  "<html><head><title>Internal server error</title></head><body>Please ask the developer of this Web server to carefully read the GNU libmicrohttpd documentation about connection management and blocking.</body></html>"
#else
#define INTERNAL_ERROR ""
#endif


/**
 * sendfile() chuck size
 */
#define MHD_SENFILE_CHUNK_         (0x20000)

/**
 * sendfile() chuck size for thread-per-connection
 */
#define MHD_SENFILE_CHUNK_THR_P_C_ (0x200000)

#ifdef HAVE_MESSAGES
/**
 * Return text description for MHD_ERR_*_ codes
 * @param mhd_err_code the error code
 * @return pointer to static string with error description
 */
static const char *
str_conn_error_ (ssize_t mhd_err_code)
{
  switch (mhd_err_code)
  {
  case MHD_ERR_AGAIN_:
    return _ ("The operation would block, retry later");
  case MHD_ERR_CONNRESET_:
    return _ ("The connection was forcibly closed by remote peer");
  case MHD_ERR_NOTCONN_:
    return _ ("The socket is not connected");
  case MHD_ERR_NOMEM_:
    return _ ("Not enough system resources to serve the request");
  case MHD_ERR_BADF_:
    return _ ("Bad FD value");
  case MHD_ERR_INVAL_:
    return _ ("Argument value is invalid");
  case MHD_ERR_OPNOTSUPP_:
    return _ ("Argument value is not supported");
  case MHD_ERR_PIPE_:
    return _ ("The socket is no longer available for sending");
  case MHD_ERR_TLS_:
    return _ ("TLS encryption or decryption error");
  default:
    break;   /* Mute compiler warning */
  }
  if (0 <= mhd_err_code)
    return _ ("Not an error code");

  mhd_assert (0); /* Should never be reachable */
  return _ ("Wrong error code value");
}


#endif /* HAVE_MESSAGES */

/**
 * Callback for receiving data from the socket.
 *
 * @param connection the MHD connection structure
 * @param other where to write received data to
 * @param i maximum size of other (in bytes)
 * @return positive value for number of bytes actually received or
 *         negative value for error number MHD_ERR_xxx_
 */
static ssize_t
recv_param_adapter (struct MHD_Connection *connection,
                    void *other,
                    size_t i)
{
  ssize_t ret;

  if ( (MHD_INVALID_SOCKET == connection->socket_fd) ||
       (MHD_CONNECTION_CLOSED == connection->state) )
  {
    return MHD_ERR_NOTCONN_;
  }
  if (i > MHD_SCKT_SEND_MAX_SIZE_)
    i = MHD_SCKT_SEND_MAX_SIZE_; /* return value limit */

  ret = MHD_recv_ (connection->socket_fd,
                   other,
                   i);
  if (0 > ret)
  {
    const int err = MHD_socket_get_error_ ();
    if (MHD_SCKT_ERR_IS_EAGAIN_ (err))
    {
#ifdef EPOLL_SUPPORT
      /* Got EAGAIN --- no longer read-ready */
      connection->epoll_state &= ~MHD_EPOLL_STATE_READ_READY;
#endif /* EPOLL_SUPPORT */
      return MHD_ERR_AGAIN_;
    }
    if (MHD_SCKT_ERR_IS_EINTR_ (err))
      return MHD_ERR_AGAIN_;
    if (MHD_SCKT_ERR_IS_REMOTE_DISCNN_ (err))
      return MHD_ERR_CONNRESET_;
    if (MHD_SCKT_ERR_IS_ (err, MHD_SCKT_EOPNOTSUPP_))
      return MHD_ERR_OPNOTSUPP_;
    if (MHD_SCKT_ERR_IS_ (err, MHD_SCKT_ENOTCONN_))
      return MHD_ERR_NOTCONN_;
    if (MHD_SCKT_ERR_IS_ (err, MHD_SCKT_EINVAL_))
      return MHD_ERR_INVAL_;
    if (MHD_SCKT_ERR_IS_LOW_RESOURCES_ (err))
      return MHD_ERR_NOMEM_;
    if (MHD_SCKT_ERR_IS_ (err, MHD_SCKT_EBADF_))
      return MHD_ERR_BADF_;
    /* Treat any other error as a hard error. */
    return MHD_ERR_NOTCONN_;
  }
#ifdef EPOLL_SUPPORT
  else if (i > (size_t) ret)
    connection->epoll_state &= ~MHD_EPOLL_STATE_READ_READY;
#endif /* EPOLL_SUPPORT */
  return ret;
}


/**
 * Get all of the headers from the request.
 *
 * @param connection connection to get values from
 * @param kind types of values to iterate over, can be a bitmask
 * @param iterator callback to call on each header;
 *        maybe NULL (then just count headers)
 * @param iterator_cls extra argument to @a iterator
 * @return number of entries iterated over
 *         -1 if connection is NULL.
 * @ingroup request
 */
int
MHD_get_connection_values (struct MHD_Connection *connection,
                           enum MHD_ValueKind kind,
                           MHD_KeyValueIterator iterator,
                           void *iterator_cls)
{
  int ret;
  struct MHD_HTTP_Header *pos;

  if (NULL == connection)
    return -1;
  ret = 0;
  for (pos = connection->headers_received; NULL != pos; pos = pos->next)
    if (0 != (pos->kind & kind))
    {
      ret++;
      if ( (NULL != iterator) &&
           (MHD_NO == iterator (iterator_cls,
                                pos->kind,
                                pos->header,
                                pos->value)) )
        return ret;
    }
  return ret;
}


/**
 * Get all of the headers from the request.
 *
 * @param connection connection to get values from
 * @param kind types of values to iterate over, can be a bitmask
 * @param iterator callback to call on each header;
 *        maybe NULL (then just count headers)
 * @param iterator_cls extra argument to @a iterator
 * @return number of entries iterated over,
 *         -1 if connection is NULL.
 * @ingroup request
 */
int
MHD_get_connection_values_n (struct MHD_Connection *connection,
                             enum MHD_ValueKind kind,
                             MHD_KeyValueIteratorN iterator,
                             void *iterator_cls)
{
  int ret;
  struct MHD_HTTP_Header *pos;

  if (NULL == connection)
    return -1;
  ret = 0;

  if (NULL == iterator)
    for (pos = connection->headers_received; NULL != pos; pos = pos->next)
    {
      if (0 != (kind & pos->kind))
        ret++;
    }
  else
    for (pos = connection->headers_received; NULL != pos; pos = pos->next)
      if (0 != (kind & pos->kind))
      {
        ret++;
        if (MHD_NO == iterator (iterator_cls,
                                pos->kind,
                                pos->header,
                                pos->header_size,
                                pos->value,
                                pos->value_size))
          return ret;
      }
  return ret;
}


/**
 * This function can be used to add an arbitrary entry to connection.
 * Internal version of #MHD_set_connection_value_n() without checking
 * of arguments values.
 *
 * @param connection the connection for which a
 *                   value should be set
 * @param kind kind of the value
 * @param key key for the value, must be zero-terminated
 * @param key_size number of bytes in @a key (excluding 0-terminator)
 * @param value the value itself, must be zero-terminated
 * @param value_size number of bytes in @a value (excluding 0-terminator)
 * @return #MHD_NO if the operation could not be
 *         performed due to insufficient memory;
 *         #MHD_YES on success
 * @ingroup request
 */
static enum MHD_Result
MHD_set_connection_value_n_nocheck_ (struct MHD_Connection *connection,
                                     enum MHD_ValueKind kind,
                                     const char *key,
                                     size_t key_size,
                                     const char *value,
                                     size_t value_size)
{
  struct MHD_HTTP_Header *pos;

  pos = MHD_pool_allocate (connection->pool,
                           sizeof (struct MHD_HTTP_Header),
                           true);
  if (NULL == pos)
    return MHD_NO;
  pos->header = (char *) key;
  pos->header_size = key_size;
  pos->value = (char *) value;
  pos->value_size = value_size;
  pos->kind = kind;
  pos->next = NULL;
  /* append 'pos' to the linked list of headers */
  if (NULL == connection->headers_received_tail)
  {
    connection->headers_received = pos;
    connection->headers_received_tail = pos;
  }
  else
  {
    connection->headers_received_tail->next = pos;
    connection->headers_received_tail = pos;
  }
  return MHD_YES;
}


/**
 * This function can be used to add an arbitrary entry to connection.
 * This function could add entry with binary zero, which is allowed
 * for #MHD_GET_ARGUMENT_KIND. For other kind on entries it is
 * recommended to use #MHD_set_connection_value.
 *
 * This function MUST only be called from within the
 * #MHD_AccessHandlerCallback (otherwise, access maybe improperly
 * synchronized).  Furthermore, the client must guarantee that the key
 * and value arguments are 0-terminated strings that are NOT freed
 * until the connection is closed.  (The easiest way to do this is by
 * passing only arguments to permanently allocated strings.).
 *
 * @param connection the connection for which a
 *  value should be set
 * @param kind kind of the value
 * @param key key for the value, must be zero-terminated
 * @param key_size number of bytes in @a key (excluding 0-terminator)
 * @param value the value itself, must be zero-terminated
 * @param value_size number of bytes in @a value (excluding 0-terminator)
 * @return #MHD_NO if the operation could not be
 *         performed due to insufficient memory;
 *         #MHD_YES on success
 * @ingroup request
 */
enum MHD_Result
MHD_set_connection_value_n (struct MHD_Connection *connection,
                            enum MHD_ValueKind kind,
                            const char *key,
                            size_t key_size,
                            const char *value,
                            size_t value_size)
{
  if ( (MHD_GET_ARGUMENT_KIND != kind) &&
       ( ((key ? strlen (key) : 0) != key_size) ||
         ((value ? strlen (value) : 0) != value_size) ) )
    return MHD_NO; /* binary zero is allowed only in GET arguments */

  return MHD_set_connection_value_n_nocheck_ (connection,
                                              kind,
                                              key,
                                              key_size,
                                              value,
                                              value_size);
}


/**
 * This function can be used to add an entry to the HTTP headers of a
 * connection (so that the #MHD_get_connection_values function will
 * return them -- and the `struct MHD_PostProcessor` will also see
 * them).  This maybe required in certain situations (see Mantis
 * #1399) where (broken) HTTP implementations fail to supply values
 * needed by the post processor (or other parts of the application).
 *
 * This function MUST only be called from within the
 * #MHD_AccessHandlerCallback (otherwise, access maybe improperly
 * synchronized).  Furthermore, the client must guarantee that the key
 * and value arguments are 0-terminated strings that are NOT freed
 * until the connection is closed.  (The easiest way to do this is by
 * passing only arguments to permanently allocated strings.).
 *
 * @param connection the connection for which a
 *  value should be set
 * @param kind kind of the value
 * @param key key for the value
 * @param value the value itself
 * @return #MHD_NO if the operation could not be
 *         performed due to insufficient memory;
 *         #MHD_YES on success
 * @ingroup request
 */
enum MHD_Result
MHD_set_connection_value (struct MHD_Connection *connection,
                          enum MHD_ValueKind kind,
                          const char *key,
                          const char *value)
{
  return MHD_set_connection_value_n_nocheck_ (connection,
                                              kind,
                                              key,
                                              NULL != key
                                              ? strlen (key)
                                              : 0,
                                              value,
                                              NULL != value
                                              ? strlen (value)
                                              : 0);
}


/**
 * Get a particular header value.  If multiple
 * values match the kind, return any one of them.
 *
 * @param connection connection to get values from
 * @param kind what kind of value are we looking for
 * @param key the header to look for, NULL to lookup 'trailing' value without a key
 * @return NULL if no such item was found
 * @ingroup request
 */
const char *
MHD_lookup_connection_value (struct MHD_Connection *connection,
                             enum MHD_ValueKind kind,
                             const char *key)
{
  const char *value;

  value = NULL;
  (void) MHD_lookup_connection_value_n (connection,
                                        kind,
                                        key,
                                        (NULL == key) ? 0 : strlen (key),
                                        &value,
                                        NULL);
  return value;
}


/**
 * Get a particular header value.  If multiple
 * values match the kind, return any one of them.
 * @note Since MHD_VERSION 0x00096304
 *
 * @param connection connection to get values from
 * @param kind what kind of value are we looking for
 * @param key the header to look for, NULL to lookup 'trailing' value without a key
 * @param key_size the length of @a key in bytes
 * @param[out] value_ptr the pointer to variable, which will be set to found value,
 *                       will not be updated if key not found,
 *                       could be NULL to just check for presence of @a key
 * @param[out] value_size_ptr the pointer variable, which will set to found value,
 *                            will not be updated if key not found,
 *                            could be NULL
 * @return #MHD_YES if key is found,
 *         #MHD_NO otherwise.
 * @ingroup request
 */
_MHD_EXTERN enum MHD_Result
MHD_lookup_connection_value_n (struct MHD_Connection *connection,
                               enum MHD_ValueKind kind,
                               const char *key,
                               size_t key_size,
                               const char **value_ptr,
                               size_t *value_size_ptr)
{
  struct MHD_HTTP_Header *pos;

  if (NULL == connection)
    return MHD_NO;

  if (NULL == key)
  {
    for (pos = connection->headers_received; NULL != pos; pos = pos->next)
    {
      if ( (0 != (kind & pos->kind)) &&
           (NULL == pos->header) )
        break;
    }
  }
  else
  {
    for (pos = connection->headers_received; NULL != pos; pos = pos->next)
    {
      if ( (0 != (kind & pos->kind)) &&
           (key_size == pos->header_size) &&
           ( (key == pos->header) ||
             (MHD_str_equal_caseless_bin_n_ (key,
                                             pos->header,
                                             key_size) ) ) )
        break;
    }
  }

  if (NULL == pos)
    return MHD_NO;

  if (NULL != value_ptr)
    *value_ptr = pos->value;

  if (NULL != value_size_ptr)
    *value_size_ptr = pos->value_size;

  return MHD_YES;
}


/**
 * Check whether request header contains particular token.
 *
 * Token could be surrounded by spaces and tabs and delimited by comma.
 * Case-insensitive match used for header names and tokens.
 * @param connection the connection to get values from
 * @param header     the header name
 * @param header_len the length of header, not including optional
 *                   terminating null-character
 * @param token      the token to find
 * @param token_len  the length of token, not including optional
 *                   terminating null-character.
 * @return true if token is found in specified header,
 *         false otherwise
 */
static bool
MHD_lookup_header_token_ci (const struct MHD_Connection *connection,
                            const char *header,
                            size_t header_len,
                            const char *token,
                            size_t token_len)
{
  struct MHD_HTTP_Header *pos;

  if ((NULL == connection) || (NULL == header) || (0 == header[0]) || (NULL ==
                                                                       token) ||
      (0 ==
       token
       [
         0]) )
    return false;

  for (pos = connection->headers_received; NULL != pos; pos = pos->next)
  {
    if ((0 != (pos->kind & MHD_HEADER_KIND)) &&
        (header_len == pos->header_size) &&
        ( (header == pos->header) ||
          (MHD_str_equal_caseless_bin_n_ (header,
                                          pos->header,
                                          header_len)) ) &&
        (MHD_str_has_token_caseless_ (pos->value, token, token_len)))
      return true;
  }
  return false;
}


/**
 * Check whether request header contains particular static @a tkn.
 *
 * Token could be surrounded by spaces and tabs and delimited by comma.
 * Case-insensitive match used for header names and tokens.
 * @param c   the connection to get values from
 * @param h   the static string of header name
 * @param tkn the static string of token to find
 * @return true if token is found in specified header,
 *         false otherwise
 */
#define MHD_lookup_header_s_token_ci(c,h,tkn) \
  MHD_lookup_header_token_ci ((c),(h),MHD_STATICSTR_LEN_ (h), \
                              (tkn),MHD_STATICSTR_LEN_ (tkn))


/**
 * Do we (still) need to send a 100 continue
 * message for this connection?
 *
 * @param connection connection to test
 * @return false if we don't need 100 CONTINUE, true if we do
 */
static bool
need_100_continue (struct MHD_Connection *connection)
{
  const char *expect;

  return ( (NULL != connection->version) &&
           (MHD_str_equal_caseless_ (connection->version,
                                     MHD_HTTP_VERSION_1_1)) &&
           (MHD_NO != MHD_lookup_connection_value_n (connection,
                                                     MHD_HEADER_KIND,
                                                     MHD_HTTP_HEADER_EXPECT,
                                                     MHD_STATICSTR_LEN_ (
                                                       MHD_HTTP_HEADER_EXPECT),
                                                     &expect,
                                                     NULL)) &&
           (MHD_str_equal_caseless_ (expect,
                                     "100-continue")) );
}


/**
 * Mark connection as "closed".
 * @remark To be called from any thread.
 *
 * @param connection connection to close
 */
void
MHD_connection_mark_closed_ (struct MHD_Connection *connection)
{
  const struct MHD_Daemon *daemon = connection->daemon;

  connection->state = MHD_CONNECTION_CLOSED;
  connection->event_loop_info = MHD_EVENT_LOOP_INFO_CLEANUP;
  if (0 == (daemon->options & MHD_USE_TURBO))
  {
#ifdef HTTPS_SUPPORT
    /* For TLS connection use shutdown of TLS layer
     * and do not shutdown TCP socket. This give more
     * chances to send TLS closure data to remote side.
     * Closure of TLS layer will be interpreted by
     * remote side as end of transmission. */
    if (0 != (daemon->options & MHD_USE_TLS))
    {
      if (! MHD_tls_connection_shutdown (connection))
        shutdown (connection->socket_fd,
                  SHUT_WR);
    }
    else   /* Combined with next 'shutdown()'. */
#endif /* HTTPS_SUPPORT */
    shutdown (connection->socket_fd,
              SHUT_WR);
  }
}


/**
 * Close the given connection and give the
 * specified termination code to the user.
 * @remark To be called only from thread that
 * process connection's recv(), send() and response.
 *
 * @param connection connection to close
 * @param termination_code termination reason to give
 */
void
MHD_connection_close_ (struct MHD_Connection *connection,
                       enum MHD_RequestTerminationCode termination_code)
{
  struct MHD_Daemon *daemon = connection->daemon;
  struct MHD_Response *resp = connection->response;

#ifdef MHD_USE_THREADS
  mhd_assert ( (0 == (daemon->options & MHD_USE_INTERNAL_POLLING_THREAD)) || \
               MHD_thread_ID_match_current_ (connection->pid) );
#endif /* MHD_USE_THREADS */

  MHD_connection_mark_closed_ (connection);
  if (NULL != resp)
  {
    connection->response = NULL;
    MHD_destroy_response (resp);
  }
  if ( (NULL != daemon->notify_completed) &&
       (connection->client_aware) )
    daemon->notify_completed (daemon->notify_completed_cls,
                              connection,
                              &connection->client_context,
                              termination_code);
  connection->client_aware = false;
}


#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
/**
 * Stop TLS forwarding on upgraded connection and
 * reflect remote disconnect state to socketpair.
 * @remark In thread-per-connection mode this function
 * can be called from any thread, in other modes this
 * function must be called only from thread that process
 * daemon's select()/poll()/etc.
 *
 * @param connection the upgraded connection
 */
void
MHD_connection_finish_forward_ (struct MHD_Connection *connection)
{
  struct MHD_Daemon *daemon = connection->daemon;
  struct MHD_UpgradeResponseHandle *urh = connection->urh;

#ifdef MHD_USE_THREADS
  mhd_assert ( (0 == (daemon->options & MHD_USE_INTERNAL_POLLING_THREAD)) || \
               (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) || \
               MHD_thread_ID_match_current_ (daemon->pid) );
#endif /* MHD_USE_THREADS */

  if (0 == (daemon->options & MHD_USE_TLS))
    return; /* Nothing to do with non-TLS connection. */

  if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    DLL_remove (daemon->urh_head,
                daemon->urh_tail,
                urh);
#if EPOLL_SUPPORT
  if ( (0 != (daemon->options & MHD_USE_EPOLL)) &&
       (0 != epoll_ctl (daemon->epoll_upgrade_fd,
                        EPOLL_CTL_DEL,
                        connection->socket_fd,
                        NULL)) )
  {
    MHD_PANIC (_ ("Failed to remove FD from epoll set.\n"));
  }
  if (urh->in_eready_list)
  {
    EDLL_remove (daemon->eready_urh_head,
                 daemon->eready_urh_tail,
                 urh);
    urh->in_eready_list = false;
  }
#endif /* EPOLL_SUPPORT */
  if (MHD_INVALID_SOCKET != urh->mhd.socket)
  {
#if EPOLL_SUPPORT
    if ( (0 != (daemon->options & MHD_USE_EPOLL)) &&
         (0 != epoll_ctl (daemon->epoll_upgrade_fd,
                          EPOLL_CTL_DEL,
                          urh->mhd.socket,
                          NULL)) )
    {
      MHD_PANIC (_ ("Failed to remove FD from epoll set.\n"));
    }
#endif /* EPOLL_SUPPORT */
    /* Reflect remote disconnect to application by breaking
     * socketpair connection. */
    shutdown (urh->mhd.socket, SHUT_RDWR);
  }
  /* Socketpair sockets will remain open as they will be
   * used with MHD_UPGRADE_ACTION_CLOSE. They will be
   * closed by cleanup_upgraded_connection() during
   * connection's final cleanup.
   */
}


#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT*/


/**
 * A serious error occurred, close the
 * connection (and notify the application).
 *
 * @param connection connection to close with error
 * @param emsg error message (can be NULL)
 */
static void
connection_close_error (struct MHD_Connection *connection,
                        const char *emsg)
{
#ifdef HAVE_MESSAGES
  if (NULL != emsg)
    MHD_DLOG (connection->daemon,
              "%s\n",
              emsg);
#else  /* ! HAVE_MESSAGES */
  (void) emsg; /* Mute compiler warning. */
#endif /* ! HAVE_MESSAGES */
  MHD_connection_close_ (connection,
                         MHD_REQUEST_TERMINATED_WITH_ERROR);
}


/**
 * Macro to only include error message in call to
 * #connection_close_error() if we have HAVE_MESSAGES.
 */
#ifdef HAVE_MESSAGES
#define CONNECTION_CLOSE_ERROR(c, emsg) connection_close_error (c, emsg)
#else
#define CONNECTION_CLOSE_ERROR(c, emsg) connection_close_error (c, NULL)
#endif


/**
 * Prepare the response buffer of this connection for
 * sending.  Assumes that the response mutex is
 * already held.  If the transmission is complete,
 * this function may close the socket (and return
 * #MHD_NO).
 *
 * @param connection the connection
 * @return #MHD_NO if readying the response failed (the
 *  lock on the response will have been released already
 *  in this case).
 */
static enum MHD_Result
try_ready_normal_body (struct MHD_Connection *connection)
{
  ssize_t ret;
  struct MHD_Response *response;

  response = connection->response;
  if ( (0 == response->total_size) ||
       (connection->response_write_position == response->total_size) )
    return MHD_YES; /* 0-byte response is always ready */
  if (NULL != response->data_iov)
  {
    size_t copy_size;

    if (NULL != connection->resp_iov.iov)
      return MHD_YES;
    copy_size = response->data_iovcnt * sizeof(MHD_iovec_);
    connection->resp_iov.iov = MHD_pool_allocate (connection->pool,
                                                  copy_size,
                                                  true);
    if (NULL == connection->resp_iov.iov)
    {
      MHD_mutex_unlock_chk_ (&response->mutex);
      /* not enough memory */
      CONNECTION_CLOSE_ERROR (connection,
                              _ ("Closing connection (out of memory)."));
      return MHD_NO;
    }
    memcpy (connection->resp_iov.iov,
            response->data_iov,
            copy_size);
    connection->resp_iov.cnt = response->data_iovcnt;
    connection->resp_iov.sent = 0;
    return MHD_YES;
  }
  if (NULL == response->crc)
    return MHD_YES;
  if ( (response->data_start <=
        connection->response_write_position) &&
       (response->data_size + response->data_start >
        connection->response_write_position) )
    return MHD_YES; /* response already ready */
#if defined(_MHD_HAVE_SENDFILE)
  if (MHD_resp_sender_sendfile == connection->resp_sender)
  {
    /* will use sendfile, no need to bother response crc */
    return MHD_YES;
  }
#endif /* _MHD_HAVE_SENDFILE */

  ret = response->crc (response->crc_cls,
                       connection->response_write_position,
                       response->data,
                       (size_t) MHD_MIN ((uint64_t) response->data_buffer_size,
                                         response->total_size
                                         - connection->response_write_position));
  if ( (((ssize_t) MHD_CONTENT_READER_END_OF_STREAM) == ret) ||
       (((ssize_t) MHD_CONTENT_READER_END_WITH_ERROR) == ret) )
  {
    /* either error or http 1.0 transfer, close socket! */
    response->total_size = connection->response_write_position;
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
    MHD_mutex_unlock_chk_ (&response->mutex);
#endif
    if ( ((ssize_t) MHD_CONTENT_READER_END_OF_STREAM) == ret)
      MHD_connection_close_ (connection,
                             MHD_REQUEST_TERMINATED_COMPLETED_OK);
    else
      CONNECTION_CLOSE_ERROR (connection,
                              _ (
                                "Closing connection (application reported error generating data)."));
    return MHD_NO;
  }
  response->data_start = connection->response_write_position;
  response->data_size = ret;
  if (0 == ret)
  {
    connection->state = MHD_CONNECTION_NORMAL_BODY_UNREADY;
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
    MHD_mutex_unlock_chk_ (&response->mutex);
#endif
    return MHD_NO;
  }
  return MHD_YES;
}


/**
 * Prepare the response buffer of this connection for sending.
 * Assumes that the response mutex is already held.  If the
 * transmission is complete, this function may close the socket (and
 * return #MHD_NO).
 *
 * @param connection the connection
 * @return #MHD_NO if readying the response failed
 */
static enum MHD_Result
try_ready_chunked_body (struct MHD_Connection *connection)
{
  ssize_t ret;
  struct MHD_Response *response;
  char cbuf[10];                /* 10: max strlen of "%x\r\n" */
  int cblen;

  response = connection->response;
  if (NULL == response->crc)
    return MHD_YES;
  if (0 == connection->write_buffer_size)
  {
    size_t size;

    size = MHD_pool_get_free (connection->pool);
    if (size < 128)
    {
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
      MHD_mutex_unlock_chk_ (&response->mutex);
#endif
      /* not enough memory */
      CONNECTION_CLOSE_ERROR (connection,
                              _ ("Closing connection (out of memory)."));
      return MHD_NO;
    }
    if ( (2 * (0xFFFFFF + sizeof(cbuf) + 2)) < size)
      size = 2 * (0xFFFFFF + sizeof(cbuf) + 2);
    connection->write_buffer = MHD_pool_allocate (connection->pool,
                                                  size,
                                                  false);
    mhd_assert (NULL != connection->write_buffer);
    connection->write_buffer_size = size;
  }

  if (0 == response->total_size)
    ret = 0; /* response must be empty, don't bother calling crc */
  else if ( (response->data_start <=
             connection->response_write_position) &&
            (response->data_start + response->data_size >
             connection->response_write_position) )
  {
    /* difference between response_write_position and data_start is less
       than data_size which is size_t type, no need to check for overflow */
    const size_t data_write_offset
      = (size_t) (connection->response_write_position - response->data_start);
    /* buffer already ready, use what is there for the chunk */
    ret = response->data_size - data_write_offset;
    if ( ((size_t) ret) > connection->write_buffer_size - sizeof (cbuf) - 2)
      ret = connection->write_buffer_size - sizeof (cbuf) - 2;
    memcpy (&connection->write_buffer[sizeof (cbuf)],
            &response->data[data_write_offset],
            ret);
  }
  else
  {
    /* buffer not in range, try to fill it */
    ret = response->crc (response->crc_cls,
                         connection->response_write_position,
                         &connection->write_buffer[sizeof (cbuf)],
                         connection->write_buffer_size - sizeof (cbuf) - 2);
  }
  if ( ((ssize_t) MHD_CONTENT_READER_END_WITH_ERROR) == ret)
  {
    /* error, close socket! */
    response->total_size = connection->response_write_position;
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
    MHD_mutex_unlock_chk_ (&response->mutex);
#endif
    CONNECTION_CLOSE_ERROR (connection,
                            _ (
                              "Closing connection (application error generating response)."));
    return MHD_NO;
  }
  if ( (((ssize_t) MHD_CONTENT_READER_END_OF_STREAM) == ret) ||
       (0 == response->total_size) )
  {
    /* end of message, signal other side! */
    memcpy (connection->write_buffer,
            "0\r\n",
            3);
    connection->write_buffer_append_offset = 3;
    connection->write_buffer_send_offset = 0;
    response->total_size = connection->response_write_position;
    return MHD_YES;
  }
  if (0 == ret)
  {
    connection->state = MHD_CONNECTION_CHUNKED_BODY_UNREADY;
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
    MHD_mutex_unlock_chk_ (&response->mutex);
#endif
    return MHD_NO;
  }
  if (ret > 0xFFFFFF)
    ret = 0xFFFFFF;
  cblen = MHD_snprintf_ (cbuf,
                         sizeof (cbuf),
                         "%X\r\n",
                         (unsigned int) ret);
  mhd_assert (cblen > 0);
  mhd_assert ((size_t) cblen < sizeof(cbuf));
  memcpy (&connection->write_buffer[sizeof (cbuf) - cblen],
          cbuf,
          cblen);
  memcpy (&connection->write_buffer[sizeof (cbuf) + ret],
          "\r\n",
          2);
  connection->response_write_position += ret;
  connection->write_buffer_send_offset = sizeof (cbuf) - cblen;
  connection->write_buffer_append_offset = sizeof (cbuf) + ret + 2;
  return MHD_YES;
}


/**
 * Are we allowed to keep the given connection alive?  We can use the
 * TCP stream for a second request if the connection is HTTP 1.1 and
 * the "Connection" header either does not exist or is not set to
 * "close", or if the connection is HTTP 1.0 and the "Connection"
 * header is explicitly set to "keep-alive".  If no HTTP version is
 * specified (or if it is not 1.0 or 1.1), we definitively close the
 * connection.  If the "Connection" header is not exactly "close" or
 * "keep-alive", we proceed to use the default for the respective HTTP
 * version (which is conservative for HTTP 1.0, but might be a bit
 * optimistic for HTTP 1.1).
 *
 * @param connection the connection to check for keepalive
 * @return #MHD_YES if (based on the request), a keepalive is
 *        legal
 */
static enum MHD_Result
keepalive_possible (struct MHD_Connection *connection)
{
  if (MHD_CONN_MUST_CLOSE == connection->keepalive)
    return MHD_NO;
  if (NULL == connection->version)
    return MHD_NO;
  if ( (NULL != connection->response) &&
       (0 != (connection->response->flags & MHD_RF_HTTP_VERSION_1_0_ONLY) ) )
    return MHD_NO;

  if (MHD_str_equal_caseless_ (connection->version,
                               MHD_HTTP_VERSION_1_1) &&
      ( (NULL == connection->response) ||
        (0 == (connection->response->flags
               & MHD_RF_HTTP_VERSION_1_0_RESPONSE) ) ) )
  {
    if (MHD_lookup_header_s_token_ci (connection,
                                      MHD_HTTP_HEADER_CONNECTION,
                                      "upgrade"))
      return MHD_NO;

    if (MHD_lookup_header_s_token_ci (connection,
                                      MHD_HTTP_HEADER_CONNECTION,
                                      "close"))
      return MHD_NO;

    return MHD_YES;
  }
  if (MHD_str_equal_caseless_ (connection->version,
                               MHD_HTTP_VERSION_1_0))
  {
    if (MHD_lookup_header_s_token_ci (connection,
                                      MHD_HTTP_HEADER_CONNECTION,
                                      "Keep-Alive"))
      return MHD_YES;

    return MHD_NO;
  }
  return MHD_NO;
}


/**
 * Produce HTTP time stamp.
 *
 * @param date where to write the header, with
 *        at least 128 bytes available space.
 * @param date_len number of bytes in @a date
 */
static void
get_date_string (char *date,
                 size_t date_len)
{
  static const char *const days[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };
  static const char *const mons[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  struct tm now;
  time_t t;
#if ! defined(HAVE_C11_GMTIME_S) && ! defined(HAVE_W32_GMTIME_S) && \
  ! defined(HAVE_GMTIME_R)
  struct tm*pNow;
#endif

  date[0] = 0;
  time (&t);
#if defined(HAVE_C11_GMTIME_S)
  if (NULL == gmtime_s (&t,
                        &now))
    return;
#elif defined(HAVE_W32_GMTIME_S)
  if (0 != gmtime_s (&now,
                     &t))
    return;
#elif defined(HAVE_GMTIME_R)
  if (NULL == gmtime_r (&t,
                        &now))
    return;
#else
  pNow = gmtime (&t);
  if (NULL == pNow)
    return;
  now = *pNow;
#endif
  MHD_snprintf_ (date,
                 date_len,
                 "Date: %3s, %02u %3s %04u %02u:%02u:%02u GMT\r\n",
                 days[now.tm_wday % 7],
                 (unsigned int) now.tm_mday,
                 mons[now.tm_mon % 12],
                 (unsigned int) (1900 + now.tm_year),
                 (unsigned int) now.tm_hour,
                 (unsigned int) now.tm_min,
                 (unsigned int) now.tm_sec);
}


/**
 * Try growing the read buffer.  We initially claim half the available
 * buffer space for the read buffer (the other half being left for
 * management data structures; the write buffer can in the end take
 * virtually everything as the read buffer can be reduced to the
 * minimum necessary at that point.
 *
 * @param connection the connection
 * @param required set to 'true' if grow is required, i.e. connection
 *                 will fail if no additional space is granted
 * @return 'true' on success, 'false' on failure
 */
static bool
try_grow_read_buffer (struct MHD_Connection *connection,
                      bool required)
{
  size_t new_size;
  size_t avail_size;
  void *rb;

  avail_size = MHD_pool_get_free (connection->pool);
  if (0 == avail_size)
    return false;               /* No more space available */
  if (0 == connection->read_buffer_size)
    new_size = avail_size / 2;  /* Use half of available buffer for reading */
  else
  {
    size_t grow_size;

    grow_size = avail_size / 8;
    if (MHD_BUF_INC_SIZE > grow_size)
    {                  /* Shortage of space */
      if (! required)
        return false;  /* Grow is not mandatory, leave some space in pool */
      else
      {
        /* Shortage of space, but grow is mandatory */
        static const size_t small_inc = MHD_BUF_INC_SIZE / 8;
        if (small_inc < avail_size)
          grow_size = small_inc;
        else
          grow_size = avail_size;
      }
    }
    new_size = connection->read_buffer_size + grow_size;
  }
  /* we can actually grow the buffer, do it! */
  rb = MHD_pool_reallocate (connection->pool,
                            connection->read_buffer,
                            connection->read_buffer_size,
                            new_size);
  if (NULL == rb)
  {
    /* This should NOT be possible: we just computed 'new_size' so that
       it should fit. If it happens, somehow our read buffer is not in
       the right position in the pool, say because someone called
       MHD_pool_allocate() without 'from_end' set to 'true'? Anyway,
       should be investigated! (Ideally provide all data from
       *pool and connection->read_buffer and new_size for debugging). */
    mhd_assert (0);
    return false;
  }
  connection->read_buffer = rb;
  mhd_assert (NULL != connection->read_buffer);
  connection->read_buffer_size = new_size;
  return true;
}


/**
 * Allocate the connection's write buffer and fill it with all of the
 * headers (or footers, if we have already sent the body) from the
 * HTTPd's response.  If headers are missing in the response supplied
 * by the application, additional headers may be added here.
 *
 * @param connection the connection
 * @return #MHD_YES on success, #MHD_NO on failure (out of memory)
 */
static enum MHD_Result
build_header_response (struct MHD_Connection *connection)
{
  struct MHD_Response *response = connection->response;
  size_t size;
  size_t off;
  struct MHD_HTTP_Header *pos;
  char code[256];
  char date[128];
  size_t datelen;
  char content_length_buf[128];
  size_t content_length_len;
  char *data;
  enum MHD_ValueKind kind;
  const char *reason_phrase;
  uint32_t rc;
  bool client_requested_close;
  bool response_has_close;
  bool response_has_keepalive;
  const char *have_encoding;
  bool must_add_close;
  bool must_add_chunked_encoding;
  bool must_add_keep_alive;
  bool must_add_content_length;
  bool may_add_content_length;

  mhd_assert (NULL != connection->version);
  if (0 == connection->version[0])
  {
    data = MHD_pool_allocate (connection->pool,
                              0,
                              true);
    connection->write_buffer = data;
    connection->write_buffer_append_offset = 0;
    connection->write_buffer_send_offset = 0;
    connection->write_buffer_size = 0;
    return MHD_YES;
  }
  rc = connection->responseCode & (~MHD_ICY_FLAG);
  if (MHD_CONNECTION_FOOTERS_RECEIVED == connection->state)
  {
    reason_phrase = MHD_get_reason_phrase_for (rc);
    off = MHD_snprintf_ (code,
                         sizeof (code),
                         "%s %u %s\r\n",
                         (0 != (connection->responseCode & MHD_ICY_FLAG))
                         ? "ICY"
                         : ( (MHD_str_equal_caseless_ (MHD_HTTP_VERSION_1_0,
                                                       connection->version) ||
                              (0 != (connection->response->flags
                                     & MHD_RF_HTTP_VERSION_1_0_RESPONSE)) )
                             ? MHD_HTTP_VERSION_1_0
                             : MHD_HTTP_VERSION_1_1),
                         rc,
                         reason_phrase);
    /* estimate size */
    size = off + 2;             /* +2 for extra "\r\n" at the end */
    kind = MHD_HEADER_KIND;
    if ( (0 == (connection->daemon->options
                & MHD_USE_SUPPRESS_DATE_NO_CLOCK)) &&
         (NULL == MHD_get_response_header (response,
                                           MHD_HTTP_HEADER_DATE)) )
      get_date_string (date,
                       sizeof (date));
    else
      date[0] = '\0';
    datelen = strlen (date);
    size += datelen;
  }
  else
  {
    /* 2 bytes for final CRLF of a Chunked-Body */
    size = 2;
    kind = MHD_FOOTER_KIND;
    off = 0;
    datelen = 0;
  }

  /* calculate extra headers we need to add, such as 'Connection: close',
     first see what was explicitly requested by the application */
  must_add_close = false;
  must_add_chunked_encoding = false;
  must_add_keep_alive = false;
  must_add_content_length = false;
  content_length_len = 0; /* Mute compiler warning only */
  response_has_close = false;
  switch (connection->state)
  {
  case MHD_CONNECTION_FOOTERS_RECEIVED:
    response_has_close = MHD_check_response_header_s_token_ci (response,
                                                               MHD_HTTP_HEADER_CONNECTION,
                                                               "close");
    response_has_keepalive = MHD_check_response_header_s_token_ci (response,
                                                                   MHD_HTTP_HEADER_CONNECTION,
                                                                   "Keep-Alive");
    client_requested_close = MHD_lookup_header_s_token_ci (connection,
                                                           MHD_HTTP_HEADER_CONNECTION,
                                                           "close");

    if (0 != (response->flags & MHD_RF_HTTP_VERSION_1_0_ONLY))
      connection->keepalive = MHD_CONN_MUST_CLOSE;
#ifdef UPGRADE_SUPPORT
    else if (NULL != response->upgrade_handler)
      /* If this connection will not be "upgraded", it must be closed. */
      connection->keepalive = MHD_CONN_MUST_CLOSE;
#endif /* UPGRADE_SUPPORT */

    /* now analyze chunked encoding situation */
    connection->have_chunked_upload = false;
    have_encoding = MHD_get_response_header (response,
                                             MHD_HTTP_HEADER_TRANSFER_ENCODING);
    if (NULL == have_encoding)
      may_add_content_length = true;
    else
      may_add_content_length = false;   /* RFC 7230, Section 3.3.2 forbids header */
    if ( (MHD_SIZE_UNKNOWN == response->total_size) &&
#ifdef UPGRADE_SUPPORT
         (NULL == response->upgrade_handler) &&
#endif /* UPGRADE_SUPPORT */
         (! response_has_close) &&
         (! client_requested_close) )
    {
      /* size is unknown, and close was not explicitly requested;
         need to either to HTTP 1.1 chunked encoding or
         close the connection */
      /* 'close' header doesn't exist yet, see if we need to add one;
         if the client asked for a close, no need to start chunk'ing */
      if ( (MHD_NO != keepalive_possible (connection)) &&
           (MHD_str_equal_caseless_ (MHD_HTTP_VERSION_1_1,
                                     connection->version) ) )
      {
        if (NULL == have_encoding)
        {
          must_add_chunked_encoding = true;
          connection->have_chunked_upload = true;
        }
        else
        {
          if (MHD_str_equal_caseless_ (have_encoding,
                                       "identity"))
          {
            /* application forced identity encoding, can't do 'chunked' */
            must_add_close = true;
          }
          else
          {
            connection->have_chunked_upload = true;
          }
        }
      }
      else
      {
        /* Keep alive or chunking not possible
           => set close header (we know response_has_close
           is false here) */
        must_add_close = true;
      }
    }

    /* check for other reasons to add 'close' header */
    if ( ( (client_requested_close) ||
           (connection->read_closed) ||
           (MHD_CONN_MUST_CLOSE == connection->keepalive)) &&
         (! response_has_close) &&
#ifdef UPGRADE_SUPPORT
         (NULL == response->upgrade_handler) &&
#endif /* UPGRADE_SUPPORT */
         (0 == (response->flags & MHD_RF_HTTP_VERSION_1_0_ONLY) ) )
      must_add_close = true;

    /* check if we must add 'close' header because we cannot add content-length
       because it is forbidden AND we don't have a 'chunked' encoding */
    if ( (! may_add_content_length) &&
         (! connection->have_chunked_upload) &&
         (! response_has_close) )
      must_add_close = true;
    /* #MHD_HTTP_NO_CONTENT, #MHD_HTTP_NOT_MODIFIED and 1xx-status
       codes SHOULD NOT have a Content-Length according to spec;
       also chunked encoding / unknown length or CONNECT... */
    if ( (MHD_SIZE_UNKNOWN != response->total_size) &&
         (MHD_HTTP_NO_CONTENT != rc) &&
         (MHD_HTTP_NOT_MODIFIED != rc) &&
         (MHD_HTTP_OK <= rc) &&
         (NULL ==   /* this COULD fail if the check in
                       MHD_add_response_header() was bypassed
                       via #MHD_RF_INSANITY_HEADER_CONTENT_LENGTH */
          MHD_get_response_header (response,
                                   MHD_HTTP_HEADER_CONTENT_LENGTH)) &&
         (may_add_content_length) &&
         ( (NULL == connection->method) ||
           (! MHD_str_equal_caseless_ (connection->method,
                                       MHD_HTTP_METHOD_CONNECT)) ) )
    {
      /*
        Here we add a content-length if one is missing; however,
        for 'connect' methods, the responses MUST NOT include a
        content-length header *if* the response code is 2xx (in
        which case we expect there to be no body).  Still,
        as we don't know the response code here in some cases, we
        simply only force adding a content-length header if this
        is not a 'connect' or if the response is not empty
        (which is kind of more sane, because if some crazy
        application did return content with a 2xx status code,
        then having a content-length might again be a good idea).

        Note that the change from 'SHOULD NOT' to 'MUST NOT' is
        a recent development of the HTTP 1.1 specification.
      */
      content_length_len
        = MHD_snprintf_ (content_length_buf,
                         sizeof (content_length_buf),
                         MHD_HTTP_HEADER_CONTENT_LENGTH ": "
                         MHD_UNSIGNED_LONG_LONG_PRINTF "\r\n",
                         (MHD_UNSIGNED_LONG_LONG) response->total_size);
      must_add_content_length = true;
    }

    /* check for adding keep alive */
    if ( (! response_has_keepalive) &&
         (! response_has_close) &&
         (! must_add_close) &&
         (MHD_CONN_MUST_CLOSE != connection->keepalive) &&
#ifdef UPGRADE_SUPPORT
         (NULL == response->upgrade_handler) &&
#endif /* UPGRADE_SUPPORT */
         (MHD_NO != keepalive_possible (connection)) )
      must_add_keep_alive = true;
    break;
  case MHD_CONNECTION_BODY_SENT:
    response_has_keepalive = false;
    break;
  default:
    mhd_assert (0);
    return MHD_NO;
  }

  if (MHD_CONN_MUST_CLOSE != connection->keepalive)
  {
    if ( (must_add_close) || (response_has_close) )
      connection->keepalive = MHD_CONN_MUST_CLOSE;
    else if ( (must_add_keep_alive) || (response_has_keepalive) )
      connection->keepalive = MHD_CONN_USE_KEEPALIVE;
  }

  if (must_add_close)
    size += MHD_STATICSTR_LEN_ ("Connection: close\r\n");
  if (must_add_keep_alive)
    size += MHD_STATICSTR_LEN_ ("Connection: Keep-Alive\r\n");
  if (must_add_chunked_encoding)
    size += MHD_STATICSTR_LEN_ ("Transfer-Encoding: chunked\r\n");
  if (must_add_content_length)
    size += content_length_len;
  mhd_assert (! (must_add_close && must_add_keep_alive) );
  mhd_assert (! (must_add_chunked_encoding && must_add_content_length) );

  for (pos = response->first_header; NULL != pos; pos = pos->next)
  {
    /* TODO: add proper support for excluding "Keep-Alive" token. */
    if ( (pos->kind == kind) &&
         (! ( (must_add_close) &&
              (response_has_keepalive) &&
              (pos->header_size == MHD_STATICSTR_LEN_ (
                 MHD_HTTP_HEADER_CONNECTION)) &&
              (MHD_str_equal_caseless_bin_n_ (pos->header,
                                              MHD_HTTP_HEADER_CONNECTION,
                                              MHD_STATICSTR_LEN_ (
                                                MHD_HTTP_HEADER_CONNECTION))) &&
              (MHD_str_equal_caseless_ (pos->value,
                                        "Keep-Alive")) ) ) )
      size += pos->header_size + pos->value_size + 4;   /* colon, space, linefeeds */
  }
  /* produce data */
  data = MHD_pool_allocate (connection->pool,
                            size + 1,
                            false);
  if (NULL == data)
  {
#ifdef HAVE_MESSAGES
    MHD_DLOG (connection->daemon,
              "Not enough memory for write!\n");
#endif
    return MHD_NO;
  }
  if (MHD_CONNECTION_FOOTERS_RECEIVED == connection->state)
  {
    memcpy (data,
            code,
            off);
  }
  if (must_add_close)
  {
    /* we must add the 'Connection: close' header */
    memcpy (&data[off],
            "Connection: close\r\n",
            MHD_STATICSTR_LEN_ ("Connection: close\r\n"));
    off += MHD_STATICSTR_LEN_ ("Connection: close\r\n");
  }
  if (must_add_keep_alive)
  {
    /* we must add the 'Connection: Keep-Alive' header */
    memcpy (&data[off],
            "Connection: Keep-Alive\r\n",
            MHD_STATICSTR_LEN_ ("Connection: Keep-Alive\r\n"));
    off += MHD_STATICSTR_LEN_ ("Connection: Keep-Alive\r\n");
  }
  if (must_add_chunked_encoding)
  {
    /* we must add the 'Transfer-Encoding: chunked' header */
    memcpy (&data[off],
            "Transfer-Encoding: chunked\r\n",
            MHD_STATICSTR_LEN_ ("Transfer-Encoding: chunked\r\n"));
    off += MHD_STATICSTR_LEN_ ("Transfer-Encoding: chunked\r\n");
  }
  if (must_add_content_length)
  {
    /* we must add the 'Content-Length' header */
    memcpy (&data[off],
            content_length_buf,
            content_length_len);
    off += content_length_len;
  }
  for (pos = response->first_header; NULL != pos; pos = pos->next)
  {
    /* TODO: add proper support for excluding "Keep-Alive" token. */
    if ( (pos->kind == kind) &&
         (! ( (must_add_close) &&
              (response_has_keepalive) &&
              (pos->header_size == MHD_STATICSTR_LEN_ (
                 MHD_HTTP_HEADER_CONNECTION)) &&
              (MHD_str_equal_caseless_bin_n_ (pos->header,
                                              MHD_HTTP_HEADER_CONNECTION,
                                              MHD_STATICSTR_LEN_ (
                                                MHD_HTTP_HEADER_CONNECTION))) &&
              (MHD_str_equal_caseless_ (pos->value,
                                        "Keep-Alive")) ) ) )
      off += MHD_snprintf_ (&data[off],
                            size - off,
                            "%s: %s\r\n",
                            pos->header,
                            pos->value);
  }
  if (MHD_CONNECTION_FOOTERS_RECEIVED == connection->state)
  {
    memcpy (&data[off],
            date,
            datelen);
    off += datelen;
  }
  memcpy (&data[off],
          "\r\n",
          2);
  off += 2;

  if (off != size)
    mhd_panic (mhd_panic_cls,
               __FILE__,
               __LINE__,
               NULL);
  connection->write_buffer = data;
  connection->write_buffer_append_offset = size;
  connection->write_buffer_send_offset = 0;
  connection->write_buffer_size = size + 1;
  return MHD_YES;
}


/**
 * We encountered an error processing the request.
 * Handle it properly by stopping to read data
 * and sending the indicated response code and message.
 *
 * @param connection the connection
 * @param status_code the response code to send (400, 413 or 414)
 * @param message the error message to send
 */
static void
transmit_error_response (struct MHD_Connection *connection,
                         unsigned int status_code,
                         const char *message)
{
  struct MHD_Response *response;
  enum MHD_Result iret;

  if (NULL == connection->version)
  {
    /* we were unable to process the full header line, so we don't
 really know what version the client speaks; assume 1.0 */
    connection->version = MHD_HTTP_VERSION_1_0;
  }
  connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
  connection->read_closed = true;
  if (0 != connection->read_buffer_size)
  {
    /* Read buffer is not needed anymore, discard it
     * to free some space for error response. */
    connection->read_buffer = MHD_pool_reallocate (connection->pool,
                                                   connection->read_buffer,
                                                   connection->read_buffer_size,
                                                   0);
    connection->read_buffer_size = 0;
  }
#ifdef HAVE_MESSAGES
  MHD_DLOG (connection->daemon,
            _ (
              "Error processing request (HTTP response code is %u (`%s')). Closing connection.\n"),
            status_code,
            message);
#endif
  if (NULL != connection->response)
  {
    MHD_destroy_response (connection->response);
    connection->response = NULL;
  }
  response = MHD_create_response_from_buffer (strlen (message),
                                              (void *) message,
                                              MHD_RESPMEM_PERSISTENT);
  if (NULL == response)
  {
    /* can't even send a reply, at least close the connection */
    connection->state = MHD_CONNECTION_CLOSED;
    return;
  }
  iret = MHD_queue_response (connection,
                             status_code,
                             response);
  MHD_destroy_response (response);
  if (MHD_NO == iret)
  {
    /* can't even send a reply, at least close the connection */
    CONNECTION_CLOSE_ERROR (connection,
                            _ (
                              "Closing connection (failed to queue response)."));
    return;
  }
  mhd_assert (NULL != connection->response);
  /* Do not reuse this connection. */
  connection->keepalive = MHD_CONN_MUST_CLOSE;
  if (MHD_NO == build_header_response (connection))
  {
    /* oops - close! */
    CONNECTION_CLOSE_ERROR (connection,
                            _ (
                              "Closing connection (failed to create response header)."));
  }
  else
  {
    connection->state = MHD_CONNECTION_HEADERS_SENDING;
  }
}


/**
 * Update the 'event_loop_info' field of this connection based on the state
 * that the connection is now in.  May also close the connection or
 * perform other updates to the connection if needed to prepare for
 * the next round of the event loop.
 *
 * @param connection connection to get poll set for
 */
static void
MHD_connection_update_event_loop_info (struct MHD_Connection *connection)
{
  /* Do not update states of suspended connection */
  if (connection->suspended)
    return; /* States will be updated after resume. */
#ifdef HTTPS_SUPPORT
  if (MHD_TLS_CONN_NO_TLS != connection->tls_state)
  {   /* HTTPS connection. */
    switch (connection->tls_state)
    {
    case MHD_TLS_CONN_INIT:
      connection->event_loop_info = MHD_EVENT_LOOP_INFO_READ;
      return;
    case MHD_TLS_CONN_HANDSHAKING:
      if (0 == gnutls_record_get_direction (connection->tls_session))
        connection->event_loop_info = MHD_EVENT_LOOP_INFO_READ;
      else
        connection->event_loop_info = MHD_EVENT_LOOP_INFO_WRITE;
      return;
    default:
      break;
    }
  }
#endif /* HTTPS_SUPPORT */
  while (1)
  {
#if DEBUG_STATES
    MHD_DLOG (connection->daemon,
              _ ("In function %s handling connection at state: %s\n"),
              __FUNCTION__,
              MHD_state_to_string (connection->state));
#endif
    switch (connection->state)
    {
    case MHD_CONNECTION_INIT:
    case MHD_CONNECTION_URL_RECEIVED:
    case MHD_CONNECTION_HEADER_PART_RECEIVED:
      /* while reading headers, we always grow the
         read buffer if needed, no size-check required */
      if ( (connection->read_buffer_offset == connection->read_buffer_size) &&
           (! try_grow_read_buffer (connection, true)) )
      {
        transmit_error_response (connection,
                                 (connection->url != NULL)
                                 ? MHD_HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE
                                 : MHD_HTTP_URI_TOO_LONG,
                                 REQUEST_TOO_BIG);
        continue;
      }
      if (! connection->read_closed)
        connection->event_loop_info = MHD_EVENT_LOOP_INFO_READ;
      else
        connection->event_loop_info = MHD_EVENT_LOOP_INFO_BLOCK;
      break;
    case MHD_CONNECTION_HEADERS_RECEIVED:
      mhd_assert (0);
      break;
    case MHD_CONNECTION_HEADERS_PROCESSED:
      mhd_assert (0);
      break;
    case MHD_CONNECTION_CONTINUE_SENDING:
      connection->event_loop_info = MHD_EVENT_LOOP_INFO_WRITE;
      break;
    case MHD_CONNECTION_CONTINUE_SENT:
      if (connection->read_buffer_offset == connection->read_buffer_size)
      {
        const bool internal_poll = (0 != (connection->daemon->options
                                          & MHD_USE_INTERNAL_POLLING_THREAD));
        if ( (! try_grow_read_buffer (connection, true)) &&
             internal_poll)
        {
          /* failed to grow the read buffer, and the
             client which is supposed to handle the
             received data in a *blocking* fashion
             (in this mode) did not handle the data as
             it was supposed to!
             => we would either have to do busy-waiting
             (on the client, which would likely fail),
             or if we do nothing, we would just timeout
             on the connection (if a timeout is even
             set!).
             Solution: we kill the connection with an error */
          transmit_error_response (connection,
                                   MHD_HTTP_INTERNAL_SERVER_ERROR,
                                   INTERNAL_ERROR);
          continue;
        }
      }
      if ( (connection->read_buffer_offset < connection->read_buffer_size) &&
           (! connection->read_closed) )
        connection->event_loop_info = MHD_EVENT_LOOP_INFO_READ;
      else
        connection->event_loop_info = MHD_EVENT_LOOP_INFO_BLOCK;
      break;
    case MHD_CONNECTION_BODY_RECEIVED:
    case MHD_CONNECTION_FOOTER_PART_RECEIVED:
      /* while reading footers, we always grow the
         read buffer if needed, no size-check required */
      if (connection->read_closed)
      {
        CONNECTION_CLOSE_ERROR (connection,
                                NULL);
        continue;
      }
      connection->event_loop_info = MHD_EVENT_LOOP_INFO_READ;
      /* transition to FOOTERS_RECEIVED
         happens in read handler */
      break;
    case MHD_CONNECTION_FOOTERS_RECEIVED:
      connection->event_loop_info = MHD_EVENT_LOOP_INFO_BLOCK;
      break;
    case MHD_CONNECTION_HEADERS_SENDING:
      /* headers in buffer, keep writing */
      connection->event_loop_info = MHD_EVENT_LOOP_INFO_WRITE;
      break;
    case MHD_CONNECTION_HEADERS_SENT:
      mhd_assert (0);
      break;
    case MHD_CONNECTION_NORMAL_BODY_READY:
      connection->event_loop_info = MHD_EVENT_LOOP_INFO_WRITE;
      break;
    case MHD_CONNECTION_NORMAL_BODY_UNREADY:
      connection->event_loop_info = MHD_EVENT_LOOP_INFO_BLOCK;
      break;
    case MHD_CONNECTION_CHUNKED_BODY_READY:
      connection->event_loop_info = MHD_EVENT_LOOP_INFO_WRITE;
      break;
    case MHD_CONNECTION_CHUNKED_BODY_UNREADY:
      connection->event_loop_info = MHD_EVENT_LOOP_INFO_BLOCK;
      break;
    case MHD_CONNECTION_BODY_SENT:
      mhd_assert (0);
      break;
    case MHD_CONNECTION_FOOTERS_SENDING:
      connection->event_loop_info = MHD_EVENT_LOOP_INFO_WRITE;
      break;
    case MHD_CONNECTION_FOOTERS_SENT:
      mhd_assert (0);
      break;
    case MHD_CONNECTION_CLOSED:
      connection->event_loop_info = MHD_EVENT_LOOP_INFO_CLEANUP;
      return;           /* do nothing, not even reading */
#ifdef UPGRADE_SUPPORT
    case MHD_CONNECTION_UPGRADE:
      mhd_assert (0);
      break;
#endif /* UPGRADE_SUPPORT */
    default:
      mhd_assert (0);
    }
    break;
  }
}


/**
 * Parse a single line of the HTTP header.  Advance read_buffer (!)
 * appropriately.  If the current line does not fit, consider growing
 * the buffer.  If the line is far too long, close the connection.  If
 * no line is found (incomplete, buffer too small, line too long),
 * return NULL.  Otherwise return a pointer to the line.
 *
 * @param connection connection we're processing
 * @param[out] line_len pointer to variable that receive
 *             length of line or NULL
 * @return NULL if no full line is available; note that the returned
 *         string will not be 0-termianted
 */
static char *
get_next_header_line (struct MHD_Connection *connection,
                      size_t *line_len)
{
  char *rbuf;
  size_t pos;

  if (0 == connection->read_buffer_offset)
    return NULL;
  pos = 0;
  rbuf = connection->read_buffer;
  while ( (pos < connection->read_buffer_offset - 1) &&
          ('\r' != rbuf[pos]) &&
          ('\n' != rbuf[pos]) )
    pos++;
  if ( (pos == connection->read_buffer_offset - 1) &&
       ('\n' != rbuf[pos]) )
  {
    /* not found, consider growing... */
    if ( (connection->read_buffer_offset == connection->read_buffer_size) &&
         (! try_grow_read_buffer (connection, true)) )
    {
      transmit_error_response (connection,
                               (NULL != connection->url)
                               ? MHD_HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE
                               : MHD_HTTP_URI_TOO_LONG,
                               REQUEST_TOO_BIG);
    }
    if (line_len)
      *line_len = 0;
    return NULL;
  }

  if (line_len)
    *line_len = pos;
  /* found, check if we have proper LFCR */
  if ( ('\r' == rbuf[pos]) &&
       ('\n' == rbuf[pos + 1]) )
    rbuf[pos++] = '\0';         /* skip both r and n */
  rbuf[pos++] = '\0';
  connection->read_buffer += pos;
  connection->read_buffer_size -= pos;
  connection->read_buffer_offset -= pos;
  return rbuf;
}


/**
 * Add an entry to the HTTP headers of a connection.  If this fails,
 * transmit an error response (request too big).
 *
 * @param connection the connection for which a
 *  value should be set
 * @param kind kind of the value
 * @param key key for the value
 * @param key_size number of bytes in @a key
 * @param value the value itself
 * @param value_size number of bytes in @a value
 * @return #MHD_NO on failure (out of memory), #MHD_YES for success
 */
static enum MHD_Result
connection_add_header (struct MHD_Connection *connection,
                       const char *key,
                       size_t key_size,
                       const char *value,
                       size_t value_size,
                       enum MHD_ValueKind kind)
{
  if (MHD_NO ==
      MHD_set_connection_value_n (connection,
                                  kind,
                                  key,
                                  key_size,
                                  value,
                                  value_size))
  {
#ifdef HAVE_MESSAGES
    MHD_DLOG (connection->daemon,
              _ ("Not enough memory in pool to allocate header record!\n"));
#endif
    transmit_error_response (connection,
                             MHD_HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE,
                             REQUEST_TOO_BIG);
    return MHD_NO;
  }
  return MHD_YES;
}


/**
 * Parse the cookie header (see RFC 2109).
 *
 * @param connection connection to parse header of
 * @return #MHD_YES for success, #MHD_NO for failure (malformed, out of memory)
 */
static enum MHD_Result
parse_cookie_header (struct MHD_Connection *connection)
{
  const char *hdr;
  size_t hdr_len;
  char *cpy;
  char *pos;
  char *sce;
  char *semicolon;
  char *equals;
  char *ekill;
  char *end;
  char old;
  int quotes;

  if (MHD_NO == MHD_lookup_connection_value_n (connection,
                                               MHD_HEADER_KIND,
                                               MHD_HTTP_HEADER_COOKIE,
                                               MHD_STATICSTR_LEN_ (
                                                 MHD_HTTP_HEADER_COOKIE),
                                               &hdr,
                                               &hdr_len))
    return MHD_YES;
  cpy = MHD_pool_allocate (connection->pool,
                           hdr_len + 1,
                           true);
  if (NULL == cpy)
  {
#ifdef HAVE_MESSAGES
    MHD_DLOG (connection->daemon,
              _ ("Not enough memory in pool to parse cookies!\n"));
#endif
    transmit_error_response (connection,
                             MHD_HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE,
                             REQUEST_TOO_BIG);
    return MHD_NO;
  }
  memcpy (cpy,
          hdr,
          hdr_len);
  cpy[hdr_len] = '\0';
  pos = cpy;
  while (NULL != pos)
  {
    while (' ' == *pos)
      pos++;                    /* skip spaces */

    sce = pos;
    while ( ((*sce) != '\0') &&
            ((*sce) != ',') &&
            ((*sce) != ';') &&
            ((*sce) != '=') )
      sce++;
    /* remove tailing whitespace (if any) from key */
    ekill = sce - 1;
    while ( (*ekill == ' ') &&
            (ekill >= pos) )
      *(ekill--) = '\0';
    old = *sce;
    *sce = '\0';
    if (old != '=')
    {
      /* value part omitted, use empty string... */
      if (MHD_NO ==
          connection_add_header (connection,
                                 pos,
                                 ekill - pos + 1,
                                 "",
                                 0,
                                 MHD_COOKIE_KIND))
        return MHD_NO;
      if (old == '\0')
        break;
      pos = sce + 1;
      continue;
    }
    equals = sce + 1;
    quotes = 0;
    semicolon = equals;
    while ( ('\0' != semicolon[0]) &&
            ( (0 != quotes) ||
              ( (';' != semicolon[0]) &&
                (',' != semicolon[0]) ) ) )
    {
      if ('"' == semicolon[0])
        quotes = (quotes + 1) & 1;
      semicolon++;
    }
    end = semicolon;
    if ('\0' == semicolon[0])
      semicolon = NULL;
    if (NULL != semicolon)
    {
      semicolon[0] = '\0';
      semicolon++;
    }
    /* remove quotes */
    if ( ('"' == equals[0]) &&
         ('"' == end[-1]) )
    {
      equals++;
      end--;
      *end = '\0';
    }
    if (MHD_NO ==
        connection_add_header (connection,
                               pos,
                               ekill - pos + 1,
                               equals,
                               end - equals,
                               MHD_COOKIE_KIND))
      return MHD_NO;
    pos = semicolon;
  }
  return MHD_YES;
}


/**
 * Parse the first line of the HTTP HEADER.
 *
 * @param connection the connection (updated)
 * @param line the first line, not 0-terminated
 * @param line_len length of the first @a line
 * @return #MHD_YES if the line is ok, #MHD_NO if it is malformed
 */
static enum MHD_Result
parse_initial_message_line (struct MHD_Connection *connection,
                            char *line,
                            size_t line_len)
{
  struct MHD_Daemon *daemon = connection->daemon;
  const char *curi;
  char *uri;
  char *http_version;
  char *args;
  unsigned int unused_num_headers;

  if (NULL == (uri = memchr (line,
                             ' ',
                             line_len)))
    return MHD_NO;              /* serious error */
  uri[0] = '\0';
  connection->method = line;
  uri++;
  /* Skip any spaces. Not required by standard but allow
     to be more tolerant. */
  while ( (' ' == uri[0]) &&
          ( (size_t) (uri - line) < line_len) )
    uri++;
  if ((size_t) (uri - line) == line_len)
  {
    /* No URI and no http version given */
    curi = "";
    uri = NULL;
    connection->version = "";
    args = NULL;
  }
  else
  {
    size_t uri_len;
    curi = uri;
    /* Search from back to accept malformed URI with space */
    http_version = line + line_len - 1;
    /* Skip any trailing spaces */
    while ( (' ' == http_version[0]) &&
            (http_version > uri) )
      http_version--;
    /* Find first space in reverse direction */
    while ( (' ' != http_version[0]) &&
            (http_version > uri) )
      http_version--;
    if (http_version > uri)
    {
      /* http_version points to character before HTTP version string */
      http_version[0] = '\0';
      connection->version = http_version + 1;
      uri_len = http_version - uri;
    }
    else
    {
      connection->version = "";
      uri_len = line_len - (uri - line);
    }
    /* check for spaces in URI if we are "strict" */
    if ( (1 <= daemon->strict_for_client) &&
         (NULL != memchr (uri,
                          ' ',
                          uri_len)) )
    {
      /* space exists in URI and we are supposed to be strict, reject */
      return MHD_NO;
    }

    args = memchr (uri,
                   '?',
                   uri_len);
  }

  /* log callback before we modify URI *or* args */
  if (NULL != daemon->uri_log_callback)
  {
    connection->client_aware = true;
    connection->client_context
      = daemon->uri_log_callback (daemon->uri_log_callback_cls,
                                  uri,
                                  connection);
  }

  if (NULL != args)
  {
    args[0] = '\0';
    args++;
    /* note that this call clobbers 'args' */
    MHD_parse_arguments_ (connection,
                          MHD_GET_ARGUMENT_KIND,
                          args,
                          &connection_add_header,
                          &unused_num_headers);
  }

  /* unescape URI *after* searching for arguments and log callback */
  if (NULL != uri)
    daemon->unescape_callback (daemon->unescape_callback_cls,
                               connection,
                               uri);
  connection->url = curi;
  return MHD_YES;
}


/**
 * Call the handler of the application for this
 * connection.  Handles chunking of the upload
 * as well as normal uploads.
 *
 * @param connection connection we're processing
 */
static void
call_connection_handler (struct MHD_Connection *connection)
{
  struct MHD_Daemon *daemon = connection->daemon;
  size_t processed;

  if (NULL != connection->response)
    return;                     /* already queued a response */
  processed = 0;
  connection->client_aware = true;
  if (MHD_NO ==
      daemon->default_handler (daemon->default_handler_cls,
                               connection,
                               connection->url,
                               connection->method,
                               connection->version,
                               NULL,
                               &processed,
                               &connection->client_context))
  {
    /* serious internal error, close connection */
    CONNECTION_CLOSE_ERROR (connection,
                            _ (
                              "Application reported internal error, closing connection."));
    return;
  }
}


/**
 * Call the handler of the application for this
 * connection.  Handles chunking of the upload
 * as well as normal uploads.
 *
 * @param connection connection we're processing
 */
static void
process_request_body (struct MHD_Connection *connection)
{
  struct MHD_Daemon *daemon = connection->daemon;
  size_t available;
  int instant_retry;
  char *buffer_head;

  if (NULL != connection->response)
  {
    /* already queued a response, discard remaining upload
       (but not more, there might be another request after it) */
    size_t purge;

    purge = (size_t) MHD_MIN (connection->remaining_upload_size,
                              (uint64_t) connection->read_buffer_offset);
    connection->remaining_upload_size -= purge;
    if (connection->read_buffer_offset > purge)
      memmove (connection->read_buffer,
               &connection->read_buffer[purge],
               connection->read_buffer_offset - purge);
    connection->read_buffer_offset -= purge;
    return;
  }

  buffer_head = connection->read_buffer;
  available = connection->read_buffer_offset;
  do
  {
    size_t to_be_processed;
    size_t left_unprocessed;
    size_t processed_size;

    instant_retry = MHD_NO;
    if ( (connection->have_chunked_upload) &&
         (MHD_SIZE_UNKNOWN == connection->remaining_upload_size) )
    {
      if ( (connection->current_chunk_offset ==
            connection->current_chunk_size) &&
           (0LLU != connection->current_chunk_offset) &&
           (available >= 2) )
      {
        size_t i;
        /* skip new line at the *end* of a chunk */
        i = 0;
        if ( ('\r' == buffer_head[i]) ||
             ('\n' == buffer_head[i]) )
          i++;                  /* skip 1st part of line feed */
        if ( ('\r' == buffer_head[i]) ||
             ('\n' == buffer_head[i]) )
          i++;                  /* skip 2nd part of line feed */
        if (0 == i)
        {
          /* malformed encoding */
          CONNECTION_CLOSE_ERROR (connection,
                                  _ (
                                    "Received malformed HTTP request (bad chunked encoding). Closing connection."));
          return;
        }
        available -= i;
        buffer_head += i;
        connection->current_chunk_offset = 0;
        connection->current_chunk_size = 0;
      }
      if (connection->current_chunk_offset <
          connection->current_chunk_size)
      {
        uint64_t cur_chunk_left;
        /* we are in the middle of a chunk, give
           as much as possible to the client (without
           crossing chunk boundaries) */
        cur_chunk_left
          = connection->current_chunk_size - connection->current_chunk_offset;
        if (cur_chunk_left > available)
          to_be_processed = available;
        else
        {         /* cur_chunk_left <= (size_t)available */
          to_be_processed = (size_t) cur_chunk_left;
          if (available > to_be_processed)
            instant_retry = MHD_YES;
        }
      }
      else
      {
        size_t i;
        size_t end_size;
        bool malformed;

        /* we need to read chunk boundaries */
        i = 0;
        while (i < available)
        {
          if ( ('\r' == buffer_head[i]) ||
               ('\n' == buffer_head[i]) ||
               (';' == buffer_head[i]) )
            break;
          i++;
          if (i >= 16)
            break;
        }
        end_size = i;
        /* find beginning of CRLF (skip over chunk extensions) */
        if (';' == buffer_head[i])
        {
          while (i < available)
          {
            if ( ('\r' == buffer_head[i]) ||
                 ('\n' == buffer_head[i]) )
              break;
            i++;
          }
        }
        /* take '\n' into account; if '\n' is the unavailable
           character, we will need to wait until we have it
           before going further */
        if ( (i + 1 >= available) &&
             ! ( (1 == i) &&
                 (2 == available) &&
                 ('0' == buffer_head[0]) ) )
          break;                /* need more data... */
        i++;
        malformed = (end_size >= 16);
        if (! malformed)
        {
          size_t num_dig = MHD_strx_to_uint64_n_ (buffer_head,
                                                  end_size,
                                                  &connection->
                                                  current_chunk_size);
          malformed = (end_size != num_dig);
        }
        if (malformed)
        {
          /* malformed encoding */
          CONNECTION_CLOSE_ERROR (connection,
                                  _ (
                                    "Received malformed HTTP request (bad chunked encoding). Closing connection."));
          return;
        }
        /* skip 2nd part of line feed */
        if ( (i < available) &&
             ( ('\r' == buffer_head[i]) ||
               ('\n' == buffer_head[i]) ) )
          i++;

        buffer_head += i;
        available -= i;
        connection->current_chunk_offset = 0;

        if (available > 0)
          instant_retry = MHD_YES;
        if (0LLU == connection->current_chunk_size)
        {
          connection->remaining_upload_size = 0;
          break;
        }
        continue;
      }
    }
    else
    {
      /* no chunked encoding, give all to the client */
      if ( (0 != connection->remaining_upload_size) &&
           (MHD_SIZE_UNKNOWN != connection->remaining_upload_size) &&
           (connection->remaining_upload_size < available) )
      {
        to_be_processed = (size_t) connection->remaining_upload_size;
      }
      else
      {
        /**
         * 1. no chunked encoding, give all to the client
         * 2. client may send large chunked data, but only a smaller part is available at one time.
         */
        to_be_processed = available;
      }
    }
    left_unprocessed = to_be_processed;
    connection->client_aware = true;
    if (MHD_NO ==
        daemon->default_handler (daemon->default_handler_cls,
                                 connection,
                                 connection->url,
                                 connection->method,
                                 connection->version,
                                 buffer_head,
                                 &left_unprocessed,
                                 &connection->client_context))
    {
      /* serious internal error, close connection */
      CONNECTION_CLOSE_ERROR (connection,
                              _ (
                                "Application reported internal error, closing connection."));
      return;
    }
    if (left_unprocessed > to_be_processed)
      mhd_panic (mhd_panic_cls,
                 __FILE__,
                 __LINE__
#ifdef HAVE_MESSAGES
                 , _ ("libmicrohttpd API violation.\n")
#else
                 , NULL
#endif
                 );
    if (0 != left_unprocessed)
    {
      instant_retry = MHD_NO; /* client did not process everything */
#ifdef HAVE_MESSAGES
      /* client did not process all upload data, complain if
         the setup was incorrect, which may prevent us from
         handling the rest of the request */
      if ( (0 != (daemon->options & MHD_USE_INTERNAL_POLLING_THREAD)) &&
           (! connection->suspended) )
        MHD_DLOG (daemon,
                  _ (
                    "WARNING: incomplete upload processing and connection not suspended may result in hung connection.\n"));
#endif
    }
    processed_size = to_be_processed - left_unprocessed;
    if (connection->have_chunked_upload)
      connection->current_chunk_offset += processed_size;
    /* dh left "processed" bytes in buffer for next time... */
    buffer_head += processed_size;
    available -= processed_size;
    if (MHD_SIZE_UNKNOWN != connection->remaining_upload_size)
      connection->remaining_upload_size -= processed_size;
  }
  while (MHD_NO != instant_retry);
  if ( (available > 0) &&
       (buffer_head != connection->read_buffer) )
    memmove (connection->read_buffer,
             buffer_head,
             available);
  connection->read_buffer_offset = available;
}


/**
 * Check if we are done sending the write-buffer.
 * If so, transition into "next_state".
 *
 * @param connection connection to check write status for
 * @param next_state the next state to transition to
 * @return #MHD_NO if we are not done, #MHD_YES if we are
 */
static enum MHD_Result
check_write_done (struct MHD_Connection *connection,
                  enum MHD_CONNECTION_STATE next_state)
{
  if ( (connection->write_buffer_append_offset !=
        connection->write_buffer_send_offset)
       /* || data_in_tls_buffers == true  */
       )
    return MHD_NO;
  connection->write_buffer_append_offset = 0;
  connection->write_buffer_send_offset = 0;
  connection->state = next_state;
  MHD_pool_reallocate (connection->pool,
                       connection->write_buffer,
                       connection->write_buffer_size,
                       0);
  connection->write_buffer = NULL;
  connection->write_buffer_size = 0;
  return MHD_YES;
}


/**
 * We have received (possibly the beginning of) a line in the
 * header (or footer).  Validate (check for ":") and prepare
 * to process.
 *
 * @param connection connection we're processing
 * @param line line from the header to process
 * @return #MHD_YES on success, #MHD_NO on error (malformed @a line)
 */
static enum MHD_Result
process_header_line (struct MHD_Connection *connection,
                     char *line)
{
  char *colon;

  /* line should be normal header line, find colon */
  colon = strchr (line, ':');
  if (NULL == colon)
  {
    /* error in header line, die hard */
    CONNECTION_CLOSE_ERROR (connection,
                            _ (
                              "Received malformed line (no colon). Closing connection."));
    return MHD_NO;
  }
  if (-1 >= connection->daemon->strict_for_client)
  {
    /* check for whitespace before colon, which is not allowed
 by RFC 7230 section 3.2.4; we count space ' ' and
 tab '\t', but not '\r\n' as those would have ended the line. */
    const char *white;

    white = strchr (line, ' ');
    if ( (NULL != white) &&
         (white < colon) )
      return MHD_NO;
    white = strchr (line, '\t');
    if ( (NULL != white) &&
         (white < colon) )
      return MHD_NO;
  }
  /* zero-terminate header */
  colon[0] = '\0';
  colon++;                      /* advance to value */
  while ( ('\0' != colon[0]) &&
          ( (' ' == colon[0]) ||
            ('\t' == colon[0]) ) )
    colon++;
  /* we do the actual adding of the connection
     header at the beginning of the while
     loop since we need to be able to inspect
     the *next* header line (in case it starts
     with a space...) */
  connection->last = line;
  connection->colon = colon;
  return MHD_YES;
}


/**
 * Process a header value that spans multiple lines.
 * The previous line(s) are in connection->last.
 *
 * @param connection connection we're processing
 * @param line the current input line
 * @param kind if the line is complete, add a header
 *        of the given kind
 * @return #MHD_YES if the line was processed successfully
 */
static enum MHD_Result
process_broken_line (struct MHD_Connection *connection,
                     char *line,
                     enum MHD_ValueKind kind)
{
  char *last;
  char *tmp;
  size_t last_len;
  size_t tmp_len;

  last = connection->last;
  if ( (' ' == line[0]) ||
       ('\t' == line[0]) )
  {
    /* value was continued on the next line, see
       http://www.jmarshall.com/easy/http/ */
    last_len = strlen (last);
    /* skip whitespace at start of 2nd line */
    tmp = line;
    while ( (' ' == tmp[0]) ||
            ('\t' == tmp[0]) )
      tmp++;
    tmp_len = strlen (tmp);
    /* FIXME: we might be able to do this better (faster!), as most
       likely 'last' and 'line' should already be adjacent in
       memory; however, doing this right gets tricky if we have a
       value continued over multiple lines (in which case we need to
       record how often we have done this so we can check for
       adjacency); also, in the case where these are not adjacent
       (not sure how it can happen!), we would want to allocate from
       the end of the pool, so as to not destroy the read-buffer's
       ability to grow nicely. */
    last = MHD_pool_reallocate (connection->pool,
                                last,
                                last_len + 1,
                                last_len + tmp_len + 1);
    if (NULL == last)
    {
      transmit_error_response (connection,
                               MHD_HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE,
                               REQUEST_TOO_BIG);
      return MHD_NO;
    }
    memcpy (&last[last_len],
            tmp,
            tmp_len + 1);
    connection->last = last;
    return MHD_YES;             /* possibly more than 2 lines... */
  }
  mhd_assert ( (NULL != last) &&
               (NULL != connection->colon) );
  if (MHD_NO ==
      connection_add_header (connection,
                             last,
                             strlen (last),
                             connection->colon,
                             strlen (connection->colon),
                             kind))
  {
    transmit_error_response (connection,
                             MHD_HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE,
                             REQUEST_TOO_BIG);
    return MHD_NO;
  }
  /* we still have the current line to deal with... */
  if (0 != line[0])
  {
    if (MHD_NO == process_header_line (connection,
                                       line))
    {
      transmit_error_response (connection,
                               MHD_HTTP_BAD_REQUEST,
                               REQUEST_MALFORMED);
      return MHD_NO;
    }
  }
  return MHD_YES;
}


/**
 * Parse the various headers; figure out the size
 * of the upload and make sure the headers follow
 * the protocol.  Advance to the appropriate state.
 *
 * @param connection connection we're processing
 */
static void
parse_connection_headers (struct MHD_Connection *connection)
{
  const char *clen;
  struct MHD_Response *response;
  const char *enc;
  const char *end;

  parse_cookie_header (connection);
  if ( (1 <= connection->daemon->strict_for_client) &&
       (NULL != connection->version) &&
       (MHD_str_equal_caseless_ (MHD_HTTP_VERSION_1_1,
                                 connection->version)) &&
       (MHD_NO ==
        MHD_lookup_connection_value_n (connection,
                                       MHD_HEADER_KIND,
                                       MHD_HTTP_HEADER_HOST,
                                       MHD_STATICSTR_LEN_ (
                                         MHD_HTTP_HEADER_HOST),
                                       NULL,
                                       NULL)) )
  {
    enum MHD_Result iret;

    /* die, http 1.1 request without host and we are pedantic */
    connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
    connection->read_closed = true;
#ifdef HAVE_MESSAGES
    MHD_DLOG (connection->daemon,
              _ ("Received HTTP 1.1 request without `Host' header.\n"));
#endif
    mhd_assert (NULL == connection->response);
    response =
      MHD_create_response_from_buffer (MHD_STATICSTR_LEN_ (REQUEST_LACKS_HOST),
                                       REQUEST_LACKS_HOST,
                                       MHD_RESPMEM_PERSISTENT);
    if (NULL == response)
    {
      /* can't even send a reply, at least close the connection */
      CONNECTION_CLOSE_ERROR (connection,
                              _ (
                                "Closing connection (failed to create response)."));
      return;
    }
    iret = MHD_queue_response (connection,
                               MHD_HTTP_BAD_REQUEST,
                               response);
    MHD_destroy_response (response);
    if (MHD_NO == iret)
    {
      /* can't even send a reply, at least close the connection */
      CONNECTION_CLOSE_ERROR (connection,
                              _ (
                                "Closing connection (failed to queue response)."));
    }
    return;
  }

  connection->remaining_upload_size = 0;
  if (MHD_NO != MHD_lookup_connection_value_n (connection,
                                               MHD_HEADER_KIND,
                                               MHD_HTTP_HEADER_TRANSFER_ENCODING,
                                               MHD_STATICSTR_LEN_ (
                                                 MHD_HTTP_HEADER_TRANSFER_ENCODING),
                                               &enc,
                                               NULL))
  {
    connection->remaining_upload_size = MHD_SIZE_UNKNOWN;
    if (MHD_str_equal_caseless_ (enc,
                                 "chunked"))
      connection->have_chunked_upload = true;
  }
  else
  {
    if (MHD_NO != MHD_lookup_connection_value_n (connection,
                                                 MHD_HEADER_KIND,
                                                 MHD_HTTP_HEADER_CONTENT_LENGTH,
                                                 MHD_STATICSTR_LEN_ (
                                                   MHD_HTTP_HEADER_CONTENT_LENGTH),
                                                 &clen,
                                                 NULL))
    {
      end = clen + MHD_str_to_uint64_ (clen,
                                       &connection->remaining_upload_size);
      if ( (clen == end) ||
           ('\0' != *end) )
      {
        connection->remaining_upload_size = 0;
#ifdef HAVE_MESSAGES
        MHD_DLOG (connection->daemon,
                  _ (
                    "Failed to parse `Content-Length' header. Closing connection.\n"));
#endif
        CONNECTION_CLOSE_ERROR (connection,
                                NULL);
        return;
      }
    }
  }
}


/**
 * Update the 'last_activity' field of the connection to the current time
 * and move the connection to the head of the 'normal_timeout' list if
 * the timeout for the connection uses the default value.
 *
 * @param connection the connection that saw some activity
 */
void
MHD_update_last_activity_ (struct MHD_Connection *connection)
{
  struct MHD_Daemon *daemon = connection->daemon;

  if (0 == connection->connection_timeout)
    return;  /* Skip update of activity for connections
               without timeout timer. */
  if (connection->suspended)
    return;  /* no activity on suspended connections */

  connection->last_activity = MHD_monotonic_sec_counter ();
  if (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    return; /* each connection has personal timeout */

  if (connection->connection_timeout != daemon->connection_timeout)
    return; /* custom timeout, no need to move it in "normal" DLL */
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
  MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);
#endif
  /* move connection to head of timeout list (by remove + add operation) */
  XDLL_remove (daemon->normal_timeout_head,
               daemon->normal_timeout_tail,
               connection);
  XDLL_insert (daemon->normal_timeout_head,
               daemon->normal_timeout_tail,
               connection);
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
  MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);
#endif
}


/**
 * This function handles a particular connection when it has been
 * determined that there is data to be read off a socket.
 *
 * @param connection connection to handle
 */
void
MHD_connection_handle_read (struct MHD_Connection *connection)
{
  ssize_t bytes_read;

  if ( (MHD_CONNECTION_CLOSED == connection->state) ||
       (connection->suspended) )
    return;
#ifdef HTTPS_SUPPORT
  if (MHD_TLS_CONN_NO_TLS != connection->tls_state)
  {   /* HTTPS connection. */
    if (MHD_TLS_CONN_CONNECTED > connection->tls_state)
    {
      if (! MHD_run_tls_handshake_ (connection))
        return;
    }
  }
#endif /* HTTPS_SUPPORT */

  /* make sure "read" has a reasonable number of bytes
     in buffer to use per system call (if possible) */
  if (connection->read_buffer_offset + connection->daemon->pool_increment >
      connection->read_buffer_size)
    try_grow_read_buffer (connection,
                          (connection->read_buffer_size ==
                           connection->read_buffer_offset));

  if (connection->read_buffer_size == connection->read_buffer_offset)
    return; /* No space for receiving data. */
  bytes_read = connection->recv_cls (connection,
                                     &connection->read_buffer
                                     [connection->read_buffer_offset],
                                     connection->read_buffer_size
                                     - connection->read_buffer_offset);
  if (bytes_read < 0)
  {
    if (MHD_ERR_AGAIN_ == bytes_read)
      return;     /* No new data to process. */
    if (MHD_ERR_CONNRESET_ == bytes_read)
    {
      CONNECTION_CLOSE_ERROR (connection,
                              (MHD_CONNECTION_INIT == connection->state) ?
                              NULL :
                              _ (
                                "Socket disconnected while reading request."));
      return;
    }

#ifdef HAVE_MESSAGES
    if (MHD_CONNECTION_INIT != connection->state)
      MHD_DLOG (connection->daemon,
                _ ("Connection socket is closed when reading " \
                   "request due to the error: %s\n"),
                str_conn_error_ (bytes_read));
#endif
    CONNECTION_CLOSE_ERROR (connection,
                            NULL);
    return;
  }

  if (0 == bytes_read)
  {   /* Remote side closed connection. */
    connection->read_closed = true;
    MHD_connection_close_ (connection,
                           MHD_REQUEST_TERMINATED_CLIENT_ABORT);
    return;
  }
  connection->read_buffer_offset += bytes_read;
  MHD_update_last_activity_ (connection);
#if DEBUG_STATES
  MHD_DLOG (connection->daemon,
            _ ("In function %s handling connection at state: %s\n"),
            __FUNCTION__,
            MHD_state_to_string (connection->state));
#endif
  switch (connection->state)
  {
  case MHD_CONNECTION_INIT:
  case MHD_CONNECTION_URL_RECEIVED:
  case MHD_CONNECTION_HEADER_PART_RECEIVED:
  case MHD_CONNECTION_HEADERS_RECEIVED:
  case MHD_CONNECTION_HEADERS_PROCESSED:
  case MHD_CONNECTION_CONTINUE_SENDING:
  case MHD_CONNECTION_CONTINUE_SENT:
  case MHD_CONNECTION_BODY_RECEIVED:
  case MHD_CONNECTION_FOOTER_PART_RECEIVED:
    /* nothing to do but default action */
    if (connection->read_closed)
    {
      MHD_connection_close_ (connection,
                             MHD_REQUEST_TERMINATED_READ_ERROR);
    }
    return;
  case MHD_CONNECTION_CLOSED:
    return;
#ifdef UPGRADE_SUPPORT
  case MHD_CONNECTION_UPGRADE:
    mhd_assert (0);
    return;
#endif /* UPGRADE_SUPPORT */
  default:
    /* shrink read buffer to how much is actually used */
    MHD_pool_reallocate (connection->pool,
                         connection->read_buffer,
                         connection->read_buffer_size + 1,
                         connection->read_buffer_offset);
    break;
  }
  return;
}


/**
 * This function was created to handle writes to sockets when it has
 * been determined that the socket can be written to.
 *
 * @param connection connection to handle
 */
void
MHD_connection_handle_write (struct MHD_Connection *connection)
{
  struct MHD_Response *response;
  ssize_t ret;
  if (connection->suspended)
    return;

#ifdef HTTPS_SUPPORT
  if (MHD_TLS_CONN_NO_TLS != connection->tls_state)
  {   /* HTTPS connection. */
    if (MHD_TLS_CONN_CONNECTED > connection->tls_state)
    {
      if (! MHD_run_tls_handshake_ (connection))
        return;
    }
  }
#endif /* HTTPS_SUPPORT */

#if DEBUG_STATES
  MHD_DLOG (connection->daemon,
            _ ("In function %s handling connection at state: %s\n"),
            __FUNCTION__,
            MHD_state_to_string (connection->state));
#endif
  switch (connection->state)
  {
  case MHD_CONNECTION_INIT:
  case MHD_CONNECTION_URL_RECEIVED:
  case MHD_CONNECTION_HEADER_PART_RECEIVED:
  case MHD_CONNECTION_HEADERS_RECEIVED:
    mhd_assert (0);
    return;
  case MHD_CONNECTION_HEADERS_PROCESSED:
    return;
  case MHD_CONNECTION_CONTINUE_SENDING:
    ret = MHD_send_data_ (connection,
                          &HTTP_100_CONTINUE
                          [connection->continue_message_write_offset],
                          MHD_STATICSTR_LEN_ (HTTP_100_CONTINUE)
                          - connection->continue_message_write_offset,
                          true);
    if (ret < 0)
    {
      if (MHD_ERR_AGAIN_ == ret)
        return;
#ifdef HAVE_MESSAGES
      MHD_DLOG (connection->daemon,
                _ ("Failed to send data in request for %s.\n"),
                connection->url);
#endif
      CONNECTION_CLOSE_ERROR (connection,
                              NULL);
      return;
    }
#if _MHD_DEBUG_SEND_DATA
    fprintf (stderr,
             _ ("Sent 100 continue response: `%.*s'\n"),
             (int) ret,
             &HTTP_100_CONTINUE[connection->continue_message_write_offset]);
#endif
    connection->continue_message_write_offset += ret;
    MHD_update_last_activity_ (connection);
    return;
  case MHD_CONNECTION_CONTINUE_SENT:
  case MHD_CONNECTION_BODY_RECEIVED:
  case MHD_CONNECTION_FOOTER_PART_RECEIVED:
  case MHD_CONNECTION_FOOTERS_RECEIVED:
    mhd_assert (0);
    return;
  case MHD_CONNECTION_HEADERS_SENDING:
    {
      struct MHD_Response *const resp = connection->response;
      const size_t wb_ready = connection->write_buffer_append_offset
                              - connection->write_buffer_send_offset;
      mhd_assert (connection->write_buffer_append_offset >= \
                  connection->write_buffer_send_offset);
      mhd_assert (NULL != resp);
      mhd_assert ( (0 == resp->data_size) || \
                   (0 == resp->data_start) || \
                   (NULL != resp->crc) );
      mhd_assert ( (0 == connection->response_write_position) || \
                   (resp->total_size ==
                    connection->response_write_position) || \
                   (MHD_SIZE_UNKNOWN ==
                    connection->response_write_position) );

      if ( (NULL == resp->crc) &&
           (NULL == resp->data_iov) &&
           (0 == connection->response_write_position) )
      {
        mhd_assert (resp->total_size >= resp->data_size);
        /* Send response headers alongside the response body, if the body
         * data is available. */
        ret = MHD_send_hdr_and_body_ (connection,
                                      &connection->write_buffer
                                      [connection->write_buffer_send_offset],
                                      wb_ready,
                                      false,
                                      resp->data,
                                      resp->data_size,
                                      (resp->total_size == resp->data_size));
      }
      else
      {
        /* This is response for HEAD request or reply body is not allowed
         * for any other reason or reply body is dynamically generated. */
        /* Do not send the body data even if it's available. */
        ret = MHD_send_hdr_and_body_ (connection,
                                      &connection->write_buffer
                                      [connection->write_buffer_send_offset],
                                      wb_ready,
                                      false,
                                      NULL,
                                      0,
                                      ((0 == resp->total_size) ||
                                       (resp->total_size ==
                                        connection->response_write_position) ||
                                       (MHD_SIZE_UNKNOWN ==
                                        connection->response_write_position)));
      }

      if (ret < 0)
      {
        if (MHD_ERR_AGAIN_ == ret)
          return;
#ifdef HAVE_MESSAGES
        MHD_DLOG (connection->daemon,
                  _ ("Failed to send the response headers for the " \
                     "request for `%s'. Error: %s\n"),
                  connection->url,
                  str_conn_error_ (ret));
#endif
        CONNECTION_CLOSE_ERROR (connection,
                                NULL);
        return;
      }
      /* 'ret' is not negative, it's safe to cast it to 'size_t'. */
      if (((size_t) ret) > wb_ready)
      {
        /* The complete header and some response data have been sent,
         * update both offsets. */
        mhd_assert (0 == connection->response_write_position);
        mhd_assert (! connection->have_chunked_upload);
        connection->write_buffer_send_offset += wb_ready;
        connection->response_write_position = ret - wb_ready;
      }
      else
        connection->write_buffer_send_offset += ret;
      MHD_update_last_activity_ (connection);
      if (MHD_CONNECTION_HEADERS_SENDING != connection->state)
        return;
      check_write_done (connection,
                        MHD_CONNECTION_HEADERS_SENT);
      return;
    }
  case MHD_CONNECTION_HEADERS_SENT:
    return;
  case MHD_CONNECTION_NORMAL_BODY_READY:
    response = connection->response;
    if (connection->response_write_position <
        connection->response->total_size)
    {
      uint64_t data_write_offset;

#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
      if (NULL != response->crc)
        MHD_mutex_lock_chk_ (&response->mutex);
#endif
      if (MHD_NO == try_ready_normal_body (connection))
      {
        /* mutex was already unlocked by try_ready_normal_body */
        return;
      }
#if defined(_MHD_HAVE_SENDFILE)
      if (MHD_resp_sender_sendfile == connection->resp_sender)
      {
        mhd_assert (NULL == response->data_iov);
        ret = MHD_send_sendfile_ (connection);
      }
      else /* combined with the next 'if' */
#endif /* _MHD_HAVE_SENDFILE */
      if (NULL != response->data_iov)
      {
        ret = MHD_send_iovec_ (connection,
                               &connection->resp_iov,
                               true);
      }
      else
      {
        data_write_offset = connection->response_write_position
                            - response->data_start;
        if (data_write_offset > (uint64_t) SIZE_MAX)
          MHD_PANIC (_ ("Data offset exceeds limit.\n"));
        ret = MHD_send_data_ (connection,
                              &response->data
                              [(size_t) data_write_offset],
                              response->data_size
                              - (size_t) data_write_offset,
                              true);
#if _MHD_DEBUG_SEND_DATA
        if (ret > 0)
          fprintf (stderr,
                   _ ("Sent %d-byte DATA response: `%.*s'\n"),
                   (int) ret,
                   (int) ret,
                   &response->data[connection->response_write_position
                                   - response->data_start]);
#endif
      }
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
      if (NULL != response->crc)
        MHD_mutex_unlock_chk_ (&response->mutex);
#endif
      if (ret < 0)
      {
        if (MHD_ERR_AGAIN_ == ret)
          return;
#ifdef HAVE_MESSAGES
        MHD_DLOG (connection->daemon,
                  _ ("Failed to send the response body for the " \
                     "request for `%s'. Error: %s\n"),
                  connection->url,
                  str_conn_error_ (ret));
#endif
        CONNECTION_CLOSE_ERROR (connection,
                                NULL);
        return;
      }
      connection->response_write_position += ret;
      MHD_update_last_activity_ (connection);
    }
    if (connection->response_write_position ==
        connection->response->total_size)
      connection->state = MHD_CONNECTION_FOOTERS_SENT;   /* have no footers */
    return;
  case MHD_CONNECTION_NORMAL_BODY_UNREADY:
    mhd_assert (0);
    return;
  case MHD_CONNECTION_CHUNKED_BODY_READY:
    ret = MHD_send_data_ (connection,
                          &connection->write_buffer
                          [connection->write_buffer_send_offset],
                          connection->write_buffer_append_offset
                          - connection->write_buffer_send_offset,
                          true);
    if (ret < 0)
    {
      if (MHD_ERR_AGAIN_ == ret)
        return;
#ifdef HAVE_MESSAGES
      MHD_DLOG (connection->daemon,
                _ ("Failed to send the chunked response body for the " \
                   "request for `%s'. Error: %s\n"),
                connection->url,
                str_conn_error_ (ret));
#endif
      CONNECTION_CLOSE_ERROR (connection,
                              NULL);
      return;
    }
    connection->write_buffer_send_offset += ret;
    MHD_update_last_activity_ (connection);
    if (MHD_CONNECTION_CHUNKED_BODY_READY != connection->state)
      return;
    check_write_done (connection,
                      (connection->response->total_size ==
                       connection->response_write_position) ?
                      MHD_CONNECTION_BODY_SENT :
                      MHD_CONNECTION_CHUNKED_BODY_UNREADY);
    return;
  case MHD_CONNECTION_CHUNKED_BODY_UNREADY:
  case MHD_CONNECTION_BODY_SENT:
    mhd_assert (0);
    return;
  case MHD_CONNECTION_FOOTERS_SENDING:
    ret = MHD_send_data_ (connection,
                          &connection->write_buffer
                          [connection->write_buffer_send_offset],
                          connection->write_buffer_append_offset
                          - connection->write_buffer_send_offset,
                          true);
    if (ret < 0)
    {
      if (MHD_ERR_AGAIN_ == ret)
        return;
#ifdef HAVE_MESSAGES
      MHD_DLOG (connection->daemon,
                _ ("Failed to send the footers for the " \
                   "request for `%s'. Error: %s\n"),
                connection->url,
                str_conn_error_ (ret));
#endif
      CONNECTION_CLOSE_ERROR (connection,
                              NULL);
      return;
    }
    connection->write_buffer_send_offset += ret;
    MHD_update_last_activity_ (connection);
    if (MHD_CONNECTION_FOOTERS_SENDING != connection->state)
      return;
    check_write_done (connection,
                      MHD_CONNECTION_FOOTERS_SENT);
    return;
  case MHD_CONNECTION_FOOTERS_SENT:
    mhd_assert (0);
    return;
  case MHD_CONNECTION_CLOSED:
    return;
#ifdef UPGRADE_SUPPORT
  case MHD_CONNECTION_UPGRADE:
    mhd_assert (0);
    return;
#endif /* UPGRADE_SUPPORT */
  default:
    mhd_assert (0);
    CONNECTION_CLOSE_ERROR (connection,
                            _ ("Internal error.\n"));
    break;
  }
  return;
}


/**
 * Clean up the state of the given connection and move it into the
 * clean up queue for final disposal.
 * @remark To be called only from thread that process connection's
 * recv(), send() and response.
 *
 * @param connection handle for the connection to clean up
 */
static void
cleanup_connection (struct MHD_Connection *connection)
{
  struct MHD_Daemon *daemon = connection->daemon;
#ifdef MHD_USE_THREADS
  mhd_assert ( (0 == (daemon->options & MHD_USE_INTERNAL_POLLING_THREAD)) || \
               MHD_thread_ID_match_current_ (connection->pid) );
#endif /* MHD_USE_THREADS */

  if (connection->in_cleanup)
    return; /* Prevent double cleanup. */
  connection->in_cleanup = true;
  if (NULL != connection->response)
  {
    MHD_destroy_response (connection->response);
    connection->response = NULL;
  }
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
  MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);
#endif
  if (connection->suspended)
  {
    DLL_remove (daemon->suspended_connections_head,
                daemon->suspended_connections_tail,
                connection);
    connection->suspended = false;
  }
  else
  {
    if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
      if (connection->connection_timeout == daemon->connection_timeout)
        XDLL_remove (daemon->normal_timeout_head,
                     daemon->normal_timeout_tail,
                     connection);
      else
        XDLL_remove (daemon->manual_timeout_head,
                     daemon->manual_timeout_tail,
                     connection);
    }
    DLL_remove (daemon->connections_head,
                daemon->connections_tail,
                connection);
  }
  DLL_insert (daemon->cleanup_head,
              daemon->cleanup_tail,
              connection);
  connection->resuming = false;
  connection->in_idle = false;
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
  MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);
#endif
  if (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
  {
    /* if we were at the connection limit before and are in
       thread-per-connection mode, signal the main thread
       to resume accepting connections */
    if ( (MHD_ITC_IS_VALID_ (daemon->itc)) &&
         (! MHD_itc_activate_ (daemon->itc, "c")) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _ (
                  "Failed to signal end of connection via inter-thread communication channel.\n"));
#endif
    }
  }
}


/**
 * This function was created to handle per-connection processing that
 * has to happen even if the socket cannot be read or written to.
 * @remark To be called only from thread that process connection's
 * recv(), send() and response.
 *
 * @param connection connection to handle
 * @return #MHD_YES if we should continue to process the
 *         connection (not dead yet), #MHD_NO if it died
 */
enum MHD_Result
MHD_connection_handle_idle (struct MHD_Connection *connection)
{
  struct MHD_Daemon *daemon = connection->daemon;
  char *line;
  size_t line_len;
  enum MHD_Result ret;
#ifdef MHD_USE_THREADS
  mhd_assert ( (0 == (daemon->options & MHD_USE_INTERNAL_POLLING_THREAD)) || \
               MHD_thread_ID_match_current_ (connection->pid) );
#endif /* MHD_USE_THREADS */

  connection->in_idle = true;
  while (! connection->suspended)
  {
#ifdef HTTPS_SUPPORT
    if (MHD_TLS_CONN_NO_TLS != connection->tls_state)
    {     /* HTTPS connection. */
      if ((MHD_TLS_CONN_INIT <= connection->tls_state) &&
          (MHD_TLS_CONN_CONNECTED > connection->tls_state))
        break;
    }
#endif /* HTTPS_SUPPORT */
#if DEBUG_STATES
    MHD_DLOG (daemon,
              _ ("In function %s handling connection at state: %s\n"),
              __FUNCTION__,
              MHD_state_to_string (connection->state));
#endif
    switch (connection->state)
    {
    case MHD_CONNECTION_INIT:
      line = get_next_header_line (connection,
                                   &line_len);
      /* Check for empty string, as we might want
         to tolerate 'spurious' empty lines; also
         NULL means we didn't get a full line yet;
         line is not 0-terminated here. */
      if ( (NULL == line) ||
           (0 == line[0]) )
      {
        if (MHD_CONNECTION_INIT != connection->state)
          continue;
        if (connection->read_closed)
        {
          CONNECTION_CLOSE_ERROR (connection,
                                  NULL);
          continue;
        }
        break;
      }
      if (MHD_NO == parse_initial_message_line (connection,
                                                line,
                                                line_len))
        CONNECTION_CLOSE_ERROR (connection,
                                NULL);
      else
        connection->state = MHD_CONNECTION_URL_RECEIVED;
      continue;
    case MHD_CONNECTION_URL_RECEIVED:
      line = get_next_header_line (connection,
                                   NULL);
      if (NULL == line)
      {
        if (MHD_CONNECTION_URL_RECEIVED != connection->state)
          continue;
        if (connection->read_closed)
        {
          CONNECTION_CLOSE_ERROR (connection,
                                  NULL);
          continue;
        }
        break;
      }
      if (0 == line[0])
      {
        connection->state = MHD_CONNECTION_HEADERS_RECEIVED;
        connection->header_size = (size_t) (line - connection->read_buffer);
        continue;
      }
      if (MHD_NO == process_header_line (connection,
                                         line))
      {
        transmit_error_response (connection,
                                 MHD_HTTP_BAD_REQUEST,
                                 REQUEST_MALFORMED);
        break;
      }
      connection->state = MHD_CONNECTION_HEADER_PART_RECEIVED;
      continue;
    case MHD_CONNECTION_HEADER_PART_RECEIVED:
      line = get_next_header_line (connection,
                                   NULL);
      if (NULL == line)
      {
        if (connection->state != MHD_CONNECTION_HEADER_PART_RECEIVED)
          continue;
        if (connection->read_closed)
        {
          CONNECTION_CLOSE_ERROR (connection,
                                  NULL);
          continue;
        }
        break;
      }
      if (MHD_NO ==
          process_broken_line (connection,
                               line,
                               MHD_HEADER_KIND))
        continue;
      if (0 == line[0])
      {
        connection->state = MHD_CONNECTION_HEADERS_RECEIVED;
        connection->header_size = (size_t) (line - connection->read_buffer);
        continue;
      }
      continue;
    case MHD_CONNECTION_HEADERS_RECEIVED:
      parse_connection_headers (connection);
      if (MHD_CONNECTION_CLOSED == connection->state)
        continue;
      connection->state = MHD_CONNECTION_HEADERS_PROCESSED;
      if (connection->suspended)
        break;
      continue;
    case MHD_CONNECTION_HEADERS_PROCESSED:
      call_connection_handler (connection);     /* first call */
      if (MHD_CONNECTION_CLOSED == connection->state)
        continue;
      if (connection->suspended)
        continue;
      if ( (NULL == connection->response) &&
           (need_100_continue (connection)) )
      {
        connection->state = MHD_CONNECTION_CONTINUE_SENDING;
        break;
      }
      if ( (NULL != connection->response) &&
           (0 != connection->remaining_upload_size) )
      {
        /* we refused (no upload allowed!) */
        connection->remaining_upload_size = 0;
        /* force close, in case client still tries to upload... */
        connection->read_closed = true;
      }
      connection->state = (0 == connection->remaining_upload_size)
                          ? MHD_CONNECTION_FOOTERS_RECEIVED
                          : MHD_CONNECTION_CONTINUE_SENT;
      if (connection->suspended)
        break;
      continue;
    case MHD_CONNECTION_CONTINUE_SENDING:
      if (connection->continue_message_write_offset ==
          MHD_STATICSTR_LEN_ (HTTP_100_CONTINUE))
      {
        connection->state = MHD_CONNECTION_CONTINUE_SENT;
        continue;
      }
      break;
    case MHD_CONNECTION_CONTINUE_SENT:
      if (0 != connection->read_buffer_offset)
      {
        process_request_body (connection);           /* loop call */
        if (MHD_CONNECTION_CLOSED == connection->state)
          continue;
      }
      if ( (0 == connection->remaining_upload_size) ||
           ( (MHD_SIZE_UNKNOWN == connection->remaining_upload_size) &&
             (0 == connection->read_buffer_offset) &&
             (connection->read_closed) ) )
      {
        if ( (connection->have_chunked_upload) &&
             (! connection->read_closed) )
          connection->state = MHD_CONNECTION_BODY_RECEIVED;
        else
          connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
        if (connection->suspended)
          break;
        continue;
      }
      break;
    case MHD_CONNECTION_BODY_RECEIVED:
      line = get_next_header_line (connection,
                                   NULL);
      if (NULL == line)
      {
        if (connection->state != MHD_CONNECTION_BODY_RECEIVED)
          continue;
        if (connection->read_closed)
        {
          CONNECTION_CLOSE_ERROR (connection,
                                  NULL);
          continue;
        }
        break;
      }
      if (0 == line[0])
      {
        connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
        if (connection->suspended)
          break;
        continue;
      }
      if (MHD_NO == process_header_line (connection,
                                         line))
      {
        transmit_error_response (connection,
                                 MHD_HTTP_BAD_REQUEST,
                                 REQUEST_MALFORMED);
        break;
      }
      connection->state = MHD_CONNECTION_FOOTER_PART_RECEIVED;
      continue;
    case MHD_CONNECTION_FOOTER_PART_RECEIVED:
      line = get_next_header_line (connection,
                                   NULL);
      if (NULL == line)
      {
        if (connection->state != MHD_CONNECTION_FOOTER_PART_RECEIVED)
          continue;
        if (connection->read_closed)
        {
          CONNECTION_CLOSE_ERROR (connection,
                                  NULL);
          continue;
        }
        break;
      }
      if (MHD_NO ==
          process_broken_line (connection,
                               line,
                               MHD_FOOTER_KIND))
        continue;
      if (0 == line[0])
      {
        connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
        if (connection->suspended)
          break;
        continue;
      }
      continue;
    case MHD_CONNECTION_FOOTERS_RECEIVED:
      call_connection_handler (connection);     /* "final" call */
      if (connection->state == MHD_CONNECTION_CLOSED)
        continue;
      if (NULL == connection->response)
        break;                  /* try again next time */
      if (MHD_NO == build_header_response (connection))
      {
        /* oops - close! */
        CONNECTION_CLOSE_ERROR (connection,
                                _ (
                                  "Closing connection (failed to create response header).\n"));
        continue;
      }
      connection->state = MHD_CONNECTION_HEADERS_SENDING;
      break;
    case MHD_CONNECTION_HEADERS_SENDING:
      /* no default action */
      break;
    case MHD_CONNECTION_HEADERS_SENT:
      /* Some clients may take some actions right after header receive */
#ifdef UPGRADE_SUPPORT
      if (NULL != connection->response->upgrade_handler)
      {
        connection->state = MHD_CONNECTION_UPGRADE;
        /* This connection is "upgraded".  Pass socket to application. */
        if (MHD_NO ==
            MHD_response_execute_upgrade_ (connection->response,
                                           connection))
        {
          /* upgrade failed, fail hard */
          CONNECTION_CLOSE_ERROR (connection,
                                  NULL);
          continue;
        }
        /* Response is not required anymore for this connection. */
        {
          struct MHD_Response *const resp = connection->response;

          connection->response = NULL;
          MHD_destroy_response (resp);
        }
        continue;
      }
#endif /* UPGRADE_SUPPORT */

      if (connection->have_chunked_upload)
        connection->state = MHD_CONNECTION_CHUNKED_BODY_UNREADY;
      else
        connection->state = MHD_CONNECTION_NORMAL_BODY_UNREADY;
      continue;
    case MHD_CONNECTION_NORMAL_BODY_READY:
      /* nothing to do here */
      break;
    case MHD_CONNECTION_NORMAL_BODY_UNREADY:
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
      if (NULL != connection->response->crc)
        MHD_mutex_lock_chk_ (&connection->response->mutex);
#endif
      if (0 == connection->response->total_size)
      {
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
        if (NULL != connection->response->crc)
          MHD_mutex_unlock_chk_ (&connection->response->mutex);
#endif
        connection->state = MHD_CONNECTION_BODY_SENT;
        continue;
      }
      if (MHD_NO != try_ready_normal_body (connection))
      {
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
        if (NULL != connection->response->crc)
          MHD_mutex_unlock_chk_ (&connection->response->mutex);
#endif
        connection->state = MHD_CONNECTION_NORMAL_BODY_READY;
        /* Buffering for flushable socket was already enabled*/

        break;
      }
      /* mutex was already unlocked by "try_ready_normal_body */
      /* not ready, no socket action */
      break;
    case MHD_CONNECTION_CHUNKED_BODY_READY:
      /* nothing to do here */
      break;
    case MHD_CONNECTION_CHUNKED_BODY_UNREADY:
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
      if (NULL != connection->response->crc)
        MHD_mutex_lock_chk_ (&connection->response->mutex);
#endif
      if ( (0 == connection->response->total_size) ||
           (connection->response_write_position ==
            connection->response->total_size) )
      {
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
        if (NULL != connection->response->crc)
          MHD_mutex_unlock_chk_ (&connection->response->mutex);
#endif
        connection->state = MHD_CONNECTION_BODY_SENT;
        continue;
      }
      if (MHD_NO != try_ready_chunked_body (connection))
      {
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
        if (NULL != connection->response->crc)
          MHD_mutex_unlock_chk_ (&connection->response->mutex);
#endif
        connection->state = MHD_CONNECTION_CHUNKED_BODY_READY;
        /* Buffering for flushable socket was already enabled */

        continue;
      }
      /* mutex was already unlocked by try_ready_chunked_body */
      break;
    case MHD_CONNECTION_BODY_SENT:
      if (MHD_NO == build_header_response (connection))
      {
        /* oops - close! */
        CONNECTION_CLOSE_ERROR (connection,
                                _ (
                                  "Closing connection (failed to create response header)."));
        continue;
      }
      if ( (! connection->have_chunked_upload) ||
           (connection->write_buffer_send_offset ==
            connection->write_buffer_append_offset) )
        connection->state = MHD_CONNECTION_FOOTERS_SENT;
      else
        connection->state = MHD_CONNECTION_FOOTERS_SENDING;
      continue;
    case MHD_CONNECTION_FOOTERS_SENDING:
      /* no default action */
      break;
    case MHD_CONNECTION_FOOTERS_SENT:
      if (MHD_HTTP_PROCESSING == connection->responseCode)
      {
        /* After this type of response, we allow sending another! */
        connection->state = MHD_CONNECTION_HEADERS_PROCESSED;
        MHD_destroy_response (connection->response);
        connection->response = NULL;
        /* FIXME: maybe partially reset memory pool? */
        continue;
      }
      MHD_destroy_response (connection->response);
      connection->response = NULL;
      if ( (NULL != daemon->notify_completed) &&
           (connection->client_aware) )
      {
        daemon->notify_completed (daemon->notify_completed_cls,
                                  connection,
                                  &connection->client_context,
                                  MHD_REQUEST_TERMINATED_COMPLETED_OK);
      }
      connection->client_aware = false;
      if ( (MHD_CONN_USE_KEEPALIVE != connection->keepalive) ||
           (connection->read_closed) )
      {
        /* have to close for some reason */
        MHD_connection_close_ (connection,
                               MHD_REQUEST_TERMINATED_COMPLETED_OK);
        MHD_pool_destroy (connection->pool);
        connection->pool = NULL;
        connection->read_buffer = NULL;
        connection->read_buffer_size = 0;
        connection->read_buffer_offset = 0;
      }
      else
      {
        /* can try to keep-alive */

        connection->version = NULL;
        connection->state = MHD_CONNECTION_INIT;
        connection->last = NULL;
        connection->colon = NULL;
        connection->header_size = 0;
        connection->keepalive = MHD_CONN_KEEPALIVE_UNKOWN;
        /* Reset the read buffer to the starting size,
           preserving the bytes we have already read. */
        connection->read_buffer
          = MHD_pool_reset (connection->pool,
                            connection->read_buffer,
                            connection->read_buffer_offset,
                            connection->daemon->pool_size / 2);
        connection->read_buffer_size
          = connection->daemon->pool_size / 2;
      }
      connection->client_context = NULL;
      connection->continue_message_write_offset = 0;
      connection->responseCode = 0;
      connection->headers_received = NULL;
      connection->headers_received_tail = NULL;
      connection->response_write_position = 0;
      connection->have_chunked_upload = false;
      connection->current_chunk_size = 0;
      connection->current_chunk_offset = 0;
      connection->method = NULL;
      connection->url = NULL;
      connection->write_buffer = NULL;
      connection->write_buffer_size = 0;
      connection->write_buffer_send_offset = 0;
      connection->write_buffer_append_offset = 0;
      /* iov (if any) was deallocated by MHD_pool_reset */
      memset (&connection->resp_iov, 0, sizeof(connection->resp_iov));
      continue;
    case MHD_CONNECTION_CLOSED:
      cleanup_connection (connection);
      connection->in_idle = false;
      return MHD_NO;
#ifdef UPGRADE_SUPPORT
    case MHD_CONNECTION_UPGRADE:
      connection->in_idle = false;
      return MHD_YES;     /* keep open */
#endif /* UPGRADE_SUPPORT */
    default:
      mhd_assert (0);
      break;
    }
    break;
  }
  if (! connection->suspended)
  {
    time_t timeout;
    timeout = connection->connection_timeout;
    if ( (0 != timeout) &&
         (timeout < (MHD_monotonic_sec_counter ()
                     - connection->last_activity)) )
    {
      MHD_connection_close_ (connection,
                             MHD_REQUEST_TERMINATED_TIMEOUT_REACHED);
      connection->in_idle = false;
      return MHD_YES;
    }
  }
  MHD_connection_update_event_loop_info (connection);
  ret = MHD_YES;
#ifdef EPOLL_SUPPORT
  if ( (! connection->suspended) &&
       (0 != (daemon->options & MHD_USE_EPOLL)) )
  {
    ret = MHD_connection_epoll_update_ (connection);
  }
#endif /* EPOLL_SUPPORT */
  connection->in_idle = false;
  return ret;
}


#ifdef EPOLL_SUPPORT
/**
 * Perform epoll() processing, possibly moving the connection back into
 * the epoll() set if needed.
 *
 * @param connection connection to process
 * @return #MHD_YES if we should continue to process the
 *         connection (not dead yet), #MHD_NO if it died
 */
enum MHD_Result
MHD_connection_epoll_update_ (struct MHD_Connection *connection)
{
  struct MHD_Daemon *daemon = connection->daemon;

  if ( (0 != (daemon->options & MHD_USE_EPOLL)) &&
       (0 == (connection->epoll_state & MHD_EPOLL_STATE_IN_EPOLL_SET)) &&
       (0 == (connection->epoll_state & MHD_EPOLL_STATE_SUSPENDED)) &&
       ( ( (MHD_EVENT_LOOP_INFO_WRITE == connection->event_loop_info) &&
           (0 == (connection->epoll_state & MHD_EPOLL_STATE_WRITE_READY))) ||
         ( (MHD_EVENT_LOOP_INFO_READ == connection->event_loop_info) &&
           (0 == (connection->epoll_state & MHD_EPOLL_STATE_READ_READY)) ) ) )
  {
    /* add to epoll set */
    struct epoll_event event;

    event.events = EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLET;
    event.data.ptr = connection;
    if (0 != epoll_ctl (daemon->epoll_fd,
                        EPOLL_CTL_ADD,
                        connection->socket_fd,
                        &event))
    {
#ifdef HAVE_MESSAGES
      if (0 != (daemon->options & MHD_USE_ERROR_LOG))
        MHD_DLOG (daemon,
                  _ ("Call to epoll_ctl failed: %s\n"),
                  MHD_socket_last_strerr_ ());
#endif
      connection->state = MHD_CONNECTION_CLOSED;
      cleanup_connection (connection);
      return MHD_NO;
    }
    connection->epoll_state |= MHD_EPOLL_STATE_IN_EPOLL_SET;
  }
  return MHD_YES;
}


#endif


/**
 * Set callbacks for this connection to those for HTTP.
 *
 * @param connection connection to initialize
 */
void
MHD_set_http_callbacks_ (struct MHD_Connection *connection)
{
  connection->recv_cls = &recv_param_adapter;
}


/**
 * Obtain information about the given connection.
 *
 * @param connection what connection to get information about
 * @param info_type what information is desired?
 * @param ... depends on @a info_type
 * @return NULL if this information is not available
 *         (or if the @a info_type is unknown)
 * @ingroup specialized
 */
const union MHD_ConnectionInfo *
MHD_get_connection_info (struct MHD_Connection *connection,
                         enum MHD_ConnectionInfoType info_type,
                         ...)
{
  switch (info_type)
  {
#ifdef HTTPS_SUPPORT
  case MHD_CONNECTION_INFO_CIPHER_ALGO:
    if (NULL == connection->tls_session)
      return NULL;
    connection->cipher = gnutls_cipher_get (connection->tls_session);
    return (const union MHD_ConnectionInfo *) &connection->cipher;
  case MHD_CONNECTION_INFO_PROTOCOL:
    if (NULL == connection->tls_session)
      return NULL;
    connection->protocol = gnutls_protocol_get_version (
      connection->tls_session);
    return (const union MHD_ConnectionInfo *) &connection->protocol;
  case MHD_CONNECTION_INFO_GNUTLS_SESSION:
    if (NULL == connection->tls_session)
      return NULL;
    return (const union MHD_ConnectionInfo *) &connection->tls_session;
#endif /* HTTPS_SUPPORT */
  case MHD_CONNECTION_INFO_CLIENT_ADDRESS:
    return (const union MHD_ConnectionInfo *) &connection->addr;
  case MHD_CONNECTION_INFO_DAEMON:
    return (const union MHD_ConnectionInfo *) &connection->daemon;
  case MHD_CONNECTION_INFO_CONNECTION_FD:
    return (const union MHD_ConnectionInfo *) &connection->socket_fd;
  case MHD_CONNECTION_INFO_SOCKET_CONTEXT:
    return (const union MHD_ConnectionInfo *) &connection->socket_context;
  case MHD_CONNECTION_INFO_CONNECTION_SUSPENDED:
    connection->suspended_dummy = connection->suspended ? MHD_YES : MHD_NO;
    return (const union MHD_ConnectionInfo *) &connection->suspended_dummy;
  case MHD_CONNECTION_INFO_CONNECTION_TIMEOUT:
    connection->connection_timeout_dummy = (unsigned
                                            int) connection->connection_timeout;
    return (const union MHD_ConnectionInfo *) &connection->
           connection_timeout_dummy;
  case MHD_CONNECTION_INFO_REQUEST_HEADER_SIZE:
    if ( (MHD_CONNECTION_HEADERS_RECEIVED > connection->state) ||
         (MHD_CONNECTION_CLOSED == connection->state) )
      return NULL;   /* invalid, too early! */
    return (const union MHD_ConnectionInfo *) &connection->header_size;
  default:
    return NULL;
  }
}


/**
 * Set a custom option for the given connection, overriding defaults.
 *
 * @param connection connection to modify
 * @param option option to set
 * @param ... arguments to the option, depending on the option type
 * @return #MHD_YES on success, #MHD_NO if setting the option failed
 * @ingroup specialized
 */
enum MHD_Result
MHD_set_connection_option (struct MHD_Connection *connection,
                           enum MHD_CONNECTION_OPTION option,
                           ...)
{
  va_list ap;
  struct MHD_Daemon *daemon;

  daemon = connection->daemon;
  switch (option)
  {
  case MHD_CONNECTION_OPTION_TIMEOUT:
    if (0 == connection->connection_timeout)
      connection->last_activity = MHD_monotonic_sec_counter ();
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
    MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);
#endif
    if ( (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) &&
         (! connection->suspended) )
    {
      if (connection->connection_timeout == daemon->connection_timeout)
        XDLL_remove (daemon->normal_timeout_head,
                     daemon->normal_timeout_tail,
                     connection);
      else
        XDLL_remove (daemon->manual_timeout_head,
                     daemon->manual_timeout_tail,
                     connection);
    }
    va_start (ap, option);
    connection->connection_timeout = va_arg (ap,
                                             unsigned int);
    va_end (ap);
    if ( (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) &&
         (! connection->suspended) )
    {
      if (connection->connection_timeout == daemon->connection_timeout)
        XDLL_insert (daemon->normal_timeout_head,
                     daemon->normal_timeout_tail,
                     connection);
      else
        XDLL_insert (daemon->manual_timeout_head,
                     daemon->manual_timeout_tail,
                     connection);
    }
#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
    MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);
#endif
    return MHD_YES;
  default:
    return MHD_NO;
  }
}


/**
 * Queue a response to be transmitted to the client (as soon as
 * possible but after #MHD_AccessHandlerCallback returns).
 *
 * @param connection the connection identifying the client
 * @param status_code HTTP status code (i.e. #MHD_HTTP_OK)
 * @param response response to transmit
 * @return #MHD_NO on error (i.e. reply already sent),
 *         #MHD_YES on success or if message has been queued
 * @ingroup response
 */
enum MHD_Result
MHD_queue_response (struct MHD_Connection *connection,
                    unsigned int status_code,
                    struct MHD_Response *response)
{
  struct MHD_Daemon *daemon;

  if ( (NULL == connection) ||
       (NULL == response) ||
       (NULL != connection->response) ||
       ( (MHD_CONNECTION_HEADERS_PROCESSED != connection->state) &&
         (MHD_CONNECTION_FOOTERS_RECEIVED != connection->state) ) )
    return MHD_NO;
  daemon = connection->daemon;

  if (daemon->shutdown)
    return MHD_YES; /* If daemon was shut down in parallel,
                     * response will be aborted now or on later stage. */

#if defined(MHD_USE_POSIX_THREADS) || defined(MHD_USE_W32_THREADS)
  if ( (! connection->suspended) &&
       (0 != (daemon->options & MHD_USE_INTERNAL_POLLING_THREAD)) &&
       (! MHD_thread_ID_match_current_ (connection->pid)) )
  {
#ifdef HAVE_MESSAGES
    MHD_DLOG (daemon,
              _ ("Attempted to queue response on wrong thread!\n"));
#endif
    return MHD_NO;
  }
#endif
#ifdef UPGRADE_SUPPORT
  if ( (NULL != response->upgrade_handler) &&
       (0 == (daemon->options & MHD_ALLOW_UPGRADE)) )
  {
#ifdef HAVE_MESSAGES
    MHD_DLOG (daemon,
              _ (
                "Attempted 'upgrade' connection on daemon without MHD_ALLOW_UPGRADE option!\n"));
#endif
    return MHD_NO;
  }
  if ( (MHD_HTTP_SWITCHING_PROTOCOLS != status_code) &&
       (NULL != response->upgrade_handler) )
  {
#ifdef HAVE_MESSAGES
    MHD_DLOG (daemon,
              _ (
                "Application used invalid status code for 'upgrade' response!\n"));
#endif
    return MHD_NO;
  }
#endif /* UPGRADE_SUPPORT */
  MHD_increment_response_rc (response);
  connection->response = response;
  connection->responseCode = status_code;
#if defined(_MHD_HAVE_SENDFILE)
  if ( (response->fd == -1) ||
       (response->is_pipe) ||
       (0 != (connection->daemon->options & MHD_USE_TLS))
#if defined(MHD_SEND_SPIPE_SUPPRESS_NEEDED) && \
       defined(MHD_SEND_SPIPE_SUPPRESS_POSSIBLE)
       || (! daemon->sigpipe_blocked && ! connection->sk_spipe_suppress)
#endif /* MHD_SEND_SPIPE_SUPPRESS_NEEDED &&
          MHD_SEND_SPIPE_SUPPRESS_POSSIBLE */
       )
    connection->resp_sender = MHD_resp_sender_std;
  else
    connection->resp_sender = MHD_resp_sender_sendfile;
#endif /* _MHD_HAVE_SENDFILE */
  /* FIXME: if 'is_pipe' is set, TLS is off, and we have *splice*, we could use splice()
     to avoid two user-space copies... */

  if ( ( (NULL != connection->method) &&
         (MHD_str_equal_caseless_ (connection->method,
                                   MHD_HTTP_METHOD_HEAD)) ) ||
       (MHD_HTTP_OK > status_code) ||
       (MHD_HTTP_NO_CONTENT == status_code) ||
       (MHD_HTTP_NOT_MODIFIED == status_code) )
  {
    /* if this is a "HEAD" request, or a status code for
       which a body is not allowed, pretend that we
       have already sent the full message body. */
    connection->response_write_position = response->total_size;
  }
  if (MHD_CONNECTION_HEADERS_PROCESSED == connection->state)
  {
    /* response was queued "early", refuse to read body / footers or
       further requests! */
    connection->read_closed = true;
    connection->state = MHD_CONNECTION_FOOTERS_RECEIVED;
    connection->remaining_upload_size = 0;
  }
  if (! connection->in_idle)
    (void) MHD_connection_handle_idle (connection);
  MHD_update_last_activity_ (connection);
  return MHD_YES;
}


/* end of connection.c */
