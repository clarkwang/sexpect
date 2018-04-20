
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
ptag_free(ptag_t ** head)
{
    ptag_t * cur, * next;

    if (* head == NULL) {
        return;
    }

    for (cur = * head; cur != NULL; cur = next) {
        next = cur->next;
        if (cur->child != NULL) {
            ptag_free(&cur->child);
        }
        free(cur);
    }

    * head = NULL;
    return;
}

int
ptag_calc_size_ex(ptag_t * head, bool cur_tag_only)
{
    int size = 0;
    ptag_t * p = NULL;

    /* a structure may have no children */
    if (head == NULL) {
        return 0;
    }

    for (p = head; p != NULL; p = p->next) {
        if (p->type != PTYPE_STRUCT) {
            size += PTAG_HDR_SIZE + ROUND8(p->length);
        } else {
            size += PTAG_HDR_SIZE + ptag_calc_size_ex(p->child, false);
        }

        if (cur_tag_only) {
            break;
        }
    }

    return size;
}

ptag_t *
ptag_new(uint32_t tag, ptag_type_t type, uint32_t length)
{
    ptag_t * p;

    if (type == PTYPE_TEXT || type == PTYPE_RAW) {
        p = calloc(1, sizeof(ptag_t) - 8 /* == sizeof(the union) */ + length + 1);
    } else {
        p = calloc(1, sizeof(ptag_t) - 8 /* == sizeof(the union) */ + length);
    }
    p->tag = tag;
    p->type = type;
    p->length = length;

    return p;
}

ptag_t *
ptag_new_int(uint32_t tag, int32_t value)
{
    ptag_t * p;

    p = ptag_new(tag, PTYPE_INT, sizeof(p->v_int) );
    p->v_int = value;

    return p;
}

ptag_t *
ptag_new_long(uint32_t tag, int64_t value)
{
    ptag_t * p;

    p = ptag_new(tag, PTYPE_LONG, sizeof(p->v_long) );
    p->v_long = value;

    return p;
}

ptag_t *
ptag_new_bool(uint32_t tag, uint32_t value)
{
    ptag_t * p;

    p = ptag_new(tag, PTYPE_BOOL, sizeof(p->v_bool) );
    p->v_bool = !! value;

    return p;
}

ptag_t *
ptag_new_text(uint32_t tag, uint32_t length, char *data)
{
    ptag_t * p;

#if 0
    /* DO NOT DO THIS! THE BUFFER MAY BE REUSED SO THERE MAY BE OLD DATA. */
    if (length == 0) {
        length = strlen(data);
    }
#endif
    p = ptag_new(tag, PTYPE_TEXT, length);
    memcpy(p->v_raw, data, length);
    p->v_raw[length] = 0;

    return p;
}

ptag_t *
ptag_new_raw(uint32_t tag, uint32_t length, char *data)
{
    ptag_t * p;

#if 0
    /* DO NOT DO THIS! THE BUFFER MAY BE REUSED SO THERE MAY BE OLD DATA. */
    if (length == 0) {
        length = strlen(data);
    }
#endif
    p = ptag_new(tag, PTYPE_RAW, length);
    memcpy(p->v_raw, data, length);
    p->v_raw[length] = 0;

    return p;
}

ptag_t *
ptag_new_struct(uint32_t tag)
{
    ptag_t * p;

    p = ptag_new(tag, PTYPE_STRUCT, 0);

    return p;
}

ptag_t *
ptag_append(ptag_t * head, ptag_t * p)
{
    ptag_t * q = head;

    while (q->next != NULL) {
        q = q->next;
    }
    q->next = p;

    return head;
}

ptag_t *
ptag_append_child(ptag_t * head, ... /* ptag_t * child, ..., NULL */)
{
    ptag_t * child, * tail = head->child;
    va_list ap;

    while (tail != NULL && tail->next != NULL) {
        tail = tail->next;
    }

    va_start(ap, head);
    while (1) {
        child = va_arg(ap, ptag_t *);
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

ptag_t *
ptag_find_child(ptag_t *parent, uint32_t tag)
{
    if (parent == NULL || parent->child == NULL)
        return NULL;
    if (parent->child->tag == tag)
        return parent->child;
    return ptag_find_sibling(parent->child, tag);
}

ptag_t *
ptag_find_sibling(ptag_t *first, uint32_t tag)
{
    ptag_t *next;

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
ptag_value_hton(ptag_t * head, uint8_t * buf, uint32_t buf_len)
{
    char zeros[8] = { 0 };

    switch (head->type) {
    case PTYPE_STRUCT:
        return 0;

    case PTYPE_INT:
    case PTYPE_BOOL:
        if (buf_len < 8) {
            goto L_buf_too_small;
        }
        net_put32(head->v_int, buf);
        /* padding */
        *(uint32_t *)(buf + 4) = 0;
        return 8;

    case PTYPE_LONG:
        if (buf_len < 8) {
            goto L_buf_too_small;
        }
        net_put64(head->v_long, buf);
        return 8;

    case PTYPE_TEXT:
    case PTYPE_RAW:
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
ptag_value_ntoh(ptag_t * head, uint8_t * buf, uint32_t buf_len)
{
    switch (head->type) {
    case PTYPE_STRUCT:
        return 0;

    case PTYPE_INT:
    case PTYPE_BOOL:
        if (buf_len < 8) {
            return -1;
        }
        head->v_int = net_get32(buf);
        return 8;

    case PTYPE_LONG:
        if (buf_len < 8) {
            return -1;
        }
        head->v_long = net_get64(buf);
        return 8;

    case PTYPE_TEXT:
    case PTYPE_RAW:
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
ptag_valid_len(uint32_t type, uint32_t length)
{
    switch (type) {
    case PTYPE_STRUCT:
        if (length % 8 != 0) {
            return false;
        }
        break;

    case PTYPE_INT:
    case PTYPE_BOOL:
        if (length != 4) {
            return false;
        }
        break;

    case PTYPE_LONG:
        if (length != 8) {
            return false;
        }
        break;
    }

    return true;
}

/* FIXME: Is it required the pad with NULL bytes? */
bool
ptag_valid_pad(uint32_t type, uint32_t length, char *data)
{
    return true;
}

/*
 * RETURN:
 *  >= 0: # of bytes encoded
 *  <  0: Failed
 */
int
ptag_encode(ptag_t * head, uint8_t * buf, uint32_t buf_len)
{
    uint32_t tagtype;
    uint32_t * plength = NULL;
    int ret = 0;
    uint8_t * next_buf = buf;
    ptag_t * p = NULL;

    /* a structure may have no children */
    if (head == NULL) {
        return 0;
    }

    for (p = head; p != NULL; p = p->next) {
        /* buffer enough for tag, type & length? */
        if (next_buf + PTAG_HDR_SIZE > buf + buf_len) {
            fprintf(stderr, "%s: buffer not enough (tag %#x)\n",
                __func__, head->tag);
            return -1;
        }

        /* tag & type */
        tagtype = (p->tag << 8) | p->type;
        net_put32(tagtype, next_buf);
        next_buf += 4;

        if (p->type != PTYPE_STRUCT) {
            /* length */
            net_put32(p->length, next_buf);
            next_buf += 4;

            /* value */
            ret = ptag_value_hton(p, next_buf, buf + buf_len - next_buf);
            if (ret < 0) {
                return ret;
            }
            next_buf += ret;
        } else {
            /* length */
            plength = (void *)next_buf;
            next_buf += 4;

            /* child */
            ret = ptag_encode(p->child, next_buf, buf + buf_len - next_buf);
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
ptag_decode(uint8_t * buf, uint32_t buf_len, ptag_t ** head)
{
    uint32_t tagtype, tag, type;
    uint32_t length;
    uint8_t * next_buf = buf;
    ptag_t * new = NULL, * tail = NULL;
    int ret;

    *head = NULL;

    /* a structure may have no children */
    if (buf_len == 0) {
        return 0;
    }

    while (next_buf < buf + buf_len) {
        /* tag, type, length */
        if (next_buf + PTAG_HDR_SIZE > buf + buf_len) {
            ptag_free(head);
            return -1;
        }

        /* tag & type */
        tagtype = net_get32(next_buf);
        next_buf += 4;
        tag = tagtype >> 8;
        type = tagtype & 0xff;

        /* check if type is valid */
        if (type == 0 || type > PTYPE_MAX) {
            ptag_free(head);
            return -1;
        }

        /* FIXME: check tag types */

        /* length */
        length = net_get32(next_buf);
        next_buf += 4;

        /* check if length is valid */
        if (!ptag_valid_len(tagtype, length)) {
            ptag_free(head);
            return -1;
        }

        /* value */
        if (next_buf + length > buf + buf_len) {
            ptag_free(head);
            return -1;
        }

        /* check if padding is valid */
        if (!ptag_valid_pad(tagtype, length, (void *)next_buf)) {
            ptag_free(head);
            return -1;
        }

        /* decode */
        if (type != PTYPE_STRUCT) {
            /* not structures */
            new = ptag_new(tag, type, length);
            ret = ptag_value_ntoh(new, next_buf, buf + buf_len - next_buf);
            if (ret < 0) {
                return ret;
            }
            next_buf += ret;
        } else {
            /* structures */
            new = ptag_new_struct(tag);
            ret = ptag_decode(next_buf, length, & new->child);
            if (ret < 0) {
                ptag_free(head);
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
        ptag_free(head);
        return -1;
    }
}
