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

#include <glib.h>
#include <gio/gio.h>

#include "protocol.h"
#include "receiver.h"

struct _SockMuxReceiverCallback {
  guint message_id;
  SockMuxReceiverCallbackFunc func;
  gpointer userdata;
};

typedef struct _SockMuxReceiverCallback SockMuxReceiverCallback;

struct _SockMuxReceiver {
  GObject  parent;

  GInputStream  *input;
  GByteArray    *input_buf;
  guchar         input_read_buffer[8192];
  GCancellable  *input_cancellable;
  gboolean       handshake_received;
  guint          magic;
  guint          protocol_version;

  GSList        *callbacks;
  guint          max_message_size;
  guint          skip;
};

static GObjectClass *parent_class = NULL;

enum {
  SIGNAL_PROTOCOL_ERROR,
  SIGNAL_STREAM_END,
  SIGNAL_MESSAGE_DROPPED,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];    

enum {
  PROP_0,
  PROP_MAX_MESSAGE_SIZE,
};

static void
sockmux_receiver_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  SockMuxReceiver *receiver = SOCKMUX_RECEIVER(object);
  g_return_if_fail(SOCKMUX_IS_RECEIVER(receiver));

  switch (property_id)
    {
      case PROP_MAX_MESSAGE_SIZE:
        g_value_set_int(value, receiver->max_message_size);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
sockmux_receiver_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  SockMuxReceiver *receiver = SOCKMUX_RECEIVER(object);
  g_return_if_fail(SOCKMUX_IS_RECEIVER(receiver));

  switch (property_id)
    {
      case PROP_MAX_MESSAGE_SIZE:
        receiver->max_message_size = g_value_get_int(value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static gint
dispatch_message (SockMuxReceiver *receiver)
{
  SockMuxMessage *msg;
  guint32 msg_len, msg_id;
  guint available_len;
  GSList *iter;

  msg = (SockMuxMessage *) receiver->input_buf->data;
  available_len = receiver->input_buf->len;

  if (available_len < sizeof(*msg))
    return 0;

  if (GUINT_FROM_BE(msg->magic) != receiver->magic)
    {
      g_signal_emit(receiver, signals[SIGNAL_PROTOCOL_ERROR], 0);
      return 0;
    }

  msg_len = GUINT_FROM_BE(msg->length);
  msg_id = GUINT_FROM_BE(msg->message_id);

  if (receiver->max_message_size > 0 &&
      msg_len > receiver->max_message_size)
    {
      g_signal_emit(receiver, signals[SIGNAL_MESSAGE_DROPPED], 0);
      receiver->skip = msg_len;

      return G_UNLIKELY(msg_len > available_len) ? available_len : msg_len;
    }

  if (available_len < msg_len + sizeof(*msg))
    return 0;

  /* walk the list of callbacks and see if anyone is interessted */
  for (iter = receiver->callbacks; iter; iter = iter->next)
    {
      SockMuxReceiverCallback *cb = iter->data;

      if (cb->message_id == msg_id)
        cb->func(receiver, msg_id, msg->data, msg_len, cb->userdata);
    }

  return msg_len + sizeof(*msg);
}

static void
dispatch_input (SockMuxReceiver *receiver)
{
  gint len;

  if (G_UNLIKELY(!receiver->handshake_received))
    {
      SockMuxHandshake *hs = (SockMuxHandshake *) receiver->input_buf->data;
      if (GUINT_FROM_BE(hs->magic) != receiver->magic)
        {
          receiver->protocol_version = GUINT_FROM_BE(hs->protocol_version);
          g_byte_array_remove_range(receiver->input_buf, 0, sizeof(*hs));
        }
      else
        {
          g_signal_emit(receiver, signals[SIGNAL_PROTOCOL_ERROR], 0);
          return;
        }

      receiver->handshake_received = TRUE;
    }

  while ((len = dispatch_message(receiver)))
    {
      g_byte_array_remove_range(receiver->input_buf, 0, len);
    }
}

static void
async_read_cb (GObject *source,
               GAsyncResult *result,
               gpointer data)
{
  gsize len;
  GError *error = NULL;
  SockMuxReceiver *receiver;
  guint offset = 0;
 
  /* FIXME: is there really no clean solution to cancel a pending async operation!? */
  if (g_input_stream_is_closed(G_INPUT_STREAM(source)))
    return;

  receiver = SOCKMUX_RECEIVER(data);
  g_return_if_fail(SOCKMUX_IS_RECEIVER(receiver));

  len = g_input_stream_read_finish (receiver->input, result, &error);
  if (len <= 0)
    {
      if (error == NULL)
        {
          /* stream ended */
          g_signal_emit(receiver, signals[SIGNAL_STREAM_END], 0);
          return;
        }

      g_error("%s(): %s", __func__, error->message);
      g_error_free (error);
      return;
    }

  if (receiver->skip > 0)
    {
      if (receiver->skip >= len)
        {
          receiver->skip -= len;
          len = 0;
        }
      else
        {
          offset = receiver->skip;
          len -= receiver->skip;
          receiver->skip = 0;
        }
    }

  if (len > 0)
    {
      g_byte_array_append(receiver->input_buf, receiver->input_read_buffer + offset, len);
      dispatch_input(receiver);
    }

  g_input_stream_read_async(receiver->input,
                            receiver->input_read_buffer,
                            sizeof(receiver->input_read_buffer),
                            G_PRIORITY_DEFAULT,
                            receiver->input_cancellable,
                            async_read_cb, receiver);
}

static void
sockmux_receiver_init (SockMuxReceiver *receiver)
{
}

void sockmux_receiver_connect (SockMuxReceiver *receiver,
                               guint message_id,
                               SockMuxReceiverCallbackFunc func,
                               gpointer userdata)
{
  SockMuxReceiverCallback *cb;
 
  g_return_if_fail(SOCKMUX_IS_RECEIVER(receiver));
  g_return_if_fail(func != NULL);

  cb = g_new0(SockMuxReceiverCallback, 1);

  if (cb == NULL)
    {
      g_error("Unable to allocate memory!?");
      return;
    }

  cb->message_id = message_id;
  cb->func = func;
  cb->userdata = userdata;

  receiver->callbacks = g_slist_append(receiver->callbacks, cb);
}

void sockmux_receiver_set_max_message_size (SockMuxReceiver *receiver,
                                            guint max_message_size)
{
  g_return_if_fail(SOCKMUX_IS_RECEIVER(receiver));
  receiver->max_message_size = max_message_size;
}

SockMuxReceiver *sockmux_receiver_new (GInputStream *stream,
                                       guint magic)
{
  SockMuxReceiver *receiver = g_object_new(SOCKMUX_TYPE_RECEIVER, NULL);

  receiver->input = stream;
  receiver->magic = magic;
  receiver->input_buf = g_byte_array_new();
  receiver->input_cancellable = g_cancellable_new();
  
  g_input_stream_read_async(receiver->input,
                            receiver->input_read_buffer,
                            sizeof(receiver->input_read_buffer),
                            G_PRIORITY_DEFAULT,
                            receiver->input_cancellable,
                            async_read_cb, receiver);

  return receiver;
}

static void
sockmux_receiver_finalize (GObject *object)
{
  SockMuxReceiver *receiver = SOCKMUX_RECEIVER(object);

  if (receiver->input_cancellable)
    g_cancellable_cancel(receiver->input_cancellable);
  
  g_byte_array_free(receiver->input_buf, TRUE);
  receiver->input_buf = NULL;

  g_slist_free_full(receiver->callbacks, g_free);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
sockmux_receiver_class_init (SockMuxReceiverClass *klass)
{
  GObjectClass *object_class;
  GParamSpec *pspec;

  parent_class = (GObjectClass *) g_type_class_peek_parent (klass);
  object_class = (GObjectClass *) klass;

  object_class->get_property = sockmux_receiver_get_property;
  object_class->set_property = sockmux_receiver_set_property;
  object_class->finalize = sockmux_receiver_finalize;

  pspec = g_param_spec_int(SOCKMUX_RECEIVER_PROP_MAX_MESSAGE_SIZE,
                           "The maximum length of an incoming message",
                           "Get the number",
                           0, G_MAXINT, 0,
                           G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_MAX_MESSAGE_SIZE, pspec);

  signals[SIGNAL_STREAM_END] =
    g_signal_new ("stream-end",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  signals[SIGNAL_PROTOCOL_ERROR] =
    g_signal_new ("protocol-error",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  signals[SIGNAL_MESSAGE_DROPPED] =
    g_signal_new ("message-dropped",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

G_DEFINE_TYPE (SockMuxReceiver, sockmux_receiver, G_TYPE_OBJECT)

