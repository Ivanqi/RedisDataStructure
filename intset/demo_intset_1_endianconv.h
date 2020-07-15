#ifndef __ENDIANCONV_1_H
#define __ENDIANCONV_1_H

#include <stdint.h>

void memrev16(void *p);
void memrev32(void *p);
void memrev64(void *p);

uint16_t intrev16(uint64_t v);
uint32_t intrev32(uint32_t v);
uint64_t intrev64(uint64_t v);

#define memrev16ifbe(p) memrev16(p)
#define memrev32ifbe(p) memrev32(p)
#define memrev64ifbe(p) memrev64(p)
#define intrev16ifbe(v) intrev16(v)
#define intrev32ifbe(v) intrev32(v)
#define intrev64ifbe(v) intrev64(v)

#endif