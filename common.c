
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <signal.h>
#include <ctype.h>
#include <math.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"

#define V2N_MAP(v) { v, #v }

static struct {
    int debug;
} g;

static struct v2n_map g_v2n_error[] = {
    V2N_MAP(ERROR_DETACH),
    V2N_MAP(ERROR_EOF),
    V2N_MAP(ERROR_EXITED),
    V2N_MAP(ERROR_GENERAL),
    V2N_MAP(ERROR_INTERNAL),
    V2N_MAP(ERROR_NOTTY),
    V2N_MAP(ERROR_PROTO),
    V2N_MAP(ERROR_SYS),
    V2N_MAP(ERROR_TIMEOUT),
    V2N_MAP(ERROR_USAGE),
    { 0, NULL },
};

static struct v2n_map g_v2n_tag[] = {
    V2N_MAP(TAG_ACK),
    V2N_MAP(TAG_AUTOWAIT),
    V2N_MAP(TAG_CLOSE),
    V2N_MAP(TAG_DISCONN),
    V2N_MAP(TAG_EOF),
    V2N_MAP(TAG_ERROR),
    V2N_MAP(TAG_ERROR_CODE),
    V2N_MAP(TAG_ERROR_MSG),
    V2N_MAP(TAG_EXITED),
    V2N_MAP(TAG_EXPOUT),
    V2N_MAP(TAG_EXPOUT_INDEX),
    V2N_MAP(TAG_EXPOUT_TEXT),
    V2N_MAP(TAG_EXP_FLAGS),
    V2N_MAP(TAG_EXP_TIMEOUT),
    V2N_MAP(TAG_HELLO),
    V2N_MAP(TAG_IDLETIME),
    V2N_MAP(TAG_INFO),
    V2N_MAP(TAG_INPUT),
    V2N_MAP(TAG_KILL),
    V2N_MAP(TAG_LOGFILE),
    V2N_MAP(TAG_LOGFILE_APPEND),
    V2N_MAP(TAG_LOOKBACK),
    V2N_MAP(TAG_MATCHED),
    V2N_MAP(TAG_NOHUP),
    V2N_MAP(TAG_NONBLOCK),
    V2N_MAP(TAG_OUTPUT),
    V2N_MAP(TAG_PASS),
    V2N_MAP(TAG_PASS_SUBCMD),
    V2N_MAP(TAG_PATTERN),
    V2N_MAP(TAG_PID),
    V2N_MAP(TAG_PPID),
    V2N_MAP(TAG_PTSNAME),
    V2N_MAP(TAG_SEND),
    V2N_MAP(TAG_SET),
    V2N_MAP(TAG_TIMED_OUT),
    V2N_MAP(TAG_TTL),
    V2N_MAP(TAG_WINCH),
    V2N_MAP(TAG_WINSIZE_COL),
    V2N_MAP(TAG_WINSIZE_ROW),
    V2N_MAP(TAG_ZOMBIE_TTL),
    { 0, NULL },
};

void
common_init(void)
{
    if (ARRAY_SIZE(g_v2n_error) != ERROR_END__ - ERROR_START__) {
        fatal(ERROR_GENERAL, "g_v2n_error is not complete");
    }
    if (ARRAY_SIZE(g_v2n_tag) != TAG_END__ - TAG_START__) {
        fatal(ERROR_GENERAL, "g_v2n_tag is not complete");
    }
}

static char *
v2n(struct v2n_map map[], int val, char *buf, size_t len)
{
    int i;

    for (i = 0; map[i].name != NULL; ++i) {
        if (map[i].val == val) {
            if (buf == NULL) {
                return map[i].name;
            } else {
                if (len > 0) {
                    snprintf(buf, len, "%s", map[i].name);
                    return buf;
                } else {
                    return map[i].name;
                }
            }
        }
    }

    return NULL;
}

char *
v2n_error(int err, char *buf, size_t len)
{
    char * p;

    p = v2n(g_v2n_error, err, buf, len);
    if (p != NULL) {
        return p;
    }

    snprintf(buf, len, "ERROR %d", err);
    return buf;
}

char *
v2n_tag(int tag, char *buf, size_t len)
{
    char * p;

    p = v2n(g_v2n_tag, tag, buf, len);
    if (p != NULL) {
        return p;
    }

    snprintf(buf, len, "ERROR %d", tag);
    return buf;
}

void
debug_on(void)
{
    g.debug = 1;
}

void
bug(const char *fmt, ...)
{
    va_list ap;
    char buf[1024];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    fprintf(stderr, "[BUG!?] %s\r\n", buf);
}

void
debug(const char *fmt, ...)
{
    if (g.debug != 0) {
        va_list ap;
        char buf[1024];

        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        fprintf(stderr, "[DEBUG] %s\r\n", buf);
    }
}

void
error(const char *fmt, ...)
{
    if (g.debug != 0) {
        va_list ap;
        char buf[1024];

        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        fprintf(stderr, "[ERROR] %s\r\n", buf);
    }
}

void
fatal(int rcode, const char *fmt, ...)
{
    va_list ap;
    char buf[1024];

    if (fmt) {
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        /* in case stdout and stderr are the same */
        fflush(stdout);

        fprintf(stderr, "[ERROR] %s\r\n", buf);

        /* flush all open files */
        fflush(NULL);
    }

    exit(rcode);
}

void
fatal_sys(const char *fmt, ...)
{
    va_list ap;
    char buf[1024];
    int error = errno;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    fatal(ERROR_SYS, "%s: %s (%d)", buf, strerror(error), error);
}

int
count1bits(unsigned n)
{
    int cnt = 0;

    while (n) {
        ++cnt;
        n &= n - 1;
    }

    return cnt;
}

int
Clock_gettime(struct timespec * spec)
{
    static bool firstime = true;
    static clockid_t clock = CLOCK_REALTIME;

    if (firstime) {
        firstime = false;
#ifdef CLOCK_MONOTONIC
        if (clock_gettime(CLOCK_MONOTONIC, spec) == 0) {
            clock = CLOCK_MONOTONIC;
            return 0;
        }
#endif
    }

    return clock_gettime(clock, spec);
}

/*
 * t2 can be NULL which means NOW.
 */
double
Clock_diff(struct timespec * t1, struct timespec * t2)
{
    struct timespec nowspec;
    double d1, d2;

    if (NULL == t2) {
        Clock_gettime(& nowspec);
        t2 = & nowspec;
    }

    d1 = t1->tv_sec + t1->tv_nsec / 1e9;
    d2 = t2->tv_sec + t2->tv_nsec / 1e9;

    return fabs(d2 - d1);
}

bool
str1of(const char *s, ...)
{
    va_list ap;
    char *next;
    bool found = false;

    va_start(ap, s);
    while ((next = va_arg(ap, char *)) != NULL) {
        if (streq(s, next) ) {
            found = true;
            break;
        }
    }
    va_end(ap);

    return found;
}

bool
strmatch_ex(const char *s, const char *pattern, bool icase)
{
    regex_t re;
    int ret;
    bool matched = false;

    ret = regcomp(&re, pattern, REG_EXTENDED | (icase ? REG_ICASE : 0) );
    if (ret != 0) {
        return false;
    }

    ret = regexec(&re, s, 0, NULL, 0);
    if (ret == 0) {
        matched = true;
    }

    regfree(&re);
    return matched;
}

bool
strmatch(const char *s, const char *pattern)
{
    return strmatch_ex(s, pattern, false);
}

bool
strcasematch(const char *s, const char *pattern)
{
    return strmatch_ex(s, pattern, true);
}

char *
str_rstrip(char *s)
{
    int len, i;

    if (s == NULL || s[0] == 0) {
        return s;
    }

    len = strlen(s);
    for (i = len - 1; i >= 0; i--) {
        if (isspace(s[i])) {
            s[i] = 0;
        } else {
            return s;
        }
    }

    return s;
}

/*
 * \\
 * \a \b \f \n \r \t \v (native C escapes)
 * \xH \xHH
 * \o \oo \ooo
 * \e \E                (ESC)
 * \cx                  (CTRL-x)
 * \<unknown>           <unknown>
 */
char *
strunesc(const char * in, char ** out_, int * len_)
{
    int      len;
    char   * out;

    * out_ = NULL;
    if (len_ != NULL) {
        * len_ = 0;
    }

    if (NULL == in) {
        return NULL;
    }

    out = malloc(strlen(in) + 1);
    if (NULL == out) {
        return NULL;
    }

    len = 0;
    while (* in) {
        if (in[0] == '\\') {
            if (in[1] == '\\') {
                out[len++] = '\\';
                in += 2;
            } else if (in[1] == 'a') {
                out[len++] = '\a';
                in += 2;
            } else if (in[1] == 'b') {
                out[len++] = '\b';
                in += 2;
            } else if (in[1] == 'e' || in[1] == 'E') {
                out[len++] = '\033';
                in += 2;
            } else if (in[1] == 'f') {
                out[len++] = '\f';
                in += 2;
            } else if (in[1] == 'n') {
                out[len++] = '\n';
                in += 2;
            } else if (in[1] == 'r') {
                out[len++] = '\r';
                in += 2;
            } else if (in[1] == 't') {
                out[len++] = '\t';
                in += 2;
            } else if (in[1] == 'v') {
                out[len++] = '\v';
                in += 2;

                /* ctrl-x */
            } else if (in[1] == 'c') {
                char c = toupper(in[2]);
                if (c < 'A' || c > '_') {
                    free(out);
                    return NULL;
                }
                out[len++] = c - 'A' + 1;
                in += 3;

                /* \xHH \xH */
            } else if (in[1] == 'x') {
                if ( ! isxdigit(in[2]) ) {
                    free(out);
                    return NULL;
                }
                if (isxdigit(in[3]) ) {
                    /* \xHH */
                    out[len++] = c_H2(in[2], in[3]);
                    in += 4;
                } else {
                    /* \xH */
                    out[len++] = c_H1(in[2]);
                    in += 3;
                }

                /* \o \oo \ooo */
            } else if (isodigit(in[1]) ) {
                if (isodigit(in[2]) ) {
                    if (isodigit(in[3]) ) {
                        /* \ooo */
                        int n = c_O3(in[1], in[2], in[3]);
                        if (n > 0xff) {
                            /* \oo */
                            out[len++] = c_O2(in[1], in[2]);
                            in += 3;
                        } else {
                            /* \ooo */
                            out[len++] = c_O3(in[1], in[2], in[3]);
                            in += 4;
                        }
                    } else {
                        /* \oo */
                        out[len++] = c_O2(in[1], in[2]);
                        in += 3;
                    }
                } else {
                    /* \o */
                    out[len++] = c_O1(in[1]);
                    in += 2;
                }
            } else if (in[1] == '\0') {
                out[len++] = '\\';
                in += 1;
            } else {
                /* \<unknown> */
                out[len++] = in[1];
                in += 2;
            }

            continue;
        }

        out[len++] = * (in++);
    }
    out[len] = '\0';

    * out_ = out;
    if (len_ != NULL) {
        * len_ = len;
    }

    return out;
}

/*
 *          GLOB | RE
 * --------------+-------
 *             * | .*
 *             ? | .
 *             ] | ]
 *         [...] | [...]
 *           []] | []]
 *          [^]] | [^]]
 *          [!]] | [^]]
 *        [^...] | [^...]
 *        [!...] | [^...]
 *          ^... | ^...
 *          ...^ | ...\^
 *          $... | [$]...
 *          ...$ | ...$
 *           .|+ | [CHAR]
 *        <>(){} | [CHAR]
 *            \\ | \\
 *            \* | [*]
 *            \? | [?]
 *            \[ | [[]
 *            \] | []]
 *               |
 *        \OTHER | ERROR
 *   [[:class:]] | ERROR
 *  [[:collat:]] | ERROR
 *   [[=equiv=]] | ERROR
 */
char *
glob2re(const char * in, char ** out_, int * len_)
{
    int len;
    char * out;
    char c, * p = NULL;
    const char * save_in = in;

    * out_ = NULL;
    if (len_ != NULL) {
        * len_ = 0;
    }

    if (NULL == in) {
        return NULL;
    }

    out = malloc(3 * strlen(in) + 1);
    if (NULL == out) {
        return NULL;
    }

    len = 0;
    while (* in) {
        c = in[0];

        /* `*' */
        if (c == '*') {
            out[len++] = '.';
            out[len++] = '*';

            in += 1;

            /* `?' */
        } else if (c == '?') {
            out[len++] = '.';

            in += 1;

            /* `]' */
        } else if (c == ']') {
            out[len++] = ']';
            in += 1;

            /* [...] */
        } else if (c == '[') {
            out[len++] = '[';
            in += 1;

            /* ! or ^ */
            c = in[0];
            if (c == 0) {
                free(out);
                return NULL;
            } else if ( c == '!' || c == '^') {
                out[len++] = '^';
                in += 1;
            }

            /* the first char can be `]' */
            c = in[0];
            if (c == 0) {
                free(out);
                return NULL;
            } else if (c == ']') {
                out[len++] = ']';
                in += 1;
            }

            /* find the next `]' */
            p = strchr(in, ']');
            if (p == NULL) {
                free(out);
                return NULL;
            } else {
                int i;
                int l = p + 1 - in;

                /* check if there's `[:', `[.' or `[=' */
                for (i = 0; i < l - 1; ++i) {
                    if (in[i] == '[') {
                        char c2 = in[i + 1];
                        if (c2 == ':' || c2 == '.' || c2 == '=') {
                            free(out);
                            return NULL;
                        }
                    }
                }

                memmove(out + len, in, l);
                len += l;
                in += l;
            }

            /* ^ */
        } else if (c == '^') {
            if (in != save_in) {
                out[len++] = '\\';
            }
            out[len++] = '^';

            in += 1;

            /* $ */
        } else if (c == '$') {
            if (in[1] != 0) {
                out[len++] = '[';
            }
            out[len++] = '$';
            if (in[1] != 0) {
                out[len++] = ']';
            }

            in += 1;

            /* . + < > ( ) { } | */
        } else if (c == '.' || c == '+' || c == '|' || c == '<' || c == '>'
                   || c == '(' || c == ')' || c == '{' || c == '}') {
            out[len++] = '[';
            out[len++] = c;
            out[len++] = ']';

            in += 1;

            /* \CHAR */
        } else if (c == '\\') {
            /* \\ */
            if (in[1] == '\\') {
                out[len++] = '\\';
                out[len++] = '\\';
                in += 2;

                /* \* \? \[ \] */
            } else if (in[1] == '*' || in[1] == '?' || in[1] == '[' || in[1] == ']') {
                out[len++] = '[';
                out[len++] = in[1];
                out[len++] = ']';
                in += 2;

                /* other \CHAR is not allowed */
            } else {
                free(out);
                return NULL;
            }
        } else {
            out[len++] = c;
            in += 1;
        }
    }
    out[len] = '\0';

    * out_ = out;
    if (len_ != NULL) {
        * len_ = len;
    }

    return out;
}

#if 0
ssize_t
read_if_ready(int fd, char *buf, size_t n)
{
    struct timeval timeout;
    fd_set fds;
    int nread;

    timeout.tv_sec = 0;
    timeout.tv_usec = 100 * 1000;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if (select(fd + 1, &fds, NULL, NULL, &timeout) < 0) {
        return -1;
    }
    if (! FD_ISSET(fd, &fds) ) {
        return 0;
    }
    if ((nread = read(fd, buf, n) ) < 0) {
        return -1;
    }
    return nread;
}
#endif

/* RETURN:
 *  -1: Error and no data is read
 *   0: EOF
 *  >0: Some data (may be < n) is read
 */
ssize_t
readn(int fd, void *buf, size_t n)
{
    size_t nleft;
    ssize_t nread;

    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, buf, nleft)) < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                debug("read: %s (%d)", strerror(errno), errno);
                return -1;
            }
        } else if (nread == 0) {
            /* EOF */
            break;
        }
        nleft -= nread;
        buf   += nread;
    }
    return n - nleft;
}

/* RETURN:
 *  -1: Error and no data is written
 *   0: This may really happen?
 *  >0: Some data (may be < n) is written
 */
ssize_t
writen(int fd, const void *buf, size_t n)
{
    size_t nleft;
    ssize_t nwritten;

    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, buf, nleft)) < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                debug("write: %s (%d)", strerror(errno), errno);
                return -1;
            }
        } else if (nwritten == 0) {
            /* This may really happen? */
        }
        nleft -= nwritten;
        buf += nwritten;
    }
    return n - nleft;
}

int
name2sig(const char * name)
{
    static struct v2n_map num2name[] = {
        V2N_MAP(SIGCONT),
        V2N_MAP(SIGHUP),
        V2N_MAP(SIGINT),
        V2N_MAP(SIGKILL),
        V2N_MAP(SIGQUIT),
        V2N_MAP(SIGSTOP),
        V2N_MAP(SIGTERM),
        V2N_MAP(SIGUSR1),
        V2N_MAP(SIGUSR2),
    };
    int  i;
    char longname[32];

    if (name == NULL) {
        return -1;
    }

    if (strncasecmp(name, "SIG", 3) != 0) {
        snprintf(longname, sizeof(longname), "SIG%s", name);
    } else {
        snprintf(longname, sizeof(longname), "%s", name);
    }

    for (i = 0; i < ARRAY_SIZE(num2name); ++i) {
        if (strcaseeq(num2name[i].name, longname) ) {
            return num2name[i].val;
        }
    }

    return -1;
}

/*
 * The only portable use of signal() is to set a signal's disposition
 * to SIG_DFL or SIG_IGN.  The semantics when using signal() to
 * establish a signal handler vary across systems (and POSIX.1
 * explicitly  permits  this variation); do not use it for this purpose.
 *
 * POSIX.1 solved the portability mess by specifying sigaction(2),
 * which provides explicit control of the semantics when a signal
 * handler is invoked; use that interface instead of signal().
 *
 * In the original UNIX systems, when a handler that was established
 * using signal() was invoked by the delivery of a signal, the
 * disposition of the signal would be reset to SIG_DFL, and the system
 * did not block delivery of further instances of the signal.  This is
 * equivalent to calling sigaction(2) with the following flags:
 *
 *   sa.sa_flags = SA_RESETHAND | SA_NODEFER;
 */
void
sig_handle(int signo, void (*handler)(int) )
{
    struct sigaction act = { 0 };

    act.sa_handler = handler;
    sigaction(signo, &act, NULL);
}

int
sock_connect(char * sockpath)
{
    int sock_fd;
    struct sockaddr_un srv_addr = { 0 };

    /* connect to the socket */
    sock_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        fatal_sys("socket");
    }

    srv_addr.sun_family = AF_LOCAL;
    snprintf(srv_addr.sun_path, sizeof(srv_addr.sun_path), "%s", sockpath);
    if (connect(sock_fd, (struct sockaddr *) & srv_addr, sizeof(srv_addr) ) < 0) {
        return -1;
    }

    return sock_fd;
}

size_t
msg_size(ttlv_t *msg)
{
    return ttlv_calc_size(msg);
}

void
msg_free(ttlv_t **msg)
{
    ttlv_free(msg);
}

/* RETURN:
 *   NULL: error
 *    ptr: The whole received message.
 */
ttlv_t *
msg_recv(int fd)
{
    const int bufsize = PASS_MAX_MSG;
    static unsigned char * buf = NULL;
    uint32_t taglen; /* not including the header */
    uint32_t magic;
    int ret;
    ttlv_t * msg = NULL;

    if (buf == NULL) {
        buf = malloc(bufsize);
        if (buf == NULL) {
            fatal_sys("malloc(%d) returned NULL", bufsize);
        }
    }

    /* the magic number */
    ret = readn(fd, & magic, 4);
    if (ret < 4) {
        debug("MAGIC number is 4 bytes but got %d", ret);
        return NULL;
    }
    magic = net_get32( & magic);
    if (magic != PASS_MAGIC) {
        error("MAIGC number is 0x%08x but got 0x%08x", PASS_MAGIC, magic);
        return NULL;
    }

    ret = readn(fd, buf, TAG_HDR_SIZE);
    if (ret < TAG_HDR_SIZE) {
        error("header expected to be %d bytes but got %d", TAG_HDR_SIZE, ret);
        return NULL;
    }

    taglen = ROUND8(net_get32(buf + TAG_HDR_LEN_OFFSET) );
    if (taglen + TAG_HDR_SIZE > bufsize) {
        error("message too big: %u > %d",
              taglen + TAG_HDR_SIZE, bufsize);
        return NULL;
    }

    ret = readn(fd, buf + TAG_HDR_SIZE, taglen);
    if (ret < taglen) {
        error("tag.length is %d but got %d bytes", taglen, ret);
        return NULL;
    }

    ret = ttlv_decode(buf, TAG_HDR_SIZE + taglen, & msg);
    if (ret < TAG_HDR_SIZE + taglen) {
        error("message is %d bytes but only decoded %d",
            TAG_HDR_SIZE + taglen, ret);
        msg_free( & msg);
        return NULL;
    }

    return msg;
}

/* RETURN:
 *  -1: error
 *  >0: The whole message has been sent.
 */
ssize_t
msg_send(int fd, ttlv_t *msg)
{
    const int bufsize = PASS_MAX_MSG;
    static unsigned char * buf = NULL;
    uint32_t magic;
    int ret, size;

    if (buf == NULL) {
        buf = malloc(bufsize);
        if (buf == NULL) {
            fatal_sys("malloc(%d) returned NULL", bufsize);
            return -1;
        }
    }

    size = ttlv_calc_size(msg);
    if (size > bufsize) {
        error("message too large (%d bytes)", size);
        return -1;
    }

    ret = ttlv_encode(msg, buf, bufsize);
    if (size != ret) {
        error("message is %d bytes but only encoded %d", size, ret);
        return -1;
    }

    /* the magic number */
    net_put32(PASS_MAGIC, & magic);
    ret = writen(fd, & magic, 4);
    if (ret < 4) {
        debug("write(MAGIC) returned %d", ret);
        return -1;
    }

    ret = writen(fd, buf, size);
    if (ret < size) {
        debug("writen(%d) returned %d", size, ret);
        return -1;
    } else {
        return ret;
    }
}

ssize_t
msg_hello(int fd)
{
    int ret;
    ttlv_t * msg;

    msg = ttlv_new_struct(TAG_HELLO);
    ret = msg_send(fd, msg);
    msg_free(&msg);

    return ret;
}

ssize_t
msg_disconn(int fd)
{
    int ret;
    ttlv_t * msg;

    msg = ttlv_new_struct(TAG_DISCONN);
    ret = msg_send(fd, msg);
    msg_free(&msg);

    return ret;
}
