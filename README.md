# rushit

`rushit` is a fork of Google's `neper` networking performance testing
tool that aims to add Lua scripting support to it. In a word, the goal
is to be able to script the tool instead of extend it with new options
for things like setting socket options with `setsockopt(2)`.

This project is a work in progress. The scripting functionality is
still under development and the documentation is missing.

The excellent original project's README has been preserved in
[README.neper.md](README.neper.md). You might want to take a look at
it in the meantime.

## Why the fork?

`neper` project is neatly structured and has a compact code
base. Embedding Lua scripting engine in it will perturb the code base
quite a bit and likely compromise the goal of the original
project. Hence, forking and renaming it seems appropriate.
