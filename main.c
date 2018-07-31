
#if defined(__gnu_linux__) || defined(__CYGWIN__)
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>

#include "common.h"
#include "pty.h"

#define str_true(s)   str1of(s, "1", "on",  "yes", "y", "true",  NULL)
#define str_false(s)  str1of(s, "0", "off", "no",  "n", "false", NULL)

char * const SEXPECT  = "sexpect";
char * const VERSION_ = "2.1.14";

static struct {
    char * progname;
    struct st_cmdopts cmdopts;
} g;

static void
usage(int exitcode)
{
    FILE * fp;

    fp = (exitcode == 0) ? stdout : stderr;
    fprintf(fp, "\
USAGE:\n\
\n\
  sexpect [OPTION] [SUB-COMMAND]\n\
\n\
DESCRIPTION:\n\
\n\
  Yet Another Expect ('s' is for either simple or super)\n\
\n\
  Unlike Expect (Tcl), Expect.pm (Perl), Pexpect (Python) or other similar\n\
  Expect implementations, 'sexpect' is not bound to any specific programming\n\
  language so it can be used with any languages which support running external\n\
  commands.\n\
\n\
  Another interesting 'sexpect' feature is that the spawned child process is\n\
  running in background. You can attach to and detach from it as needed.\n\
\n\
GLOBAL OPTIONS:\n\
\n\
  -debug | -d\n\
        Debug mode. The server will run in foreground.\n\
\n\
  -help | --help | -h\n\
\n\
  -sock SOCKFILE | -s SOCKFILE\n\
        The socket file used for client/server communication. This option is\n\
        required for most sub-commands.\n\
\n\
        You can set the SEXPECT_SOCKFILE environment variable so you don't\n\
        need to specify '-sock' for each command.\n\
\n\
        The socket file will be automatically created if it does not exist.\n\
\n\
  -version | --version\n\
\n\
ENVIRONMENT VARIABLES:\n\
\n\
  SEXPECT_SOCKFILE\n\
        The same as global option '-sock' but has lower precedence.\n\
\n\
SUB-COMMANDS:\n\
=============\n\
\n\
spawn (sp)\n\
----------\n\
\n\
  USAGE:\n\
    spawn [OPTION] COMMAND...\n\
\n\
  DESCRIPTION:\n\
    The 'spawn' command will first make itself run as a background server. Then\n\
    the server will start the specified COMMAND on a new allocated pty. The\n\
    server will continue running until the child process exits and is waited.\n\
\n\
  OPTIONS:\n\
    -append\n\
        See option '-logfile'.\n\
\n\
    -autowait | -nowait\n\
        Turn on 'autowait' which by default is off. See sub-command 'set' for\n\
        more information.\n\
\n\
    -close-on-exit | -cloexit\n\
        Close the pty after the child process has exited even if the child's\n\
        child processes are still opening the pty. (Example: 'ssh -f')\n\
\n\
    -discard\n\
        Turn on 'discard' which by default is off. See sub-command 'set' for\n\
        more information.\n\
\n\
    -logfile FILE | -logf FILE\n\
        All output from the child process will be copied to the log file.\n\
        By default the log file will be overwritten. Use '-append' if you want\n\
        to append to it.\n\
\n\
    -nohup\n\
        Make the spawned child process ignore SIGHUP.\n\
\n\
    -term TERM | -T TERM\n\
        Set the env var TERM for the child process.\n\
\n\
    -timeout N | -t N\n\
        Set the default timeout for the 'expect' command.\n\
        The default value is 10 seconds. A negative value means no timeout.\n\
\n\
    -ttl N\n\
        The background server process will close the PTY and exit N seconds\n\
        after the child process is spawned. Usually this would cause the child\n\
        to receive SIGHUP and be killed.\n\
\n\
expect (exp, ex, x)\n\
-------------------\n\
\n\
  USAGE\n\
    expect [OPTION] [-exact] PATTERN\n\
    expect [OPTION] -glob PATTERN\n\
    expect [OPTION] -re PATTERN\n\
    expect [OPTION] -eof\n\
    expect\n\
\n\
  DESCRIPTION\n\
    Only one of '-exact', '-glob', '-re' or '-eof' options can be specified.\n\
    If none of them is specified then it defaults to\n\
\n\
        expect -timeout 0 -re '.*'\n\
\n\
  OPTIONS\n\
    -cstring | -cstr | -c\n\
        C style backslash escapes would be recognized and replaced in STRING\n\
        or PATTERN. See 'send' for more information.\n\
\n\
    -eof\n\
        Wait until EOF from the child process.\n\
\n\
    -exact PATTERN | -ex PATTERN\n\
        Handle PATTERN as an 'exact' string.\n\
\n\
    -glob PATTERN | -gl PATTERN\n\
        (NOT_IMPLEMENTED_YET)\n\
\n\
    -lookback N | -lb N\n\
        Show the most recent last N lines of output so you'd know where you\n\
        were last time.\n\
\n\
    -nocase, -icase, -i\n\
        Ignore case. Used with '-exact', '-glob' or '-re'.\n\
\n\
    -re PATTERN\n\
        Expect the ERE (extended RE) PATTERN.\n\
\n\
    -timeout N | -t N\n\
        Override the default timeout (see 'spawn -timeout').\n\
\n\
  EXIT:\n\
    0 will be returned if the match succeeds before timeout or EOF.\n\
\n\
    If the command fails, 'chkerr' can be used to check if the failure is\n\
    caused by EOF or TIMEOUT. For example (with Bash):\n\
\n\
        sexpect expect -re foobar\n\
        ret=$?\n\
        if [[ $ret == 0 ]]; then\n\
            # Cool we got the expected output\n\
        elif sexpect chkerr -errno $ret -is eof; then\n\
            # EOF from the child (most probably dead)\n\
        elif sexpect chkerr -errno $ret -is timeout; then\n\
            # Timed out waiting for the expected output\n\
        else\n\
            # Other errors\n\
        fi\n\
\n\
send (s)\n\
--------\n\
\n\
  USAGE:\n\
    send [OPTION] [--] STRING\n\
\n\
  DESCRIPTION:\n\
\n\
  OPTIONS:\n\
    -cstring | -cstr | -c\n\
        C style backslash escapes would be recognized and replaced before\n\
        sending to the server.\n\
\n\
        The following standard C escapes are supported:\n\
\n\
            \\\\ \\a \\b \\f \\n \\r \\t \\v\n\
            \\xHH \\xH\n\
            \\o \\ooo \\ooo\n\
\n\
        Other supported escapes:\n\
\n\
            \\e \\E : ESC, the escape char.\n\
            \\cX   : CTRL-X, e.g. \\cc will be converted to the CTRL-C char.\n\
\n\
    -enter | -cr\n\
        Append ENTER (\\r) to the specified STRING before sending to the server.\n\
\n\
    -file FILE | -f FILE\n\
        Send the content of the FILE to the server.\n\
\n\
    -env NAME | -var NAME\n\
        Send the value of env var NAME to the server.\n\
\n\
\n\
interact (i)\n\
------------\n\
\n\
  USAGE:\n\
    interact [OPTION]\n\
\n\
  DESCRIPTION:\n\
    'interact' is used to attach to the child process and manually interact\n\
    with it. To detach from the child, press CTRL-] .\n\
\n\
    `interact' would fail if it's not running on a tty/pty.\n\
\n\
    If the child process exits when you're interacting with it then 'interact'\n\
    will exit with the same exit code of the child process and you don't need\n\
    to call the 'wait' command any more. And the background server will also\n\
    exit.\n\
\n\
  OPTIONS:\n\
    -lookback N | -lb N\n\
        Show the most recent last N lines of output after 'interact' so you'd\n\
        know where you were last time.\n\
\n\
wait (w)\n\
--------\n\
\n\
  USAGE:\n\
    wait\n\
\n\
  DESCRIPTION:\n\
    'wait' waits for the child process to complete and 'wait' will exit with\n\
    same exit code as the child process.\n\
\n\
expect_out (expout, out)\n\
------------------------\n\
\n\
  USAGE:\n\
    expect_out [-index N]\n\
\n\
  DESCRIPTION:\n\
    After a successful 'expect -re PATTERN', you can use 'expect_out' to get\n\
    substring matches. Up to 9 (1-9) RE substring matches are saved in the\n\
    server side. 0 refers to the string which matched the whole PATTERN.\n\
\n\
    For example, if the command\n\
\n\
        sexpect expect -re 'a(bc)d(ef)g'\n\
\n\
    succeeds (exits 0) then the following commands\n\
\n\
        sexpect expect_out -index 0\n\
        sexpect expect_out -index 1\n\
        sexpect expect_out -index 2\n\
\n\
    would output 'abcdefg', 'bc' and 'ef', respectively.\n\
\n\
  OPTIONS:\n\
    -index N | -i N\n\
        N can be 0-9. The default is 0.\n\
\n\
chkerr (chk, ck)\n\
----------------\n\
\n\
  USAGE:\n\
    chkerr -errno NUM -is REASON\n\
\n\
  DESCRIPTION:\n\
    If the previous 'expect' command fails, 'chkerr' can be used to check if\n\
    the failure is caused by EOF or TIMEOUT.\n\
\n\
    See 'expect' for an example.\n\
\n\
  OPTIONS:\n\
    -errno NUM | -err NUM | -no NUM\n\
        NUM is the exit code of the previous failed 'expect' command.\n\
\n\
    -is REASON\n\
        REASON can be 'eof', 'timeout'.\n\
\n\
  EXIT:\n\
    0 will be exited if the specified error NUM is caused by the REASON.\n\
    1 will be exited if the specified error NUM is NOT caused by the REASON.\n\
\n\
close (c)\n\
---------\n\
\n\
  USAGE:\n\
    close\n\
\n\
  DESCRIPTION:\n\
    Close the child process's pty by force. This would usually cause the child\n\
    to receive SIGHUP and be killed.\n\
\n\
kill (k)\n\
--------\n\
\n\
  USAGE:\n\
    kill -NAME\n\
    kill -NUM\n\
\n\
  DESCRIPTION:\n\
    Send the specified signal to the child process.\n\
\n\
  OPTIONS:\n\
    -SIGNAME\n\
        Specify the signal with name. The following signal names are\n\
        supported:\n\
\n\
            SIGCONT SIGHUP  SIGINT  SIGKILL SIGQUIT\n\
            SIGSTOP SIGTERM SIGUSR1 SIGUSR2\n\
\n\
        The SIGNAME is case insensitive and the prefix 'SIG' is optional.\n\
\n\
    -SIGNUM\n\
        Specify the signal with number.\n\
\n\
set\n\
----\n\
\n\
  USAGE:\n\
    set [OPTION]\n\
\n\
  DESCRIPTION:\n\
    The 'set' sub-command can be used to dynamically change server side's\n\
    parameters after 'spawn'.\n\
\n\
  OPTIONS:\n\
    -autowait FLAG | -nowait FLAG\n\
        FLAG can be 0, 1, on, off.\n\
\n\
        By default, after the child process exits, the server side will wait\n\
        for the client to call 'wait' to get the exit status of the child and\n\
        then the server will exit.\n\
\n\
        When 'autowait' is turned on, after the child process exits it'll\n\
        be automatically waited and then the server will exit.\n\
\n\
    -discard FLAG\n\
        FLAG can be 0, 1, on, off.\n\
\n\
        By default, the child process will be blocked if it outputs too much\n\
        and the client (either 'expect', 'interact' or 'wait') does not read\n\
        the output in time.\n\
\n\
        When 'discard' is turned on, the output from the child will be silently\n\
        discarded so the child can continue running in without being blocked.\n\
\n\
    -timeout N | -t N\n\
        See 'spawn'.\n\
\n\
    -ttl N\n\
        See 'spawn'.\n\
\n\
get\n\
----\n\
\n\
  USAGE:\n\
    get [OPTION]\n\
\n\
  DESCRIPTION:\n\
    Retrieve server side information.\n\
\n\
  OPTIONS:\n\
    -all | -a\n\
        Get all supported information from server side.\n\
\n\
    -autowait | -nowait\n\
        Get the 'autowait' flag.\n\
\n\
    -discard\n\
        Get the 'discard' flag.\n\
\n\
    -pid\n\
        Get the child process's PID.\n\
\n\
    -ppid\n\
        Get the child process's PPID (the server's PID).\n\
\n\
    -tty | -pty | -pts\n\
        Get the child process's tty.\n\
\n\
    -timeout | -t\n\
        Get the current default timeout value.\n\
\n\
    -ttl\n\
        Get the TTL value. See 'spawn' for details.\n\
\n\
BUGS:\n\
  Report bugs to Clark Wang <dearvoid @ gmail.com> or\n\
  https://github.com/clarkwang/sexpect\n\
");

    exit(exitcode);
}

static void
startup()
{
    common_init();
}

static int
arg2int(const char * s)
{
    long lval;
    char * pend = NULL;

    if ( * s == '\0') {
        fatal(ERROR_USAGE, "invalid number: %s", s);
    }

    lval = strtol(s, & pend, 10);
    if (pend[0] != '\0') {
        fatal(ERROR_USAGE, "invalid number: %s", s);
    }

    if (lval < INT_MIN || lval > INT_MAX) {
        fatal(ERROR_USAGE, "out of range: %s", s);
    }

    return lval;
}

static int
arg2uint(const char * s)
{
    int n = arg2int(s);

    if (n < 0) {
        fatal(ERROR_USAGE, "out of range: %s", s);
    }

    return n;
}

static char *
nextarg(char ** argv, char * prev_arg, int * cur_idx)
{
    if (argv[*cur_idx + 1] != NULL) {
        ++(*cur_idx);
        return argv[*cur_idx];
    } else {
        fatal(ERROR_USAGE, "%s requires an argument", prev_arg);
    }

    return NULL;
}

static char *
readfile(const char * fname, int * len)
{
    int fd, ret;
    char * buf = NULL;
    struct stat st;

    * len = 0;

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        fatal_sys("open(%s)", fname);
    }

    ret = fstat(fd, & st);
    if (ret < 0) {
        fatal_sys("fstat(%s)", fname);
    }

    if ( ! S_ISREG(st.st_mode) ) {
        fatal(ERROR_USAGE, "not a regular file: %s", fname);
    }

    if (st.st_size > PASS_MAX_SEND) {
        fatal(ERROR_USAGE,
            "file too large (%d > %d)", (int)st.st_size, PASS_MAX_SEND);
    }

    buf = malloc(st.st_size);
    ret = read(fd, buf, st.st_size);
    if (ret < 0) {
        fatal_sys("read(%s)", fname);
    } else {
        * len = ret;
    }

    close(fd);
    return buf;
}

static void
getargs(int argc, char **argv)
{
    int i;
    char * arg, * next;
    bool usage_err = false;

#if 0
    if ((g.progname = strrchr(argv[0], '/')) != NULL) {
        ++g.progname;
    } else {
        g.progname = argv[0];
    }
#else
    g.progname = SEXPECT;
#endif

    for (i = 1; i < argc; ++i) {
        arg = argv[i];
        if (g.cmdopts.cmd == NULL) {
            /* -help */
            if (str1of(arg, "-h", "-help", "--help", "help", NULL) ) {
                g.cmdopts.cmd = "help";

                /* -version */
            } else if (str1of(arg, "-version", "--version", "-ver",
                              "version", "ver", NULL) ) {
                g.cmdopts.cmd = "version";

                /* -debug */
            } else if (str1of(arg, "-debug", "-d", NULL) ) {
                g.cmdopts.debug = true;
                debug_on();

                /* -sock */
            } else if (str1of(arg, "-sock", "-s", NULL) ) {
                g.cmdopts.sockpath = nextarg(argv, arg, & i);

                /* -unknown */
            } else if (arg[0] == '-') {
                fatal(ERROR_USAGE, "unknown global option: %s", arg);

                /* sub-commands */
            } else {
                /* chkerr */
                if (str1of(arg, "chkerr", "chk", "ck", "err", NULL) ) {
                    g.cmdopts.cmd = "chkerr";
                    g.cmdopts.chkerr.errcode = -1;

                    /* close */
                } else if (str1of(arg, "close", "c", NULL) ) {
                    g.cmdopts.cmd = "close";

                    /* expect */
                } else if (str1of(arg, "expect", "exp", "ex", "x", NULL) ) {
                    g.cmdopts.cmd = "expect";
                    g.cmdopts.passing = true;
                    g.cmdopts.pass.subcmd = PASS_SUBCMD_EXPECT;
                    g.cmdopts.pass.no_input = true;

                    /* expect_out */
                } else if (str1of(arg, "expect_out", "expout", "out", NULL) ) {
                    g.cmdopts.cmd = "expect_out";
                    g.cmdopts.expout.index = 0;

                    /* get */
                } else if (str1of(arg, "get", NULL) ) {
                    g.cmdopts.cmd = "get";
                    g.cmdopts.get.get_all = true;

                    /* interact */
                } else if (str1of(arg, "interact", "i", NULL) ) {
                    g.cmdopts.cmd = "interact";
                    g.cmdopts.passing = true;
                    g.cmdopts.pass.subcmd = PASS_SUBCMD_INTERACT;
                    g.cmdopts.pass.has_timeout = true;
                    g.cmdopts.pass.timeout = -1;
                    g.cmdopts.pass.expflags = PASS_EXPECT_EXIT;

                    /* kill */
                } else if (str1of(arg, "kill", "k", NULL) ) {
                    g.cmdopts.cmd = "kill";
                    g.cmdopts.kill.signal = -1;

                    /* send */
                } else if (str1of(arg, "send", "s", NULL) ) {
                    g.cmdopts.cmd = "send";

                    /* set */
                } else if (str1of(arg, "set", NULL) ) {
                    g.cmdopts.cmd = "set";
                    g.cmdopts.set.timeout = PASS_DEF_TMOUT;

                    /* spawn */
                } else if (str1of(arg, "spawn", "sp", NULL) ) {
                    g.cmdopts.cmd = "spawn";
                    g.cmdopts.spawn.def_timeout = PASS_DEF_TMOUT;
                    g.cmdopts.spawn.logfd = -1;

                    /* wait */
                } else if (str1of(arg, "wait", "w", NULL) ) {
                    g.cmdopts.cmd = "wait";
                    g.cmdopts.passing = true;
                    g.cmdopts.pass.subcmd = PASS_SUBCMD_WAIT;
                    g.cmdopts.pass.no_input = true;
                    g.cmdopts.pass.has_timeout = true;
                    g.cmdopts.pass.timeout = -1;
                    g.cmdopts.pass.expflags = PASS_EXPECT_EXIT;
                } else {
                    g.cmdopts.cmd = arg;
                }
            }

            /* chkerr */
        } else if (streq(g.cmdopts.cmd, "chkerr") ) {
            if (str1of(arg, "-errno", "-err", "-no", "-code", NULL) ) {
                next = nextarg(argv, "-errno", & i);
                g.cmdopts.chkerr.errcode = arg2uint(next);
            } else if (streq(arg, "-is") ) {
                next = nextarg(argv, "-is", & i);
                g.cmdopts.chkerr.cmpto = next;
            } else {
                usage_err = true;
                break;
            }

            /* close */
        } else if (streq(g.cmdopts.cmd, "close") ) {
            usage_err = true;
            break;

            /* expect */
        } else if (streq(g.cmdopts.cmd, "expect") ) {
            struct st_pass * st = & g.cmdopts.pass;
            if (str1of(arg, "-exact", "-ex", "-re", NULL) ) {
                next = nextarg(argv, "-exact", & i);
                st->pattern = next;
                if (str1of(arg, "-exact", "-ex", NULL) ) {
                    st->expflags |= PASS_EXPECT_EXACT;
                } else if (streq(arg, "-re") ) {
                    st->expflags |= PASS_EXPECT_ERE;
                }
            } else if (str1of(arg, "-nocase", "-icase", "-ic", "-i", NULL) ) {
                st->expflags |= PASS_EXPECT_ICASE;
            } else if (str1of(arg, "-cstring", "-cstr", "-c", NULL) ) {
                st->cstring = true;
            } else if (streq(arg, "-eof") ) {
                st->expflags |= PASS_EXPECT_EOF;
            } else if (str1of(arg, "-timeout", "-t", NULL) ) {
                st->has_timeout = true;
                st->timeout = arg2int(nextarg(argv, arg, & i) );
                if (st->timeout < 0) {
                    st->timeout = -1;
                }
            } else if (str1of(arg, "-lookback", "-lb", NULL) ) {
                next = nextarg(argv, "-lookback", & i);
                g.cmdopts.pass.lookback = arg2uint(next);
            } else if (arg[0] == '-') {
                fatal(ERROR_USAGE, "unknown expect option: %s", arg);
            } else if (arg[0] == '\0') {
                fatal(ERROR_USAGE, "pattern cannot be empty");
            } else if ( (st->expflags & PASS_EXPECT_EXACT) != 0) {
                usage_err = true;
                break;
            } else {
                st->pattern = arg;
                st->expflags |= PASS_EXPECT_EXACT;
            }

            /* expect_out */
        } else if (streq(g.cmdopts.cmd, "expect_out") ) {
            if (str1of(arg, "-index", "-i", NULL) ) {
                next = nextarg(argv, "-index", & i);
                g.cmdopts.expout.index = arg2uint(next);
            } else {
                usage_err = true;
                break;
            }

            /* get */
        } else if (streq(g.cmdopts.cmd, "get") ) {
            static bool got_one = false;

            if ( got_one) {
                fatal(ERROR_USAGE, "can only specify one option for get");
            } else {
                got_one = true;
            }

            if (str1of(arg, "-all", "-a", NULL) ) {
                g.cmdopts.get.get_all = true;
            } else {
                g.cmdopts.get.get_all = false;

                if (streq(arg, "-pid") ) {
                    g.cmdopts.get.get_pid = true;
                } else if (streq(arg, "-ppid") ) {
                    g.cmdopts.get.get_ppid = true;
                } else if (str1of(arg, "-tty", "-pty", "-pts", NULL) ) {
                    g.cmdopts.get.get_tty = true;
                } else if (str1of(arg, "-timeout", "-t", NULL) ) {
                    g.cmdopts.get.get_timeout = true;
                } else if (str1of(arg, "-discard", NULL) ) {
                    g.cmdopts.get.get_discard = true;
                } else if (str1of(arg, "-autowait", "-nowait", "-now", NULL) ) {
                    g.cmdopts.get.get_autowait = true;
                } else if (str1of(arg, "-ttl", NULL) ) {
                    g.cmdopts.get.get_ttl = true;
                } else {
                    usage_err = true;
                    break;
                }
            }

            /* help */
        } else if (streq(g.cmdopts.cmd, "help") ) {
            usage_err = true;
            break;

            /* interact */
        } else if (streq(g.cmdopts.cmd, "interact") ) {
            if (str1of(arg, "-lookback", "-lb", NULL) ) {
                next = nextarg(argv, "-lookback", & i);
                g.cmdopts.pass.lookback = arg2uint(next);
            } else {
                usage_err = true;
                break;
            }

            /* kill */
        } else if (streq(g.cmdopts.cmd, "kill") ) {
            if (strmatch(arg, "^-[0-9]+$") ) {
                g.cmdopts.kill.signal = arg2uint(arg + 1);
            } else if (arg[0] == '-') {
                g.cmdopts.kill.signal = name2sig(arg + 1);
                if (g.cmdopts.kill.signal < 0) {
                    fatal(ERROR_USAGE,
                        "%s not supported, please use signal number", arg);
                }
            } else {
                usage_err = true;
                break;
            }

            /* send */
        } else if (streq(g.cmdopts.cmd, "send") ) {
            struct st_send * st = & g.cmdopts.send;
            if (str1of(arg, "-cstring", "-cstr", "-c", NULL) ) {
                st->cstring = true;
            } else if (str1of(arg, "-cr", "-enter", NULL) ) {
                st->enter = true;
            } else if (str1of(arg, "-env", "-var", NULL) ) {
                if (st->data != NULL) {
                    usage_err = true;
                    break;
                }
                st->env = true;
                next = nextarg(argv, "-env", & i);
                st->data = getenv(next);
                if (st->data == NULL) {
                    fatal(ERROR_USAGE, "env var not found: %s", next);
                } else {
                    st->len = strlen(st->data);
                }
            } else if (str1of(arg, "-file", "-f", NULL) ) {
                if (st->data != NULL) {
                    usage_err = true;
                    break;
                }
                st->file = true;
                next = nextarg(argv, "-file", & i);
                st->data = readfile(next, & st->len);
            } else if (streq(arg, "--" ) ) {
                if (argv[i + 1] != NULL) {
                    if (argv[i + 2] != NULL) {
                        arg = argv[i + 2];
                        usage_err = true;
                        break;
                    }
                    st->data = strdup(argv[i + 1]);
                    st->len  = strlen(argv[i + 1]);

                    memset(argv[i + 1], '*', st->len);
                }
                break;
            } else if (arg[0] == '-') {
                fatal(ERROR_USAGE, "unknown send option: %s", arg);
            } else if (st->data == NULL) {
                st->data = strdup(arg);
                st->len  = strlen(arg);

                memset(arg, '*', st->len);
            } else {
                usage_err = true;
                break;
            }

            /* set */
        } else if (streq(g.cmdopts.cmd, "set") ) {
            struct st_set * st = & g.cmdopts.set;
            if (str1of(arg, "-autowait", "-nowait", "-now", NULL) ) {
                st->set_autowait = true;

                next = nextarg(argv, "-autowait", & i);
                if (str_true(next) ) {
                    st->autowait = true;
                } else if (str_false(next) ) {
                    st->autowait = false;
                } else {
                    arg = next;
                    usage_err = true;
                    break;
                }
            } else if (str1of(arg, "-discard", NULL ) ) {
                st->set_discard = true;

                next = nextarg(argv, "-discard", & i);
                if (str_true(next) ) {
                    st->discard = true;
                } else if (str_false(next) ) {
                    st->discard = false;
                } else {
                    arg = next;
                    usage_err = true;
                    break;
                }
            } else if (str1of(arg, "-timeout", "-t", NULL ) ) {
                st->set_timeout = true;
                next = nextarg(argv, "-timeout", & i);
                st->timeout = arg2int(next);

                if (st->timeout < 0) {
                    st->timeout = -1;
                }
            } else if (str1of(arg, "-ttl", NULL ) ) {
                st->set_ttl = true;
                next = nextarg(argv, "-timeout", & i);
                st->ttl = arg2int(next);

                if (st->ttl < 0) {
                    st->ttl = 0;
                }
            } else {
                usage_err = true;
                break;
            }

            /* spawn */
        } else if (streq(g.cmdopts.cmd, "spawn") ) {
            struct st_spawn * st = & g.cmdopts.spawn;
            if (streq(arg, "-nohup") ) {
                st->nohup = true;
            } else if (str1of(arg, "-autowait", "-nowait", "-now", NULL) ) {
                st->autowait = true;
            } else if (str1of(arg, "-discard", NULL) ) {
                st->discard = true;
            } else if (str1of(arg, "-close-on-exit", "-cloexit", NULL) ) {
                st->cloexit = true;
            } else if (str1of(arg, "-term", "-T", NULL) ) {
                next = nextarg(argv, "-term", & i);
                if (strlen(next) == 0) {
                    fatal(ERROR_USAGE, "-term cannot be empty");
                }
                st->TERM = next;
            } else if (str1of(arg, "-timeout", "-t", NULL) ) {
                st->def_timeout = arg2int(nextarg(argv, arg, & i) );
            } else if (str1of(arg, "-ttl", NULL) ) {
                st->ttl = arg2int(nextarg(argv, arg, & i) );
                if (st->ttl < 0) {
                    st->ttl = 0;
                }
            } else if (str1of(arg, "-logfile", "-logf", NULL) ) {
                st->logfile = nextarg(argv, arg, & i);
            } else if (str1of(arg, "-append", NULL) ) {
                st->append = true;
            } else if (arg[0] == '-') {
                fatal(ERROR_USAGE, "unknown spawn option: %s", arg);
            } else {
                st->argv = & argv[i];

                break;
            }

            /* version */
        } else if (streq(g.cmdopts.cmd, "version") ) {
            usage_err = true;
            break;

            /* wait */
        } else if (streq(g.cmdopts.cmd, "wait") ) {
            usage_err = true;
            break;
        }
    }
    if (usage_err) {
        fatal(ERROR_USAGE, "unexpected argument: %s", arg);
    }

    /* no arguments specified */
    if (g.cmdopts.cmd == NULL) {
        fatal(ERROR_USAGE, "run %s -h for help", SEXPECT);
    }

    /* expect */
    if (streq(g.cmdopts.cmd, "expect") ) {
        struct st_pass * st = & g.cmdopts.pass;
        int flags = st->expflags & (PASS_EXPECT_EOF | PASS_EXPECT_EXACT | PASS_EXPECT_ERE);
        if (count1bits(flags) > 1) {
            fatal(ERROR_USAGE, "-eof, -exact and -re are exclusive");
        }

        if (st->pattern != NULL) {
            if (st->cstring) {
                char * pattern = NULL;
                int len = 0;
                strunesc(st->pattern, & pattern, & len);
                if (pattern == NULL) {
                    fatal(ERROR_USAGE, "invalid backslash escapes: %s", st->pattern);
                } else if (strlen(pattern) != len) {
                    fatal(ERROR_USAGE, "pattern cannot include NULL bytes");
                } else {
                    st->pattern = pattern;
                }
            }
            if (strlen(st->pattern) ==  0) {
                fatal(ERROR_USAGE, "pattern cannot be empty");
            }
        }

        /* help */
    } else if (streq(g.cmdopts.cmd, "help") ) {
        usage(0);

        /* send */
    } else if (streq(g.cmdopts.cmd, "send") ) {
        struct st_send * st = & g.cmdopts.send;
        char * data = NULL;

        if (st->data == NULL) {
            st->data = "";
        }

        if ( (st->file || st->env) && st->cstring) {
            fatal(ERROR_USAGE, "-cstring cannot be used with -file or -env");
        }

        if (st->data != NULL && st->cstring) {
            strunesc(st->data, & data, & st->len);
            if (data == NULL) {
                fatal(ERROR_USAGE, "invalid backslash escapes: %s", st->data);
            } else {
                st->data = data;
            }
        }

        if (st->len > PASS_MAX_SEND) {
            fatal(ERROR_USAGE, "send: string length must be < %d", PASS_MAX_SEND);
        }

        /* spawn */
    } else if (streq(g.cmdopts.cmd, "spawn") && g.cmdopts.spawn.argv == NULL) {
        fatal(ERROR_USAGE, "spawn requires more arguments");

        /* version */
    } else if (streq(g.cmdopts.cmd, "version") ) {
        printf("%s %s\n", SEXPECT, VERSION_);
        exit(0);
    }

    /* $SEXPECT_SOCKFILE */
    if (g.cmdopts.sockpath == NULL) {
        g.cmdopts.sockpath = getenv("SEXPECT_SOCKFILE");
    }
    /* most commands require ``-sock'' */
    if (g.cmdopts.sockpath == NULL && ! streq(g.cmdopts.cmd, "chkerr") ) {
        fatal(ERROR_USAGE, "-sock not specified");
    }
    /* if sockfile exists it must be a socket file */
    if (g.cmdopts.sockpath != NULL) {
        if (access(g.cmdopts.sockpath, F_OK) == 0) {
            struct stat st;

            if (stat(g.cmdopts.sockpath, & st) < 0) {
                fatal_sys("stat(%s)", g.cmdopts.sockpath);
            }

            if ( ! S_ISSOCK(st.st_mode) ) {
                fatal(ERROR_GENERAL, "not a socket file: %s", g.cmdopts.sockpath);
            }
        }
    }
}

int
main(int argc, char *argv[])
{
    startup();

    getargs(argc, argv);

    if (streq(g.cmdopts.cmd, "spawn") ) {
        serv_main( & g.cmdopts);
    } else {
        cli_main( & g.cmdopts);
    }

    return 0;
}
