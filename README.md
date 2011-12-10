# Introduction

libsockmux is a simple library for muxing data streams on a message base
onto a single socket using GIO data types. The interface offered to
applications is simple and straight forward.

The code is licensed under the LGPL v2.

It should be easy to port the protocol to more languages and environments.

An example usually explains things best, so here we go.

##Receiver example

Let's start with the receiver's side, and assume you have a GInputStream
already. Most probably, this would come from a set up TCP or socket
connection. The library doesn't care, as long as you provide a GInputStream,
or one of its subclasses:
    
    +----GInputStream
            +----GFilterInputStream
            +----GFileInputStream
            +----GMemoryInputStream
            +----GUnixInputStream

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
                                 gconstpointer data,
                                 guint size,
                                 gpointer userdata)
    {
      g_message("Callback for message %08x received, size = %d!", message_id, size);
    }

    void start(GInputStream *input)
    {
      /* set up the receiver, passing the input stream */
      receiver = sockmux_receiver_new(input, MAGIC);

      /* register a message id and connect it to our callback function */
      sockmux_receiver_connect(receiver, 0x2342, message_callback, NULL);

      /* That's it. Now enter the run loop and wait for messages to arrive. */
    }

##Sender example

The sender's side is as easy. Here, we assume you have a GOutputStream,
or an instance of one of the subclasses:

    +----GOutputStream
          +----GFilterOutputStream
          +----GFileOutputStream
          +----GMemoryOutputStream
          +----GUnixOutputStream

Make sure you pass the same magic again.

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

