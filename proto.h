
#ifndef KMIP_H__
#define KMIP_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <inttypes.h>

#define ROUND8(n)  ( (n+7) & ~7 )

#define net_get16(src)        ntohs( *(uint16_t *)(src))
#define net_get32(src)        ntohl( *(uint32_t *)(src))
#define net_get64(src)       _ntohll(*(uint64_t *)(src))
#define net_put16(src, dst)   *(uint16_t *)(dst) =  htons(src)
#define net_put32(src, dst)   *(uint32_t *)(dst) =  htonl(src)
#define net_put64(src, dst)   *(uint64_t *)(dst) = _htonll(src)

/* tag + type + length */
#define PTAG_HDR_SIZE       8
#define PTAG_HDR_LEN_OFFSET 4

typedef enum {
    PTYPE_INT    = 0x01, /* int32 */
    PTYPE_LONG   = 0x02, /* int64 */
    PTYPE_BOOL   = 0x03, /* int32 */
    PTYPE_TEXT   = 0x04,
    PTYPE_RAW    = 0x05,
    PTYPE_STRUCT = 0x06,
} ptag_type_t;
#define PTYPE_MAX PTYPE_STRUCT

typedef struct ptag {
    struct ptag * child;
    struct ptag * next;

    uint32_t    tag:24;
    uint32_t    type:8;
    uint32_t    length;

    union {
        int32_t         v_int;
        uint32_t        v_uint;

        int64_t         v_long;
        uint64_t        v_ulong;

        uint32_t        v_bool;

        uint8_t         v_raw[1];
        uint8_t         v_text[1];
    };
} ptag_t;

uint64_t _htonll(uint64_t n);
uint64_t _ntohll(uint64_t n);

bool    ptag_valid_len(uint32_t type, uint32_t length);
bool    ptag_valid_pad(uint32_t type, uint32_t length, char *data);
int     ptag_count_tags(ptag_t *head, uint32_t tag);
int     ptag_encode(ptag_t * head, uint8_t * buf, uint32_t buf_len);
int     ptag_decode(uint8_t * buf, uint32_t buf_len, ptag_t ** head);
int     ptag_value_hton(ptag_t * head, uint8_t * buf, uint32_t buf_len);
int     ptag_value_ntoh(ptag_t * head, uint8_t * buf, uint32_t buf_len);
ptag_t  *ptag_append(ptag_t * head, ptag_t * p);
ptag_t  *ptag_append_child(ptag_t * head, ... /* child, child, ... NULL */);
ptag_t  *ptag_find_child(ptag_t *parent, uint32_t tag);
ptag_t  *ptag_find_sibling(ptag_t *first, uint32_t tag);

ptag_t  *ptag_new(uint32_t tag, ptag_type_t type, uint32_t length);
ptag_t  *ptag_new_int(uint32_t tag, int32_t value);
ptag_t  *ptag_new_long(uint32_t tag, int64_t value);
ptag_t  *ptag_new_bool(uint32_t tag, uint32_t value);
ptag_t  *ptag_new_text(uint32_t tag, uint32_t length, char *text);
ptag_t  *ptag_new_raw(uint32_t tag, uint32_t length, char *text);
ptag_t  *ptag_new_struct(uint32_t tag);

void    ptag_free(ptag_t ** head);

int     ptag_calc_size_ex(ptag_t * head, bool cur_tag_only);
#define ptag_calc_size(head)            ptag_calc_size_ex(head, false)

#endif
