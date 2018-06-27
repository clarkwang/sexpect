# What's `sexpect`

[`Expect`][expect] is for Tcl. [`Expect.pm`][expect.pm] is for Perl.
[`Pexpect`][pexpect] is for Python.  What's for shells (bash, ksh, zsh, ...)?
`sexpect`!

Unlike [`Expect`][expect], [`Expect.pm`][expect.pm], [`Pexpect`][pexpect] or
other similar `Expect` implementations, `sexpect` is not bound to any specific
programming languages so it can be used with any languages which support running
external commands. Users who write shell scripts would love this because they
don't have to learn other languages just to use the *Expect* functionality.

Another interesting `sexpect` feature is that the spawned child process is
running in background. You can *attach* to and *detach* from it as needed (just
like [GNU screen][screen]).

# How to build

## The *Make* Way

    $ make

## The [*CMake*](https://cmake.org/) Way

    $ mkdir build; cd build; cmake ..; make
    
## Supported platforms                                                                
                                                                                      
Tested on:                                                                            
                                                                                      
* OpenWRT 15.05.1, ramips/mt7620 (on [Newifi Mini, or Lenovo Y1 v1][newifi])
* Debian Linux 9 (Stretch)                                                            
* macOS 10.13 (High Sierra)                                                           
* FreeBSD 11.1                                                                        
* Cygwin on Windows 10

  [newifi]: https://openwrt.org/toh/lenovo/lenovo_y1_v1
    
# Manual

See [doc/manual.txt](doc/manual.txt) .
    
# Examples

See the [examples/](https://github.com/clarkwang/sexpect/tree/master/examples) dir.

[expect]:    https://www.nist.gov/services-resources/software/expect
[expect.pm]: http://search.cpan.org/perldoc?Expect
[pexpect]:   https://pexpect.readthedocs.io/
[screen]: https://www.gnu.org/software/screen/
