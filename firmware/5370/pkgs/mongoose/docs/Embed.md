# Mongoose Embedding Guide

Embedding Mongoose is done in two steps:

   1. Copy
    [mongoose.c](https://raw.github.com/cesanta/mongoose/master/mongoose.c) and
    [mongoose.h](https://raw.github.com/cesanta/mongoose/master/mongoose.h)
    to your application's source tree and include them in the build.
   2. Somewhere in the application code, call `mg_create_server()` to create
    a server, configure it with `mg_set_option()` and loop with
    `mg_poll_server()` until done. Call `mg_destroy_server()` to cleanup.

Here's a minimal application `app.c` that embeds mongoose:

    #include "mongoose.h"

    int main(void) {
      struct mg_server *server = mg_create_server(NULL, NULL);
      mg_set_option(server, "document_root", ".");      // Serve current directory
      mg_set_option(server, "listening_port", "8080");  // Open port 8080
      for (;;) mg_poll_server(server, 1000);            // Infinite loop, Ctrl-C to stop
      mg_destroy_server(&server);
      return 0;
    }

To compile it, put `mongoose.c`, `mongoose.h` and `app.c` into one
folder, start terminal on UNIX or Visual Studio command line prompt on Windows,
and run the following command:

    cc app.c mongoose.c -pthread -o app    # on Unix
    cl app.c mongoose.c /TC /MD            # on Windows

When run, this simple application opens port 8080 and serves static files,
CGI files and lists directory content in the current working directory.

Mongoose can call user-defined functions when certain URIs are requested.
These functions are _called uri handlers_.  `mg_add_uri_handler()` registers
an URI handler, and there is no restriction exist on the number of URI handlers.
Also, mongoose can call a user-defined function when it is about to send
HTTP error back to client. That function is called _http error handler_ and
can be registered by `mg_set_http_error_handler()`. Handlers are called
by Mongoose with `struct mg_connection *` pointer as a parameter, which
has all information about the request: HTTP headers, POST or websocket
data buffer, etcetera.

Let's extend our minimal application example and
create an URI that will be served by user's C code. The app will handle
`/hello` URI by showing a hello message. So, when app is run,
http://127.0.0.1:8080/hello will say hello, and here's the code:

    #include <string.h>
    #include "mongoose.h"

    static int event_handler(struct mg_connection *conn, enum mg_event ev) {
      if (ev == MG_AUTH) {
        return MG_TRUE;   // Authorize all requests
      } else if (ev == MG_REQ_BEGIN) {
        mg_printf_data(conn, "%s", "Hello world");
        return MG_TRUE;   // Mark as processed
      } else {
        return MG_FALSE;  // Rest of the events are not processed
      }
    }

    int main(void) {
      struct mg_server *server = mg_create_server(NULL, event_handler);
      mg_set_option(server, "document_root", ".");
      mg_set_option(server, "listening_port", "8080");

      for (;;) {
        mg_poll_server(server, 1000);  // Infinite loop, Ctrl-C to stop
      }
      mg_destroy_server(&server);

      return 0;
    }

Below is the list of compilation flags that enable or disable certain
features. By default, some features are enabled, and could be disabled
by setting appropriate `NO_*` flag. Features that are disabled by default
could be enabled by setting appropriate `USE_*` flag. Bare bones Mongoose
is quite small, about 30 kilobytes of compiled x86 code. Each feature adds
a couple of kilobytes to the executable size, and also has some runtime penalty.

    -DMONGOOSE_NO_AUTH          Disable MD5 authorization support
    -DMONGOOSE_NO_CGI           Disable CGI support
    -DMONGOOSE_NO_DAV           Disable WebDAV support
                                (PUT, DELETE, MKCOL, PROPFIND methods)
    -DMONGOOSE_NO_DIRECTORY_LISTING  Disable directory listing
    -DMONGOOSE_NO_FILESYSTEM    Disables all file IO, serving from memory only
    -DMONGOOSE_NO_LOGGING       Disable access/error logging
    -DMONGOOSE_NO_THREADS
    -DMONGOOSE_NO_WEBSOCKET     Disable WebSocket support

    -DMONGOOSE_USE_IDLE_TIMEOUT_SECONDS=X Idle connection timeout, default is 30
    -DMONGOOSE_USE_IPV6         Enable IPv6 support
    -DMONGOOSE_USE_LUA          Enable Lua scripting
    -DMONGOOSE_USE_LUA_SQLITE3  Enable sqlite3 binding for Lua
    -DMONGOOSE_USE_SSL          Enable SSL
    -DMONGOOSE_USE_POST_SIZE_LIMIT=X      POST requests larger than X will be
                                          rejected, not set by default
    -DMONGOOSE_USE_EXTRA_HTTP_HEADERS=X   Append X to the HTTP headers
                                          for static files, empty by default
    -DMONGOOSE_USE_STACK_SIZE=X           Let mg_start_thread() use stack
                                          size X, default is OS default
    -DMONGOOSE_ENABLE_DEBUG     Enables debug messages on stdout, very noisy
    -DMONGOOSE_HEXDUMP=\"XXX\"  Enables hexdump of sent and received traffic
                                to the text files. XXX must be a prefix of the
                                IP address whose traffic must be hexdumped.

Mongoose source code contains a well-commented example code, listed below:

   * [hello.c](https://github.com/cesanta/mongoose/blob/master/examples/hello.c)
   a minimalistic hello world example
   * [post.c](https://github.com/cesanta/mongoose/blob/master/examples/post.c)
   shows how to handle form input
   * [upload.c](https://github.com/cesanta/mongoose/blob/master/examples/post.c)
   shows how to upload files
   * [websocket.c](https://github.com/cesanta/mongoose/blob/master/examples/websocket.c) demonstrates websocket usage
   * [auth.c](https://github.com/cesanta/mongoose/blob/master/examples/websocket.c) demonstrates API-controlled Digest authorization
