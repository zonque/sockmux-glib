/*
 *  libsockmux - A socket muxer library
 *
 *    Copyright (C) 2011 Daniel Mack <sockmux@zonque.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include "src/sender.h"
#include "src/receiver.h"

static GMainLoop *loop;
static guint step = 0;
static SockMuxSender *sender = NULL;
static SockMuxReceiver *receiver = NULL;
static GChecksum *checksum;

static void trigger(void);

static void receiver_cb (SockMuxReceiver *rec,
                         guint message_id,
                         const guint8 *data,
                         guint size,
                         gpointer userdata)
{
  GChecksum *checksum2 = g_checksum_new(G_CHECKSUM_SHA1);

  if (userdata != rec)
    {
      g_error("userdata = %p, rec = %p", userdata, rec);
      g_main_loop_quit(loop);
      return;
    }

  if (message_id != step)
    {
      g_error("message_id = %d, step = %d", message_id, step);
      g_main_loop_quit(loop);
      return;
    }

  g_message("%s() - rec %p, message_id 0x%0x, size %d",
            __func__, rec, message_id, size);

  g_checksum_update(checksum2, data, size);
  if (!g_str_equal(g_checksum_get_string(checksum),
                   g_checksum_get_string(checksum2)))
    {
      g_error("Checksum mismatch! Expected '%s', got '%s'",
              g_checksum_get_string(checksum),
              g_checksum_get_string(checksum2));
      g_main_loop_quit(loop);
      return;
    }

  g_checksum_free(checksum2);
  trigger();
}

static void trigger(void)
{
  guint8 *data;
  gsize size, i;

  if (step++ < 2000)
    {
      size = g_random_int_range(0, 1024 * 24);
      data = g_malloc0(size);

      for (i = 0; i < size; i++)
        data[i] = g_random_int();

      g_checksum_reset(checksum);
      g_checksum_update(checksum, data, size);

      sockmux_receiver_connect(receiver, step, receiver_cb, receiver);
      sockmux_sender_send(sender, step, data, size);

      g_free(data);
    }
  else
    g_main_loop_quit(loop);
}

int main(int argc, char *argv[])
{
  gint ret, fds[2];
  GInputStream *input;
  GOutputStream *output;

  g_type_init();  
  ret = pipe(fds);

  if (ret < 0)
    {
      g_error("pipe() failed, ret = %d", ret);
		  exit(EXIT_FAILURE);
    }

  input = g_unix_input_stream_new(fds[0], TRUE);
  output = g_unix_output_stream_new(fds[1], TRUE);

  sender = sockmux_sender_new(output);
  if (sender == NULL)
    {
      g_error("sockmux_sender_new() failed");
		  exit(EXIT_FAILURE);
    }

  receiver = sockmux_receiver_new(input);
  if (receiver == NULL)
    {
      g_error("sockmux_receiver_new() failed");
		  exit(EXIT_FAILURE);
    }

  loop = g_main_loop_new(NULL, FALSE);
  checksum = g_checksum_new(G_CHECKSUM_SHA1);
  trigger();
  g_main_loop_run(loop);

  return EXIT_SUCCESS;
}
