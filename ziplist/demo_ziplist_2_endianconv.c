#include <stdint.h>
#include "demo_ziplist_2_endianconv.h"

// 将 *p 指向的16位无符号整数从 little endian 切换到 big endian
void memrev16(void *p) {

    unsigned char *x = (unsigned char *)p, t;

    t = x[0];
    x[0] = x[1];
    x[1] = t;
}

// 将 *p指向的32位无符号整数从little endian 切换到 big endian
void memrev32(void *p) {

    unsigned char *x = (unsigned char *) p, t;

    t = x[0];
    x[0] = x[3];
    x[3] = t;

    t = x[1];
    x[1] = x[2];
    x[2] = t;
}

// 将 *p 指向的64位无符号整数从little endia 切换到  big endia
void memrev64(void *p) {

    unsigned char *x = (unsigned char *) p, t;

    t = x[0];
    x[0] = x[7];
    x[7] = t;

    t = x[1];
    x[1] = x[6];
    x[6] = t;

    t = x[2];
    x[2] = x[5];
    x[5] = t;

    t = x[3];
    x[3] = x[4];
    x[4] = t;
}

uint16_t intrev16(uint16_t v) {

    memrev16(&v);
    return v;
}

uint32_t intrev32(uint32_t v) {

    memrev32(&v);
    return v;
}

uint64_t intrev64(uint64_t v) {

    memrev64(&v);
    return v;
}
