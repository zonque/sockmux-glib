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
#include "sender.h"

#define PROTOCOL_VERSION 1

struct _SockMuxSender {
  GObject  parent;

  GOutputStream *output;
  GCancellable  *output_cancellable;
  GSList        *output_queue;
  guint          max_output_queue;
  guint          magic;
  GMutex        *mutex;
};

struct _SockMuxAsync {
  GByteArray *array;
  SockMuxSender *sender;
};

typedef struct _SockMuxAsync SockMuxAsync;

static GObjectClass *parent_class = NULL;

enum {
  SIGNAL_WRITE_ERROR,
  SIGNAL_STREAM_OVERFLOW,
  SIGNAL_LAST
};

static gint signals[SIGNAL_LAST];

enum {
  PROP_0,
  PROP_MAX_OUTPUT_QUEUE,
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
      case PROP_MAX_OUTPUT_QUEUE:
        g_value_set_int(value, sender->max_output_queue);
        break;

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
  SockMuxSender *sender = SOCKMUX_SENDER(object);
  g_return_if_fail(SOCKMUX_IS_SENDER(sender));

  switch (property_id)
    {
      case PROP_MAX_OUTPUT_QUEUE:
        sender->max_output_queue = g_value_get_int(value);
        break;

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
      g_critical("%s() %s", __func__, error->message);
      g_signal_emit(sender, signals[SIGNAL_WRITE_ERROR], 0);
      g_error_free(error);
    }
  else
    feed_output_stream(sender);
}

static void
async_write_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      data)
{
  gsize len;
  GError *error = NULL;
  SockMuxAsync *async;
  SockMuxSender *sender;

  /* FIXME: is there really no clean solution to cancel a pending async operation!? */
  if (g_output_stream_is_closing(G_OUTPUT_STREAM(source)) ||
      g_output_stream_is_closed(G_OUTPUT_STREAM(source)))
    return;

  async = data;
  sender = async->sender;
  g_return_if_fail(SOCKMUX_IS_SENDER(sender));

  len = g_output_stream_write_finish(sender->output, result, &error);
  if (len <= 0)
    {
      if (error == NULL)
        {
          /* write error? */
          g_critical("Sender write error (%p)", sender);
          return;
        }

      g_critical("%s() %s", __func__, error->message);
      g_error_free(error);
      g_signal_emit(sender, signals[SIGNAL_WRITE_ERROR], 0);
      return;
    }

  g_mutex_lock(sender->mutex);
  sender->output_queue = g_slist_remove(sender->output_queue, async);
  g_mutex_unlock(sender->mutex);

  g_byte_array_free(async->array, TRUE);
  g_object_unref(async->sender);
  g_free(async);

  g_output_stream_flush_async(sender->output,
                              G_PRIORITY_DEFAULT,
                              NULL,
                              async_flush_cb, sender);
}

static void
feed_output_stream (SockMuxSender *sender)
{
  SockMuxAsync *async = NULL;

  if (g_output_stream_has_pending(sender->output))
    return;

  g_mutex_lock(sender->mutex);
  if (sender->output_queue)
    {
      async = sender->output_queue->data;
      sender->output_queue = g_slist_remove(sender->output_queue, async);
    }
  g_mutex_unlock(sender->mutex);

  if (async == NULL)
    return;

  g_output_stream_write_async(sender->output,
                              async->array->data,
                              async->array->len,
                              G_PRIORITY_DEFAULT,
                              sender->output_cancellable,
                              async_write_cb, async);
}

static void
sockmux_sender_queue (SockMuxSender *sender,
                      gconstpointer  data1,
                      guint          size1,
                      gconstpointer  data2,
                      guint          size2)
{
  SockMuxAsync *async = g_new(SockMuxAsync, 1);
  async->sender = g_object_ref(sender);
  async->array = g_byte_array_sized_new(size1 + size2);
  g_byte_array_append(async->array, (guint8 *) data1, size1);
  if (data2)
    g_byte_array_append(async->array, (guint8 *) data2, size2);

  g_mutex_lock(sender->mutex);
  sender->output_queue = g_slist_append(sender->output_queue, async);
  g_mutex_unlock(sender->mutex);

  feed_output_stream(sender);
}

static guint
sockmux_sender_queue_size (SockMuxSender *sender)
{
  guint size = 0;
  GSList *iter;

  g_mutex_lock(sender->mutex);
  for (iter = sender->output_queue; iter; iter = iter->next)
    {
      SockMuxAsync *async = iter->data;
      size += async->array->len;
    }
  g_mutex_unlock(sender->mutex);

  return size;
}

static void
sockmux_sender_flush_queue (SockMuxSender *sender)
{
  GSList *iter;

  g_mutex_lock(sender->mutex);
  for (iter = sender->output_queue; iter; iter = iter->next)
    {
      SockMuxAsync *async = iter->data;
      g_byte_array_free(async->array, TRUE);
      g_object_unref(async->sender);
    }

  g_slist_free_full(sender->output_queue, g_free);
  sender->output_queue = NULL;
  g_mutex_unlock(sender->mutex);
}

void
sockmux_sender_send (SockMuxSender  *sender,
                     guint           message_id,
                     gconstpointer   data,
                     gsize           size)
{
  SockMuxMessage msg;
  g_return_if_fail(SOCKMUX_IS_SENDER(sender));

  if (sender->max_output_queue > 0 &&
      sockmux_sender_queue_size(sender) > sender->max_output_queue)
    {
      g_signal_emit(sender, signals[SIGNAL_STREAM_OVERFLOW], 0);
      return;
    }

  msg.magic = GUINT_TO_BE(sender->magic);
  msg.message_id = GUINT_TO_BE(message_id);
  msg.length = GUINT_TO_BE(size);
  sockmux_sender_queue(sender, (gconstpointer) &msg, sizeof(msg), data, size);
}

void
sockmux_sender_set_max_output_queue (SockMuxSender *sender,
                                     guint max_output_queue)
{
  g_return_if_fail(SOCKMUX_IS_SENDER(sender));
  sender->max_output_queue = max_output_queue;
}

void
sockmux_sender_reset (SockMuxSender *sender)
{
  g_return_if_fail(SOCKMUX_IS_SENDER(sender));
  sockmux_sender_flush_queue(sender);
  g_output_stream_flush(sender->output, NULL, NULL);
  g_output_stream_clear_pending(sender->output);
}

static void
sockmux_sender_init (SockMuxSender *sender)
{
  sender->output_cancellable = g_cancellable_new();
  sender->mutex = g_mutex_new();
}

SockMuxSender *sockmux_sender_new (GOutputStream *stream,
                                   guint magic)
{
  SockMuxSender *sender = g_object_new(SOCKMUX_TYPE_SENDER, NULL);
  SockMuxHandshake hs;

  sender->output = stream;
  sender->magic = magic;

  /* send protocol handshake */
  hs.magic = GUINT_TO_BE(sender->magic);
  hs.protocol_version = GUINT_TO_BE(PROTOCOL_VERSION);
  sockmux_sender_queue(sender, (gconstpointer) &hs, sizeof(hs), NULL, 0);

  return sender;
}

static void
sockmux_sender_finalize (GObject *object)
{
  SockMuxSender *sender = SOCKMUX_SENDER(object);

  sockmux_sender_flush_queue(sender);

  if (sender->output_cancellable)
    {
      g_object_unref(sender->output_cancellable);
      sender->output_cancellable = NULL;
    }

  if (sender->mutex)
    {
      g_mutex_free(sender->mutex);
      sender->mutex = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
sockmux_sender_class_init (SockMuxSenderClass *klass)
{
  GObjectClass *object_class;
  GParamSpec *pspec;

  parent_class = (GObjectClass *) g_type_class_peek_parent (klass);
  object_class = (GObjectClass *) klass;

  object_class->get_property = sockmux_sender_get_property;
  object_class->set_property = sockmux_sender_set_property;
  object_class->finalize = sockmux_sender_finalize;

  pspec = g_param_spec_int(SOCKMUX_SENDER_PROP_MAX_OUTPUT_QUEUE,
                           "The maximum length of the output queue",
                           "Get the number",
                           0, G_MAXINT, 0,
                           G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_MAX_OUTPUT_QUEUE, pspec);

  signals[SIGNAL_WRITE_ERROR] =
    g_signal_new ("write-error",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  signals[SIGNAL_STREAM_OVERFLOW] =
    g_signal_new ("stream-overflow",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

G_DEFINE_TYPE (SockMuxSender, sockmux_sender, G_TYPE_OBJECT)

