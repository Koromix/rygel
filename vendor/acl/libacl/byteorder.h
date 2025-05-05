/*
  Copyright (C) 2000, 2002  Andreas Gruenbacher <agruen@suse.de>

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

#ifdef WORDS_BIGENDIAN
# define cpu_to_le16(w16) le16_to_cpu(w16)
# define le16_to_cpu(w16) ((u_int16_t)((u_int16_t)(w16) >> 8) | \
                           (u_int16_t)((u_int16_t)(w16) << 8))
# define cpu_to_le32(w32) le32_to_cpu(w32)
# define le32_to_cpu(w32) ((u_int32_t)( (u_int32_t)(w32) >>24) | \
                           (u_int32_t)(((u_int32_t)(w32) >> 8) & 0xFF00) | \
                           (u_int32_t)(((u_int32_t)(w32) << 8) & 0xFF0000) | \
			   (u_int32_t)( (u_int32_t)(w32) <<24))
#else
# define cpu_to_le16(w16) ((u_int16_t)(w16))
# define le16_to_cpu(w16) ((u_int16_t)(w16))
# define cpu_to_le32(w32) ((u_int32_t)(w32))
# define le32_to_cpu(w32) ((u_int32_t)(w32))
#endif

