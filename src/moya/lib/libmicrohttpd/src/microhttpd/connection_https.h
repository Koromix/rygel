/*
     This file is part of libmicrohttpd
     Copyright (C) 2008 Daniel Pittman and Christian Grothoff

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
 * @file connection_https.h
 * @brief  Methods for managing connections
 * @author Christian Grothoff
 */

#ifndef CONNECTION_HTTPS_H
#define CONNECTION_HTTPS_H

#include "internal.h"

#ifdef HTTPS_SUPPORT
/**
 * Set connection callback function to be used through out
 * the processing of this secure connection.
 *
 * @param connection which callbacks should be modified
 */
void 
MHD_set_https_callbacks (struct MHD_Connection *connection);


/**
 * Give gnuTLS chance to work on the TLS handshake.
 *
 * @param connection connection to handshake on
 * @return true if the handshake has completed successfully
 *         and we should start to read/write data,
 *         false is handshake in progress or in case
 *         of error
 */
bool
MHD_run_tls_handshake_ (struct MHD_Connection *connection);


/**
 * Initiate shutdown of TLS layer of connection.
 *
 * @param connection to use
 * @return true if succeed, false otherwise.
 */
bool
MHD_tls_connection_shutdown (struct MHD_Connection *connection);
#endif /* HTTPS_SUPPORT */

#endif
