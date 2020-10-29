
#ifndef PROTO_H__
#define PROTO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <inttypes.h>

#define ROUND8(n)  ( (n+7) & ~7 )

#define net_get16(ptr)        ntohs( *(uint16_t *)(ptr))
#define net_get32(ptr)        ntohl( *(uint32_t *)(ptr))
#define net_get64(ptr)       _ntohll(*(uint64_t *)(ptr))
#define net_put16(val, ptr)   *(uint16_t *)(ptr) =  htons(val)
#define net_put32(val, ptr)   *(uint32_t *)(ptr) =  htonl(val)
#define net_put64(val, ptr)   *(uint64_t *)(ptr) = _htonll(val)

/* tag + type + length */
#define TAG_HDR_SIZE       8
#define TAG_HDR_LEN_OFFSET 4

typedef enum {
    TTYPE_INT    = 0x01, /* int32 */
    TTYPE_LONG   = 0x02, /* int64 */
    TTYPE_BOOL   = 0x03, /* int32 */
    TTYPE_TEXT   = 0x04, /* NULL terminated string */
    TTYPE_RAW    = 0x05, /* raw bytes */
    TTYPE_STRUCT = 0x06, /* struct */

    TTYPE_END__,
} ttlv_type_t;

typedef struct ttlv {
    struct ttlv * child;
    struct ttlv * next;

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
} ttlv_t;

uint64_t _htonll(uint64_t n);
uint64_t _ntohll(uint64_t n);

bool    ttlv_valid_len(uint32_t type, uint32_t length);
bool    ttlv_valid_pad(uint32_t type, uint32_t length, char *data);
int     ttlv_count_tags(ttlv_t *head, uint32_t tag);
int     ttlv_encode(ttlv_t * head, uint8_t * buf, uint32_t buf_len);
int     ttlv_decode(uint8_t * buf, uint32_t buf_len, ttlv_t ** head);
int     ttlv_value_hton(ttlv_t * head, uint8_t * buf, uint32_t buf_len);
int     ttlv_value_ntoh(ttlv_t * head, uint8_t * buf, uint32_t buf_len);
ttlv_t  *ttlv_append(ttlv_t * head, ttlv_t * p);
ttlv_t  *ttlv_append_child(ttlv_t * head, ... /* child, child, ... NULL */);
ttlv_t  *ttlv_find_child(ttlv_t *parent, uint32_t tag);
ttlv_t  *ttlv_find_sibling(ttlv_t *first, uint32_t tag);

ttlv_t  *ttlv_new(uint32_t tag, ttlv_type_t type, uint32_t length);
ttlv_t  *ttlv_new_int(uint32_t tag, int32_t value);
ttlv_t  *ttlv_new_long(uint32_t tag, int64_t value);
ttlv_t  *ttlv_new_bool(uint32_t tag, uint32_t value);
ttlv_t  *ttlv_new_text(uint32_t tag, uint32_t length, char *text);
ttlv_t  *ttlv_new_raw(uint32_t tag, uint32_t length, char *text);
ttlv_t  *ttlv_new_struct(uint32_t tag);

void    ttlv_free(ttlv_t ** head);

int     ttlv_calc_size_ex(ttlv_t * head, bool cur_tag_only);
#define ttlv_calc_size(head)            ttlv_calc_size_ex(head, false)

#endif
