# What's `sexpect`

`sexpect` is another implementation of [`Expect`][expect] (`s` is for either *simple* or
*super*).

Unlike [`Expect`][expect] (Tcl), [`Expect.pm`][expect.pm] (Perl),
[`Pexpect`][pexpect] (Python) or other similar
`Expect` implementations, `sexpect` is not bound to any specific programming
languages so it can be used with any languages which support running external
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
    
## Supported platforms                                                                
                                                                                      
Tested on:                                                                            
                                                                                      
* OpenWRT 15.05.1, ramips/mt7620 (on [Newifi Mini, or Lenovo Y1 v1][newifi])
* Debian Linux 9 (Stretch)                                                            
* macOS 10.13 (High Sierra)                                                           
* FreeBSD 11.1                                                                        
* Cygwin on Windows 10

  [newifi]: https://wiki.openwrt.org/toh/lenovo/lenovo_y1_v1
    
# Manual

See [doc/manual.txt](doc/manual.txt) .
    
# Examples

See the [examples/](https://github.com/clarkwang/sexpect/tree/master/examples) dir.

[expect]:    https://www.nist.gov/services-resources/software/expect
[expect.pm]: http://search.cpan.org/perldoc?Expect
[pexpect]:   https://pexpect.readthedocs.io/
