..
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

         http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

    Convention for heading levels in documentation:

    =======  Heading 0 (reserved for the title in a document)
    -------  Heading 1
    ~~~~~~~  Heading 2
    +++++++  Heading 3
    '''''''  Heading 4

.. TODO:
   * Move the docs to the source code.
   * Use a Sphinx domain for Lua?

.. highlight:: lua

.. _script-api:

==========
Script API
==========

This document describes the API available for use from within the Lua
script that can be passed on the command line using the ``--script``
option. Such script will be executed by the main thread that controls
the client/server network threads.

Typical script can be split into 4 parts:

1. initialize shared variables
2. register hook functions
3. trigger the test run
4. process collected data

Hooks
-----

Hooks are user-provided functions that allow you to "hook up" into
well-defined points of the client/server thread logic and execute a
custom script.

There are currently two types of hooks: :ref:`socket-hooks` and
:ref:`packet-hooks`.

.. _socket-hooks:

Socket Hooks
~~~~~~~~~~~~

Socket hooks are tied to socket events. Hooks are called once per each
socket that gets opened or closed by the client/server threads.

They are intended for configuring the socket (e.g. with
``setsockopt(2)``) or collecting the information about the socket
(e.g. with ``getsockopt(2)``).

.. note::

   In TCP workloads (``tcp_stream`` or ``tcp_rr``), server-side socket
   hooks operate on the *listening* socket, not the *connection*
   socket. This might change in the future.

.. c:type:: socket_hook_fn(sockfd, addr)

   User provided function invoked right *after* the socket has been
   opened (i.e. after the ``socket(2)`` call), or just *before* the
   socket will be closed (i.e. before the ``close(2)``).

   :param sockfd: Socket descriptor client/server thread uses to
                  ``read(2)`` / ``write(2)`` data.
   :type sockfd: int
   :param addr: Address used to ``bind(2)`` (for server threads) or
                ``connect(2)`` (for client threads) the socket.
   :type addr: struct addrinfo

.. c:function:: client_socket(socket_hook)
		server_socket(socket_hook)

   Registers a hook function to be invoked after the client/server
   thread opens a connection/listening socket with a ``socket(2)``
   call.

   :param socket_hook: Hook function invoked after ``socket(2)`` call.
   :type socket_hook: socket_hook_fn

.. c:function:: client_close(socket_hook)
		server_close(socket_hook)

   Registers a hook function to be invoked before the client/server
   thread closes a connection/listening socket with a ``close(2)``
   call.

   :param socket_hook: Hook function invoked before ``close(2)`` call.
   :type socket_hook: socket_hook_fn

.. _packet-hooks:

Packet Hooks
~~~~~~~~~~~~

Packet hooks are tied to the socket message queue and socket error
queue events. They can be used to implement a custom way to read from
or write to a socket within the client/server thread's main loop.

For TCP workloads (``tcp_stream`` and ``tcp_rr``), packet hooks always
operate on connection sockets.

.. c:type:: packet_hook_fn(sockfd, msg, flags)

   User provided function invoked when the socket's message queue (or
   error queue) is ready to read/write. The packet hook function is
   called *instead* of a ``read(2)`` / ``write(2)`` call.

   Packet hook function must return the number of bytes read/written
   or -1 in the event of an error. This is usually achieved by passing
   up the return value from either ``read()`` / ``recv()`` /
   ``recvfrom()`` / ``recvmsg()``, or ``write()`` / ``send()`` /
   ``sendto()`` / ``sendmsg()``.

   :param sockfd: Socket descriptor to read from or write to.
   :type sockfd: int
   :param msg: Message buffer to read data into or write data
              from. Buffer size is determined by command line option
              ``--buffer-size`` / ``-B`` (16 KiB or 16384 bytes by
              default). In case of reading from the error queue,
              ``msg`` also has a 512 byte control message buffer.
   :type msg: struct msghdr
   :param flags: ``MSG_*`` flags that should be passed to ``recv*()``
                 / ``send*()`` calls.
   :type flags: int
   :return: Number of bytes read/written or -1 in the event of an error.

.. c:function:: client_sendmsg(packet_hook)
		server_sendmsg(packet_hook)

   Registers a hook function to be invoked when a socket is ready for
   writting. i.e. on ``EPOLLOUT`` ``epoll(7)`` event.

   :param packet_hook: Hook function to write data to the socket.
   :type packet_hook: packet_hook_fn

.. c:function:: client_recvmsg(packet_hook)
		server_recvmsg(packet_hook)

   Registers a hook function to be invoked when a socket is ready for
   reading, i.e on ``EPOLLIN`` ``epoll(7)`` event.

   :param packet_hook: Hook function to read data from the socket.
   :type packet_hook: packet_hook_fn

.. c:function:: client_recverr(packet_hook)
		server_recverr(packet_hook)

   Registers a hook function to be invoked when socket's error queue
   is ready for reading, i.e. on ``EPOLLERR`` ``epoll(7)`` event.

   :param packet_hook: Hook function to read data from the socket
                       error queue.
   :type packet_hook: packet_hook_fn


Run Control
-----------

.. c:function:: run()

   Triggers the test run and waits for the client/server threads to
   finish.

   It is used to separate the first part of the script that needs to
   be executed before the network threads start running from the
   second part of the script that can be executed only when the network
   threads have stopped running.

   Before :c:func:`run()` returns it collects values of local
   variables that have been marked for collection from client/server
   threads. See :c:func:`collect`.

Data Passing
------------

.. c:function:: collect(value)

   Marks a value for collection from the client/server threads after
   the test run.

   Returns the given value wrapped in a table with metadata that
   identifies it for collection. Returned table should be treated as
   an opaque object until after the test run.

   The value will be automatically unwrapped (i.e. extracted from the
   table) when copied to the client/server thread.

   After the test run (i.e. a call to :c:func:`run`), the wrapper
   table will be populated with corresponding values from each
   client/server thread for access from outside of the hook functions.

   :return: Wrapped value that will be replaced by a table with values
            collected from client/server threads after the call to
            :c:func:`run`.

.. todo::

   Add link to an example.

Syscall Wrappers
----------------

Lua syscall wrappers are provided by the ljsyscall library. We provide
convenience aliases for symbols exported by ljsyscall so that the
symbol names are more C-like. That is::

  S = require("syscall")
  -- Aliases for syscalls
  recvmsg = S.recvmsg
  -- Aliases for constants
  AF_INET = S.c.AF.INET becomes
  -- Aliases for data types
  sockaddr_in = S.types.t.sockaddr_in

.. warning::

   Only a small set of symbols have aliases at the moment (see
   ``script_prelude.lua``). This will be resolved in the near
   future. In the meantime please access any symbol that is missing an
   alias via the ``S`` global variable.
