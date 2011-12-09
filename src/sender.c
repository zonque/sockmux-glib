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

#include "message.h"
#include "sender.h"

#define PROTOCOL_VERSION 1

struct _SockMuxSender {
  GObject  parent;

  gint fd;
  GOutputStream *output;
  GByteArray    *output_buf;
  GCancellable  *output_cancellable;  
};

static GObjectClass *parent_class = NULL;

enum {
  SIGNAL_WRITE_ERROR,
  SIGNAL_LAST
};

static gint signals[SIGNAL_LAST];

enum {
  PROP_0,
};

static void
sockmux_sender_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  SockMuxSender *sender = SOCKMUX_SENDER(object);

  g_return_if_fail(SOCKMUX_IS_SENDER(sender));

  switch (property_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
sockmux_sender_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  switch (property_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
feed_output_stream (SockMuxSender *sender);

static void
async_flush_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      data)
{
  GError *error = NULL;

  if (g_output_stream_is_closing(G_OUTPUT_STREAM(source)) ||
      g_output_stream_is_closed(G_OUTPUT_STREAM(source)))
    return;

  SockMuxSender *sender = SOCKMUX_SENDER(data);
  g_return_if_fail(SOCKMUX_IS_SENDER(sender));

  g_output_stream_flush_finish(G_OUTPUT_STREAM(source), result, &error);

  if (error)
    {
      g_error("%s() %s\n", __func__, error->message);
      g_error_free(error);
    }

  feed_output_stream(sender);
}

static void
async_write_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      data)
{
  gsize len;
  GError *error = NULL;
  SockMuxSender *sender;
 
  /* FIXME: is there really no clean solution to cancel a pending async operation!? */
  if (g_output_stream_is_closing(G_OUTPUT_STREAM(source)) ||
      g_output_stream_is_closed(G_OUTPUT_STREAM(source)))
    return;
  
  sender = SOCKMUX_SENDER(data);
  g_return_if_fail(SOCKMUX_IS_SENDER(sender));

  len = g_output_stream_write_finish(sender->output, result, &error);
  if (len <= 0)
    {
      if (error == NULL)
        {
          /* write error? */
          g_critical ("Sender write error (%p)", sender);
          g_signal_emit(sender, signals[SIGNAL_WRITE_ERROR], 0);
          return;
        }
      
      g_error("%s() %s\n", __func__, error->message);
      g_error_free(error);
      return;
    }

  g_byte_array_remove_range(sender->output_buf, 0, len);
  g_output_stream_flush_async(sender->output,
                              G_PRIORITY_DEFAULT,
                              NULL,
                              async_flush_cb, sender);
}

static void
feed_output_stream (SockMuxSender *sender)
{
  if (sender->output_buf->len == 0 ||
      g_output_stream_has_pending(sender->output))
    return;

  g_output_stream_write_async(sender->output,
                              sender->output_buf->data,
                              sender->output_buf->len,
                              G_PRIORITY_DEFAULT,
                              sender->output_cancellable,
                              async_write_cb, sender);
}

void
sockmux_sender_send (SockMuxSender  *sender,
                     guint           message_id,
                     const guint8   *data,
                     gsize           size)
{
  SockMuxMessage msg;
  g_return_if_fail(SOCKMUX_IS_SENDER(sender));

  msg.magic = GUINT_TO_BE(SOCKMUX_PROTOCOL_MAGIC);
  msg.message_id = GUINT_TO_BE(message_id);
  msg.length = GUINT_TO_BE(size);
  g_byte_array_append(sender->output_buf, (guchar *) &msg, sizeof(msg));
  g_byte_array_append(sender->output_buf, data, size);

  feed_output_stream(sender);
}

static void
sockmux_sender_init (SockMuxSender *sender)
{
}

SockMuxSender *sockmux_sender_new (GOutputStream *stream)
{
  SockMuxSender *sender = g_object_new(SOCKMUX_TYPE_SENDER, NULL);
  SockMuxHandshake hs;
  
  sender->output = stream;
  sender->output_cancellable = g_cancellable_new();
  sender->output_buf = g_byte_array_new();

  /* send protocol handshake */
  hs.magic = GUINT_TO_BE(SOCKMUX_PROTOCOL_MAGIC);
  hs.handshake_magic = GUINT_TO_BE(SOCKMUX_PROTOCOL_HANDSHAKE_MAGIC);
  hs.protocol_version = GUINT_TO_BE(PROTOCOL_VERSION);
  g_byte_array_append(sender->output_buf, &hs, sizeof(hs));
  feed_output_stream(sender);

  return sender;
}

static void
sockmux_sender_finalize (GObject *object)
{
  SockMuxSender *sender = SOCKMUX_SENDER(object);

  g_byte_array_free(sender->output_buf, TRUE);
  sender->output_buf = NULL;

  if (sender->output_cancellable)
    {
      g_object_unref(sender->output_cancellable);
      sender->output_cancellable = NULL;
    }
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
sockmux_sender_class_init (SockMuxSenderClass *klass)
{
  GObjectClass *object_class;

  parent_class = (GObjectClass *) g_type_class_peek_parent (klass);
  object_class = (GObjectClass *) klass;

  object_class->get_property = sockmux_sender_get_property;
  object_class->set_property = sockmux_sender_set_property;
  object_class->finalize = sockmux_sender_finalize;

  signals[SIGNAL_WRITE_ERROR] =
    g_signal_new ("write-error",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

G_DEFINE_TYPE (SockMuxSender, sockmux_sender, G_TYPE_OBJECT)

