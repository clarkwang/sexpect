
#if defined(__gnu_linux__) || defined(__CYGWIN__)
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <regex.h>
#include <limits.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "common.h"
#include "proto.h"
#include "pty.h"

#define SIZE_RAW_BUF    (16 * 1024)
#define MAX_OLD_DATA    ( 8 * 1024)
#define EXPECT_OUT_NUM  (9 + 1)

#if (SIZE_RAW_BUF + 1024) > PASS_MAX_MSG
#error "SIZE_RAW_BUF too large compared to PASS_MAX_MSG"
#endif

/* N.B.:
 *  - Remember to update `serv_init()' accordingly when adding new fields
 *    to the struct.
 */
static struct {
    struct st_cmdopts * cmdopts;

    pid_t child;
    char  ptsname[32];
    int   fd_ptm, fd_listen;

    bool SIGCHLDed;
    bool waited;        /* client has called wait */
#if 0
    int  lasterr;       /* last errno */
#endif

    /* conn specific data, needs to be memset'ed for new conn */
    struct {
        int  sock;
        bool passing;
        struct {
            int    subcmd;      /* expect, interact, wait */
            int    expflags;
            char * pattern;
            int    timeout;
            int    lookback;
            struct timespec startime;
        } pass;
    } conn;

    /*
     * This will be updated when
     *  1) The server receives requests (including HELLO, DISCONN) from client.
     *  2) The connection is broken unexpectedly (e.g. when the client is killed).
     *
     * The initial value is the time when the process is spawned.
     */
    struct timespec lastactive;

    int64_t ntotal;     /* total # of bytes from ptm */
    int64_t rawoffset;  /* the offset (in `ntotal' bytes) of `rawbuf' */
    int64_t expoffset;  /* offset of next byte which needs to be copied
                         * to `expbuf' */
    char * rawbuf;      /* raw output from pts */
    int    rawbufsize;
    char * rawnew;      /* data not sent to client yet */
    int    newcnt;
    char * expbuf;      /* NULL bytes removed */
    int    expbufsize;
    int    expcnt;      /* current data in `expbuf' */
    char * expout[EXPECT_OUT_NUM]; /* $expect_out(N,string) */
} g;
#define is_CONNECTED    (g.conn.sock >= 0)
#define not_CONNECTED   ( ! is_CONNECTED)
#define is_PTM_OPEN     (g.fd_ptm    >= 0)
#define not_PTM_OPEN    ( ! is_PTM_OPEN)
#define is_CHLD_DEAD    (g.SIGCHLDed)
#define is_CHLD_WAITED  (g.waited)
#define is_PASSING      (g.conn.passing)
#define is_EXPECT       (g.conn.pass.subcmd == PASS_SUBCMD_EXPECT)
#define is_WAIT         (g.conn.pass.subcmd == PASS_SUBCMD_WAIT)
#define is_INTERACT     (g.conn.pass.subcmd == PASS_SUBCMD_INTERACT)
#define has_PATTERN     (g.conn.pass.pattern != NULL)

static void
daemonize(void)
{
    pid_t pid;

#if 0
    umask(0);
#endif

    pid = fork();
    if (pid < 0) {
        fatal_sys("fork");
    } else if (pid) {
        exit(0);
    }

    setsid();

#if 0
    pid = fork();
    if (pid < 0) {
        fatal_sys("fork");
    } else if (pid) {
        exit(0);
    }
#endif

    /* move this to after exec() or the child's cwd would also be changed */
#if 0
    chdir("/");
#endif
}

static void
free_expect_out(void)
{
    int i;

    for (i = 0; i < EXPECT_OUT_NUM; ++i) {
        free(g.expout[i]);
        g.expout[i] = NULL;
    }
}

static void
serv_sigCHLD(int signo)
{
    g.SIGCHLDed = true;

    Clock_gettime( & g.cmdopts->spawn.exittime);

    /* Don't close(fd_ptm) here! There may still data from pts for reading. */
}

static ttlv_t *
serv_new_error(int code, char * msg)
{
    ttlv_t * error;

    error = ttlv_new_struct(TAG_ERROR);
    ttlv_append_child(error,
        ttlv_new_int(TAG_ERROR_CODE, code),
        ttlv_new_text(TAG_ERROR_MSG, strlen(msg), msg),
        NULL);
    return error;
}

static ttlv_t *
serv_msg_recv(void)
{
    ttlv_t *msg;

    if (is_CONNECTED) {
        msg = msg_recv(g.conn.sock);
        if (msg == NULL) {
            debug("msg_recv failed (client dead?), closing the socket");
            close(g.conn.sock);
            g.conn.sock = -1;
            Clock_gettime( & g.lastactive);
            return NULL;
        } else {
            return msg;
        }
    } else {
        return NULL;
    }
}

static ssize_t
serv_msg_send(ttlv_t **msg, bool free_msg)
{
    int ret;

    if (not_CONNECTED) {
        bug("msg_send: connection already closed");
        errno = EBADF;
        return -1;
    }

    ret = msg_send(g.conn.sock, *msg);
    if (ret < 0) {
        debug("msg_send failed (client dead?), closing the socket");
        close(g.conn.sock);
        g.conn.sock = -1;
        Clock_gettime( & g.lastactive);
    }

    if (free_msg) {
        msg_free(msg);
    }

    return ret;
}

static void
serv_hello(void)
{
    if (not_CONNECTED) {
        return;
    }

    debug("sending HELLO");
    if (msg_hello(g.conn.sock) < 0) {
        debug("msg_hello failed (client dead?)");
        close(g.conn.sock);
        g.conn.sock = -1;
        Clock_gettime( & g.lastactive);
    }
}

static void
serv_disconn(void)
{
    if (not_CONNECTED) {
        return;
    }

    debug("sending DISCONN");
    if (msg_disconn(g.conn.sock) < 0) {
        debug("msg_disconn failed (client dead?)");
    }

    /* receive no more */
    debug("closing the socket");
    close(g.conn.sock);
    g.conn.sock = -1;

    Clock_gettime( & g.lastactive);
}

static void
serv_process_msg(void)
{
    char buf[1024];
    ttlv_t * msg_in = NULL;
    ttlv_t * msg_out = NULL;

    /* This _must_ be called before serv_msg_recv(), otherwise, for example, a
     * killed `sexpect expect -t N' would not update `g.lastactive'.
     *
     * Or this can be moved into `serv_msg_recv()'.
     */
    Clock_gettime( & g.lastactive);

    msg_in = serv_msg_recv();
    if (msg_in == NULL) {
        return;
    }

    switch (msg_in->tag) {

    case TAG_HELLO:
        debug("received HELLO");
        serv_hello();

        break;

    case TAG_DISCONN:
        debug("received DISCONN");
        serv_disconn();

        break;

    case TAG_SEND:
    case TAG_INPUT:
        {
            if (is_PTM_OPEN) {
                int nwritten = write(g.fd_ptm, msg_in->v_raw, msg_in->length);
                if (nwritten < 0) {
                    debug("write(ptm): %s (%d)", strerror(errno), errno);
                } else if (nwritten < msg_in->length) {
                    debug("write(ptm) returned %d (< %d)", nwritten, msg_in->length);
                }
            }
            if (msg_in->tag == TAG_SEND) {
                /* FIXME: send back data which are not written to the ptm */
                msg_out = ttlv_new_struct(TAG_ACK);
                serv_msg_send(&msg_out, true);
            }

            break;
        }

    case TAG_PASS:
        {
            ttlv_t * t = NULL;

            g.conn.passing = true;

            t = ttlv_find_child(msg_in, TAG_PASS_SUBCMD);
            g.conn.pass.subcmd = t->v_int;

            t = ttlv_find_child(msg_in, TAG_EXP_FLAGS);
            g.conn.pass.expflags = t->v_int;

            /* {expect|interact} with a pattern */
            if ( (t = ttlv_find_child(msg_in, TAG_PATTERN) ) != NULL) {
                g.conn.pass.pattern = strdup( (char *) t->v_text);

                free_expect_out();
            }

            /* expect -timeout */
            if ( (t = ttlv_find_child(msg_in, TAG_EXP_TIMEOUT) ) != NULL) {
                g.conn.pass.timeout = t->v_int;
            } else {
                g.conn.pass.timeout = g.cmdopts->spawn.def_timeout;
            }

            /* {interact|expect} -lookback */
            if ( (t = ttlv_find_child(msg_in, TAG_LOOKBACK) ) != NULL) {
                if (t->v_int > 0) {
                    g.conn.pass.lookback = t->v_int;
                }
            }

            break;
        }

    case TAG_EXPOUT:
        {
            int index = msg_in->v_int;

            if (index >= 0 && index < EXPECT_OUT_NUM) {
                if (g.expout[index] != NULL) {
                    msg_out = ttlv_new_text(TAG_EXPOUT_TEXT,
                        strlen(g.expout[index]),
                        g.expout[index]);
                } else {
                    msg_out = ttlv_new_text(TAG_EXPOUT_TEXT, 0, "");
                }
            } else {
                msg_out = serv_new_error(ERROR_USAGE, "index must in range 0-9");
            }
            serv_msg_send(&msg_out, true);

            break;
        }

    case TAG_WINCH:
        {
            struct winsize size = { 0 };
            ttlv_t * row, * col;

            if (not_PTM_OPEN) {
                break;
            }

            row = ttlv_find_child(msg_in, TAG_WINSIZE_ROW);
            col = ttlv_find_child(msg_in, TAG_WINSIZE_COL);

            /* Don't resize if new_size is the same as old_size. */
            if (ioctl(g.fd_ptm, TIOCGWINSZ, &size) < 0) {
                debug("ioctl(ptm, TIOCGWINSZ): %s (%d)", strerror(errno), errno);
            } else if (size.ws_row == row->v_int && size.ws_col == col->v_int) {
                debug("WINCH: already same winsize as the client");
                break;
            }

            size.ws_row = row->v_int;
            size.ws_col = col->v_int;
            debug("WINCH: change to %dx%d", (int)size.ws_col, (int)size.ws_row);
            if (ioctl(g.fd_ptm, TIOCSWINSZ, &size) < 0) {
                debug("ioctl(ptm, TIOCSWINSZ): %s (%d)", strerror(errno), errno);
            }

            break;
        }

    case TAG_CLOSE:
        if (is_PTM_OPEN) {
            close(g.fd_ptm);
            g.fd_ptm = -1;
        }
        msg_out = ttlv_new_struct(TAG_ACK);
        serv_msg_send(&msg_out, true);

        break;

    case TAG_KILL:
        {
            int signal = msg_in->v_int;
            if (kill(g.child, signal) < 0) {
                snprintf(buf, sizeof(buf), "kill: %s", strerror(errno) );
                debug("%s", buf);
                msg_out = serv_new_error(ERROR_SYS, buf);
            } else {
                msg_out = ttlv_new_struct(TAG_ACK);
            }
            serv_msg_send(&msg_out, true);

            break;
        }

    case TAG_SET:
        {
            ttlv_t * t;

            if ( (t = ttlv_find_child(msg_in, TAG_AUTOWAIT) ) != NULL) {
                g.cmdopts->spawn.autowait = t->v_bool;
            }
            if ( (t = ttlv_find_child(msg_in, TAG_NONBLOCK) ) != NULL) {
                g.cmdopts->spawn.nonblock = t->v_bool;
            }
            if ( (t = ttlv_find_child(msg_in, TAG_EXP_TIMEOUT) ) != NULL) {
                g.cmdopts->spawn.def_timeout = t->v_int;
            }
            if ( (t = ttlv_find_child(msg_in, TAG_TTL) ) != NULL) {
                g.cmdopts->spawn.ttl = t->v_int;
            }
            if ( (t = ttlv_find_child(msg_in, TAG_IDLETIME) ) != NULL) {
                g.cmdopts->spawn.idle = t->v_int;
            }

            msg_out = ttlv_new_struct(TAG_ACK);
            serv_msg_send(&msg_out, true);

            break;
        }

    case TAG_INFO:
        {
            msg_out = ttlv_new_struct(TAG_INFO);

            ttlv_append_child(
                msg_out,
                ttlv_new_int(TAG_PID,  (int) g.child),
                ttlv_new_int(TAG_PPID, (int) getpid() ),
                ttlv_new_text(TAG_PTSNAME, strlen(g.ptsname), g.ptsname),
                ttlv_new_int(TAG_EXP_TIMEOUT, g.cmdopts->spawn.def_timeout),
                ttlv_new_bool(TAG_AUTOWAIT,   g.cmdopts->spawn.autowait),
                ttlv_new_bool(TAG_NONBLOCK,   g.cmdopts->spawn.nonblock),
                ttlv_new_int(TAG_TTL,         g.cmdopts->spawn.ttl),
                ttlv_new_int(TAG_IDLETIME,    g.cmdopts->spawn.idle),
                ttlv_new_int(TAG_ZOMBIE_TTL,  g.cmdopts->spawn.zombie_idle),
                NULL);
            serv_msg_send( & msg_out, true);

            break;
        }
    }

    msg_free(&msg_in);
}

static void
serv_read_ptm(void)
{
    int nread, ntoread;
    int oldcnt;

    oldcnt = g.rawnew - g.rawbuf;

    while (true) {
        ntoread = g.rawbufsize - (g.newcnt + oldcnt);

        /* I used to be stupid and forget to check `== 0'. */
        if (ntoread == 0) {
            /* raw buffer full */
            return;
        }

        /* cannot use `readn()' here as it's in non-blocking mode */
        nread = read(g.fd_ptm, g.rawnew + g.newcnt,
                     g.rawbufsize - (g.newcnt + oldcnt) );
        if (nread > 0) {
            break;
        }

        if (nread <= 0) {
            /*
             * - On Linux, read() would return -1 (EIO) after the pts is closed
             *   (by all child processes).
             * - On macOS, read() would return 0 (EOF) after the pts is closed.
             *   Note that it does not wait for child processes to close the
             *   pts. Only the parent process exiting would cause read() to
             *   return 0.
             */
            if (nread == 0) {
                debug("read(ptm) returned 0 (EOF)");
            } else {
                if (errno == EINTR) {
                    continue;
                } else if (errno == EAGAIN) {
                    debug("read(ptm) returned EAGAIN");
                    return;
                } else {
                    /* other fatal errors */
                    debug("read(ptm) returned -1: %s (%d)", strerror(errno), errno);
                }
            }

            debug("close(ptm)");
            close(g.fd_ptm);
            g.fd_ptm = -1;

            return;
        }
    }

    /* logfile */
    if (g.cmdopts->spawn.logfd >= 0) {
        /* ignore any errors */
        write(g.cmdopts->spawn.logfd, g.rawnew + g.newcnt, nread);
    }

    g.newcnt += nread;
    g.ntotal += nread;
}

static void
drop_old_data(void)
{
    int oldcnt, dropsize;

    /* keep at most MAX_OLD_DATA old raw data */
    oldcnt = g.rawnew - g.rawbuf;
    if (oldcnt > MAX_OLD_DATA) {
        dropsize = oldcnt - MAX_OLD_DATA;
        oldcnt = MAX_OLD_DATA;

        memmove(g.rawbuf, g.rawbuf + dropsize, oldcnt + g.newcnt);
        g.rawnew -= dropsize;
        g.rawoffset += dropsize;
    }

    /* keep at most MAX_OLD_DATA expect buffer data */
    if (g.expcnt > MAX_OLD_DATA) {
        dropsize = g.expcnt - MAX_OLD_DATA;

        memmove(g.expbuf, g.expbuf + dropsize,  MAX_OLD_DATA);
        g.expcnt = MAX_OLD_DATA;
        g.expbuf[g.expcnt] = '\0';
    }
}

/* copy to "expect" buffer with NULL bytes removed */
static void
buf_raw2expect(void)
{
    int i, ncopy;
    char * copy_start;

    if (g.expoffset < g.rawoffset) {
        g.expoffset = g.rawoffset;
        g.expcnt = 0;
    }

    ncopy = g.ntotal - g.expoffset;
    assert(g.expcnt + ncopy <= g.expbufsize);

    g.expoffset = g.ntotal;
    copy_start = g.rawnew + g.newcnt - ncopy;
    for (i = 0; i < ncopy; ++i) {
        if (copy_start[i] != '\0') {
            g.expbuf[g.expcnt] = copy_start[i];
            ++ g.expcnt;
        }
    }
    g.expbuf[g.expcnt] = '\0';
}

static bool
expect_exact(void)
{
    int pattern_len;
    char * found;

    if ((g.conn.pass.expflags & PASS_EXPECT_ICASE) != 0) {
        found = strcasestr(g.expbuf, g.conn.pass.pattern);
    } else {
        found = strstr(g.expbuf, g.conn.pass.pattern);
    }
    if (found != NULL) {
        pattern_len = strlen(g.conn.pass.pattern);
        g.expcnt = strlen(found + pattern_len);
        memmove(g.expbuf, found + pattern_len, g.expcnt);
        g.expbuf[g.expcnt] = '\0';

        free_expect_out();
        g.expout[0] = strdup(g.conn.pass.pattern);

        return true;
    }

    return false;
}

static bool
expect_glob(void)
{
    bug("server side should never see PASS_EXPECT_GLOB");
    return false;
}

static bool
expect_ere(void)
{
    regex_t re;
    regmatch_t matches[EXPECT_OUT_NUM];
    int reflags = REG_EXTENDED;
    int i, ret, len;
    bool nosub = false;

    if ((g.conn.pass.expflags & PASS_EXPECT_NOSUB) != 0) {
        nosub = true;
    }
    if ((g.conn.pass.expflags & PASS_EXPECT_ICASE) != 0) {
        reflags |= REG_ICASE;
    }

    ret = regcomp( & re, g.conn.pass.pattern, reflags);
    if (ret != 0) {
        return false;
    }

    ret = regexec( & re, g.expbuf, EXPECT_OUT_NUM, matches, 0);
    regfree( & re);
    if (ret != 0) {
        return false;
    }

    /* $expect_out(N,string) */
    if ( ! nosub) {
        free_expect_out();

        for (i = 0; i < EXPECT_OUT_NUM; ++i) {
            if (matches[i].rm_so == -1) {
                continue;
            }

            len = matches[i].rm_eo - matches[i].rm_so;
            g.expout[i] = malloc(len + 1);
            memcpy(g.expout[i], g.expbuf + matches[i].rm_so, len);
            g.expout[i][len] = 0;
        }
    }

    g.expcnt = strlen(g.expbuf + matches[0].rm_eo);
    memmove(g.expbuf, g.expbuf + matches[0].rm_eo, g.expcnt);
    g.expbuf[g.expcnt] = '\0';

    return true;
}

static bool
serv_expect(void)
{
    if (g.expcnt == 0 && not_PTM_OPEN) {
        /* ptm is closed and there's no data in expect buf */
        return false;
    }

    if ((g.conn.pass.expflags & PASS_EXPECT_EXACT) != 0) {
        return expect_exact();
    } else if ((g.conn.pass.expflags & PASS_EXPECT_GLOB) != 0) {
        return expect_glob();
    } else if ((g.conn.pass.expflags & PASS_EXPECT_ERE) != 0) {
        return expect_ere();
    } else {
        return false;
    }
}

static bool
exp_timed_out(void)
{
    if (g.conn.pass.timeout < 0) {
        return false;
    } else if (g.conn.pass.timeout == 0) {
        return true;
    }

    if (Clock_diff( & g.conn.pass.startime, NULL) > g.conn.pass.timeout) {
        return true;
    }

    return false;
}

static void
serv_pass(void)
{
    ttlv_t * msg_out;
    int exitstatus;
    int lookback, newlines, nsend;
    char * pc = NULL, * psend = NULL;

    /* expect/interact/wait */
    if (not_CONNECTED || ! is_PASSING) {
        return;
    }

    /* output from child */
#if 1
    lookback = g.conn.pass.lookback;
    if (lookback <= 0) {
        psend = g.rawnew;
    } else {
        /* don't forget this ! */
        g.conn.pass.lookback = 0;

        /* printf 'foo\nbar' | tail -n 1
         *
         *   vs.
         *
         * printf 'foo\nbar\n' | tail -n 1
         */
        if (g.ntotal > 0 && g.rawnew[g.newcnt - 1] == '\n') {
            ++lookback;
        }

        newlines = 0;
        for (pc = g.rawnew + g.newcnt - 1; pc >= g.rawbuf; --pc) {
            if (pc[0] == '\n') {
                if (++newlines >= lookback) {
                    break;
                }
            }
        }
        if (newlines == 0) {
            /* [<] No NLs at all, e.g. the first shell prompt */
            psend = g.rawbuf;
        } else if (newlines < lookback) {
            /* [<] There are not enough NLs in the whole buffer (old + new),
             *     start from the first NL, or rawbuf if rawoffset is 0.
             */
            if (g.rawoffset == 0) {
                psend = g.rawbuf;
            } else {
                /* find the first NL */
                pc = strchr(g.rawbuf, '\n');
                if (pc < g.rawnew) {
                    /* [<] The fist NL is in the OLD buffer */
                    psend = pc + 1;
                } else {
                    /* [<] The fist NL is in the NEW buffer */
                    psend = g.rawnew;
                }
            }
        } else {
            /* [<] Found #lookback NLs, `pc' now points to a NL */
            if (pc < g.rawnew) {
                psend = pc + 1;
            } else {
                psend = g.rawnew;

                /* start the output from the nearest NL before `rawnew'
                 * if possible */
                for (pc = g.rawnew - 1; pc >= g.rawbuf; --pc) {
                    if (pc[0] == '\n') {
                        psend = pc + 1;
                        break;
                    }
                }
                if (pc < g.rawbuf) {
                    /* [<] No NLs in old buffer */
                    if (g.rawoffset == 0) {
                        psend = g.rawbuf;
                    }
                }
            }
        }
    }

    nsend = g.rawnew + g.newcnt - psend;
    if (nsend > 0) {
        msg_out = ttlv_new_raw(TAG_OUTPUT, nsend, psend);
        if (serv_msg_send(&msg_out, true) < 0) {
            return;
        }

        g.rawnew += g.newcnt;
        g.newcnt = 0;
    }
#else
    /* no -lookback support */
    if (g.newcnt > 0) {
        msg_out = ttlv_new_raw(TAG_OUTPUT, g.newcnt, g.rawnew);
        if (serv_msg_send(&msg_out, true) < 0) {
            return;
        }

        g.rawnew += g.newcnt;
        g.newcnt = 0;
    }
#endif

    /* "expect" or "interact -re" */
    if (has_PATTERN) {
        /* copy data from raw buf to expect buf */
        buf_raw2expect();
    }

    /* "expect" or "interact" with a pattern */
    if (has_PATTERN) {
        if (serv_expect() ) {
            msg_out = ttlv_new_bool(TAG_MATCHED, 1);
            serv_msg_send(&msg_out, true);

            g.conn.passing = false;

            return;
        }

        /* interact, wait */
    } else if (is_INTERACT || is_WAIT) {
        g.expoffset = g.ntotal - g.newcnt;
        g.expcnt = 0;
    }

    /* Having received SIGCHLD does not necessarily mean EOF. There may still
     * data from the child for reading. So only report EOF when fd_ptm < 0.
     */
    if (not_PTM_OPEN && g.newcnt == 0) {
        if ((g.conn.pass.expflags & PASS_EXPECT_EOF) != 0) {
            /* [<] expect -eof */

            g.expoffset = g.ntotal;
            g.expcnt = 0;

            msg_out = ttlv_new_struct(TAG_EOF);
            serv_msg_send(&msg_out, true);

            g.conn.passing = false;
        } else if ( (g.conn.pass.expflags & PASS_EXPECT_EXIT) != 0) {
            /* [<] interact, wait */

            if ( ! is_CHLD_WAITED && is_CHLD_DEAD) {
                waitpid(g.child, &  exitstatus, 0);
                g.waited = true;

                msg_out = ttlv_new_int(TAG_EXITED, exitstatus);
                serv_msg_send(&msg_out, true);

                g.conn.passing = false;
            } else {
                /* wait for the child to exit */
            }
        } else {
            /* expect with a pattern */
            msg_out = serv_new_error(ERROR_EOF, "PTY closed");
            serv_msg_send(&msg_out, true);

            g.conn.passing = false;
        }

        return;
    }

    /* "expect" timed out */
    if (exp_timed_out() ) {
        msg_out = serv_new_error(ERROR_TIMEOUT, "expect timed out");
        serv_msg_send(&msg_out, true);

        g.conn.passing = false;

        return;
    }
}

static void
serv_cleanup_conn(void)
{
    if (g.conn.pass.pattern != NULL) {
        free(g.conn.pass.pattern);
    }

    memset( & g.conn, 0, sizeof(g.conn) );
    g.conn.sock = -1;
}

static void
serv_loop(void)
{
    int r;
    int newconn, fd_max;
    struct sockaddr_un cli_addr;
    socklen_t sock_len;
    fd_set readfds;
    struct timeval timeout;
    struct st_spawn * spawn = & g.cmdopts->spawn;

    /* N.B.:
     *  - The child's exiting does not necessarily mean the pty has been closed
     *    because the child's children are still opening the pty.
     *  - read(ptm) returning EOF does not necessarily mean the child has
     *    exited.
     *  - After the child exits (SIGCHLD) and before closing ptm, there may
     *    still some data in the ptm side for reading.
     *  - After the child exits and ptm is closed, there may still some data
     *    in "rawbuf" when can be sent to the client (interact/expect/wait).
     *  - After the child exits and ptm is closed, there may still some data
     *    in "rawbuf" which has not been copied to "expbuf" for "expect".
     */
    while ( ! is_CHLD_WAITED || is_CONNECTED) {
        /* -cloexit */
        if (spawn->cloexit && is_CHLD_DEAD && is_PTM_OPEN) {
            /* [<] The child has exited but the pty is still open which
             *     means the child's children are still opening the pty. */
            static int n = 0;
            if (++n > 1) {
                debug("child exited, closing ptm (-cloexit)");
                close(g.fd_ptm);
                g.fd_ptm = -1;
            }
        }

        /* -nowait */
        if (spawn->autowait
            && is_CHLD_DEAD && not_PTM_OPEN && not_CONNECTED) {
            debug("child exited, exiting too (-nowait)");
            break;
        }

        /* -zombie-idle */
        if (spawn->zombie_idle >= 0
            && is_CHLD_DEAD && not_PTM_OPEN && not_CONNECTED) {
            if (Clock_diff( & g.lastactive, NULL) > spawn->zombie_idle
                && Clock_diff( & spawn->exittime, NULL) > spawn->zombie_idle) {
                debug("the zombie's been idle for %d seconds. killing it now.",
                      spawn->zombie_idle);
                break;
            }
        }

        /* -ttl */
        if (spawn->ttl > 0 && not_CONNECTED) {
            if (Clock_diff( & spawn->startime, NULL) > spawn->ttl) {
                debug("server has been alive for TTL (=%d) seconds, bye",
                      spawn->ttl);
                break;
            }
        }

        /* -idle */
        if (spawn->idle > 0 && not_CONNECTED) {
            if (Clock_diff( & g.lastactive, NULL) > spawn->idle) {
                debug("server has been IDLE for %d seconds, bye", spawn->idle);
                break;
            }
        }

        FD_ZERO( & readfds);
        fd_max = 0;

        /* listen to new connections */
        if (not_CONNECTED) {
            FD_SET(g.fd_listen, & readfds);
            if (g.fd_listen > fd_max) {
                fd_max = g.fd_listen;
            }
        }

        /* read from ptm */
        if (is_PTM_OPEN) {
            /* [>] This checking is very important or the server may use 100%
             *     CPU. (example: sexpect sp hexdump /dev/urandom) */
            if (g.rawnew + g.newcnt < g.rawbuf + g.rawbufsize) {
                FD_SET(g.fd_ptm, & readfds);
                if (g.fd_ptm > fd_max) {
                    fd_max = g.fd_ptm;
                }
            }
        }

        /* wait for client requests */
        if (is_CONNECTED) {
            FD_SET(g.conn.sock, & readfds);
            if (g.conn.sock > fd_max) {
                fd_max = g.conn.sock;
            }
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 10 * 1000;
        /* FIXME: [??] On macOS, select returns -1 (EBADF) after pts is closed */
        r = select(fd_max + 1, & readfds, NULL, NULL, & timeout);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                fatal_sys("select");
            }
        }

        /* new connect request */
        if (FD_ISSET(g.fd_listen, & readfds) ) {
            sock_len = sizeof(cli_addr);
            newconn = accept(g.fd_listen, (struct sockaddr *) & cli_addr,
                             & sock_len);
            if (newconn < 0) {
                if (errno == EINTR) {
                    continue;
                } else {
                    fatal_sys("accept");
                }
            } else {
                if (is_CONNECTED) {
                    bug("old conn still alive!");
                    close(g.conn.sock);
                }
                debug("new client connected");
                serv_cleanup_conn();
                g.conn.sock = newconn;
                Clock_gettime( & g.conn.pass.startime);
            }
        }

        /* new data from pty */
        if (is_PTM_OPEN) {
            if (FD_ISSET(g.fd_ptm, & readfds) ) {
                serv_read_ptm();
            }
        }
        /* -nonblock is ON */
        if (spawn->nonblock && not_CONNECTED) {
            /* mark "new" data as "old" when -nonblock is ON */
            if (g.newcnt > 0) {
                g.rawnew += g.newcnt;
                g.newcnt = 0;
            }

            /* DO NOT UNCOMMENT THIS! */
#if 0
            /* mark all data as expect'ed */
            g.expoffset = g.ntotal;
            g.expcnt = 0;
            g.expbuf[0] = '\0';
#endif
        }

        /* new message from client */
        if (is_CONNECTED && FD_ISSET(g.conn.sock, & readfds) ) {
            serv_process_msg();
        }

        /* expect/interact/wait */
        if (is_CONNECTED && is_PASSING) {
            serv_pass();
        }

        drop_old_data();
    }
}

static void
serv_init(void)
{
    g.conn.sock   = -1;

    Clock_gettime( & g.lastactive);

    g.rawbufsize = SIZE_RAW_BUF;
    g.expbufsize = SIZE_RAW_BUF;

    g.rawbuf = malloc(g.rawbufsize + 1);
    g.expbuf = malloc(g.expbufsize + 1);
    g.rawnew = g.rawbuf;

    g.newcnt = 0;
    g.expcnt = 0;

    g.ntotal    = 0;
    g.rawoffset = 0;
    g.expoffset = 0;
}

void
serv_main(struct st_cmdopts * cmdopts)
{
    pid_t pid;
    int ret;
    int fd_conn;
    bool ontty = false;
    struct winsize ws;
    struct sockaddr_un srv_addr = { 0 };
    struct st_spawn * spawn = NULL;

    serv_init();

    g.cmdopts = cmdopts;
    spawn = & cmdopts->spawn;

    /* get current winsize */
    if (isatty(STDIN_FILENO) ) {
        if (ioctl(STDIN_FILENO, TIOCGWINSZ, & ws) < 0) {
            debug("ioctl(stdin, TIOCGWINSZ) failed: %s (%d)",
                  strerror(errno), errno);
        } else {
            ontty = true;
            debug("current winsize: %dx%d", ws.ws_col, ws.ws_row);
        }
    }

    /*
     * check if the sockpath is in use
     */
    if (1) {
        debug("check if another server is using the sockfile");
        fd_conn = sock_connect(cmdopts->sockpath);
        if (fd_conn >= 0) {
            fatal(ERROR_GENERAL, "sockpath in use");
        }
    }

    /* open logfile
     *
     * NOTE: This must be done before chdir("/") in case the logfile is
     *       specified with a relative pathname.
     */
    if (spawn->logfile != NULL) {
        debug("open the logfile");
        if (spawn->append) {
            spawn->logfd = open(spawn->logfile, O_WRONLY | O_APPEND);
        } else {
            spawn->logfd = open(spawn->logfile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        }
        if (spawn->logfd < 0) {
            debug("open(logfile): %s (%d)", strerror(errno), errno);
        }
    }

    /* socket() */
    g.fd_listen = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (g.fd_listen < 0) {
        fatal_sys("socket");
    }

    /* or bind() would fail: Address already in use (EADDRINUSE) */
    unlink(cmdopts->sockpath);

    /* bind() */
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sun_family = AF_LOCAL;
    strcpy(srv_addr.sun_path, cmdopts->sockpath);
    /* don't let other users connect to my server! */
    umask(0077);
    if (bind(g.fd_listen, (struct sockaddr *) &srv_addr, sizeof(srv_addr) ) < 0) {
        fatal_sys("bind");
    }

    /* convert to full pathname */
    {
        char * fullpath = realpath(cmdopts->sockpath, NULL);
        if (fullpath == NULL) {
            fatal_sys("realpath(%s)", cmdopts->sockpath);
        } else {
            cmdopts->sockpath = fullpath;
        }
    }

    /* start listening before becoming a daemon so client does not need to wait
     * before trying to connect */
    if (listen(g.fd_listen, 0) < 0) {
        fatal_sys("listen");
    }

    /* be an evil daemon */
    if (! cmdopts->debug) {
        int fd;

        daemonize();

        fd = open("/dev/null", O_RDWR);
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);

        /* N.B: Don't close ALL ! */
        for (fd = 3; fd < 16; ++fd) {
            if (fd != g.fd_listen && fd != cmdopts->spawn.logfd) {
                close(fd);
            }
        }
    }

    /* spawn the child */
    pid = pty_fork(&g.fd_ptm, g.ptsname, sizeof(g.ptsname), NULL, NULL);
    if (pid < 0) {
        fatal_sys("fork");
    } else if (pid == 0) {
        /* child */
        close(g.fd_listen);

        if (cmdopts->spawn.nohup) {
            sig_handle(SIGHUP, SIG_IGN);
        }

        if (cmdopts->spawn.TERM != NULL) {
            if (setenv("TERM", cmdopts->spawn.TERM, 1) < 0) {
                fatal_sys("setenv(TERM)");
            }
        }

        /* don't pass SEXPECT_SOCKFILE to the spawned process */
        unsetenv("SEXPECT_SOCKFILE");

        /* set pty winsize if we are on a tty */
        if (ontty) {
            if (ioctl(STDIN_FILENO, TIOCSWINSZ, & ws) < 0) {
                error("failed to set winsize: %s (%d)", strerror(errno), errno);
            }
        }

        if (execvp(cmdopts->spawn.argv[0], cmdopts->spawn.argv) < 0) {
            fatal_sys("failed to spawn '%s'", cmdopts->spawn.argv[0]);
        }
    } else {
        /* This must be after exec() or the following usage would not work
         * as expected:
         *
         *  $ sexpect spawn cat a-file-in-current-dir
         */
        chdir("/");

        g.child = pid;
        Clock_gettime( & spawn->startime);
    }

    /* set ptm to be non-blocking */
    if (1) {
        /*
         * On macOS, fcntl(O_NONBLOCK) may fail before the child opens the
         * pts. So wait a while (until pty/master becomes writable) for the
         * child to open the pts.
         */
        fd_set writefds;
        struct timeval timeout;

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        FD_ZERO(&writefds);
        FD_SET(g.fd_ptm, &writefds);

        debug("waiting for the child to open pts");
        select(g.fd_ptm + 1, NULL, &writefds, NULL, &timeout);
        if (! FD_ISSET(g.fd_ptm, &writefds) ) {
            fatal(ERROR_GENERAL, "failed to wait for the child to open pts");
        }

        ret = fcntl(g.fd_ptm, F_SETFL, O_NONBLOCK);
        if (ret < 0) {
            fatal_sys("fcntl(ptm)");
        }
    }

    sig_handle(SIGPIPE, SIG_IGN);
    sig_handle(SIGCHLD, serv_sigCHLD);

    debug("ready to recv requests");
    serv_loop();

    debug("removing %s", cmdopts->sockpath);
    unlink(cmdopts->sockpath);

    exit(0);
}
