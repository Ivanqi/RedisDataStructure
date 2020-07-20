#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "demo_ziplist_1.h"
#include "demo_ziplist_1_util.h"

/**
 * 返回encoding 指定的整数编码方式所需的长度
 * 复杂度：O(1)
 */
static unsigned int zipIntSize(unsigned char encoding) {

    switch (encoding) {
        case ZIP_INT_8B:    return 1;
        case ZIP_INT_16B:   return 2;
        case ZIP_INT_24B:   return 3;
        case ZIP_INT_32B:   return 4;
        case ZIP_INT_64B:   return 8;
        default: return 0;  // 4 bit immediate
    }
    assert(NULL);
    return 0;
}

/**
 * 编码长度l，并将它写入到p
 * 
 * 如果p为NULL，那么返回编码 rawlen 所需的字节数
 * 
 * 复杂度: O(1)
 */
static unsigned int zipEncodeLength(unsigned char *p, unsigned char encoding, unsigned int rawlen) {

    unsigned char len = 1, buf[5];

    if (ZIP_IS_STR(encoding)) {
        // 字符串编码
        if (rawlen <= 0x3f) {
            // 长度6bit，长度 + 编码 8bit(1 byte)
            if (!p) return len;
            buf[0] = ZIP_STR_06B | rawlen;
        } else if (rawlen <= 0x3fff) {
            // 长度14bit，长度 + 编码 16bit(2 bytes)
            len += 1;
            if (!p) return len;
            buf[0] = ZIP_STR_14B | ((rawlen >> 8) & 0x3f);
            buf[1] = rawlen & 0xff;
        } else {
            // 长度 32 bit(4 bytes), 长度 + 编码 40bit（5bytes）
            len += 4;
            if (!p) return len;
            buf[0] = ZIP_STR_32B;
            buf[1] = (rawlen >> 24) & 0xff;
            buf[2] = (rawlen >> 16) & 0xff;
            buf[3] = (rawlen >> 8)  & 0xff;
            buf[4] = rawlen & 0xff;
        }
    } else {
        // 编码为整数，长度总为1bytes
        if (!p) return len;
        buf[0] = encoding;
    }

    memcpy(p, buf, len);
    return len;
}

/**
 * 编码前置节点的长度，并将它写入p
 * 如果p为NULL，那么返回编码len所需的字节数
 * 
 * 复杂度:O(1)
 */
static unsigned int zipPrevEncodingLength(unsigned char *p, unsigned int len) {

    if (p == NULL) {
        return (len < ZIP_BIGLEN) ? 1 : sizeof(len) + 1;
    } else {
        if (len < ZIP_BIGLEN) {
            p[0] = len;
            return 1;
        } else {
            p[0] = ZIP_BIGLEN;
            memcpy(p + 1, &len, sizeof(len));
            memrev32ifbe(p + 1);
            return 1 + sizeof(len);
        }
    }
}

/**
 * 前驱节点的长度 len 写入到 p 中
 * 其中p的空间比保存len所需的实际空间要更大
 * 
 * 这适用于将一个比原节点更短的新节点插入到某个节点时使用
 * 具体参考 __ziplistCascadeUpdate 函数的头注释
 * 
 * 复杂度：O(1)
 */
static void zipPrevEncodeLengthForceLarge(unsigned char *p, unsigned int len) {

    if (p == NULL) return ;
    p[0] = ZIP_BIGLEN;
    memcpy(p + 1, &len, sizeof(len));
    memrev32ifbe(p + 1);
}


/**
 * 返回编码 len 所需的长度减去编码 p 的前一个节点的大小所需的长度之差
 * 复杂度：O(1)
 */
static int zipPrevLenByteDiff(unsigned char *p, unsigned int len) {

    // 获取编码前一节点所需的长度
    unsigned int prevlensize;
    ZIP_DECODE_PREVLENSIZE(p, prevlensize);
    // 计算差
    return zipPrevEncodingLength(NULL, len) - prevlensize;
}

/**
 * 返回 p 指向的节点的空间总长度
 * 复杂度：O(1)
 */
static unsigned int zipRawEntryLength(unsigned char *p) {

    unsigned int prevlensize, encoding, lensize, len;

    // 保存前驱节点长度的空间长度
    ZIP_DECODE_PREVLENSIZE(p, prevlensize);

    // 保存本节点的空间长度
    ZIP_DECODE_LENGTH(p + prevlensize, encoding, lensize, len);

    return prevlensize + lensize + len;
}

/**
 * 检查 entry 所保存的值，看它是否编码为整数
 * 复杂度；O(N), N为 entry 所保存字符串值的长度
 * 
 * 返回值
 *  如果可以的话，返回1，并将值保存在v，将编码保存在encoding
 *  否则，返回0
 */
static int zipTryEncoding(unsigned char *entry, unsigned int entrylen, long long *v, unsigned char *encoding) {

    long long value;

    if (entrylen >= 32 || entrylen == 0) return 0;

    // 尝试转换为整数
    if (string2ll((char *)entry, entrylen, &value)) {
        // 选择编码
        if (value >= 0 && value <= 12) {
            *encoding = ZIP_INT_IMM_MIN + value;
        } else if (value >= INT8_MIN && value <= INT8_MAX) {
            *encoding = ZIP_INT_8B;
        } else if (value >= INT16_MIN && value <= INT16_MAX) {
            *encoding = ZIP_INT_16B;
        } else if (value >= INT24_MIN && value <= INT24_MAX) {
            *encoding = ZIP_INT_24B;
        } else if (value >= INT32_MIN && value <= INT32_MAX) {
            *encoding = ZIP_INT_32B;
        } else {
            *encoding = ZIP_INT_64B;
        }

        *v = value;
        return 1;
    }
    return 0;
}

/**
 * 将 value 保存 到 p，并设置编码为 encoding
 * 复杂度:O(1)
 */
static void zipSaveInteger(unsigned char *p, int64_t value, unsigned char encoding) {

    int16_t i16;
    int32_t i32;
    int64_t i64;

    // 8bit整数
    if (encoding == ZIP_INT_8B) {
        ((int8_t *)p)[0] = (int8_t)value;
    } else if (encoding == ZIP_INT_16B) {   // 16 bit 整数
        i16 = value;
        memcpy(p, &i16, sizeof(i16));
        memrev16ifbe(p);
    } else if (encoding == ZIP_INT_24B) {   // 24 bit 整数
        i32  = value << 8;
        memrev32ifbe(&i32);
        memcpy(p, ((uint8_t*)&i32) + 1, sizeof(i32) - sizeof(uint8_t));
    } else if (encoding == ZIP_INT_32B) {   // 32 bit 整数
        i32 = value;
        memcpy(p, &i32, sizeof(i32));
        memrev32ifbe(p);
    } else if (encoding == ZIP_INT_64B) {   // 64 bit 整数
        i64 = value;
        memcpy(p, &i64, sizeof(i64));
        memrev64ifbe(p);
    } else if (encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MAX) {    // 值和编码保存同一个 byte
        // Nothing to do
    } else {
        assert(NULL);
    }
}

/**
 * 根据 encoding ，从指针p中取出整数值
 * 复杂度: O(1)
 */
static int64_t zipLoadInteger(unsigned char *p, unsigned char encoding) {

    int16_t i16;
    int32_t i32;
    int64_t i64, ret = 0;

    // 8 bit
    if (encoding == ZIP_INT_8B) {
        ret = ((int8_t *)p)[0];
    } else if (encoding == ZIP_INT_16B) {   // 16 bit
        memcpy(&i16, p, sizeof(i16));
        memrev16ifbe(&16);
        ret = i16;
    } else if (encoding == ZIP_INT_32B) {   // 32 bit
        memcpy(&i32, p, sizeof(i32));
        memrev32ifbe(&i32);
        ret = i32;
    } else if (encoding == ZIP_INT_24B) {   // 24 bit
        i32 = 0;
        memcpy(((uint8_t *)&i32) +  1, p, sizeof(i32) - sizeof(uint8_t));
        memrev32ifbe(&i32);
        ret = i32 >> 8;
    } else if (encoding == ZIP_INT_64B) {   // 64bit
        memcpy(&i64, p, sizeof(i64));
        memrev64ifbe(&64);
    } else if (encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MASK) {
        ret = (encoding & ZIP_INT_IMM_MASK) - 1;
    } else {
        assert(NULL);
    }

    return ret;
}

/**
 * 从指针p 中提取出节点的各个属性，并将属性保存到 zlentry结构，然后返回
 * 复杂度: O(1)
 */
static zlentry zipEntry(unsigned char *p) {

    zlentry e;

    // 取出前一个节点的长度
    ZIP_DECODE_PREVLEN(p, e.prevrawlensize, e.prevrawlen);
    
    // 取出当前节点的编码，保存节点的长度所需的长度，以及节点的长度
    ZIP_DECODE_LENGTH(p + e.prevrawlensize, e.encoding, e.lensize, e.len);

    // 记录 header 的长度
    e.headersize = e.prevrawlensize + e.lensize;

    // 记录指针 p
    e.p = p;

    return e;
}

/**
 * 新创建一个空 ziplist
 * 复杂度: O(1)
 * 返回值：新创建的 ziplist
 */
unsigned char *ziplistNew(void) {

    /**
     * 分配2个32bit， 一个16bit，以及一个8bit
     * 分别用于 <zlbytes><zltail><zllen> 和 <zlend>
     */
    unsigned int bytes = ZIPLIST_HEADER_SIZE + 1;
    unsigned char *zl = (unsigned char *)malloc(bytes);

    // 设置长度
    ZIPLIST_BYTES(zl) = intrev32ifbe(bytes);

    // 设置表尾偏移量
    ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe(ZIPLIST_HEADER_SIZE);

    // 设置列表项数量
    ZIPLIST_LENGTH(zl) = 0;

    // 设置表尾标识
    zl[bytes - 1] = ZIP_END;

    return zl;
}

/**
 * 对zl 进行空间重分配，并更新相关属性
 * 复杂度: O(N)
 * 返回值：更新后的 ziplist
 */
static unsigned char *ziplistResize(unsigned char *zl, unsigned int len) {

    // 重分配
    zl = (unsigned char *)realloc(zl, len);
    // 更新长度
    ZIPLIST_BYTES(zl) = intrev32ifbe(len);
    // 设置表尾
    zl[len - 1] = ZIP_END;

    return zl;
}

/**
 * 编码前置节点的长度，并将它写入 p
 * 
 * 如果p为NULL，那么返回编码len所需的字节数
 * 
 * 复杂度:O(1)
 */
static unsigned int zipPrevEncodeLength(unsigned char *p, unsigned int len) {

    if (p == NULL) {
        return (len < ZIP_BIGLEN) ? 1 : sizeof(len) + 1;
    } else {
        if (len < ZIP_BIGLEN) {
            p[0] = len;
            return 1;
        } else {
            p[0] = ZIP_BIGLEN;
            memcpy(p + 1, &len, sizeof(len));
            memrev32ifbe(p + 1);
            return 1 + sizeof(len);
        }
    }
}


/**
 * 当将一个新节点添加到某个节点之前的时候，
 * 如果原节点的 prevlen 不足以保存新节点的长度
 * 那么就需要对原节点的空间进行扩展（从1字节扩展到5字节）
 * 
 * 但是，当对原节点进行扩展之后，原节点的下一个节点的prevlen 可能出现空间不足，
 * 这种情况在多个连续节点的长度都接近 ZIP_BIGLEN 是可能发生
 * 
 * 这个函数就用于处理这种连续扩展动作
 * 
 * 因此节点的长度变小而引起的连续缩小也是可能出现的
 * 不够，为了避免扩展 - 缩小 - 扩展 - 缩小这种的情况反复出现(flapping, 抖动)
 * 我们不处理这种情况，而是由prevlen比所需的长度更长
 * 
 * 复杂度：O(N ^ 2)
 * 返回值：更新后的 ziplist
 */

static unsigned char *__ziplistCascadeUpdate(unsigned char *zl, unsigned char *p) {

    size_t curlen = intrev32ifbe(ZIPLIST_BYTES(zl)), rawlen, rawlensize;
    size_t offset, noffset, extra;
    unsigned char *np;
    zlentry cur, next;

    // 一直更新直到表尾
    while (p[0] != ZIP_END) {
        // 当前节点
        cur = zipEntry(p);
        // 当前节点的长度
        rawlen = cur.headersize + cur.len;

        // 编码当前节点的长度所需的空间大小
        rawlensize = zipPrevEncodeLength(NULL, rawlen);

        // 已经到达表尾，退出
        if (p[rawlen] == ZIP_END) break;

        // 取出下一个节点
        next = zipEntry(p + rawlen);

        /**
         * 如果下一的 prevlen 等于 当前节点的 rawlen
         * 那么说明编码大小无需改变，退出
         */
        if (next.prevrawlen == rawlen) break;

        // 下一节点的长度编码空间不足，进行扩展
        if (next.prevrawlensize < rawlensize) {
            offset = p - zl;
            // 需要多添加的长度
            extra = rawlensize - next.prevrawlensize;
            // 重分配，复杂度为 O(N)
            zl = ziplistResize(zl, curlen + extra);
            p = zl + offset;

            np = p + rawlen;
            noffset = np - zl;

            if ((zl + intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl))) != np) {
                ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl)) + extra);
            }

            // 为获得空间而进行数据移动，复杂度O(N)
            memmove(np + rawlensize, np + next.prevrawlensize, curlen - noffset - next.prevrawlensize - 1);
            zipPrevEncodingLength(np, rawlen);

            p += rawlen;
            curlen += extra;
        } else {
            /**
             * 下一个节点的长度编码空间有多余，不进行收缩
             * 只是将被编码的长度写入空间
             */
            if (next.prevrawlensize > rawlensize) {
                zipPrevEncodeLengthForceLarge(p + rawlen, rawlen);
            } else {
                zipPrevEncodeLengthForceLarge(p + rawlen, rawlen);
            }

            break;
        }
    }

    return zl;
}

/**
 * 从指针 p 开始， 删除 num个节点
 * 复杂度：O(N ^ 2)
 * 返回值：删除元素后的 ziplist
 */
static unsigned char *__ziplistDelete(unsigned char *zl, unsigned char *p, unsigned int num) {

   unsigned int i, totlen, deleted = 0;
    size_t offset;
    int nextdiff = 0;
    zlentry first, tail;

    // 首个节点
    first = zipEntry(p);
    /**
     * 累积起所有删除目标（节点）的编码长度
     * 并移动指针p
     * 复杂度 O(N)
     */
    
    totlen = p - first.p;
    if (totlen > 0) {
        /**
         * 更新最后一个被删除的节点之后的一个节点
         * 将它的prevlen值设置为 first.prevrawlen
         * 也即是被删除的第一个节点的前一个节点的长度
         */
        nextdiff = zipPrevLenByteDiff(p, first.prevrawlen);
        p -= nextdiff;
        zipPrevEncodingLength(p, first.prevrawlen);

        // 更新ziplist到表尾的偏移量
        ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl)) - totlen);

        // 跟新 ziplist 的偏移量，如果有需要的话，算上nextdiff
        tail = zipEntry(p);
        if (p[tail.headersize + tail.len] != ZIP_END) {
            ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl)) + nextdiff);
        }

        /**
         * 前移内存中的数据，覆盖原本的被删除数据
         * 复杂度 O(N)
         */
        memmove(first.p, p, intrev32ifbe(ZIPLIST_BYTES(zl)) - (p -zl) - 1);
    } else {
        // 被删除的是尾节点，无须内存移动，直接更新偏移值就可以了
        ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe((first.p - zl) - first.prevrawlen);
    }

    /**
     * 调整大小，并更新 ziplist 的长度
     * 复杂度 O(N)
     */
    offset = first.p - zl;
    zl = ziplistResize(zl, intrev32ifbe(ZIPLIST_BYTES(zl)) - totlen+nextdiff);
    ZIPLIST_INCR_LENGTH(zl, -deleted);
    p = zl + offset;

    // 层级更新
    if (nextdiff != 0) {
        // 复杂度O(N ^ 2)
        zl = __ziplistCascadeUpdate(zl, p);
    }

    return zl;
}

/**
 * 添加保存给定元素 s 的新节点到地址 p
 * 
 * 复杂度：O(N ^ 2)
 * 返回值：删除元素后的 ziplist
 */
static unsigned char *__ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen) {

    size_t curlen = intrev32ifbe(ZIPLIST_BYTES(zl)), reqlen, prevlen = 0;
    size_t offset;
    int nextdiff = 0;
    unsigned char encoding = 0;
    long long value = 123456789;

    zlentry entry, tail;

    /**
     * 如果p之后不是没有节点（不是插入到末端）
     * 那么取出节点相关资料，以及prevlen
     */
    if (p[0] != ZIP_END) {
        entry = zipEntry(p);
        prevlen = entry.prevrawlen;
    } else {
        // 获取列表最后一个节点 （表尾）的地址
        unsigned char *ptail = ZIPLIST_ENTRY_TAIL(zl);
        // 如果地址之后不是末端（也即是，列表至少有一个节点）
        if (ptail[0] != ZIP_END) {
            // 保存ptail 指向的节点的空间长度
            prevlen = zipRawEntryLength(ptail);
        }
    }

    /**
     * 查看是否能将新值保存为整数
     * 如果可以的话返回1
     * 并将新值保存到value，编码形式保存到encoding
     */
    if (zipTryEncoding(s, slen, &value, &encoding)) {
        // s 可以保存为整数，那么继续计算保存它所需的空间
        reqlen = zipIntSize(encoding);
    } else {
        // 不能保存为整数，直接使用字符串长度
        reqlen = slen;
    }

    // 计算编码 prevlen 所需的长度
    reqlen += zipPrevEncodingLength(NULL, prevlen);

    // 计算编码 slen 所需的长度
    reqlen += zipEncodeLength(NULL, encoding, slen);

    /**
     * 如果添加的位置不是表尾，那么必须确定后继节点的 prevlen 空间
     * 足以保存新节点的编码长度
     * zipPrevLenByteDiff
     *  1) 新旧两个节点的编码长度相等，返回 0
     *  2) 新节点编码长度 > 旧节点编码长度，返回 5 - 1 = 4
     *  3) 旧节点编码长度 > 新编码节点长度，返回 1 - 5 = -4
     */
    nextdiff = (p[0] != ZIP_END) ? zipPrevLenByteDiff(p, reqlen) : 0;

    // 保存偏移量，因为重分配空间有可能改变 zl 的内存地址
    offset = p - zl;

    /**
     * 重分配空间，并更新长度属性和表尾
     * 新空间长度 = 现有长度 - 新节点所需长度 + 编码新节点长度所需要的长度差
     * O(N)
     */
    zl = ziplistResize(zl, curlen + reqlen + nextdiff);

    // 更新 p 的指针
    p = zl + offset;

    /**
     * 如果新节点不是添加到列表末端，那么它后面就有其他节点
     * 因此，我们需要移动这部分节点
     */
    if (p[0] != ZIP_END) {
        /**
         * 向右移动原有数据，为新节点让出空间
         * O(N)
         */
        memmove(p + reqlen, p - nextdiff, curlen - offset - 1 + nextdiff);

        // 将本节点的长度编码至下一节点
        zipPrevEncodeLength(p + reqlen, reqlen);

        // 更新ziplist的表尾偏移量
        ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl)) + reqlen);

        // 有需要的话，将nextdiff 也加上到 zltail上
        tail = zipEntry(p + reqlen);
        if (p[reqlen + tail.headersize + tail.len] != ZIP_END) {
            ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl))+nextdiff);
        }
    } else {
        // 更新 ziplist 的 zltail 属性，现在新添加节点为表尾节点
        ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe(p-zl);
    }

    if (nextdiff != 0) {
        offset = p - zl;
        // O(N^2)
        zl = __ziplistCascadeUpdate(zl, p + reqlen);
        p = zl + offset;
    }

    // 写入数据到节点

    // 编码上一节点的长度，并向后移动指针
    p += zipPrevEncodeLength(p, prevlen);

    // 编码本节点的长度和类型，并向后移动指针
    p += zipEncodeLength(p, encoding, slen);

    // 写入内容到节点
    if (ZIP_IS_STR(encoding)) {
        memcpy(p,s,slen);
    } else {
        zipSaveInteger(p,value,encoding);
    }

    // 更新节点数量
    ZIPLIST_INCR_LENGTH(zl, 1);

    return zl;
}

/*
 * 将新元素插入为列表的表头节点或者表尾节点
 *
 * 复杂度：O(N^2)
 *
 * 返回值：添加操作完成后的 ziplist
 */

unsigned char *ziplistPush(unsigned char *zl, unsigned char *s, unsigned int slen, int where) {
    unsigned char *p;
    p = (where == ZIPLIST_HEAD) ? ZIPLIST_ENTRY_HEAD(zl) : ZIPLIST_ENTRY_END(zl);
    return __ziplistInsert(zl, p, s, slen);
}

/*
 * 返回指向当前迭代节点的指针，用于 ziplistNext 进行迭代
 * 当偏移值是负数时，表示迭代是从表尾到表头进行的。
 * 当元素被遍历完时，返回 NULL
 *
 * 复杂度：O(1)
 *
 * 返回值：指向节点的指针
 */

unsigned char *ziplistIndex(unsigned char *zl, int index) {

    unsigned char *p;

    zlentry entry;

    // 向前遍历
    if (index < 0) {
        index = (-index) - 1;
        p = ZIPLIST_ENTRY_TAIL(zl);
        // 如果 ziplist 不为空。。。
        if (p[0] != ZIP_END) {
            // 那么根据 entry.prevrawlen ，进行向前迭代
            entry = zipEntry(p);
            while (entry.prevrawlen > 0 && index--) {
                // 后退地址
                p -= entry.prevrawlen;
                entry = zipEntry(p);
            }
        }
    // 向后遍历
    } else {
        p = ZIPLIST_ENTRY_HEAD(zl);
        // 根据 entry.prevrawlen 向后进行迭代
        while (p[0] != ZIP_END && index--) {
            p += zipRawEntryLength(p);
        }
    }

    return (p[0] == ZIP_END || index > 0) ? NULL : p;
}

/*
 * 返回指向 p 的下一个节点的指针，
 * 如果 p 已经到达表尾，那么返回 NULL 。
 *
 * 复杂度：O(1)
 */
unsigned char *ziplistNext(unsigned char *zl, unsigned char *p) {

    ((void) zl);

    /* "p" could be equal to ZIP_END, caused by ziplistDelete,
     * and we should return NULL. Otherwise, we should return NULL
     * when the *next* element is ZIP_END (there is no next entry). */
    if (p[0] == ZIP_END) {
        return NULL;
    }

    // 指向下一节点，O(1)
    p += zipRawEntryLength(p);
    if (p[0] == ZIP_END) {
        return NULL;
    }

    return p;
}


/*
 * 返回 p 的前一个节点
 *
 * 复杂度：O(1)
 */
unsigned char *ziplistPrev(unsigned char *zl, unsigned char *p) {

    zlentry entry;

    /* Iterating backwards from ZIP_END should return the tail. When "p" is
     * equal to the first element of the list, we're already at the head,
     * and should return NULL. */
    // 这是表尾
    if (p[0] == ZIP_END) {
        p = ZIPLIST_ENTRY_TAIL(zl);
        return (p[0] == ZIP_END) ? NULL : p;
    // 到达表头，停止
    } else if (p == ZIPLIST_ENTRY_HEAD(zl)) {
        return NULL;
    } else {
        entry = zipEntry(p);
        assert(entry.prevrawlen > 0);
        return p - entry.prevrawlen;
    }
}

/*
 * 获取 p 所指向的节点，并将相关属性保存至指针
 *
 * 如果节点保存的是字符串值，那么将 sstr 指针指向它，
 * slen 设在为字符串的长度。
 *
 * 如果节点保存的是数字值，那么用 sval 保存它。
 *
 * p 为表尾时返回 0 ，否则返回 1 。
 *
 * 复杂度：O(1)
 */
unsigned int ziplistGet(unsigned char *p, unsigned char **sstr, unsigned int *slen, long long *sval) {

    zlentry entry;
    // 表尾
    if (p == NULL || p[0] == ZIP_END) return 0;
    if (sstr) *sstr = NULL;

    // 获取节点
    entry = zipEntry(p);
    // 字符串
    if (ZIP_IS_STR(entry.encoding)) {
        if (sstr) {
            *slen = entry.len;
            *sstr = p + entry.headersize;
        }
    // 数字值
    } else {
        if (sval) {
            *sval = zipLoadInteger(p+entry.headersize,entry.encoding);
        }
    }
    return 1;
}


/*
 * 将节点插入到 p 之后
 *
 * 复杂度：O(N^2)
 *
 * 返回值：添加完成后的 ziplist
 */

unsigned char *ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen) {

    return __ziplistInsert(zl,p,s,slen);
}

/*
 * 删除 p 所指向的节点，
 * 并原地更新 p 指针，让它指向被删除节点的后一个节点，
 * 使得可以迭代地进行删除
 * 
 * 复杂度：O(N^2)
 * 
 * 返回值：删除完成后的 ziplist
 */
unsigned char *ziplistDelete(unsigned char *zl, unsigned char **p) {

    size_t offset = *p - zl;
    zl = __ziplistDelete(zl, *p, 1);

    /* Store pointer to current element in p, because ziplistDelete will
     * do a realloc which might result in a different "zl"-pointer.
     * When the delete direction is back to front, we might delete the last
     * entry and end up with "p" pointing to ZIP_END, so check this. */
    *p = zl + offset;
    return zl;
}

/*
 * 定位到索引 index ，并删除这之后的 num 个元素
 *
 * 复杂度：O(N^2)
 * 
 * 返回值：删除完成后的 ziplist
 */
unsigned char *ziplistDeleteRange(unsigned char *zl, unsigned int index, unsigned int num) {

    unsigned char *p = ziplistIndex(zl,index);
    return (p == NULL) ? zl : __ziplistDelete(zl, p, num);
}

/*
 * 将 p 所指向的节点的属性和 sstr 以及 slen 进行对比，
 * 如果相等则返回 1 。
 *
 * 复杂度：O(N)
 */
unsigned int ziplistCompare(unsigned char *p, unsigned char *sstr, unsigned int slen) {
    zlentry entry;
    unsigned char sencoding;
    long long zval, sval;

    // p 是表尾？
    if (p[0] == ZIP_END) return 0;

    // 获取节点属性
    entry = zipEntry(p);
    // 对比字符串
    if (ZIP_IS_STR(entry.encoding)) {
        /* Raw compare */
        if (entry.len == slen) {
            // O(N)
            return memcmp(p+entry.headersize,sstr,slen) == 0;
        } else {
            return 0;
        }
    // 对比整数
    } else {
        /* Try to compare encoded values. Don't compare encoding because
         * different implementations may encoded integers differently. */
        if (zipTryEncoding(sstr,slen,&sval,&sencoding)) {
          zval = zipLoadInteger(p+entry.headersize,entry.encoding);
          return zval == sval;
        }
    }

    return 0;
}

/*
 * 根据给定的 vstr 和 vlen ，查找和属性和它们相等的节点
 * 在每次比对之间，跳过 skip 个节点。
 *
 * 复杂度：O(N)
 * 返回值：
 *  查找失败返回 NULL 。
 *  查找成功返回指向目标节点的指针
 */
unsigned char *ziplistFind(unsigned char *p, unsigned char *vstr, unsigned int vlen, unsigned int skip) {

    int skipcnt = 0;
    unsigned char vencoding = 0;
    long long vll = 0;

    // 遍历整个列表
    while (p[0] != ZIP_END) {
        unsigned int prevlensize, encoding, lensize, len;
        unsigned char *q;

        // 编码前一个节点的长度所需的空间
        ZIP_DECODE_PREVLENSIZE(p, prevlensize);
        // 当前节点的长度
        ZIP_DECODE_LENGTH(p + prevlensize, encoding, lensize, len);
        // 保存下一个节点的地址
        q = p + prevlensize + lensize;

        if (skipcnt == 0) {
            /* Compare current entry with specified entry */
            // 对比字符串
            if (ZIP_IS_STR(encoding)) {
                if (len == vlen && memcmp(q, vstr, vlen) == 0) {
                    return p;
                }
            // 对比整数
            } else {
                /* Find out if the searched field can be encoded. Note that
                 * we do it only the first time, once done vencoding is set
                 * to non-zero and vll is set to the integer value. */
                // 对传入值进行 decode
                if (vencoding == 0) {
                    if (!zipTryEncoding(vstr, vlen, &vll, &vencoding)) {
                        /* If the entry can't be encoded we set it to
                         * UCHAR_MAX so that we don't retry again the next
                         * time. */
                        vencoding = UCHAR_MAX;
                    }
                    /* Must be non-zero by now */
                    assert(vencoding);
                }

                /* Compare current entry with specified entry, do it only
                 * if vencoding != UCHAR_MAX because if there is no encoding
                 * possible for the field it can't be a valid integer. */
                if (vencoding != UCHAR_MAX) {
                    // 对比
                    long long ll = zipLoadInteger(q, encoding);
                    if (ll == vll) {
                        return p;
                    }
                }
            }

            /* Reset skip count */
            skipcnt = skip;
        } else {
            /* Skip entry */
            skipcnt--;
        }

        /* Move to next entry */
        p = q + len;
    }

    return NULL;
}


/*
 * 返回 ziplist 的长度
 *
 * 复杂度：O(N)
 */
unsigned int ziplistLen(unsigned char *zl) {

    unsigned int len = 0;
    // 节点的数量 < UINT16_MAX
    if (intrev16ifbe(ZIPLIST_LENGTH(zl)) < UINT16_MAX) {
        // 长度保存在一个 uint16 整数中
        len = intrev16ifbe(ZIPLIST_LENGTH(zl));
    // 节点的数量 >= UINT16_MAX
    } else {
        // 遍历整个 ziplist ，计算长度
        unsigned char *p = zl+ZIPLIST_HEADER_SIZE;
        while (*p != ZIP_END) {
            p += zipRawEntryLength(p);
            len++;
        }

        /* Re-store length if small enough */
        if (len < UINT16_MAX) ZIPLIST_LENGTH(zl) = intrev16ifbe(len);
    }
    return len;
}

/*
 * 返回整个 ziplist 的空间大小
 * 
 * 复杂度：O(1)
 */
size_t ziplistBlobLen(unsigned char *zl) {

    return intrev32ifbe(ZIPLIST_BYTES(zl));
}

void ziplistRepr(unsigned char *zl) {

    unsigned char *p;
    int index = 0;
    zlentry entry;

    printf(
        "{total bytes %d} "
        "{length %u}\n"
        "{tail offset %u}\n",
        intrev32ifbe(ZIPLIST_BYTES(zl)),
        intrev16ifbe(ZIPLIST_LENGTH(zl)),
        intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl)));
    p = ZIPLIST_ENTRY_HEAD(zl);
    while(*p != ZIP_END) {
        entry = zipEntry(p);
        printf(
            "{"
                "addr 0x%08lx, "
                "index %2d, "
                "offset %5ld, "
                "rl: %5u, "
                "hs %2u, "
                "pl: %5u, "
                "pls: %2u, "
                "payload %5u"
            "} ",
            (long unsigned)p,
            index,
            (unsigned long) (p - zl),
            entry.headersize + entry.len,
            entry.headersize,
            entry.prevrawlen,
            entry.prevrawlensize,
            entry.len);
        p += entry.headersize;
        if (ZIP_IS_STR(entry.encoding)) {
            if (entry.len > 40) {
                if (fwrite(p,40,1,stdout) == 0) perror("fwrite");
                printf("...");
            } else {
                if (entry.len &&
                    fwrite(p,entry.len,1,stdout) == 0) perror("fwrite");
            }
        } else {
            printf("%lld", (long long) zipLoadInteger(p,entry.encoding));
        }
        printf("\n");
        p += entry.len;
        index++;
    }
    printf("{end}\n\n");
}