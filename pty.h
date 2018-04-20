
#ifndef PTY_H__
#define PTY_H__

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

int ptym_open(char *pts_name, int pts_namesz);
int ptys_open(char *pts_name);
pid_t pty_fork(int *ptrfdm, char *slave_name, int slave_namesz,
               const struct termios *slave_termios,
               const struct winsize *slave_winsize);
int tty_raw(int fd, struct termios *save_termios);
int tty_reset(int fd, struct termios *termio);

#endif
