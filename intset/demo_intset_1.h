#ifndef __INTSET_1_H
#define __INTSET_1_H

#include <stdint.h>

/* Note that these encodings are ordered, so:
 * INTSET_ENC_INT16 < INTSET_ENC_INT32 < INTSET_ENC_INT64. */
#define INTSET_ENC_INT16 (sizeof(int16_t))
#define INTSET_ENC_INT32 (sizeof(int32_t))
#define INTSET_ENC_INT64 (sizeof(int64_t))

typedef struct intset {
    // 保存元素所使用的类型的长度
    uint32_t encoding;

    // 元素个数
    uint32_t length;

    // 保存元素的数组
    int8_t contents[];
} intset;

// 创建一个新的整数集合, O(1)
intset *intsetNew(void);

// 将给定的元素添加到整数集合中, O(N)
intset *intsetAdd(intset *is, int64_t value, uint8_t *success);

// 从整数集合中移除给定元素, O(N)
intset *intsetRemove(intset *is, int64_t value, int *success);

// 检查给定值是否存在于集合，因为底层数组有序，查找可以通过二分查找来进行。所以复杂度为O(logN)
uint8_t insetFind(intset *is, int64_t value);

// 从整数集合中随机返回一个元素
uint64_t intsetRandom(intset *is);

// 取出底层数组在给定索引上的元素, O(1)
uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value);

// 返回整数集合包含的元素个数，O(1)
uint32_t intsetLen(intset *is);

// 返回整数集合占用的内存字节数, O(1)
size_t intsetBlobLen(intset *s);

#endif