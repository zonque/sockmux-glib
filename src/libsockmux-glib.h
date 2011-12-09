/*
 * libsockmux - A socket muxer library
 *
 *   Copyright (C) 2011 Daniel Mack <sockmux@zonque.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifndef _LIBSOCKMUX_GLIB_H_
#define _LIBSOCKMUX_GLIB_H_

struct _SockMuxMessage {
  guint32 message_id;
  guint32 length;
  guchar data[0];
} __attribute__((packed));

typedef struct _SockMuxMessage SockMuxMessage;

#include <libsockmux/sender.h>
#include <libsockmux/receiver.h>

#endif /* _LIBSOCKMUX_GLIB_H_ */
