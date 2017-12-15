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

======
rushit
======

``rushit`` is a network micro-benchmark tool that is scriptable with Lua. It
resembles well-known tools like `iperf <https://iperf.fr/>`_ or `netperf
<https://hewlettpackard.github.io/netperf/>`_. It originates from the `neper
<https://github.com/google/neper>`_ project.

The project aims for the sweet spot between a small C program that simulates a
network client/server and a fully featured network micro-benchmark. It provides
you with a basic multi-threaded epoll-based client/server program that is
intended to be extended, as needed, with Lua scripts.

``rushit`` can simulate the following network workloads:

* ``tcp_rr``, a request/response over TCP workload; simulates HTTP or RPC,
* ``tcp_stream``, a uni-/bi-directional bulk data transfer over TCP workload;
  simulates FTP or ``scp``,
* ``udp_stream``, a uni-directional bulk data transfer over UDP workload;
  simulates audio or video streaming.

How do I get started?
---------------------

Best place to start is the `project documentation
<http://rushit.readthedocs.io/en/latest/>`_ at Read the Docs.

:ref:`introduction` goes over basic usage and available command line options.

:ref:`script-examples` demonstrate what Lua scripting can be used for. Be sure
to check out the accompanying :ref:`script-api` documentation.

Don't know Lua?
---------------

Don't worry. See `Learn Lua in Y minutes
<https://learnxinyminutes.com/docs/lua/>`_ for a quick introduction.

Once you are hooked on Lua, take a look at these resources:

* `Lua for Programmers
  <http://nova-fusion.com/2012/08/27/lua-for-programmers-part-1/>`_ blog series
* `Programming in Lua <http://www.lua.org/pil/contents.html>`_ book
* `Lua 5.1 Reference Manual <http://www.lua.org/manual/5.1/manual.html>`_
* `The Lua language (v5.1)
  <http://lua-users.org/files/wiki_insecure/users/thomasl/luarefv51.pdf>`_ cheat
  sheet

Packages
--------

Fedora, CentOS: `In pabeni's copr repository
<https://copr.fedorainfracloud.org/coprs/pabeni/rushit/>`_ *(thanks Paolo!)*

Want to contribute?
-------------------

Great! Just create a `Pull Request
<https://github.com/rushit-tool/rushit/compare>`_. We follow these guidelines:

* C99, avoid compiler-specific extensions.
* `Linux kernel coding style
  <https://github.com/torvalds/linux/blob/master/Documentation/process/coding-style.rst>`_,
  with tabs expanded to 8 spaces.
