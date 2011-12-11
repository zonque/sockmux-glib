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

#ifndef _LIBSOCKMUX_GLIB_RECEIVER_H_
#define _LIBSOCKMUX_GLIB_RECEIVER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define SOCKMUX_RECEIVER_PROP_MAX_MESSAGE_SIZE "max-message-size"

typedef struct _SockMuxReceiver      SockMuxReceiver;
typedef struct _SockMuxReceiverClass SockMuxReceiverClass;

struct _SockMuxReceiverClass {
  GObjectClass parent_class;

  /* signals */
  void (* stream_end) (void);
  void (* message_dropped) (void);
  void (* protocol_error) (void);
};

typedef void (* SockMuxReceiverCallbackFunc) (SockMuxReceiver *receiver,
                                              guint message_id,
                                              const guint8 *data,
                                              guint size,
                                              gpointer userdata);

void sockmux_receiver_set_max_message_size (SockMuxReceiver *receiver,
                                            guint max_message_size);

void sockmux_receiver_connect (SockMuxReceiver *receiver,
                               guint message_id,
                               SockMuxReceiverCallbackFunc func,
                               gpointer userdata);

SockMuxReceiver *sockmux_receiver_new(GInputStream *stream,
                                      guint magic);

GType sockmux_receiver_get_type (void);
#define SOCKMUX_TYPE_RECEIVER             sockmux_receiver_get_type()
#define SOCKMUX_RECEIVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SOCKMUX_TYPE_RECEIVER, SockMuxReceiver))
#define SOCKMUX_RECEIVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SOCKMUX_TYPE_RECEIVER, SockMuxReceiverClass))
#define SOCKMUX_IS_RECEIVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SOCKMUX_TYPE_RECEIVER))
#define SOCKMUX_IS_RECEIVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SOCKMUX_TYPE_RECEIVER))
#define SOCKMUX_RECEIVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SOCKMUX_TYPE_RECEIVER, SockMuxReceiverClass))

G_END_DECLS

#endif /* _LIBSOCKMUX_GLIB_RECEIVER_H_ */
