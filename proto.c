
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "proto.h"

uint64_t
_htonll(uint64_t n)
{
    int endian = 1;

    if (*(char *)&endian == 1) {
        uint32_t high = n >> 32;
        uint32_t low = n;
        return ((uint64_t)htonl(low) << 32) | htonl(high);
    } else {
        /* big endian */
        return n;
    }
}

uint64_t
_ntohll(uint64_t n)
{
    int endian = 1;

    if (*(char *)&endian == 1) {
        uint32_t high = n >> 32;
        uint32_t low = n;
        return ((uint64_t)ntohl(low) << 32) | ntohl(high);
    } else {
        /* big endian */
        return n;
    }
}

void
ttlv_free(ttlv_t ** head)
{
    ttlv_t * cur, * next;

    if (* head == NULL) {
        return;
    }

    for (cur = * head; cur != NULL; cur = next) {
        next = cur->next;
        if (cur->child != NULL) {
            ttlv_free(&cur->child);
        }
        free(cur);
    }

    * head = NULL;
    return;
}

int
ttlv_calc_size_ex(ttlv_t * head, bool cur_tag_only)
{
    int size = 0;
    ttlv_t * p = NULL;

    /* a structure may have no children */
    if (head == NULL) {
        return 0;
    }

    for (p = head; p != NULL; p = p->next) {
        if (p->type != TTYPE_STRUCT) {
            size += TAG_HDR_SIZE + ROUND8(p->length);
        } else {
            size += TAG_HDR_SIZE + ttlv_calc_size_ex(p->child, false);
        }

        if (cur_tag_only) {
            break;
        }
    }

    return size;
}

ttlv_t *
ttlv_new(uint32_t tag, ttlv_type_t type, uint32_t length)
{
    ttlv_t * p;

    if (type == TTYPE_TEXT || type == TTYPE_RAW) {
        p = calloc(1, sizeof(ttlv_t) - 8 /* == sizeof(the union) */ + length + 1);
    } else {
        p = calloc(1, sizeof(ttlv_t) - 8 /* == sizeof(the union) */ + length);
    }
    p->tag = tag;
    p->type = type;
    p->length = length;

    return p;
}

ttlv_t *
ttlv_new_int(uint32_t tag, int32_t value)
{
    ttlv_t * p;

    p = ttlv_new(tag, TTYPE_INT, sizeof(p->v_int) );
    p->v_int = value;

    return p;
}

ttlv_t *
ttlv_new_long(uint32_t tag, int64_t value)
{
    ttlv_t * p;

    p = ttlv_new(tag, TTYPE_LONG, sizeof(p->v_long) );
    p->v_long = value;

    return p;
}

ttlv_t *
ttlv_new_bool(uint32_t tag, uint32_t value)
{
    ttlv_t * p;

    p = ttlv_new(tag, TTYPE_BOOL, sizeof(p->v_bool) );
    p->v_bool = !! value;

    return p;
}

ttlv_t *
ttlv_new_text(uint32_t tag, uint32_t length, char *data)
{
    ttlv_t * p;

#if 0
    /* DO NOT DO THIS! THE BUFFER MAY BE REUSED SO THERE MAY BE OLD DATA. */
    if (length == 0) {
        length = strlen(data);
    }
#endif
    p = ttlv_new(tag, TTYPE_TEXT, length);
    memcpy(p->v_raw, data, length);
    p->v_raw[length] = 0;

    return p;
}

ttlv_t *
ttlv_new_raw(uint32_t tag, uint32_t length, char *data)
{
    ttlv_t * p;

#if 0
    /* DO NOT DO THIS! THE BUFFER MAY BE REUSED SO THERE MAY BE OLD DATA. */
    if (length == 0) {
        length = strlen(data);
    }
#endif
    p = ttlv_new(tag, TTYPE_RAW, length);
    memcpy(p->v_raw, data, length);
    p->v_raw[length] = 0;

    return p;
}

ttlv_t *
ttlv_new_struct(uint32_t tag)
{
    ttlv_t * p;

    p = ttlv_new(tag, TTYPE_STRUCT, 0);

    return p;
}

ttlv_t *
ttlv_append(ttlv_t * head, ttlv_t * p)
{
    ttlv_t * q = head;

    while (q->next != NULL) {
        q = q->next;
    }
    q->next = p;

    return head;
}

ttlv_t *
ttlv_append_child(ttlv_t * head, ... /* ttlv_t * child, ..., NULL */)
{
    ttlv_t * child, * tail = head->child;
    va_list ap;

    while (tail != NULL && tail->next != NULL) {
        tail = tail->next;
    }

    va_start(ap, head);
    while (1) {
        child = va_arg(ap, ttlv_t *);
        if (child == NULL) {
            break;
        }

        if (head->child == NULL) {
            head->child = child;
        } else {
            tail->next = child;
        }
        tail = child;
    }
    va_end(ap);

    return head;
}

ttlv_t *
ttlv_find_child(ttlv_t *parent, uint32_t tag)
{
    if (parent == NULL || parent->child == NULL)
        return NULL;
    if (parent->child->tag == tag)
        return parent->child;
    return ttlv_find_sibling(parent->child, tag);
}

ttlv_t *
ttlv_find_sibling(ttlv_t *first, uint32_t tag)
{
    ttlv_t *next;

    for (next = first->next; next != NULL; next = next->next) {
        if (next->tag == tag) {
            return next;
        }
    }

    return NULL;
}

/*
 * RETURN:
 *   >=0: # of bytes
 *   <0 : buffer not enough 
 */
int
ttlv_value_hton(ttlv_t * head, uint8_t * buf, uint32_t buf_len)
{
    char zeros[8] = { 0 };

    switch (head->type) {
    case TTYPE_STRUCT:
        return 0;

    case TTYPE_INT:
    case TTYPE_BOOL:
        if (buf_len < 8) {
            goto L_buf_too_small;
        }
        net_put32(head->v_int, buf);
        /* padding */
        *(uint32_t *)(buf + 4) = 0;
        return 8;

    case TTYPE_LONG:
        if (buf_len < 8) {
            goto L_buf_too_small;
        }
        net_put64(head->v_long, buf);
        return 8;

    case TTYPE_TEXT:
    case TTYPE_RAW:
        if (buf_len < ROUND8(head->length)) {
            goto L_buf_too_small;
        }
        memcpy(buf, head->v_raw, head->length);
        /* padding */
        if (head->length % 8 != 0) {
            memcpy(buf + head->length, zeros, 8 - head->length % 8);
        }
        return ROUND8(head->length);
    }

    fprintf(stderr, "%s: unknonw type: %d (tag %#x)\n", __func__,
        head->type, head->tag);
    return -1;

L_buf_too_small:
    fprintf(stderr, "%s: buffer not enough (tag %#x)\n", __func__, head->tag);
    return -1;
}

/*
 * RETURN:
 *   >=0: # of bytes
 *   <0 : buffer not enough 
 */
int
ttlv_value_ntoh(ttlv_t * head, uint8_t * buf, uint32_t buf_len)
{
    switch (head->type) {
    case TTYPE_STRUCT:
        return 0;

    case TTYPE_INT:
    case TTYPE_BOOL:
        if (buf_len < 8) {
            return -1;
        }
        head->v_int = net_get32(buf);
        return 8;

    case TTYPE_LONG:
        if (buf_len < 8) {
            return -1;
        }
        head->v_long = net_get64(buf);
        return 8;

    case TTYPE_TEXT:
    case TTYPE_RAW:
        if (buf_len < ROUND8(head->length)) {
            return -1;
        }
        memcpy(head->v_raw, buf, head->length);
        head->v_raw[head->length] = 0;
        return ROUND8(head->length);
    }

    return -1;
}

bool
ttlv_valid_len(uint32_t type, uint32_t length)
{
    switch (type) {
    case TTYPE_STRUCT:
        if (length % 8 != 0) {
            return false;
        }
        break;

    case TTYPE_INT:
    case TTYPE_BOOL:
        if (length != 4) {
            return false;
        }
        break;

    case TTYPE_LONG:
        if (length != 8) {
            return false;
        }
        break;
    }

    return true;
}

/* FIXME: Is it required the pad with NULL bytes? */
bool
ttlv_valid_pad(uint32_t type, uint32_t length, char *data)
{
    return true;
}

/*
 * RETURN:
 *  >= 0: # of bytes encoded
 *  <  0: Failed
 */
int
ttlv_encode(ttlv_t * head, uint8_t * buf, uint32_t buf_len)
{
    uint32_t tagtype;
    uint32_t * plength = NULL;
    int ret = 0;
    uint8_t * next_buf = buf;
    ttlv_t * p = NULL;

    /* a structure may have no children */
    if (head == NULL) {
        return 0;
    }

    for (p = head; p != NULL; p = p->next) {
        /* buffer enough for tag, type & length? */
        if (next_buf + TAG_HDR_SIZE > buf + buf_len) {
            fprintf(stderr, "%s: buffer not enough (tag %#x)\n",
                __func__, head->tag);
            return -1;
        }

        /* tag & type */
        tagtype = (p->tag << 8) | p->type;
        net_put32(tagtype, next_buf);
        next_buf += 4;

        if (p->type != TTYPE_STRUCT) {
            /* length */
            net_put32(p->length, next_buf);
            next_buf += 4;

            /* value */
            ret = ttlv_value_hton(p, next_buf, buf + buf_len - next_buf);
            if (ret < 0) {
                return ret;
            }
            next_buf += ret;
        } else {
            /* length */
            plength = (void *)next_buf;
            next_buf += 4;

            /* child */
            ret = ttlv_encode(p->child, next_buf, buf + buf_len - next_buf);
            if (ret < 0) {
                return ret;
            }
            next_buf += ret;

            /* length */
            net_put32(ret, plength);
        }
    }

    return next_buf - buf;
}

/*
 * RETURN:
 *  >= 0: # of bytes decoded
 *  <  0: Failed
 */
int
ttlv_decode(uint8_t * buf, uint32_t buf_len, ttlv_t ** head)
{
    uint32_t tagtype, tag, type;
    uint32_t length;
    uint8_t * next_buf = buf;
    ttlv_t * new = NULL, * tail = NULL;
    int ret;

    *head = NULL;

    /* a structure may have no children */
    if (buf_len == 0) {
        return 0;
    }

    while (next_buf < buf + buf_len) {
        /* tag, type, length */
        if (next_buf + TAG_HDR_SIZE > buf + buf_len) {
            ttlv_free(head);
            return -1;
        }

        /* tag & type */
        tagtype = net_get32(next_buf);
        next_buf += 4;
        tag = tagtype >> 8;
        type = tagtype & 0xff;

        /* check if type is valid */
        if (type == 0 || type >= TTYPE_END__) {
            ttlv_free(head);
            return -1;
        }

        /* FIXME: check tag types */

        /* length */
        length = net_get32(next_buf);
        next_buf += 4;

        /* check if length is valid */
        if (!ttlv_valid_len(tagtype, length)) {
            ttlv_free(head);
            return -1;
        }

        /* value */
        if (next_buf + length > buf + buf_len) {
            ttlv_free(head);
            return -1;
        }

        /* check if padding is valid */
        if (!ttlv_valid_pad(tagtype, length, (void *)next_buf)) {
            ttlv_free(head);
            return -1;
        }

        /* decode */
        if (type != TTYPE_STRUCT) {
            /* not structures */
            new = ttlv_new(tag, type, length);
            ret = ttlv_value_ntoh(new, next_buf, buf + buf_len - next_buf);
            if (ret < 0) {
                return ret;
            }
            next_buf += ret;
        } else {
            /* structures */
            new = ttlv_new_struct(tag);
            ret = ttlv_decode(next_buf, length, & new->child);
            if (ret < 0) {
                ttlv_free(head);
                return ret;
            }
            next_buf += ret;
        }
        if (* head == NULL) {
            * head = new;
        } else {
            tail->next = new;
        }
        tail = new;
    }

    if (next_buf - buf == buf_len) {
        return buf_len;
    } else {
        ttlv_free(head);
        return -1;
    }
}
