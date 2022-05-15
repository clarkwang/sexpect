# What's `sexpect`

`sexpect` is another implementation of [`Expect`][expect] which is
specifically designed for *Shell* scripts (sh, bash, ksh, zsh, ...).

`sexpect` is designed in the client/server model.
`sexpect spawn PROGRAM [option]..` starts the _server_ (running as a daemon)
and runs the specified _PROGRAM_ in background.
Other `sexpect` sub-commands (`send`, `expect`, `wait`, ...) communicate to the
server as _client_ commands.

# How to build

Sexpect uses [CMake](https://cmake.org/) for building.

    $ cd /path/to/cloned/sexpect/
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ make install
    
By default it will install to `/usr/local/` and `sexpect` will be installed to `/usr/local/bin/`. To change the installation location, run `cmake` like this:

    $ cmake -D CMAKE_INSTALL_PREFIX=/opt/sexpect  ..

## Supported platforms                                                                
                                                                                      
Tested on:                                                                            
                                                                                      
* OpenWRT 15.05.1, ramips/mt7620 (on [Newifi Mini, or Lenovo Y1 v1][newifi])
* Debian Linux 9 (Stretch)                                                            
* macOS 10.13 (High Sierra)                                                           
* FreeBSD 11.1                                                                        
* Cygwin on Windows 10

  [newifi]: https://openwrt.org/toh/lenovo/lenovo_y1_v1
    
# Manual

See [doc/sexpect.adoc](doc/sexpect.adoc).
    
# Examples

See the [examples/](examples/) dir.

[expect]:    https://www.nist.gov/services-resources/software/expect
[expect.pm]: http://search.cpan.org/perldoc?Expect
[pexpect]:   https://pexpect.readthedocs.io/
