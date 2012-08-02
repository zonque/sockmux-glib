# Introduction

libsockmux specifies a simple way for muxing messages and data streams
onto a single socket for network and local communication streams.

libsockmux-glib is an implementation of this protocol base using GIO
data types and uses glib functions internally. The interface offered
to applications is very simple and straight forward.

The protocol uses a 32-bit magic for consitency checks which must be
provided when setting up the sender and the receiver.

# License

> This program is free software; you can redistribute it and/or modify
> it under the terms of the GNU Lesser General Public License as
> published by the Free Software Foundation; either version 2 of the
> License, or (at your option) any later version.
> 
> This program is distributed in the hope that it will be useful,
> but WITHOUT ANY WARRANTY; without even the implied warranty of
> MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
> GNU Lesser General Public License for more details.

#Examples

Examples usually explain things best, so here we go.

##Receiver example

Let's start with the receiver's side, and assume you have a GInputStream
already. Most probably, this would come from a set up TCP or socket
connection. The library doesn't care, as long as you provide a GInputStream,
or one of its subclasses:
    
>     +----GInputStream
>             +----GFilterInputStream
>             +----GFileInputStream
>             +----GMemoryInputStream
>             +----GUnixInputStream

The magic passed to create should be the same on both sides, of course.

    #include <glib.h>
    #include <gio/gio.h>
    #include <sockmux-glib/receiver.h>

    static SockMuxReceiver *receiver;

    #define MAGIC 0x12345678
    
    /* 
     * This message is called for all received message IDs
     * that it has been registered to.
     */

    static void message_callback(SockMuxReceiver *receiver,
                                 guint message_id,
                                 const guint8 *data,
                                 guint size,
                                 gpointer userdata)
    {
      g_message("Callback for message %08x received, size = %d!", message_id, size);
    }

    void start(GInputStream *input)
    {
      /* set up the receiver, passing the input stream */
      receiver = sockmux_receiver_new(input, MAGIC);

      /* register our callback function for message dispatching */
      sockmux_receiver_connect(receiver, message_callback, NULL);

      /* That's it. Now enter the run loop and wait for messages to arrive. */
    }

##Sender example

The sender's side is as easy. Here, we assume you have a GOutputStream,
or an instance of one of the subclasses:

>     +----GOutputStream
>           +----GFilterOutputStream
>           +----GFileOutputStream
>           +----GMemoryOutputStream
>           +----GUnixOutputStream

    #include <glib.h>
    #include <gio/gio.h>
    #include <sockmux-glib/sender.h>

    static SockMuxSender *sender;

    #define MAGIC 0x12345678
    
    void start(GOutputStream *output)
    {
      /* set up the sender, passing the output stream */
      sender = sockmux_sender_new(output, MAGIC);

      /* send some data. The receiver example above should catch it */
      sockmux_sender_send(sender, 0x2342, "Hello world", strlen("Hello world"));
    }

