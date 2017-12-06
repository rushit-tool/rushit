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

``rushit`` is a fork of Google's ``neper`` networking performance testing
tool that aims to add Lua scripting support to it. In a word, the goal
is to be able to script the tool instead of extend it with new options
for things like setting socket options with ``setsockopt(2)``.

This project is a work in progress. The scripting functionality is
still under development and the documentation is missing.

The excellent original project's README has been preserved in :doc:`README.neper
<README.neper>`. You might want to take a look at it in the meantime.

Why the fork?
-------------

``neper`` project is neatly structured and has a compact code
base. Embedding Lua scripting engine in it will perturb the code base
quite a bit and likely compromise the goal of the original
project. Hence, forking and renaming it seems appropriate.
