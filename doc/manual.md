# How to build

## The *Make* Way

    $ make

## The [*CMake*](https://cmake.org/) Way

    $ mkdir build; cd build; cmake ..; make
    
# Examples

See the [*examples*](https://github.com/clarkwang/sexpect/tree/master/examples) dir.
    
# Brief Manual

    USAGE:
      sexpect [OPTION] [SUB-COMMAND]
    
    DESCRIPTION:
      Yet Another Expect ('s' is for either simple or super)
    
      Unlike Expect (Tcl), Expect.pm (Perl), Pexpect (Python) or other similar
      Expect implementations, 'sexpect' is not bound to any specific programming
      language so it can be used with any languages which support running external
      commands.
    
      Another interesting 'sexpect' feature is that the spawned child process is
      running in background. You can attach to and detach from it as needed.
    
    GLOBAL OPTIONS:
      -debug | -d
            Debug mode. The server will run in foreground.
    
      -help | --help | -h
    
      -sock SOCKFILE | -s SOCKFILE
            The socket file used for client/server communication. This option is
            required for most sub-commands.
    
            You can set the SEXPECT_SOCKFILE environment variable so you don't
            need to specify '-sock' for each command.
    
            The socket file will be automatically created if it does not exist.
    
      -version | --version
    
    ENVIRONMENT VARIABLES:
      SEXPECT_SOCKFILE
            The same as global option '-sock' but has lower precedence.
    
    SUB-COMMANDS:
    =============
    
    spawn (sp)
    
      USAGE:
        spawn [OPTION] COMMAND...
    
      DESCRIPTION:
        The 'spawn' command will first make itself run as a background server. Then
        the server will start the specified COMMAND on a new allocated pty. The
        server will continue running until the child process exits and is waited.
    
      OPTIONS:
        -append
            See option '-logfile'.
    
        -autowait | -nowait
            Turn on 'autowait' which by default is off. See sub-command 'set' for
            more information.
    
        -logfile FILE | -logf FILE
            All output from the child process will be copied to the log file.
            By default the log file will be overwritten. Use '-append' if you want
            to append to it.
    
        -nohup
            (NOT_IMPLEMENTED_YET)
    
            Let the spawned child process ignore SIGHUP.
    
        -timeout N | -t N
            Set the default timeout for the 'expect' command.
            The default value is 10 seconds. A negative value means no timeout.
    
    expect (exp, ex, x)
    
      USAGE
        expect [OPTION] [-exact] STRING 
        expect [OPTION] -glob PATTERN
        expect [OPTION] -re PATTERN 
        expect [OPTION] -eof 
        expect
    
      DESCRIPTION
        Only one of '-exact', '-glob', '-re' or '-eof' can be specified. If none of
        them is specified then it defaults to
    
            expect -timeout 0 -re '.*'
    
      OPTIONS
        -eof
            Wait until EOF from the child process.
    
        -exact PATTERN | -ex STRING
            Expect the exact STRING.
    
        -glob PATTERN | -gl PATTERN
            (NOT_IMPLEMENTED_YET)
    
        -nocase, -icase, -i
            Ignore case. Used with '-exact', '-glob' or '-re'.
    
        -re PATTERN
            Expect the ERE (extended RE) PATTERN.
    
        -timeout N | -t N
            Override the default timeout (see 'spawn -timeout').
    
      EXIT:
        0 will be returned if the match succeeds before timeout or EOF.
    
        If the command fails, 'chkerr' can be used to check if the failure is
        caused by EOF or TIMEOUT. For example (with Bash):
    
            sexpect expect -re foobar
            ret=$?
            if [[ $ret == 0 ]]; then
                # Cool we got the expected output
            elif sexpect chkerr -errno $ret -is eof; then
                # EOF from the child (most probably dead)
            elif sexpect chkerr -errno $ret -is timeout; then
                # Timed out waiting for the expected output
            else
                # Other errors
            fi
    
    send
    
      USAGE:
        send [OPTION] [-exact] STRING
        send -cstring STRING
    
      DESCRIPTION:
    
      OPTIONS:
        -cstring STRING | -cstr STRING | -c STRING
            C style bashslash escapes would be recognized and replaced before
            sending to the server.
    
            The following standard C escapes are supported:
    
                \\ \a \b \f \n \r \t \v
                \xHH \xH
                \o \ooo \ooo
    
            Other supported escapes:
    
                \e \E : ESC, the escape char.
                \cX   : CTRL-X, e.g. \cc will be converted to the CTRL-C char.
    
        -enter | -cr
            Append ENTER (\r) to the specified STRING before sending to the server.
    
        -exact STRING | -ex STRING
    
        -fd FD
            (NOT_IMPLEMENTED_YET)
    
    
        -file FILE
            (NOT_IMPLEMENTED_YET)
    
    
        -env NAME
            (NOT_IMPLEMENTED_YET)
    
    
    interact (i)
    
      USAGE:
        interact [OPTION]
    
      DESCRIPTION:
        'interact' is used to attach to the child process and manually interact
        with it. To detach from the child, press CTRL-] .
    
        `interact' would fail if it's not running on a tty/pty.
    
        If the child process exits when you're interacting with it then 'interact'
        will exit with the same exit code of the child process and you don't need
        to call the 'wait' command any more. And the background server will also
        exit.
    
      OPTIONS:
        -lookback N | -lb N
            (NOT_IMPLEMENTED_YET)
    
            Show the most recent last N lines of output after 'interact' so you'd
            know where you were last time.
    
    wait (w)
    
      USAGE:
        wait
    
      DESCRIPTION:
        'wait' waits for the child process to complete and 'wait' will exit with
        same exit code as the child process.
    
    expect_out (expout, out)
    
      USAGE:
        expect_out [-index N]
    
      DESCRIPTION:
        After a successful 'expect -re PATTERN', you can use 'expect_out' to get
        substring matches. Up to 9 (1-9) RE substring matches are saved in the
        server side. 0 refers to the string which matched the whole PATTERN.
    
        For example, if the command
    
            sexpect expect -re 'a(bc)d(ef)g'
    
        succeeds (exits 0) then the following commands
    
            sexpect expect_out -index 0
            sexpect expect_out -index 1
            sexpect expect_out -index 2
    
        would output 'abcdefg', 'bc' and 'ef', respectively.
    
      OPTIONS:
        -index N | -i N
            N can be 0-9. The default is 0.
    
    chkerr (chk, ck)
    
      USAGE:
        chkerr -errno NUM -is REASON
    
      DESCRIPTION:
        If the previous 'expect' command fails, 'chkerr' can be used to check if
        the failure is caused by EOF or TIMEOUT.
    
        See 'expect' for an example.
    
      OPTIONS:
        -errno NUM | -err NUM | -no NUM
            NUM is the exit code of the previous failed 'expect' command.
    
        -is REASON
            REASON can be 'eof', 'timeout'.
    
      EXIT:
        0 will be exited if the specified error NUM is caused by the REASON.
        1 will be exited if the specified error NUM is NOT caused by the REASON.
    
    close (c)
    
      USAGE:
        close
    
      DESCRIPTION:
        Close the child process's pty by force. This would usually cause the child
        to receive SIGHUP and be killed.
    
    kill (k)
    
      USAGE:
        kill -NAME
        kill -NUM
    
      DESCRIPTION:
        Send the specified signal to the child process.
    
      OPTIONS:
        -SIGNAME
            Specify the signal with name. The following signal names are
            supported:
    
                SIGCONT SIGHUP  SIGINT  SIGKILL SIGQUIT
                SIGSTOP SIGTERM SIGUSR1 SIGUSR2
    
            The SIGNAME is case insensitive and the prefix 'SIG' is optional.
    
        -SIGNUM
            Specify the signal with number.
    
    set
    
      USAGE:
        set [OPTION]
    
      DESCRIPTION:
        The 'set' sub-command can be used to dynamically change server side's
        parameters after 'spawn'.
    
      OPTIONS:
        -autowait FLAG | -nowait FLAG
            FLAG can be 0, 1, on, off.
    
            By default, after the child process exits, the server side will wait
            for the client to call 'wait' to get the exit status of the child and
            then the server will exit.
    
            When 'autowait' is turned on, after the child process exits it'll
            be automatically waited and then the server will exit.
    
        -discard FLAG
            FLAG can be 0, 1, on, off.
    
            By default, the child process will be blocked if it outputs too much
            and the client (either 'expect', 'interact' or 'wait') does not read
            the output in time.
    
            When 'discard' is turned on, the output from the child will be silently
            discarded so the child can continue running in without being blocked.
    
        -timeout N | -t N
            See 'spawn'.
    
    get
    
      USAGE:
        get [OPTION]
    
      DESCRIPTION:
        Retrieve server side information.
    
      OPTIONS:
        -all | -a
            Get all supported information from server side.
    
        -autowait | -nowait
            Get the 'autowait' flag.
    
        -discard
            Get the 'discard' flag.
    
        -pid
            Get the child process's PID.
    
        -ppid
            Get the child process's PPID (the server's PID).
    
        -tty | -pty | -pts
            Get the child process's tty.
    
        -timeout | -t
            Get the current default timeout value.
    
    BUGS:
      Report bugs to Clark Wang <dearvoid @ gmail.com> or
      https://github.com/clarkwang/sexpect
