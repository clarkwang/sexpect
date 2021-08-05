
#ifndef COMMON_H__
#define COMMON_H__

#include <stdbool.h>
#include <time.h>
#include "proto.h"

#define ARRAY_SIZE(a)      ( sizeof(a) / sizeof(a[0]) )
#define streq(s1, s2)      (strcmp(s1, s2) == 0)
#define strcaseeq(s1, s2)  (strcasecmp(s1, s2) == 0)
#define isodigit(c)        ( (c) >= '0' && (c) <= '7')

#define c_H1(c)       ( (c <= '9') ? (c - '0') : (toupper(c) - 'A' + 10) )
#define c_H2(c1, c2)  ( (c_H1(c1) << 4) + c_H1(c2) )

#define c_O1(c)           ( c - '0' )
#define c_O2(c1, c2)      ( (c_O1(c1) << 3) + c_O1(c2) )
#define c_O3(c1, c2, c3)  ( (c_O2(c1, c2) << 3) + c_O1(c3) )

#define PASS_MAGIC      0x4a55575a /* JUWZ */
#define PASS_MAX_MSG    (64 * 1024)
#define PASS_MAX_SEND   1024
#define PASS_DEF_TMOUT  -1
#define PASS_DEF_ZOMBIE_TTL  (24 * 60 * 60)  // 24 hours

#define CMD_CHKERR    "chkerr"
#define CMD_CLOSE     "close"
#define CMD_EXPECT    "expect"
#define CMD_EXPOUT    "expect_out"
#define CMD_GET       "get"
#define CMD_HELP      "help"
#define CMD_INTERACT  "interact"
#define CMD_KILL      "kill"
#define CMD_SEND      "send"
#define CMD_SET       "set"
#define CMD_SPAWN     "spawn"
#define CMD_VERSION   "version"
#define CMD_WAIT      "wait"

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
    ERROR_INTERNAL,
    ERROR_DETACH,

    /* THE END */
    ERROR_END__,
};

enum {
    /* THE START */
    TAG_START__ = 0,

    /*
     * c2s
     */
    TAG_PASS,           /* expect/interact/wait */
    TAG_WINCH,          /* SIGWINCH */
    TAG_SEND,           /* for `send' */
    TAG_INPUT,          /* for `interact' */
    TAG_EXPOUT,         /* expect_out */
    TAG_CLOSE,          /* close */
    TAG_KILL,           /* kill */
    TAG_SET,

    /*
     * s2c
     */
    TAG_ACK,
    TAG_OUTPUT,                 /* output from the child */
    TAG_MATCHED,                /* successful "expect" */
    TAG_EOF,                    /* EOF from child */
    TAG_EXITED,                 /* child exited */
    TAG_ERROR,
    TAG_TIMED_OUT,              /* "expect" timed out */
    TAG_EXPOUT_TEXT,            /* $expect_out(N,string) */

    /*
     * bidir
     */
    TAG_INFO,           /* pid, pts_name */
    TAG_DISCONN,        /* disconnect gracefully */
    TAG_HELLO,

    /*
     * sub-tags
     */
    TAG_EXP_FLAGS,      /* -exact, -re, -eof, ... */
    TAG_PATTERN,        /* the expected pattern */
    TAG_EXP_TIMEOUT,    /* expect -timeout <N> */
    TAG_ERROR_CODE,
    TAG_ERROR_MSG,
    TAG_WINSIZE_ROW,
    TAG_WINSIZE_COL,
    TAG_PID,
    TAG_PPID,
    TAG_PTSNAME,
    TAG_EXPOUT_INDEX,
    TAG_NONBLOCK,
    TAG_AUTOWAIT,
    TAG_TTL,
    TAG_ZOMBIE_TTL,
    TAG_IDLETIME,
    TAG_LOGFILE,
    TAG_LOGFILE_APPEND,
    TAG_NOHUP,
    TAG_LOOKBACK,
    TAG_PASS_SUBCMD,    /* expect, interact, wait */

    /* THE END */
    TAG_END__,
};

enum {
    PASS_EXPECT_EXACT = 0x01,   /* expect -exact */
    PASS_EXPECT_GLOB  = 0x02,   /* expect -glob */
    PASS_EXPECT_ERE   = 0x04,   /* expect -re */
    PASS_EXPECT_EOF   = 0x08,   /* expect -eof */
    PASS_EXPECT_EXIT  = 0x10,   /* interact, wait */
    PASS_EXPECT_ICASE = 0x20,   /* expect -nocase */
    PASS_EXPECT_NOSUB = 0x40,
};

enum {
    PASS_SUBCMD_EXPECT = 0x01,
    PASS_SUBCMD_INTERACT,
    PASS_SUBCMD_WAIT,
};

enum {
    PASS_EXPOUT_MATCHED = 1,
    PASS_EXPOUT_EOF,
    PASS_EXPOUT_TIMEDOUT,
};

struct st_spawn {
    char ** argv;
    char  * TERM;
    bool    nohup;
    bool    nonblock;
    bool    autowait;
    bool    cloexit;    /* Close ptm when the child exits even the child's
                           children are still opening the pty */
    int     def_timeout;
    char  * logfile;
    int     logfd;
    bool    append;
    int     ttl;
    int     idle;
    int     zombie_idle;
    struct timespec startime;
    struct timespec exittime;
};

struct st_send {
    bool   cstring;
    bool   file;
    bool   strip;
    bool   env;
    bool   enter;

    /* to server */
    char * data;
    int    len;
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
    bool set_nonblock;
    bool nonblock;
    bool set_timeout;
    int  timeout;
    bool set_ttl;
    int  ttl;
    bool set_idle;
    int  idle;
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

    bool get_nonblock;
    bool nonblock;

    bool get_autowait;
    bool autowait;

    bool get_ttl;
    int  ttl;

    bool get_idle;
    int  idle;
};

struct st_kill {
    int signal;
};

/* expect, interact, wait */
struct st_pass {
    int    subcmd;      /* expect, interact, wait */
    bool   no_input;    /* expect, wait */
    bool   has_timeout;
    bool   no_detach;   /* interact: disable <ctrl-]> */
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
char * str_rstrip(char * s);
char * glob2re(const char * in, char ** out_, int * len_);
int    name2sig(const char * signame);
void   common_init(void);

int  Clock_gettime(struct timespec * spec);
double Clock_diff(struct timespec * t1, struct timespec * t2);
int  count1bits(unsigned n);
bool str1of(const char *s, ... /* , NULL */);
bool strmatch(const char *s, const char *ere);
bool strmatch_ex(const char *s, const char *ere, bool icase);
bool strcasematch(const char *s, const char *ere);
void debug_on(void);

#if defined(__GNUC__)
void bug(const char *fmt, ...)              __attribute__(( format(printf, 1, 2) ));
void debug(const char *fmt, ...)            __attribute__(( format(printf, 1, 2) ));
void error(const char *fmt, ...)            __attribute__(( format(printf, 1, 2) ));
void fatal(int rcode, const char *fmt, ...) __attribute__(( format(printf, 2, 3) ));
void fatal_sys(const char *fmt, ...)        __attribute__(( format(printf, 1, 2) ));
#else
void bug(const char *fmt, ...);
void error(const char *fmt, ...);
void fatal(int rcode, const char *fmt, ...);
void fatal_sys(const char *fmt, ...);
#endif

void sig_handle(int signo, void (*handler)(int) );
int  sock_connect(char * sockpath);

size_t   msg_size(ttlv_t *msg);
void     msg_free(ttlv_t **msg);
ttlv_t * msg_recv(int fd);
ssize_t  msg_send(int fd, ttlv_t *msg);
ssize_t  msg_hello(int fd);
ssize_t  msg_disconn(int fd);

ssize_t read_if_ready(int fd, char *buf, size_t n);
ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, const void *ptr, size_t n);

void cli_main(struct st_cmdopts * cmdopts);
void serv_main(struct st_cmdopts * cmdopts);

#endif
