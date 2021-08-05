
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
char * const VERSION_ = "2.3.7";

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
Usage:\n\
    sexpect [OPTION] SUB-COMMAND [OPTION]\n\
\n\
Global options:\n\
    -debug | -d\n\
    -help | --help | -h\n\
    -sock SOCKFILE | -s SOCKFILE\n\
    -version | --version\n\
\n\
Environment variables:\n\
    SEXPECT_SOCKFILE\n\
\n\
Sub-commands:\n\
=============\n\
spawn (sp, fork)\n\
----------------\n\
    sexpect spawn [OPTION] PROGRAM [ARGS]\n\
\n\
    Options:\n\
        -append\n\
        -autowait | -nowait\n\
        -close-on-exit | -cloexit\n\
        -idle-close N | -idle N\n\
        -logfile FILE | -logf FILE | -log FILE\n\
        -nohup\n\
        -nonblock | -nb\n\
        -term TERM | -T TERM\n\
        -timeout N | -t N\n\
        -ttl N\n\
        -zombie-idle N | -z N\n\
\n\
expect (exp, ex, x)\n\
-------------------\n\
    sexpect expect [OPTION] [-exact] PATTERN\n\
    sexpect expect [OPTION]  -glob   PATTERN\n\
    sexpect expect [OPTION]  -re     PATTERN\n\
    sexpect expect [OPTION]  -eof\n\
    sexpect expect [OPTION]\n\
\n\
    Options:\n\
        -cstring | -cstr | -c\n\
        -lookback N | -lb N\n\
        -nocase | -icase | -i\n\
        -timeout N | -t N\n\
\n\
send (s)\n\
--------\n\
    sexpect send [OPTION] [--] STRING\n\
\n\
    Options:\n\
        -cstring | -cstr | -c\n\
        -enter | -cr\n\
        -file FILE | -f FILE\n\
        -env NAME | -var NAME\n\
        -strip\n\
\n\
interact (i)\n\
------------\n\
    sexpect interact [OPTION]\n\
\n\
    Options:\n\
        -lookback N | -lb N\n\
        -nodetach | -nodet\n\
\n\
wait (w)\n\
--------\n\
    sexpect wait\n\
\n\
expect_out (expout, out)\n\
------------------------\n\
    sexpect expect_out [<-index | -i> INDEX]\n\
\n\
chkerr (chk, ck)\n\
----------------\n\
    sexpect chkerr <-errno | -err> NUM -is REASON\n\
\n\
    Options:\n\
        REASON: 'eof', 'timeout'\n\
\n\
close (c)\n\
---------\n\
    sexpect close\n\
\n\
kill (k)\n\
--------\n\
    sexpect kill [-SIGNAME] [-SIGNUM]\n\
\n\
    Options:\n\
        SIGNAME: SIGCONT SIGHUP  SIGINT  SIGKILL SIGQUIT\n\
                 SIGSTOP SIGTERM SIGUSR1 SIGUSR2\n\
\n\
set\n\
--------\n\
    sexpect set [OPTION]\n\
\n\
    Options:\n\
        -autowait FLAG | -nowait FLAG\n\
        -idle-close N | -idle N\n\
        -nonblock FLAG | -nb FLAG\n\
        -timeout N | -t N\n\
        -ttl N\n\
\n\
get\n\
--------\n\
    sexpect get [OPTION]\n\
\n\
    Options:\n\
        -all | -a\n\
        -autowait | -nowait\n\
        -idle-close | -idle\n\
        -nonblock | -nb\n\
        -pid\n\
        -ppid\n\
        -tty | -pty | -pts\n\
        -timeout | -t\n\
        -ttl\n\
\n\
Report bugs to Clark Wang <dearvoid@gmail.com> or https://github.com/clarkwang/sexpect/\n\
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
            if (str1of(arg, "-h", "-help", "--help", "help", "h", NULL) ) {
                g.cmdopts.cmd = "help";

                /* -version */
            } else if (str1of(arg, "-version", "--version",
                              "version", "ver", "v", NULL) ) {
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
                if (str1of(arg, "chkerr", "ckerr", "chk", "ck", "err", NULL) ) {
                    g.cmdopts.cmd = CMD_CHKERR;
                    g.cmdopts.chkerr.errcode = -1;

                    /* close */
                } else if (str1of(arg, "close", "c", NULL) ) {
                    g.cmdopts.cmd = CMD_CLOSE;

                    /* expect */
                } else if (str1of(arg, "expect", "exp", "ex", "x", NULL) ) {
                    g.cmdopts.cmd = CMD_EXPECT;
                    g.cmdopts.passing = true;
                    g.cmdopts.pass.subcmd = PASS_SUBCMD_EXPECT;
                    g.cmdopts.pass.no_input = true;

                    /* expect_out */
                } else if (str1of(arg, "expect_out", "expout", "out", NULL) ) {
                    g.cmdopts.cmd = CMD_EXPOUT;
                    g.cmdopts.expout.index = 0;

                    /* get */
                } else if (str1of(arg, "get", NULL) ) {
                    g.cmdopts.cmd = CMD_GET;
                    g.cmdopts.get.get_all = true;

                    /* interact */
                } else if (str1of(arg, "interact", "i", NULL) ) {
                    g.cmdopts.cmd = CMD_INTERACT;
                    g.cmdopts.passing = true;
                    g.cmdopts.pass.subcmd = PASS_SUBCMD_INTERACT;
                    g.cmdopts.pass.has_timeout = true;
                    g.cmdopts.pass.timeout = -1;
                    g.cmdopts.pass.expflags = PASS_EXPECT_EXIT;

                    /* kill */
                } else if (str1of(arg, "kill", "k", NULL) ) {
                    g.cmdopts.cmd = CMD_KILL;
                    g.cmdopts.kill.signal = -1;

                    /* send */
                } else if (str1of(arg, "send", "s", NULL) ) {
                    g.cmdopts.cmd = CMD_SEND;

                    /* set */
                } else if (str1of(arg, "set", NULL) ) {
                    g.cmdopts.cmd = CMD_SET;
                    g.cmdopts.set.timeout = PASS_DEF_TMOUT;

                    /* spawn */
                } else if (str1of(arg, "spawn", "sp", "fork", NULL) ) {
                    g.cmdopts.cmd = CMD_SPAWN;
                    g.cmdopts.spawn.def_timeout = PASS_DEF_TMOUT;
                    g.cmdopts.spawn.zombie_idle = PASS_DEF_ZOMBIE_TTL;
                    g.cmdopts.spawn.logfd = -1;

                    /* wait */
                } else if (str1of(arg, "wait", "w", NULL) ) {
                    g.cmdopts.cmd = CMD_WAIT;
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
        } else if (streq(g.cmdopts.cmd, CMD_CHKERR) ) {
            if (str1of(arg, "-errno", "-err", "-no", "-code", NULL) ) {
                next = nextarg(argv, arg, & i);
                g.cmdopts.chkerr.errcode = arg2uint(next);
            } else if (streq(arg, "-is") ) {
                next = nextarg(argv, arg, & i);
                g.cmdopts.chkerr.cmpto = next;
            } else {
                usage_err = true;
                break;
            }

            /* close */
        } else if (streq(g.cmdopts.cmd, CMD_CLOSE) ) {
            usage_err = true;
            break;

            /* expect */
        } else if (streq(g.cmdopts.cmd, CMD_EXPECT) ) {
            struct st_pass * st = & g.cmdopts.pass;
            if (str1of(arg, "-exact", "-ex", "-re", "-glob", "-gl", NULL) ) {
                next = nextarg(argv, arg, & i);
                st->pattern = next;
                if (str1of(arg, "-exact", "-ex", NULL) ) {
                    st->expflags |= PASS_EXPECT_EXACT;
                } else if (streq(arg, "-re") ) {
                    st->expflags |= PASS_EXPECT_ERE;
                } else if (str1of(arg, "-glob", "-gl", NULL) ) {
                    st->expflags |= PASS_EXPECT_GLOB;
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
                next = nextarg(argv, arg, & i);
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
        } else if (streq(g.cmdopts.cmd, CMD_EXPOUT) ) {
            if (str1of(arg, "-index", "-i", NULL) ) {
                next = nextarg(argv, arg, & i);
                g.cmdopts.expout.index = arg2uint(next);
            } else {
                usage_err = true;
                break;
            }

            /* get */
        } else if (streq(g.cmdopts.cmd, CMD_GET) ) {
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

                    /* still supports `-discard' for backward compat */
                } else if (str1of(arg, "-nonblock", "-nb", "-discard", NULL) ) {
                    g.cmdopts.get.get_nonblock = true;
                } else if (str1of(arg, "-autowait", "-nowait", "-now", NULL) ) {
                    g.cmdopts.get.get_autowait = true;
                } else if (str1of(arg, "-ttl", NULL) ) {
                    g.cmdopts.get.get_ttl = true;
                } else if (str1of(arg, "-idle-close", "-idle", NULL) ) {
                    g.cmdopts.get.get_idle = true;
                } else {
                    usage_err = true;
                    break;
                }
            }

            /* help */
        } else if (streq(g.cmdopts.cmd, CMD_HELP) ) {
            usage_err = true;
            break;

            /* interact */
        } else if (streq(g.cmdopts.cmd, CMD_INTERACT) ) {
            struct st_pass * st = & g.cmdopts.pass;
            if (str1of(arg, "-re", NULL) ) {
                next = nextarg(argv, arg, & i);
                st->pattern = next;
                st->expflags |= PASS_EXPECT_ERE;
            } else if (str1of(arg, "-nocase", "-icase", "-ic", "-i", NULL) ) {
                st->expflags |= PASS_EXPECT_ICASE;
            } else if (str1of(arg, "-cstring", "-cstr", "-c", NULL) ) {
                st->cstring = true;
            } else if (str1of(arg, "-lookback", "-lb", NULL) ) {
                next = nextarg(argv, arg, & i);
                g.cmdopts.pass.lookback = arg2uint(next);
            } else if (str1of(arg, "-nodetach", "-nodet", "-nod", NULL) ) {
                g.cmdopts.pass.no_detach = true;
            } else {
                usage_err = true;
                break;
            }

            /* kill */
        } else if (streq(g.cmdopts.cmd, CMD_KILL) ) {
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
        } else if (streq(g.cmdopts.cmd, CMD_SEND) ) {
            struct st_send * st = & g.cmdopts.send;
            if (str1of(arg, "-cstring", "-cstr", "-c", NULL) ) {
                st->cstring = true;
            } else if (str1of(arg, "-cr", "-enter", NULL) ) {
                st->enter = true;
            } else if (str1of(arg, "-strip", NULL) ) {
                st->strip = true;
            } else if (str1of(arg, "-env", "-var", NULL) ) {
                if (st->data != NULL) {
                    usage_err = true;
                    break;
                }
                st->env = true;
                next = nextarg(argv, arg, & i);
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
                next = nextarg(argv, arg, & i);
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
        } else if (streq(g.cmdopts.cmd, CMD_SET) ) {
            struct st_set * st = & g.cmdopts.set;
            if (str1of(arg, "-autowait", "-nowait", "-now", NULL) ) {
                st->set_autowait = true;

                next = nextarg(argv, arg, & i);
                if (str_true(next) ) {
                    st->autowait = true;
                } else if (str_false(next) ) {
                    st->autowait = false;
                } else {
                    arg = next;
                    usage_err = true;
                    break;
                }

                /* still supports `-discard' for backward compat */
            } else if (str1of(arg, "-nonblock", "-nb", "-discard", NULL ) ) {
                st->set_nonblock = true;

                next = nextarg(argv, arg, & i);
                if (str_true(next) ) {
                    st->nonblock = true;
                } else if (str_false(next) ) {
                    st->nonblock = false;
                } else {
                    arg = next;
                    usage_err = true;
                    break;
                }
            } else if (str1of(arg, "-timeout", "-t", NULL ) ) {
                st->set_timeout = true;
                next = nextarg(argv, arg, & i);
                st->timeout = arg2int(next);

                if (st->timeout < 0) {
                    st->timeout = -1;
                }
            } else if (str1of(arg, "-ttl", NULL ) ) {
                st->set_ttl = true;
                next = nextarg(argv, arg, & i);
                st->ttl = arg2int(next);

                if (st->ttl < 0) {
                    st->ttl = 0;
                }
            } else if (str1of(arg, "-idle-close", "-idle", NULL ) ) {
                st->set_idle = true;
                next = nextarg(argv, arg, & i);
                st->idle = arg2int(next);

                if (st->idle < 0) {
                    st->idle = 0;
                }
            } else {
                usage_err = true;
                break;
            }

            /* spawn */
        } else if (streq(g.cmdopts.cmd, CMD_SPAWN) ) {
            struct st_spawn * st = & g.cmdopts.spawn;
            if (streq(arg, "-nohup") ) {
                st->nohup = true;
            } else if (str1of(arg, "-autowait", "-nowait", "-now", NULL) ) {
                st->autowait = true;

                /* still supports `-discard' for backward compat */
            } else if (str1of(arg, "-nonblock", "-nb", "-discard", NULL) ) {
                st->nonblock = true;
            } else if (str1of(arg, "-close-on-exit", "-cloexit", NULL) ) {
                st->cloexit = true;
            } else if (str1of(arg, "-term", "-T", NULL) ) {
                next = nextarg(argv, arg, & i);
                if (strlen(next) == 0) {
                    fatal(ERROR_USAGE, "-term cannot be empty");
                }
                st->TERM = next;
            } else if (str1of(arg, "-timeout", "-t", NULL) ) {
                st->def_timeout = arg2int(nextarg(argv, arg, & i) );
                if (st->def_timeout < 0) {
                    st->def_timeout = -1;
                }
            } else if (str1of(arg, "-ttl", NULL) ) {
                st->ttl = arg2int(nextarg(argv, arg, & i) );
                if (st->ttl < 0) {
                    st->ttl = 0;
                }
            } else if (str1of(arg, "-idle-close", "-idle", NULL) ) {
                st->idle = arg2int(nextarg(argv, arg, & i) );
                if (st->idle < 0) {
                    st->idle = 0;
                }
            } else if (str1of(arg, "-logfile", "-logf", "-log", NULL) ) {
                st->logfile = nextarg(argv, arg, & i);
            } else if (str1of(arg, "-append", NULL) ) {
                st->append = true;
            } else if (str1of(arg, "-zombie-idle", "-z-idle", "-z",
                              /* DEPRECATED. It really does not mean TTL. */
                              "-zombie-ttl", "-zttl", NULL) ) {
                st->zombie_idle = arg2int(nextarg(argv, arg, & i) );
            } else if (arg[0] == '-') {
                fatal(ERROR_USAGE, "unknown spawn option: %s", arg);
            } else {
                st->argv = & argv[i];

                break;
            }

            /* version */
        } else if (streq(g.cmdopts.cmd, CMD_VERSION) ) {
            usage_err = true;
            break;

            /* wait */
        } else if (streq(g.cmdopts.cmd, CMD_WAIT) ) {
            if (str1of(arg, "-lookback", "-lb", NULL) ) {
                next = nextarg(argv, arg, & i);
                g.cmdopts.pass.lookback = arg2uint(next);
            } else {
                usage_err = true;
                break;
            }
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
    if (streq(g.cmdopts.cmd, CMD_EXPECT) ) {
        struct st_pass * st = & g.cmdopts.pass;
        int flags = st->expflags & (PASS_EXPECT_EOF | PASS_EXPECT_EXACT
                                    | PASS_EXPECT_ERE | PASS_EXPECT_GLOB);
        if (count1bits(flags) > 1) {
            fatal(ERROR_USAGE, "-eof, -exact, -glob and -re are exclusive");
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
            /* glob2re */
            if ((st->expflags & PASS_EXPECT_GLOB) != 0) {
                char * re_str = glob2re(st->pattern, & re_str, NULL);
                if (re_str == NULL) {
                    fatal(ERROR_USAGE, "invalid glob pattern: `%s'", st->pattern);
                }

                debug("glob2re: ``%s'' --> ``%s''", st->pattern, re_str);
                st->pattern = re_str;
                st->expflags &= ~PASS_EXPECT_GLOB;
                st->expflags |= PASS_EXPECT_ERE;
            }
        }

        /* help */
    } else if (streq(g.cmdopts.cmd, CMD_HELP) ) {
        usage(0);

        /* interact */
    } else if (streq(g.cmdopts.cmd, CMD_INTERACT) ) {
        struct st_pass * st = & g.cmdopts.pass;

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

        /* send */
    } else if (streq(g.cmdopts.cmd, CMD_SEND) ) {
        struct st_send * st = & g.cmdopts.send;
        char * data = NULL;

        if (st->data == NULL) {
            st->data = "";
        }

        if ( (st->file || st->env) && st->cstring) {
            fatal(ERROR_USAGE, "-cstring cannot be used with -file or -env");
        }

        if (st->strip && ! st->file) {
            fatal(ERROR_USAGE, "-strip can only be used with -file");
        }

        if (st->data != NULL && st->cstring) {
            strunesc(st->data, & data, & st->len);
            if (data == NULL) {
                fatal(ERROR_USAGE, "invalid backslash escapes: %s", st->data);
            } else {
                st->data = data;
            }
        }

        if (st->file && st->strip) {
            str_rstrip(st->data);
        }

        if (st->len > PASS_MAX_SEND) {
            fatal(ERROR_USAGE, "send: string length must be < %d", PASS_MAX_SEND);
        }

        /* spawn */
    } else if (streq(g.cmdopts.cmd, CMD_SPAWN) && g.cmdopts.spawn.argv == NULL) {
        fatal(ERROR_USAGE, "spawn requires more arguments");

        /* version */
    } else if (streq(g.cmdopts.cmd, CMD_VERSION) ) {
        printf("%s %s\n", SEXPECT, VERSION_);
        exit(0);
    }

    /* $SEXPECT_SOCKFILE */
    if (g.cmdopts.sockpath == NULL) {
        g.cmdopts.sockpath = getenv("SEXPECT_SOCKFILE");
    }
    /* most commands require ``-sock'' */
    if (g.cmdopts.sockpath == NULL && ! streq(g.cmdopts.cmd, CMD_CHKERR) ) {
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

    if (streq(g.cmdopts.cmd, CMD_SPAWN) ) {
        serv_main( & g.cmdopts);
    } else {
        cli_main( & g.cmdopts);
    }

    return 0;
}
