
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>
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

static struct {
    struct st_cmdopts * cmdopts;

    int sock;
    bool stdin_is_tty;
    struct termios saved_termios;
    bool reset_on_exit;
    bool received_winch;
} g;

static void
cli_sigWINCH(int signum)
{
    g.received_winch = true;
}

static void
cli_atexit(void)
{
    if (g.reset_on_exit) {
        tty_reset(STDIN_FILENO, &g.saved_termios);
    }
}

static void
cli_msg_send(ttlv_t *msg)
{
    if (msg_send(g.sock, msg) < 0) {
        fatal(ERROR_PROTO, "msg_send failed (server dead?)");
    }
}

static ttlv_t *
cli_msg_recv(void)
{
    ttlv_t * msg;

    msg = msg_recv(g.sock);
    if (msg == NULL) {
        fatal(ERROR_PROTO, "msg_recv failed (server dead?)");
    }

    return msg;
}

static void
cli_hello(void)
{
    ttlv_t * msg = NULL;

    debug("sending HELLO");
    if (msg_hello(g.sock) < 0) {
        fatal(ERROR_PROTO, "msg_hello failed (server dead?)");
    }

    while (true) {
        if (msg != NULL) {
            msg_free(&msg);
        }

        msg = cli_msg_recv();
        if (msg->tag == TAG_HELLO) {
            debug("received HELLO");
            break;
        } else {
            bug("cli_hello: not supposed to receive tag %s",
                v2n_tag(msg->tag, NULL, 0) );
        }
    }
}

static void
cli_disconn(int exitcode)
{
    ttlv_t * msg = NULL;

    debug("sending DISCONN");
    if (msg_disconn(g.sock) < 0) {
        fatal(ERROR_PROTO, "msg_disconn failed (server dead?)");
    }

    /* wait for DISCONN from server side */
    while (true) {
        if (msg != NULL) {
            msg_free(&msg);
        }

        msg = cli_msg_recv();
        if (msg->tag == TAG_DISCONN) {
            debug("received DISCONN, closing the socket");
            close(g.sock);
            g.sock = -1;
            break;
        } else if (msg->tag == TAG_OUTPUT) {
            if (g.cmdopts->passing) {
                debug("cli_disconn: received output");
                write(STDOUT_FILENO, msg->v_raw, msg->length);
            } else {
                bug("cli_disconn: not supposed to receive output from child");
            }
        } else {
            bug("cli_disconn: not supposed to receive tag %s",
                v2n_tag(msg->tag, NULL, 0) );
        }
    }

    if (exitcode >= 0) {
        exit(exitcode);
    }
}

static int
cli_send_winsize(void)
{
    ttlv_t * msg_out = NULL;
    struct winsize size;
    static int ourtty = -1;

    if ( ! g.stdin_is_tty) {
        return 0;
    }

    if (ourtty < 0) {
#if 0
        ourtty = open("/dev/tty", 0);
#else
        ourtty = STDIN_FILENO;
#endif
    }

    if (ioctl(ourtty, TIOCGWINSZ, &size) < 0) {
        return -1;
    }

    msg_out = ttlv_new_struct(TAG_WINCH);
    ttlv_append_child(msg_out,
                      ttlv_new_int(TAG_WINSIZE_ROW, size.ws_row),
                      ttlv_new_int(TAG_WINSIZE_COL, size.ws_col),
                      NULL);
    cli_msg_send(msg_out);
    msg_free(&msg_out);

    return 0;
}

static void
cli_loop(void)
{
    char buf[1024];
    char errname[32];
    fd_set readfds;
    int r, nread;
    int fd_max;
    ttlv_t * msg_out = NULL;
    ttlv_t * msg_in = NULL;
    struct st_cmdopts * cmdopts = g.cmdopts;
    struct timeval timeout;

    while (true) {
        /* SIGWINCH */
        if (g.received_winch && streq(cmdopts->cmd, CMD_INTERACT) ) {
            g.received_winch = false;
            cli_send_winsize();
        }

        FD_ZERO(&readfds);
        fd_max = 0;

        if (g.sock >= 0) {
            FD_SET(g.sock, &readfds);
            if (g.sock > fd_max) {
                fd_max = g.sock;
            }
        }

        if (streq(cmdopts->cmd, CMD_INTERACT) ) {
            FD_SET(STDIN_FILENO, &readfds);
            if (STDIN_FILENO > fd_max) {
                fd_max = STDIN_FILENO;
            }
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 100 * 1000;
        r = select(fd_max + 1, &readfds, NULL, NULL, & timeout);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                fatal_sys("select");
            }
        }

        /* stdin --> server */
        if (FD_ISSET(STDIN_FILENO, &readfds) ) {
            nread = read(STDIN_FILENO, buf, sizeof(buf) );
            if (nread < 0) {
                if (errno == EINTR) {
                    continue;
                } else {
                    fatal_sys("read(stdin)");
                }
            } else if (nread == 0) {
                /* When will this happen? */
                debug("read(stdin) returned 0");
            } else {
                /* press CTRL-] to detach */
                if ( ! cmdopts->pass.no_detach && nread == 1 && buf[0] == '\x1d') {
#if 0
                    system("tput cnorm");
#endif
                    cli_disconn(ERROR_DETACH);
                }

                msg_out = ttlv_new_text(TAG_INPUT, nread, buf);
                cli_msg_send(msg_out);
                msg_free( & msg_out);
            }
        }

        /* server --> client */
        if (FD_ISSET(g.sock, & readfds) ) {
            if (msg_in) {
                msg_free( & msg_in);
            }

            msg_in = cli_msg_recv();

            if (msg_in->tag == TAG_ACK) {
                cli_disconn(0);
            } else if (msg_in->tag == TAG_OUTPUT) {
                write(STDOUT_FILENO, msg_in->v_raw, msg_in->length);
            } else if (msg_in->tag == TAG_EXPOUT_TEXT) {
                write(STDOUT_FILENO, msg_in->v_text, msg_in->length);
                cli_disconn(0);
            } else if (msg_in->tag == TAG_MATCHED) {
                debug("expect: MATCHED");
                cli_disconn(0);
            } else if (msg_in->tag == TAG_EOF) {
                debug("expect: EOF");
                if ((cmdopts->pass.expflags & PASS_EXPECT_EOF) != 0) {
                    cli_disconn(0);
                } else {
                    cli_disconn(ERROR_EOF);
                }
            } else if (msg_in->tag == TAG_ERROR) {
                ttlv_t *errmsg = ttlv_find_child(msg_in, TAG_ERROR_MSG);
                ttlv_t *errcode = ttlv_find_child(msg_in, TAG_ERROR_CODE);

                v2n_error(errcode->v_int, errname, sizeof(errname) );
                debug("received ERROR: %s (%s)", errmsg->v_text, errname);
                if (cmdopts->passing) {
                    cli_disconn(errcode->v_int);
                } else {
                    cli_disconn(-1);
                    fatal(errcode->v_int, "%s (%s)", errmsg->v_text, errname);
                }
            } else if (msg_in->tag == TAG_INFO) {
                ttlv_t * t;
                struct st_get * get = & cmdopts->get;
                if (get->get_all || get->get_tty) {
                    t = ttlv_find_child(msg_in, TAG_PTSNAME);
                    printf("%s%s\n", get->get_all ? "       TTY: " : "", t->v_text);
                }
                if (get->get_all || get->get_pid) {
                    t = ttlv_find_child(msg_in, TAG_PID);
                    printf("%s%d\n", get->get_all ? " Child PID: " : "", t->v_int);
                }
                if (get->get_all || get->get_ppid) {
                    t = ttlv_find_child(msg_in, TAG_PPID);
                    printf("%s%d\n", get->get_all ? "Parent PID: " : "", t->v_int);
                }
                if (get->get_all || get->get_ttl) {
                    t = ttlv_find_child(msg_in, TAG_TTL);
                    printf("%s%d\n", get->get_all ? "       TTL: " : "", t->v_int);
                }
                if (get->get_all || get->get_idle) {
                    t = ttlv_find_child(msg_in, TAG_IDLETIME);
                    printf("%s%d\n", get->get_all ? "      Idle: " : "", t->v_int);
                }
                if (get->get_all || get->get_timeout) {
                    t = ttlv_find_child(msg_in, TAG_EXP_TIMEOUT);
                    printf("%s%d\n", get->get_all ? "   Timeout: " : "", t->v_int);
                }
                if (get->get_all || get->get_autowait) {
                    t = ttlv_find_child(msg_in, TAG_AUTOWAIT);
                    printf("%s%d\n", get->get_all ? "  Autowait: " : "", t->v_bool);
                }
                if (get->get_all || get->get_nonblock) {
                    t = ttlv_find_child(msg_in, TAG_NONBLOCK);
                    printf("%s%d\n", get->get_all ? "  Nonblock: " : "", t->v_bool);
                }
                if (get->get_all) {
                    t = ttlv_find_child(msg_in, TAG_ZOMBIE_TTL);
                    printf("%s%d\n", get->get_all ? "ZombieIdle: " : "", t->v_int);
                }

                cli_disconn(0);
            } else if (msg_in->tag == TAG_EXITED) {
                int status = msg_in->v_int;
                int ret;

                debug("child exited");
                if (WIFEXITED(status) ) {
                    ret = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status) ) {
                    ret = 128 + WTERMSIG(status);
                } else {
                    ret = 255;
                }
                cli_disconn(ret);
            } else {
                bug("unexpected tag: %d", msg_in->tag);
                fatal(ERROR_PROTO, NULL);
            }
        }
    }
}

static void
cli_chkerr(void)
{
    struct st_chkerr * chkerr = & g.cmdopts->chkerr;

    if (chkerr->errcode < 0 || chkerr->cmpto == NULL) {
        fatal(ERROR_USAGE, "both -errno and -is must be specified");
    }

    if ( ! str1of(chkerr->cmpto, "eof", "timeout", NULL) ) {
        fatal(ERROR_USAGE, "-is only supports \"eof\", \"timeout\"");
    }

    if (streq(chkerr->cmpto, "eof") && chkerr->errcode == ERROR_EOF) {
        exit(0);
    } else if (streq(chkerr->cmpto, "timeout") && chkerr->errcode == ERROR_TIMEOUT) {
        exit(0);
    }

    exit(1);
}

void
cli_main(struct st_cmdopts * cmdopts)
{
    ttlv_t * msg_out = NULL;
    char * subcmd;

    g.cmdopts = cmdopts;
    subcmd = cmdopts->cmd;

    if (streq(subcmd, CMD_CHKERR) ) {
        cli_chkerr();
        return;
    }

    sig_handle(SIGPIPE, SIG_IGN);

    g.stdin_is_tty = isatty(STDIN_FILENO);

    /* close */
    if (streq(subcmd, CMD_CLOSE) ) {
        msg_out = ttlv_new_struct(TAG_CLOSE);

        /* expect_out */
    } else if (streq(subcmd, CMD_EXPOUT) ) {
        msg_out = ttlv_new_int(TAG_EXPOUT, cmdopts->expout.index);

        /* get */
    } else if (streq(subcmd, CMD_GET) ) {
        msg_out = ttlv_new_struct(TAG_INFO);

        /* kill */
    } else if (streq(subcmd, CMD_KILL) ) {
        if (cmdopts->kill.signal < 0) {
            msg_out = ttlv_new_int(TAG_KILL, (int) SIGTERM);
        } else {
            msg_out = ttlv_new_int(TAG_KILL, cmdopts->kill.signal);
        }

        /* send */
    } else if (streq(subcmd, CMD_SEND) ) {
        if (cmdopts->send.enter) {
            msg_out = ttlv_new_raw(TAG_SEND,
                cmdopts->send.len + 1, cmdopts->send.data);
            msg_out->v_raw[cmdopts->send.len] = '\r';
        } else if (cmdopts->send.len > 0) {
            /* send even when the length is 0 or we'll block waiting for
             * the ACK */
            msg_out = ttlv_new_raw(TAG_SEND,
                cmdopts->send.len, cmdopts->send.data);
        }

        /* set */
    } else if (streq(subcmd, CMD_SET) ) {
        msg_out = ttlv_new_struct(TAG_SET);

        if (cmdopts->set.set_autowait) {
            ttlv_append_child(msg_out,
                ttlv_new_bool(TAG_AUTOWAIT, cmdopts->set.autowait),
                NULL);
        }
        if (cmdopts->set.set_nonblock) {
            ttlv_append_child(msg_out,
                ttlv_new_bool(TAG_NONBLOCK, cmdopts->set.nonblock),
                NULL);
        }
        if (cmdopts->set.set_timeout) {
            ttlv_append_child(msg_out,
                ttlv_new_int(TAG_EXP_TIMEOUT, cmdopts->set.timeout),
                NULL);
        }
        if (cmdopts->set.set_ttl) {
            ttlv_append_child(msg_out,
                ttlv_new_int(TAG_TTL, cmdopts->set.ttl),
                NULL);
        }
        if (cmdopts->set.set_idle) {
            ttlv_append_child(msg_out,
                ttlv_new_int(TAG_IDLETIME, cmdopts->set.idle),
                NULL);
        }

        /* expect, interact, wait */
    } else if (cmdopts->passing) {
        ttlv_t * expflags;
        ttlv_t * pattern;
        ttlv_t * timeout;
        ttlv_t * lookback;
        ttlv_t * subcmd;

        /* "expect" without a pattern */
        if (cmdopts->pass.subcmd == PASS_SUBCMD_EXPECT
                && cmdopts->pass.expflags == 0) {
            cmdopts->pass.expflags = PASS_EXPECT_ERE;
            cmdopts->pass.pattern = ".*";
        }

        if ( ! cmdopts->pass.no_input && ! g.stdin_is_tty) {
            fatal(ERROR_NOTTY, "stdin not a tty");
        }

        msg_out = ttlv_new_struct(TAG_PASS);

        subcmd = ttlv_new_int(TAG_PASS_SUBCMD, cmdopts->pass.subcmd);
        ttlv_append_child(msg_out, subcmd, NULL);

        expflags = ttlv_new_int(TAG_EXP_FLAGS, cmdopts->pass.expflags);
        ttlv_append_child(msg_out, expflags, NULL);

        if (cmdopts->pass.has_timeout) {
            timeout = ttlv_new_int(TAG_EXP_TIMEOUT, cmdopts->pass.timeout);
            ttlv_append_child(msg_out, timeout, NULL);
        }

        if (cmdopts->pass.pattern != NULL) {
            pattern = ttlv_new_text(TAG_PATTERN,
                strlen(cmdopts->pass.pattern), cmdopts->pass.pattern);
            ttlv_append_child(msg_out, pattern, NULL);
        }

        if (cmdopts->pass.lookback > 0) {
            lookback = ttlv_new_int(TAG_LOOKBACK, cmdopts->pass.lookback);
            ttlv_append_child(msg_out, lookback, NULL);
        }

        /* unknown */
    } else {
        fatal(ERROR_USAGE, "unknown sub-command: %s", cmdopts->cmd);
    }

    /* nothing to do */
    if (msg_out == NULL) {
        debug("nothing to do, exiting...");
        exit(0);
    }

    /* connect */
    g.sock = sock_connect(cmdopts->sockpath);
    if (g.sock < 0) {
        fatal_sys("connect");
    }

    /* HELLO */
    cli_hello();

    /* raw mode for "interact" */
    if (streq(cmdopts->cmd, CMD_INTERACT) ) {
        /* user's tty to raw mode */
        if (tty_raw(STDIN_FILENO, &g.saved_termios) < 0)
            fatal_sys("tty_raw");

        /* reset user's tty on exit */
        g.reset_on_exit = true;
        if (atexit(cli_atexit) < 0)
            fatal_sys("atexit");

        sig_handle(SIGWINCH, cli_sigWINCH);
    }

    /* send the initial command */
    cli_msg_send(msg_out);
    msg_free(&msg_out);

    if (streq(cmdopts->cmd, CMD_INTERACT) ) {
        cli_send_winsize();
    }

    cli_loop();
}
