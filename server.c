
#if defined(__gnu_linux__) || defined(__CYGWIN__)
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <regex.h>
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

#define SIZE_RAW_BUF    (8 * 1024)
#define MAX_OLD_DATA    (4 * 1024)

#if (SIZE_RAW_BUF + 1024) > PASS_MAX_MSG
#error "SIZE_RAW_BUF too large compared to PASS_MAX_MSG"
#endif

static struct {
    struct st_cmdopts * cmdopts;

    int   syncpipe[2];
    pid_t child;
    char  ptsname[32];
    int   fd_ptm, fd_listen;

    bool SIGCHLDed;
    bool waited;        /* client has called wait */
    int  lasterr;       /* last errno */

    /* conn specific data, needs to be memset'ed for new conn */
    struct {
        int  sock;
        bool passing;
        struct {
            int    expflags;
            char * pattern;
            int    timeout;
            struct timespec start_time;
        } pass;
    } conn;

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
    char * expout[10];  /* $expect_out(N,string) */
} g;

static int
daemonize(int nochdir, int noclose)
{
    int fd, ret;
    pid_t pid;

    ret = pipe(g.syncpipe);
    if (ret < 0) {
        fatal_sys("pipe");
    }

    umask(0);

    pid = fork();
    if (pid < 0) {
        fatal_sys("fork");
    } else if (pid) {
        fd_set readfds;
        struct timeval timeout;

        close(g.syncpipe[1]);

        FD_ZERO( & readfds);
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        FD_SET(g.syncpipe[0], & readfds);
        select(g.syncpipe[0] + 1, & readfds, NULL, NULL, & timeout);
        close(g.syncpipe[0]);

        exit(0);
    } else {
        close(g.syncpipe[0]);
    }

    setsid();

    pid = fork();
    if (pid < 0) {
        fatal_sys("fork");
    } else if (pid) {
        exit(0);
    }

    if (! nochdir) {
        chdir("/");
    }

    if (! noclose) {
        fd = open("/dev/null", O_RDWR);
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
#if 1
        for (fd = 3; fd < 16; ++fd) {
            if (fd != g.syncpipe[1]) {
                close(fd);
            }
        }
#endif
    }

    return 0;
}

static void
serv_sigCHLD(int signo)
{
    g.SIGCHLDed = true;

    /* Don't close(fd_ptm) here! There may still data from pts for reading. */
}

static ptag_t *
serv_new_error(int code, char * msg)
{
    ptag_t * error;

    g.lasterr = code;

    error = ptag_new_struct(PTAG_ERROR);
    ptag_append_child(error,
        ptag_new_int(PTAG_ERROR_CODE, code),
        ptag_new_text(PTAG_ERROR_MSG, strlen(msg), msg),
        NULL);
    return error;
}

static ptag_t *
serv_msg_recv(void)
{
    ptag_t *msg;

    if (g.conn.sock >= 0) {
        msg = msg_recv(g.conn.sock);
        if (msg == NULL) {
            debug("msg_recv failed (client dead?), closing the socket");
            close(g.conn.sock);
            g.conn.sock = -1;
            return NULL;
        } else {
            return msg;
        }
    } else {
        return NULL;
    }
}

static ssize_t
serv_msg_send(ptag_t **msg, bool free_msg)
{
    int ret;

    if (g.conn.sock < 0) {
        bug("msg_send: connection already closed");
        errno = EBADF;
        return -1;
    }

    ret = msg_send(g.conn.sock, *msg);
    if (ret < 0) {
        debug("msg_send failed (client dead?), closing the socket");
        close(g.conn.sock);
        g.conn.sock = -1;
    }

    if (free_msg) {
        msg_free(msg);
    }

    return ret;
}

static void
serv_disconn(void)
{
    if (g.conn.sock < 0) {
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
}

static void
serv_process_msg(void)
{
    char buf[1024];
    ptag_t * msg_in = NULL;
    ptag_t * msg_out = NULL;

    msg_in = serv_msg_recv();
    if (msg_in == NULL) {
        return;
    }

    if (g.fd_ptm < 0 || g.SIGCHLDed) {
        switch (msg_in->tag) {

            /* ignore */
        case PTAG_INPUT:
        case PTAG_WINCH:
            msg_free( & msg_in);
            return;

            /* ERROR_EOF */
        case PTAG_CLOSE:
        case PTAG_SEND:
            msg_free( & msg_in);

            snprintf(buf, sizeof(buf), "pty closed");
            msg_out = serv_new_error(ERROR_EOF, buf);
            serv_msg_send(&msg_out, true);

            return;
        }
    }

    switch (msg_in->tag) {

    case PTAG_DISCONN:
        debug("received DISCONN");
        serv_disconn();

        break;

    case PTAG_SEND:
    case PTAG_INPUT:
        {
            int nwritten = write(g.fd_ptm, msg_in->v_raw, msg_in->length);
            if (nwritten < 0) {
                debug("write(ptm): %s (%d)", strerror(errno), errno);
            } else if (nwritten < msg_in->length) {
                debug("write(ptm) returned %d (< %d)", nwritten, msg_in->length);
            }
            if (msg_in->tag == PTAG_SEND) {
                /* FIXME: send back data which are not written to the ptm */
                msg_out = ptag_new_struct(PTAG_ACK);
                serv_msg_send(&msg_out, true);
            }

            break;
        }

    case PTAG_PASS:
        {
            ptag_t * t;

            g.conn.passing = true;

            t = ptag_find_child(msg_in, PTAG_EXP_FLAGS);
            g.conn.pass.expflags = t->v_int;

            if ( (t = ptag_find_child(msg_in, PTAG_PATTERN) ) != NULL) {
                g.conn.pass.pattern = strdup( (char *) t->v_text);
            }

            if ( (t = ptag_find_child(msg_in, PTAG_EXP_TIMEOUT) ) != NULL) {
                g.conn.pass.timeout = t->v_int;
            } else {
                g.conn.pass.timeout = g.cmdopts->spawn.def_timeout;
            }

            break;
        }

    case PTAG_EXPOUT:
        {
            int index = msg_in->v_int;

            if (index >= 0 && index < 10) {
                if (g.expout[index] != NULL) {
                    msg_out = ptag_new_text(PTAG_EXPOUT_TEXT,
                        strlen(g.expout[index]),
                        g.expout[index]);
                } else {
                    msg_out = ptag_new_text(PTAG_EXPOUT_TEXT, 0, "");
                }
            } else {
                msg_out = serv_new_error(ERROR_USAGE, "index must in range 0-9");
            }
            serv_msg_send(&msg_out, true);

            break;
        }

    case PTAG_WINCH:
        {
            struct winsize size = { 0 };
            ptag_t * row, * col;

            row = ptag_find_child(msg_in, PTAG_WINSIZE_ROW);
            col = ptag_find_child(msg_in, PTAG_WINSIZE_COL);

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

    case PTAG_CLOSE:
        close(g.fd_ptm);
        g.fd_ptm = -1;
        msg_out = ptag_new_struct(PTAG_ACK);
        serv_msg_send(&msg_out, true);

        break;

    case PTAG_KILL:
        {
            int signal = msg_in->v_int;
            if (kill(g.child, signal) < 0) {
                snprintf(buf, sizeof(buf), "kill: %s", strerror(errno) );
                debug("%s", buf);
                msg_out = serv_new_error(ERROR_SYS, buf);
            } else {
                msg_out = ptag_new_struct(PTAG_ACK);
            }
            serv_msg_send(&msg_out, true);

            break;
        }

    case PTAG_SET:
        {
            ptag_t * t;

            if ( (t = ptag_find_child(msg_in, PTAG_AUTOWAIT) ) != NULL) {
                g.cmdopts->spawn.autowait = t->v_bool;
            }
            if ( (t = ptag_find_child(msg_in, PTAG_DISCARD) ) != NULL) {
                g.cmdopts->spawn.discard = t->v_bool;
            }
            if ( (t = ptag_find_child(msg_in, PTAG_EXP_TIMEOUT) ) != NULL) {
                g.cmdopts->spawn.def_timeout = t->v_int;
            }

            msg_out = ptag_new_struct(PTAG_ACK);
            serv_msg_send(&msg_out, true);

            break;
        }

    case PTAG_INFO:
        {
            msg_out = ptag_new_struct(PTAG_INFO);

            ptag_append_child(
                msg_out,
                ptag_new_int(PTAG_PID,  (int) g.child),
                ptag_new_int(PTAG_PPID, (int) getpid() ),
                ptag_new_text(PTAG_PTSNAME, strlen(g.ptsname), g.ptsname),
                ptag_new_int(PTAG_EXP_TIMEOUT, g.cmdopts->spawn.def_timeout),
                ptag_new_bool(PTAG_AUTOWAIT,   g.cmdopts->spawn.autowait),
                ptag_new_bool(PTAG_DISCARD,    g.cmdopts->spawn.discard),
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
    int oldcnt, dropsize;

    /* keep at most MAX_OLD_DATA old data */
    oldcnt = g.rawnew - g.rawbuf;
    if (oldcnt > MAX_OLD_DATA) {
        dropsize = oldcnt - MAX_OLD_DATA;
        oldcnt = MAX_OLD_DATA;

        memmove(g.rawbuf, g.rawbuf + dropsize, oldcnt + g.newcnt);
        g.rawnew -= dropsize;
        g.rawoffset += dropsize;
    }

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
            /* On Linux (and macOS?), read() would return -1 (EIO) after the
             * pts is closed. */
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
    int i;
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

        /* $expect_out(0,string) */
        for (i = 0; i < 10; ++i) {
            /* free old $expect_out */
            free(g.expout[i]);
            g.expout[i] = NULL;
        }
        g.expout[0] = strdup(g.conn.pass.pattern);

        return true;
    }

    return false;
}

static bool
expect_glob(void)
{
    return false;
}

static bool
expect_ere(void)
{
    regex_t re;
    regmatch_t matches[10];
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

    ret = regexec( & re, g.expbuf, 10, matches, 0);
    regfree( & re);
    if (ret != 0) {
        return false;
    }

    /* $expect_out(N,string) */
    if ( ! nosub) {
        for (i = 0; i < 10; ++i) {
            /* free old $expect_out */
            free(g.expout[i]);
            g.expout[i] = NULL;

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
    if (g.expcnt == 0 && g.fd_ptm < 0) {
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
    struct timespec nowspec;
    double start, now;

    if (g.conn.pass.timeout < 0) {
        return false;
    } else if (g.conn.pass.timeout == 0) {
        return true;
    }

    clock_gettime(CLOCK_REALTIME, & nowspec);

    start = g.conn.pass.start_time.tv_sec + g.conn.pass.start_time.tv_nsec / 1e9;
    now = nowspec.tv_sec + nowspec.tv_nsec / 1e9;
    if (fabs(now - start) > g.conn.pass.timeout) {
        return true;
    }

    return false;
}

static void
serv_pass(void)
{
    ptag_t * msg_out;
    int exitstatus;

    /* expect/interact/wait */
    if (g.conn.sock < 0 || ! g.conn.passing) {
        return;
    }

    /* output from child */
    if (g.newcnt > 0) {
        msg_out = ptag_new_text(PTAG_OUTPUT, g.newcnt, g.rawnew);
        if (serv_msg_send(&msg_out, true) < 0) {
            return;
        }

        g.rawnew += g.newcnt;
        g.newcnt = 0;
    }

    /* "expect" with a pattern */
    if ((g.conn.pass.expflags \
         & (PASS_EXPECT_EXACT | PASS_EXPECT_GLOB | PASS_EXPECT_ERE) ) != 0) {
        /* copy data from raw buf to expect buf */
        buf_raw2expect();

        if (serv_expect() ) {
            msg_out = ptag_new_bool(PTAG_MATCHED, 1);
            serv_msg_send(&msg_out, true);
            return;
        }

        /* expect -eof, interact, wait */
    } else {
        g.expoffset = g.ntotal;
        g.expcnt = 0;
    }

    /* Having received SIGCHLD does not necessarily mean EOF. There may still
     * data from the child for reading. So only report EOF when fd_ptm < 0.
     */
    if (g.fd_ptm < 0 && g.expoffset >= g.ntotal) {
        if ((g.conn.pass.expflags & PASS_EXPECT_EOF) != 0) {
            /* expect -eof */
            msg_out = ptag_new_struct(PTAG_EOF);
            serv_msg_send(&msg_out, true);
        } else if ( (g.conn.pass.expflags & PASS_EXPECT_EXIT) != 0) {
            /* interact, wait */
            if ( ! g.waited && g.SIGCHLDed) {
                waitpid(g.child, &  exitstatus, 0);
                g.waited = true;

                msg_out = ptag_new_int(PTAG_EXITED, exitstatus);
                serv_msg_send(&msg_out, true);
            } else {
                /* wait for the child to exit */
            }
        } else {
            /* expect with a pattern */
            msg_out = serv_new_error(ERROR_EOF, "PTY closed");
            serv_msg_send(&msg_out, true);
        }

        return;
    }

    /* "expect" timed out */
    if (exp_timed_out() ) {
        msg_out = serv_new_error(ERROR_TIMEOUT, "expect timed out");
        serv_msg_send(&msg_out, true);
    }
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

    /* N.B.:
     *  - read(ptm) returning EOF does not necessarily mean the child has
     *    exited.
     *  - After the child exits (SIGCHLD) and before closing ptm, there may
     *    still some data in the ptm side for reading.
     *  - After the child exits and ptm is closed, there may still some data
     *    in "rawbuf" when can be sent to the client (interact/expect/wait).
     *  - After the child exits and ptm is closed, there may still some data
     *    in "rawbuf" which has not been copied to "expbuf" for "expect".
     */
    while (! g.waited || g.conn.sock >= 0) {
        if (g.SIGCHLDed && g.cmdopts->spawn.autowait && g.conn.sock < 0) {
            break;
        }

        FD_ZERO( & readfds);
        fd_max = 0;

        /* listen to new connections */
        FD_SET(g.fd_listen, & readfds);
        if (g.fd_listen > fd_max) {
            fd_max = g.fd_listen;
        }

        /* read from ptm */
        if (g.fd_ptm >= 0) {
            FD_SET(g.fd_ptm, & readfds);
            if (g.fd_ptm > fd_max) {
                fd_max = g.fd_ptm;
            }
        }

        /* wait for client requests */
        if (g.conn.sock >= 0) {
            FD_SET(g.conn.sock, & readfds);
            if (g.conn.sock > fd_max) {
                fd_max = g.conn.sock;
            }
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 100 * 1000;
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
                if (g.conn.sock >= 0) {
                    /* FIXME: Don't be so rude. */
                    close(newconn);
                } else {
                    debug("new client connected");
                    memset( & g.conn, 0, sizeof(g.conn) );
                    g.conn.sock = newconn;
                    clock_gettime(CLOCK_REALTIME, & g.conn.pass.start_time);
                }
            }
        }

        /* new data from pty */
        if (g.fd_ptm >= 0 && FD_ISSET(g.fd_ptm, & readfds) ) {
            serv_read_ptm();
        }
        /* -discard is ON */
        if (g.cmdopts->spawn.discard && g.conn.sock < 0) {
            /* mark "new" data as "old" when -discard is ON */
            if (g.newcnt > 0) {
                g.rawnew += g.newcnt;
                g.newcnt = 0;
            }

            /* mark all data as expect'ed */
            g.expoffset = g.ntotal;
            g.expcnt = 0;
            g.expbuf[0] = '\0';
        }

        /* new message from client */
        if (g.conn.sock >= 0 && FD_ISSET(g.conn.sock, & readfds) ) {
            serv_process_msg();
        }

        /* expect/interact/wait */
        if (g.conn.sock >= 0 && g.conn.passing) {
            serv_pass();
        }
    }
}

static void
serv_init(void)
{
    g.syncpipe[0] = -1;
    g.syncpipe[1] = -1;
    g.conn.sock   = -1;

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
    struct sockaddr_un srv_addr = { 0 };
    struct st_spawn * spawn = NULL;

    serv_init();

    g.cmdopts = cmdopts;
    spawn = & cmdopts->spawn;

    /*
     * check if the sockpath is in use
     */
    if (1) {
        debug("check if another server is using the sockfile");
        fd_conn = sock_connect(g.cmdopts->sockpath);
        if (fd_conn >= 0) {
            fatal(ERROR_GENERAL, "sockpath in use");
        }
    }

    /* be an evil daemon */
    if (! g.cmdopts->debug) {
        daemonize(1, 0);
    }

    /* open logfile */
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
    unlink(g.cmdopts->sockpath);

    /* bind() */
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sun_family = AF_LOCAL;
    strcpy(srv_addr.sun_path, g.cmdopts->sockpath);
    if (bind(g.fd_listen, (struct sockaddr *) &srv_addr, sizeof(srv_addr) ) < 0) {
        fatal_sys("bind");
    }

    /* listen() */
    if (listen(g.fd_listen, 0) < 0) {
        fatal_sys("listen");
    }

    /* inform the main() process to exit() */
    if (g.syncpipe[1] >= 0) {
        write(g.syncpipe[1], "", 1);
        close(g.syncpipe[1]);
    }

    /* spawn the child */
    pid = pty_fork(&g.fd_ptm, g.ptsname, sizeof(g.ptsname), NULL, NULL);
    if (pid < 0) {
        fatal_sys("fork");
    } else if (pid == 0) {
        /* child */
        close(g.fd_listen);

        if (cmdopts->spawn.nohup) {
            debug("make the child ignore SIGHUP");
            sig_handle(SIGHUP, SIG_IGN);
        }
        if (execvp(g.cmdopts->spawn.argv[0], g.cmdopts->spawn.argv) < 0) {
            fatal_sys("exec(%s)", g.cmdopts->spawn.argv[0]);
        }
    }
    g.child = pid;

    /* set ptm to be non-blocking */
    if (1) {
        /*
         * On macOS, fcntl(O_NONBLOCK) may fail before the child opens the
         * pts. So wait a while for the child to open the pts.
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

    debug("removing %s", g.cmdopts->sockpath);
    unlink(g.cmdopts->sockpath);

    exit(0);
}
