
#if defined(__gnu_linux__) || defined(__CYGWIN__)
#define _GNU_SOURCE
#endif

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "common.h"
#include "pty.h"

int
ptym_open(char *pts_name, int pts_namesz)
{
    char *ptr;
    int fdm;

    snprintf(pts_name, pts_namesz, "/dev/ptmx");

#if 1
    fdm = posix_openpt(O_RDWR);
#else
    /* FeeBSD 11.1 does not have "/dev/ptmx" */
    fdm = open("/dev/ptmx", O_RDWR);
#endif
    if (fdm < 0)
        return (-1);

    if (grantpt(fdm) < 0) {
        close(fdm);
        return (-2);
    }

    if (unlockpt(fdm) < 0) {
        close(fdm);
        return (-3);
    }

    if ((ptr = ptsname(fdm)) == NULL) {
        close(fdm);
        return (-4);
    }

    snprintf(pts_name, pts_namesz, "%s", ptr);
    return (fdm);
}

int
ptys_open(char *pts_name)
{
    int fds;

    if ((fds = open(pts_name, O_RDWR)) < 0)
        return (-5);
    return (fds);
}

pid_t
pty_fork(int *ptrfdm, char *slave_name, int slave_namesz,
    const struct termios *slave_termios,
    const struct winsize *slave_winsize)
{
    int fdm, fds;
    pid_t pid;
    char pts_name[32];

    if ((fdm = ptym_open(pts_name, sizeof(pts_name))) < 0)
        fatal_sys("can't open master pty: %s, error %d", pts_name, fdm);

    if (slave_name != NULL) {
        /*
         * Return name of slave.  Null terminate to handle case
         * where strlen(pts_name) > slave_namesz.
         */
        snprintf(slave_name, slave_namesz, "%s", pts_name);
    }

    if ((pid = fork()) < 0) {
        return (-1);
    } else if (pid == 0) {
        /*
         * child
         */
        if (setsid() < 0)
            fatal_sys("setsid error");

        /*
         * System V acquires controlling terminal on open().
         */
        if ((fds = ptys_open(pts_name)) < 0)
            fatal_sys("can't open slave pty");

        /* all done with master in child */
        close(fdm);

#if defined(TIOCSCTTY)
        /*
         * TIOCSCTTY is the BSD way to acquire a controlling terminal.
         *
         * Don't check the return code. It would fail in Cygwin.
         */
        ioctl(fds, TIOCSCTTY, (char *)0);
#endif
        /*
         * Set slave's termios and window size.
         */
        if (slave_termios != NULL) {
            if (tcsetattr(fds, TCSANOW, slave_termios) < 0)
                fatal_sys("tcsetattr error on slave pty");
        }
        if (slave_winsize != NULL) {
            if (ioctl(fds, TIOCSWINSZ, slave_winsize) < 0)
                fatal_sys("TIOCSWINSZ error on slave pty");
        }

        /*
         * Slave becomes stdin/stdout/stderr of child.
         */
        if (dup2(fds, STDIN_FILENO) != STDIN_FILENO)
            fatal_sys("dup2 error to stdin");
        if (dup2(fds, STDOUT_FILENO) != STDOUT_FILENO)
            fatal_sys("dup2 error to stdout");
        if (dup2(fds, STDERR_FILENO) != STDERR_FILENO)
            fatal_sys("dup2 error to stderr");
        if (fds != STDIN_FILENO && fds != STDOUT_FILENO &&
            fds != STDERR_FILENO) {
            close(fds);
        }

        return (0);
    } else {
        /*
         * parent
         */
        *ptrfdm = fdm;
        return (pid);
    }
}

int
tty_raw(int fd, struct termios *save_termios)
{
    int err;
    struct termios buf;

    if (tcgetattr(fd, &buf) < 0)
        return (-1);
    *save_termios = buf;

#if 1
    /*
     * Echo off, canonical mode off, extended input
     * processing off, signal chars off.
     */
    buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /*
     * No SIGINT on BREAK, CR-to-NL off, input parity
     * check off, don't strip 8th bit on input, output
     * flow control off.
     */
    buf.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /*
     * Clear size bits, parity checking off.
     */
    buf.c_cflag &= ~(CSIZE | PARENB);

    /*
     * Set 8 bits/char.
     */
    buf.c_cflag |= CS8;

    /*
     * Output processing off.
     */
    buf.c_oflag &= ~(OPOST);

    /*
     * Case B: 1 byte at a time, no timer.
     */
    buf.c_cc[VMIN] = 1;
    buf.c_cc[VTIME] = 0;
#else
    cfmakeraw(&buf);
#endif
    if (tcsetattr(fd, TCSAFLUSH, &buf) < 0)
        return (-1);

    /*
     * Verify that the changes stuck.  tcsetattr can return 0 on
     * partial success.
     */
    if (tcgetattr(fd, &buf) < 0) {
        err = errno;
        tcsetattr(fd, TCSAFLUSH, save_termios);
        errno = err;
        return (-1);
    }
    if ((buf.c_lflag & (ECHO | ICANON | IEXTEN | ISIG)) ||
        (buf.c_iflag & (BRKINT | ICRNL | INPCK | ISTRIP | IXON)) ||
        (buf.c_cflag & (CSIZE | PARENB | CS8)) != CS8 ||
        (buf.c_oflag & OPOST) || buf.c_cc[VMIN] != 1 ||
        buf.c_cc[VTIME] != 0) {
        /*
         * Only some of the changes were made.  Restore the
         * original settings.
         */
        tcsetattr(fd, TCSAFLUSH, save_termios);
        errno = EINVAL;
        return (-1);
    }

    return (0);
}

int
tty_reset(int fd, struct termios *termio)
{
    if (tcsetattr(fd, TCSAFLUSH, termio) < 0)
        return (-1);
    return (0);
}
