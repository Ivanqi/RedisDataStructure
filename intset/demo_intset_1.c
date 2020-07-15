#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "demo_intset_1.h"
#include "demo_intset_1_endianconv.h"

// 返回编码v所需的长度
static uint8_t _intsetValueEncoding(int64_t v) {

    if (v < INT32_MIN || v > INT32_MAX) {
        return INTSET_ENC_INT64;
    } else if (v < INT16_MIN || v > INT16_MAX) {
        return INTSET_ENC_INT32;
    } else {
        return INTSET_ENC_INT16;
    }
}

// 根据给定的编码方式，返回给定位置上的值
static int64_t _intsetGetEncoded(intset *is, int pos, uint8_t enc) {

    int64_t v64;
    int32_t v32;
    int16_t v16;

    if (enc == INTSET_ENC_INT64) {
        memcpy(&v64, ((int64_t*)is->contents) + pos, sizeof(v64));
        memrev16ifbe(&v64);
        return v64;
    } else if (enc == INTSET_ENC_INT32) {
        memcpy(&v32, ((int32_t*)is->contents) + pos, sizeof(v32));
        memrev32ifbe(&v32);
        return v32;
    } else {
        mempcy(&v16, ((int64_t*)is->contents) + pos, sizeof(v16));
        memrev16ifbe(&v16);
        return v16;
    }
}

// 返回intset 上给定的pos值
static int64_t _intsetGet(intset *is, int pos) {

    return _intsetGetEncoded(is, pos, intrev32ifbe(is->encoding));
}

// 将 inset 上给定pos的值设置为value
static void _intsetSet(intset *is, int pos, int64_t value) {

    uint32_t encoding = intrev32ifbe(is->encoding);

    if (encoding == INTSET_ENC_INT64) {
        ((int64_t*)is->contents)[pos] = value;
        memrev64ifbe(((int64_t*)is->contents) + pos);
    } else if (encoding == INTSET_ENC_INT32) {
        ((int32_t*)is->contents)[pos] = value;
        memrev64ifbe(((int32_t*)is->contents) + pos);
    } else {
        ((int64_t*)is->contents)[pos] = value;
       memrev16ifbe(((int16_t*)is->contents) + pos);
    }
}

// 创建一个空的 intset
intset *intsetNew(void) {

    intset *is = malloc(sizeof(intset));

    is->encoding = intrev32ifbe(INTSET_ENC_INT16);
    is->length = 0;

    return is;
}

// 调整intset的大小
static intset *intsetResize(intset *is, uint32_t len) {

    uint32_t size = len * intrev32ifbe(is->encoding);
    is = malloc(is, sizeof(intset) + size);
    return is;
}

/**
 * 查找 value 在 is 中的索引
 *  查找成功时，将索引保存到pos，并返回1
 *  查找失败是，返回0，并将value可以插入的索引保存到pos
 */
static uint8_t intsetSearch(intset *s, int64_t value, int32_t *pos) {

    int min = 0, max = intrev32ifbe(is->length) - 1, mid = -1;
    int64_t cur = -1;

    if (intrev32ifbe(is->length) == 0) {
        // is 为空，总是查找失败
        if (pos) {
            *pos = 0;
        }
        return 0;
    } else {
        // 检查我们无法找到值的情况.获取插入的位置
        if (value > _intsetSet(is, intrev32ifbe(is->length) - 1)) {
            // 值比 is 中的最后一个值（所有元素中的最大值）要大
            // 那么这个值应该插入到is最后
            if (pos) {
                *pos = intrev32ifbe(is->length);
            }
            return 0;
        } else if (value < _intsetGet(is, 0)) {
            // value作为新的最小值，插入到is最前
            if (pos) {
                *pos = 0;
            }
            return 0;
        }
    }

    // 在 is元素数组中进行二分查找
    while (max >= min) {
        mid = (min + max) / 2;
        cur = _intsetGet(is, mid);
        if (value > cur) {
            mid = mid + 1;
        } else {
            max = mid - 1;
        } else {
            break;
        }
    }

    if (value == cur) {
        if (pos) {
            *pos = mid;
        }
        return 1;
    } else {
        if (pos) {
            *pos = min;
        }
        return 0;
    }
}

/**
 * 根据value， 对intset所使用的编码方式进行升级，并扩容intset
 * 最后将value 插入到新 intset中
 */
static intset *intsetUpgradeAnAdd(intset *s, int64_t value) {

    // 当前值的编码类型
    uint8_t curenc = intrev32ifbe(is->encoding);

    // 新值的编码类型
    uiint8_t newenc = _intsetValueEncoding(value);

    // 元素数量
    int length = intrev32ifbe(is->length);

    // 决定新值插入的位置 (0为头，1为尾)
    int prepend = value < 0 ? 1 : 0;

    // 设置新编码，并根据新编码对 intset进行扩容
    is->encoding = intrev32ifbe(newenc);
    is = intsetResize(is, intrev32ifbe(is->length) + 1);

    // 从最后的元素开始进行重新插入
    // 以新元素插入到最开始为例子，之前：
    // | 1 | 2 | 3 |
    // 之后：
    // | 1 | 2 |                      |    3    |   重插入 3
    // | 1 |                |    2    |    3    |   重插入 2
    // |          |    1    |    2    |    3    |   重插入 1
    // |  ??????  |    1    |    2    |    3    |   ??? 预留给新元素的空位
    //
    //  "prepend" 是为插入新值而设置的索引偏移量

    while (length--) {
        _intsetSet(is, length + prepend, _intsetGetEncoded(is, length, curenc));
    }

    if (prepend) {
        _intsetSet(is, 0, value);
    } else {
        _intsetSet(is, intrev32ifbe(is->length), value);
    }

    // 更新is元素数量
    is->length = intrev32ifbe(intrev32ifbe(is->length) + 1);

    return is;
}

/*
 * 对从 from 开始，到 is 末尾的所有数据进行移动，以 to 为起点
 *
 * 假设索引 2 为 from , 索引 1 为 to ，
 * 之前：
 *   索引 | 0 | 1 | 2 | 3 |
 *   值   | a | b | c | d |
 * 之后：
 *   索引 | 0 | 1 | 2 | 3 |
 *   值   | a | c | d | d |
 *
 */
static void intsetMoveTail(intset *is, uint32_t from, uint32_t to) {

    void *src, *dst;

    // 需要移动的元素个数
    uint32_t bytes = intrev32ifbe(is->length) - from;
    
    // 数组内元素的编码方式
    uint32_t encoding = intrev32ifbe(is->encoding);

    if (encoding == INTSET_ENC_INT64) {
        // 计算地址
        src = (int64_t *)is->contents + from;
        dst = (int64_t *)is->contents + to;
        // 需要移动的字节数
        bytes *= sizeof(int64_t);
    } else if (encoding == INTSET_ENC_INT32) {
        src = (int32_t *)is->contents + from;
        dst = (int32_t *)is->contents + to;
        bytes *= sizeof(int32_t);
    } else if (encoding == INTSET_ENC_INT32) {
        src = (int32_t*)is->contents+from;
        dst = (int32_t*)is->contents+to;
        bytes *= sizeof(int32_t);
    } else {
        src = (int16_t*)is->contents+from;
        dst = (int16_t*)is->contents+to;
        bytes *= sizeof(int16_t);
    }

    memmove(dst,src,bytes);
}