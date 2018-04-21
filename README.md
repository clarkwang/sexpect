# What's `Expect`

According to [wikipedia](https://en.wikipedia.org/wiki/Expect):

> [Expect][expect], an extension to the Tcl scripting language written by
[Don Libes][don], is a program to automate
interactions with programs that expose a text terminal interface. `Expect` was
originally written in 1990 for Unix systems, but since became available for
Microsoft Windows and other systems.

A very interesting paper (MUST READ!) written by Don Libes: [Writing a Tcl Extension in Only Seven Years][7y]

[don]: https://en.wikipedia.org/wiki/Don_Libes
[7y]:  https://ws680.nist.gov/publication/get_pdf.cfm?pub_id=821282

# What's `sexpect`

`sexpect` is another implementation of [`Expect`][expect] (`s` is for either *simple* or
*super*).

Unlike [`Expect`][expect] (Tcl), [`Expect.pm`][expect.pm] (Perl),
[`Pexpect`][pexpect] (Python) or other similar
`Expect` implementations, `sexpect` is not bound to any specific programming
language so it can be used with any languages which support running external
commands. Users who write shell (like `Bash`) scripts would love this because
they don't have to learn other languages just to use `Expect` or the *Expect*
module.

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

See the [examples/](https://github.com/clarkwang/sexpect/tree/master/examples) dir.

[expect]:    https://www.nist.gov/services-resources/software/expect
[expect.pm]: http://search.cpan.org/perldoc?Expect
[pexpect]:   https://pexpect.readthedocs.io/
