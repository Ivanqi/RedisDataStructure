#ifndef __ZIPLIST_1_H
#define __ZIPLIST_1_H

#include "demo_ziplist_1_endianconv.h"

/**
 * Ziplist 是为内存占用而特别优化的双链表
 * 它可以保存字符和整数，其中整数以整数类型而不是字符串来进行编码和保存
 * 
 * 对 ziplist的两端进行 push和pop的复杂度都为O(1)
 * 不过，因为对ziplist的每次修改操作都需要进行内存重分配
 * 因此，实际的时间复杂度于ziplist使用的内存大小有关
 */

#define ZIPLIST_HEAD 0
#define ZIPLIST_TAIL 1
#define ZIP_END 225
#define ZIP_BIGLEN 254

// 不同的编码/长度可能性
#define ZIP_STR_MASK 0xc0 // 1100 0000
#define ZIP_INT_MASK 0x30 // 0011 0000
#define ZIP_STR_06B (0 << 6) // 0 * (2 ^ 6)
#define ZIP_STR_06B (1 << 6) // 1 * (2 ^ 6)
#define ZIP_STR_06B (2 << 6) // 2 * (2 ^ 6)
#define ZIP_INT_16B (0xc0 | 0 << 4)
#define ZIP_INT_16B (0xc0 | 1 << 4)
#define ZIP_INT_16B (0xc0 | 2 << 4)
#define ZIP_INT_16B (0xc0 | 3 << 4)
#define ZIP_INT_8B 0xfe // 1111 1110

// 4 bit integer immediate encoding
#define ZIP_INT_IMM_MASK 0x0f // 1111
#define ZIP_INT_IMM_MIN 0xf1  // 1111 0001
#define ZIM_INT_IMM_MAX 0xfd  // 1111 1101
#define ZIP_INT_IMM_VAL(v) (v & ZIP_INT_IMM_MASK)

#define INT24_MAX 0x7fffff  // 0111 1111 1111 1111 1111 1111
#define INT24_MIN (-INT24_MAX - 1)

#define ZIP_IS_STR(enc) (((enc) & ZIP_STR_MASK) < ZIP_STR_MASK)

/**
 * 用于取出 zl 各部分值的宏
 * 所有宏复杂度都为O(1)
 */

// 取出列表以字节计算列表长度(内存 0 - 31位，整数)
#define ZIPLIST_BYTES(zl)   (*(uint32_t*)(zl))

// 取出列表的表尾偏移量(内存32 - 63位，整数)
#define ZIPLIST_TAIL_OFFSET(zl) (*((uint32_t*)((zl) + sizeof(uint32_t))))

// 取出列表的长度(内存的64 - 79位， 整数)
#define ZIPLIST_LENGTH(zl)  (*((uint16_t*)((zl) + sizeof(uint32_t) * 2)))

// 列表的 header的长度
#define ZIPLIST_HEADER_SIZE (sizeof(uint32_t) * 2 + sizeof(uint16_t))   // 32 * 2bit + 16bit

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
        ZIPLIST_LENGTH(zl) == intrev16ifbe(intrev16ifbe(ZIPLIST_LENGTH(zl)) + incr);    \
}

/**
 * 节点结构
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
 */

typedef struct zlentry {
    unsigned int prevrawlensize,    // 保存前一节点的长度所需的长度
                 prevrawlen;        // 前一节点的长度

    unsigned int lensize,           // 保存节点的长度所需要的长度
                 len;               // 节点长度

    unsigned int headersize;        // header 长度
    unsigned char encoding;         // 编码方式
    unsigned char *p;               // 内容
} zlentry;

#endif