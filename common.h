
#ifndef COMMON_H__
#define COMMON_H__

#include <stdbool.h>
#include "proto.h"

#define ARRAY_SIZE(a)      ( sizeof(a) / sizeof(a[0]) )
#define streq(s1, s2)      (strcmp(s1, s2) == 0)
#define strcaseeq(s1, s2)  (strcasecmp(s1, s2) == 0)
#define isodigit(c)        ( (c) >= '0' && (c) <= '7')

#define x_H(c)       ( (c <= '9') ? (c - '0') : (toupper(c) - 'A' + 10) )
#define x_HH(c1, c2) ( (x_H(c1) << 4) + x_H(c2) )

#define o_O(c)            ( c - '0' )
#define o_OO(c1, c2)      ( (o_O(c1) << 3) + o_O(c2) )
#define o_OOO(c1, c2, c3) ( (o_O(c1) << 6) + (o_O(c2) << 3) + o_O(c3) )

#define PASS_MAGIC      0x57484a4d /* WHJM */
#define PASS_MAX_MSG    (16 * 1024)
#define PASS_MAX_SEND   1024
#define PASS_DEF_TMOUT  10

enum {
    /* THE START */
    ERROR_START__ = 200,

    ERROR_GENERAL,
    ERROR_USAGE,
    ERROR_NOTTY,
    ERROR_SYS,
    ERROR_PROTO,
    ERROR_EOF,
    ERROR_TIMEOUT,
    ERROR_EXITED,

    /* THE END */
    ERROR_END__,
};

enum {
    /* THE START */
    PTAG_START__ = 0,

    /*
     * c2s
     */
    PTAG_PASS,          /* expect/interact/wait */
    PTAG_WINCH,         /* SIGWINCH */
    PTAG_SEND,          /* for `send' */
    PTAG_INPUT,         /* for `interact' */
    PTAG_EXPOUT,        /* expect_out */
    PTAG_CLOSE,         /* close */
    PTAG_KILL,          /* kill */
    PTAG_SET,

    /*
     * s2c
     */
    PTAG_ACK,
    PTAG_OUTPUT,                /* output from the child */
    PTAG_MATCHED,               /* successful "expect" */
    PTAG_EOF,                   /* EOF from child */
    PTAG_EXITED,                /* child exited */
    PTAG_ERROR,
    PTAG_TIMED_OUT,             /* "expect" timed out */
    PTAG_EXPOUT_TEXT,           /* $expect_out(N,string) */

    /*
     * bidir
     */
    PTAG_INFO,          /* pid, pts_name */
    PTAG_DISCONN,       /* disconnect gracefully */

    /*
     * sub-tags
     */
    PTAG_EXP_FLAGS,     /* -exact, -re, -eof, ... */
    PTAG_PATTERN,       /* the expected pattern */
    PTAG_EXP_TIMEOUT,   /* expect -timeout <N> */
    PTAG_ERROR_CODE,
    PTAG_ERROR_MSG,
    PTAG_WINSIZE_ROW,
    PTAG_WINSIZE_COL,
    PTAG_PID,
    PTAG_PPID,
    PTAG_PTSNAME,
    PTAG_EXPOUT_INDEX,
    PTAG_DISCARD,
    PTAG_AUTOWAIT,
    PTAG_LOGFILE,
    PTAG_LOGFILE_APPEND,
    PTAG_NOHUP,
    PTAG_LOOKBACK,

    /* THE END */
    PTAG_END__,
};

enum {
    PASS_EXPECT_EXACT = 0x01,
    PASS_EXPECT_GLOB  = 0x02,
    PASS_EXPECT_ERE   = 0x04,
    PASS_EXPECT_EOF   = 0x08,   /* expect -eof */
    PASS_EXPECT_EXIT  = 0x10,   /* interact, wait */
    PASS_EXPECT_ICASE = 0x20,
    PASS_EXPECT_NOSUB = 0x40,
};

enum {
    PASS_EXPOUT_MATCHED = 1,
    PASS_EXPOUT_EOF,
    PASS_EXPOUT_TIMEDOUT,
};

struct st_spawn {
    char ** argv;
    bool    nohup;
    bool    discard;
    bool    autowait;
    int     def_timeout;
    char  * logfile;
    int     logfd;
    bool    append;
};

struct st_send {
    bool   cstring;
    char * data;
    int    len;
    bool   enter;
};

struct st_expout {
    int  index;
};

struct st_chkerr {
    int    errcode;
    char * cmpto;
};

struct st_set {
    bool set_autowait;
    bool autowait;
    bool set_discard;
    bool discard;
    bool set_timeout;
    int  timeout;
};

struct st_get {
    bool get_all;

    bool get_pid;
    int  pid;

    bool get_ppid;
    int  ppid;

    bool get_tty;
    char tty[32];

    bool get_timeout;
    int  timeout;

    bool get_discard;
    bool discard;

    bool get_autowait;
    bool autowait;
};

struct st_kill {
    int signal;
};

/* expect, interact, wait */
struct st_pass {
    bool   no_input;    /* expect, wait */
    bool   has_timeout;
    int    timeout;     /* negative value means infinite */
    int    expflags;
    char * pattern;
    bool   cstring;
    bool   no_capture;
    int    lookback;
};

struct st_cmdopts {
    char * sockpath;
    char * cmd;
    bool   debug;
    bool   passing;

    union {
        struct st_spawn  spawn;
        struct st_send   send;
        struct st_pass   pass;
        struct st_expout expout;
        struct st_chkerr chkerr;
        struct st_kill   kill;
        struct st_get    get;
        struct st_set    set;
    };
};

struct v2n_map {
    int    val;
    char * name;
};

char * v2n_error(int err, char *buf, size_t len);
char * v2n_tag(int tag, char *buf, size_t len);
char * strunesc(const char * in, char ** out_, int * len_);
int    name2sig(const char * signame);
void   common_init(void);

int  count1bits(unsigned n);
bool str1of(const char *s, ... /* , NULL */);
bool strmatch(const char *s, const char *ere);
bool strcasematch(const char *s, const char *ere);
void debug_on(void);

#if defined(__GNUC__)
void bug(const char *fmt, ...)              __attribute__(( format(printf, 1, 2) ));
void debug(const char *fmt, ...)            __attribute__(( format(printf, 1, 2) ));
void fatal(int rcode, const char *fmt, ...) __attribute__(( format(printf, 2, 3) ));
void fatal_sys(const char *fmt, ...)        __attribute__(( format(printf, 1, 2) ));
#else
void bug(const char *fmt, ...);
void debug(const char *fmt, ...);
void fatal(int rcode, const char *fmt, ...);
void fatal_sys(const char *fmt, ...);
#endif

void sig_handle(int signo, void (*handler)(int) );
int  sock_connect(char * sockpath);

size_t   msg_size(ptag_t *msg);
void     msg_free(ptag_t **msg);
ptag_t * msg_recv(int fd);
ssize_t  msg_send(int fd, ptag_t *msg);
ssize_t  msg_disconn(int fd);

ssize_t read_if_ready(int fd, char *buf, size_t n);
ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, const void *ptr, size_t n);

void cli_main(struct st_cmdopts * cmdopts);
void serv_main(struct st_cmdopts * cmdopts);

#endif
