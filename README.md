# What's *sexpect*

`sexpect` is another implementation of `Expect` (`s` is for either *simple* or
*super*).

Unlike `Expect` (Tcl), `Expect.pm` (Perl), `Pexpect` (Python) or other similar
`Expect` implementations, `sexpect` is not bound to any specific programming
language so it can be used with any languages which support running external
commands.
    
Another interesting `sexpect` feature is that the spawned child process is
running in background. You can *attach* to and *detach* from it as needed.

# How to build

## The *Make* Way

    $ make

## The [*CMake*](https://cmake.org/) Way

    $ mkdir build; cd build; cmake ..; make
    
# Manual

See [doc/manual.md](doc/manual.md).
    
# Examples

See the [*examples/*](https://github.com/clarkwang/sexpect/tree/master/examples) dir.
