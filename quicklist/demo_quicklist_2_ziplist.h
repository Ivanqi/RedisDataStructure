#ifndef __ZIPLIST_2_H
#define __ZIPLIST_2_H

#include "demo_quicklist_2_endianconv.h"
#include <assert.h>


#define ZIPLIST_HEAD 0
#define ZIPLIST_TAIL 1
#define ZIP_END 255 
#define ZIP_BIG_PREVLEN 254

// 不同的编码/长度可能性
#define ZIP_STR_MASK 0xc0 // 1100 0000
#define ZIP_INT_MASK 0x30 // 0011 0000
#define ZIP_STR_06B (0 << 6) // 0 * (2 ^ 6)
#define ZIP_STR_14B (1 << 6) // 1 * (2 ^ 6)
#define ZIP_STR_32B (2 << 6) // 2 * (2 ^ 6)
#define ZIP_INT_16B (0xc0 | 0 << 4)
#define ZIP_INT_32B (0xc0 | 1 << 4)
#define ZIP_INT_64B (0xc0 | 2 << 4)
#define ZIP_INT_24B (0xc0 | 3 << 4)
#define ZIP_INT_8B 0xfe // 1111 1110

// 4 bit integer immediate encoding
#define ZIP_INT_IMM_MASK 0x0f // 1111
#define ZIP_INT_IMM_MIN 0xf1  // 1111 0001
#define ZIP_INT_IMM_MAX 0xfd  // 1111 1101

#define INT24_MAX 0x7fffff  // 0111 1111 1111 1111 1111 1111
#define INT24_MIN (-INT24_MAX - 1)

#define ZIP_IS_STR(enc) (((enc) & ZIP_STR_MASK) < ZIP_STR_MASK)

/**
 * 用于取出 zl 各部分值的宏
 * 所有宏复杂度都为O(1)
 */

// 取出列表以字节计算列表长度(内存 0 - 31位，整数)
#define ZIPLIST_BYTES(zl)  (*((uint32_t*)(zl)))

// 取出列表的表尾偏移量(内存32 - 63位，整数)
#define ZIPLIST_TAIL_OFFSET(zl) (*((uint32_t*)((zl) + sizeof(uint32_t))))

// 取出列表的长度(内存的64 - 79位， 整数)
#define ZIPLIST_LENGTH(zl)  (*((uint16_t*)((zl) + sizeof(uint32_t) * 2)))

// 列表的 header的长度
#define ZIPLIST_HEADER_SIZE (sizeof(uint32_t) * 2 + sizeof(uint16_t))   // 32 * 2bit + 16bit

#define ZIPLIST_END_SIZE  (sizeof(uint8_t))

// 返回列表的header之后的位置
#define ZIPLIST_ENTRY_HEAD(zl) ((zl) + ZIPLIST_HEADER_SIZE)

// 返回列表最后一个元素之后的位置
#define ZIPLIST_ENTRY_TAIL(zl) ((zl) + intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl)))

// 返回列表的结束符之前的位置
#define ZIPLIST_ENTRY_END(zl) ((zl) + intrev32ifbe(ZIPLIST_BYTES(zl)) - 1)

/**
 * 对<zllen> 增一
 * ZIPLIST_LENGTH(zl) 的最大值为 UINT16_MAX
 * 复杂度: O(1)
 */
#define ZIPLIST_INCR_LENGTH(zl, incr) {  \
    if (ZIPLIST_LENGTH(zl) < UINT16_MAX)    \
        ZIPLIST_LENGTH(zl) = intrev16ifbe(intrev16ifbe(ZIPLIST_LENGTH(zl)) + incr);    \
}

/**
 * ZIPLIST OVERALL LAYOUT:
 * 
 * ziplist 的内存结构
 *  <zlbytes><zltail><zllen><entry><entry><zlend>
 *  
 *  <zlbytes> 是一个无符号整数(uint32_t)，用于记录整个 ziplist 所占用的字节数量。
 *  通过保存这个值，可以在不遍历整个 ziplist 的前提下，对整个 ziplist 进行内存重分配。
 * 
 *  <zltail> 是到列表中最后一个节点的偏移量(同样为 uint32_t)。
 *  有了这个偏移量，就可以在常数复杂度内对表尾进行操作，而不必遍历整个列表。
 * 
 *  <zllen> 是节点的数量，为 ``uint16_t`` 。
 *  当这个值大于 2**16-2 时，需要遍历整个列表，才能计算出列表的长度
 * 
 *  <zlend> 是一个单字节的特殊值，等于 255 ，它标识了列表的末端。
 * 
 * ZIPLIST ENTRIES
 *  Ziplist 中的每个节点，都带有一个 header 作为前缀
 *  
 *  Header 包括两部分：
 *      1) 前一个节点的长度，在从后往前遍历时使用
 *      2) 当前节点所保存的值的类型和长度 
 * 
 * 前一个节点的长度的储存方式如下：
 *      1) 如果节点的长度 < 254 字节，那么直接用一个字节保存这个值。
 *      2) 如果节点的长度 >= 254 字节，那么将第一个字节设置为 254 ，再在之后用 4 个字节来表示节点的实际长度（共使用 5 个字节）。
 * 
 * 另一个 header 域保存的信息取决于这个节点所保存的内容本身。
 * 
 * 当节点保存的是字符串时，header 的前 2 位用于指示保存内容长度所使用的编码方式，之后跟着的是内容长度的值。
 * 
 * 当节点保存的是整数时，header 的前 2 位都设置为 1 ，之后的 2 位用于指示保存的整数值的类型（这个类型决定了内容所占用的空间）。
 * 
 * 以下是不同类型 header 的概览：
 *  |00pppppp| - 1 byte
 *      长度 <= 63 字节(6 位)的字符串值
 *  |01pppppp|qqqqqqqq| - 2 bytes
 *      长度 <= 16383 字节(14 位)的字符串值
 *  |10______|qqqqqqqq|rrrrrrrr|ssssssss|tttttttt| - 5 bytes
 *      长度 >= 16384 字节， <= 4294967295 的字符串值，
 *  |11000000| - 1 byte
 *      以 int16_t (2 字节)类型编码的整数
 *  |11010000| - 1 byte
 *      以 int32_t (4 字节)类型编码的整数
 *  |11100000| - 1 byte
 *      以 int64_t (8 字节)类型编码的整数
 *  |11110000| - 1 byte
 *      24 位(3 字节)有符号编码整数
 *  |11111110| - 1 byte
 *      8 位(1 字节)有符号编码整数
 *  |1111xxxx|
 *      (介于 0000 和 1101 之间)的 4 位整数，可用于表示无符号整数 0 至 12 。
 *      因为 0000 和 1111 都已经被占用，因此，可被编码的值实际上只能是 1 至 13 ，
 *      要将这个值减去 1 ，才能得到正确的值。
 *  |11111111| - End of ziplist.
 *      ziplist 的终结符
 * 
 * 所有整数都以小端表示。
 *      
 */
typedef struct zlentry {
    unsigned int prevrawlensize;    // 保存前一节点的长度所需的长度
    unsigned int prevrawlen;        // 前一节点的长度

    unsigned int lensize;           // 保存节点的长度所需要的长度
    unsigned int  len;               // 节点长度

    unsigned int headersize;        // header 长度
    unsigned char encoding;         // 编码方式
    unsigned char *p;               // 内容
} zlentry;

#define ZIPLIST_ENTRY_ZERO(zle) {                        \
    (zle)->prevrawlensize = (zle)->prevrawlen = 0;       \
    (zle)->lensize = (zle)->len = (zle)->headersize = 0; \
    (zle)->encoding = 0;                                 \
    (zle)->p = NULL;                                     \
}

/**
 * 从 ptr指针中取出编码类型，并将它保存到encoding
 * 复杂度O(1)
 */
#define ZIP_ENTRY_ENCODING(ptr, encoding) do {                  \
    (encoding) = (ptr[0]);                                      \
    if ((encoding) < ZIP_STR_MASK) (encoding) &= ZIP_STR_MASK;  \
} while(0)

static unsigned int zipIntSize(unsigned char encoding);

/**
 * 从 ptr 指针中取出节点的编码，保存节点长度所需的长度，以及节点长度
 * 复杂度: O(1)
 * 
 * 返回值
 *  int 编码节点所需的长度
 */
#define ZIP_DECODE_LENGTH(ptr, encoding, lensize, len) do {     \
    /* 取出节点的编码 */                                          \
    ZIP_ENTRY_ENCODING((ptr), (encoding));                      \
    if ((encoding) < ZIP_STR_MASK) {                            \
        /* 节点保存的是字符串，取出编码 */                           \
        if ((encoding) == ZIP_STR_06B) {                        \
            (lensize) = 1;                                      \
            (len) = (ptr)[0] & 0x3f;                            \
        } else if ((encoding) == ZIP_STR_14B) {                 \
            (lensize) = 2;                                      \
            (len) = (((ptr)[0] & 0x3f) << 8) | (ptr)[1];        \
        } else if ((encoding) == ZIP_STR_32B) {                 \
            (lensize) = 5;                                      \
            (len) = ((ptr)[1] << 24) |                          \
                    ((ptr)[2] << 16) |                          \
                    ((ptr)[3] << 8)  |                          \
                    ((ptr)[4]);                                 \
        } else {                                                \
            assert(NULL);                                       \
        }                                                       \
    } else {                                                    \
        /* 节点保存的是整数，取出编码 */                            \
        (lensize) = 1;                                          \
        (len) = zipIntSize(encoding);                           \
    }                                                           \
} while (0);

int zipStorePrevEntryLengthLarge(unsigned char *p, unsigned int len);

unsigned int zipStorePrevEntryLength(unsigned char *p, unsigned int len);

/**
 * 从指针 ptr 中取出保存前一个节点的长度所需的字节数
 * 复杂度: O(1)
 * 
 * 返回值:
 *  unsigned int
 */
#define ZIP_DECODE_PREVLENSIZE(ptr, prevlensize) do {          \
    if ((ptr)[0] < ZIP_BIG_PREVLEN) {                          \
        (prevlensize) = 1;                                     \
    } else {                                                   \
        (prevlensize) = 5;                                     \
    }                                                          \
} while (0);

/**
 * 从指针 ptr 中取出前一个节点的长度
 * 
 * 复杂度：O(1)
 * 
 * 返回值:
 *  unsigned int
 */
#define ZIP_DECODE_PREVLEN(ptr, prevlensize, prevlen) do {  \
    /* 取得保存前一个节点的长度所需的字节数 */                    \
    ZIP_DECODE_PREVLENSIZE(ptr, prevlensize);               \
    /* 获取长度值 */                                          \
    if ((prevlensize) == 1) {                               \
        (prevlen) = (ptr)[0];                               \
    } else if ((prevlensize) == 5) {                        \
        assert(sizeof((prevlen)) == 4);                     \
        memcpy(&(prevlen), ((char *)(ptr)) + 1, 4);         \
        memrev32ifbe(&prevlen);                             \
    }                                                       \
} while (0);

int zipPrevLenByteDiff(unsigned char *p, unsigned int len);

unsigned int zipRawEntryLength(unsigned char *p);

int zipTryEncoding(unsigned char *entry, unsigned int entrylen, long long *v, unsigned char *encoding);

void zipSaveInteger(unsigned char *p, int64_t value, unsigned char encoding);

int64_t zipLoadInteger(unsigned char *p, unsigned char encoding);

void zipEntry(unsigned char *p, zlentry *e);

unsigned char *ziplistNew(void);

unsigned char *ziplistResize(unsigned char *zl, unsigned int len);

unsigned char *__ziplistCascadeUpdate(unsigned char *zl, unsigned char *p);

unsigned char *__ziplistDelete(unsigned char *zl, unsigned char *p, unsigned int num);

unsigned char *__ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen);

unsigned char *ziplistMerge(unsigned char **first, unsigned char **second);

unsigned char *ziplistPush(unsigned char *zl, unsigned char *s, unsigned int slen, int where);

unsigned char *ziplistIndex(unsigned char *zl, int index);

unsigned char *ziplistNext(unsigned char *zl, unsigned char *p);

unsigned char *ziplistPrev(unsigned char *zl, unsigned char *p);

unsigned int ziplistGet(unsigned char *p, unsigned char **sval, unsigned int *slen, long long *lval);

unsigned char *ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen);

unsigned char *ziplistDelete(unsigned char *zl, unsigned char **p);

unsigned char *ziplistDeleteRange(unsigned char *zl, int index, unsigned int num);

unsigned int ziplistCompare(unsigned char *p, unsigned char *s, unsigned int slen);

unsigned char *ziplistFind(unsigned char *p, unsigned char *vstr, unsigned int vlen, unsigned int skip);

unsigned int ziplistLen(unsigned char *zl);

size_t ziplistBlobLen(unsigned char *zl);

void ziplistRepr(unsigned char *zl);

#endif