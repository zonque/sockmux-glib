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

#ifndef _LIBSOCKMUX_GLIB_SENDER_H_
#define _LIBSOCKMUX_GLIB_SENDER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define SOCKMUX_SENDER_PROP_MAX_OUTPUT_QUEUE "max-output-queue"

typedef struct _SockMuxSender      SockMuxSender;
typedef struct _SockMuxSenderClass SockMuxSenderClass;

struct _SockMuxSenderClass {
  GObjectClass parent_class;

  /* signals */
  void (* stream_end) (void);
  void (* stream_overflow) (void);
};

void sockmux_sender_send (SockMuxSender  *sender,
                          guint           message_id,
                          const guint8   *data,
                          gsize           size);

void sockmux_sender_set_max_output_queue (SockMuxSender *sender,
                                          guint max_output_queue);

void sockmux_sender_reset (SockMuxSender *sender);

SockMuxSender *sockmux_sender_new(GOutputStream *stream);

GType sockmux_sender_get_type (void);
#define SOCKMUX_TYPE_SENDER             sockmux_sender_get_type()
#define SOCKMUX_SENDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SOCKMUX_TYPE_SENDER, SockMuxSender))
#define SOCKMUX_SENDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SOCKMUX_TYPE_SENDER, SockMuxSenderClass))
#define SOCKMUX_IS_SENDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SOCKMUX_TYPE_SENDER))
#define SOCKMUX_IS_SENDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SOCKMUX_TYPE_SENDER))
#define SOCKMUX_SENDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SOCKMUX_TYPE_SENDER, SockMuxSenderClass))

G_END_DECLS

#endif /* _LIBSOCKMUX_GLIB_SENDER_H_ */
