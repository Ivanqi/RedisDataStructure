#ifndef __SDS_2_H
#define __SDS_2_H

#define SDS_MAX_PREALLOC (1024 * 1024)
const char *SDS_NOINIT;

#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>

#define SDS_LLSTR_SIZE 21

typedef char *sds;

/**
 * 在各个headers的定义中使用了 __attribute__((__packed__))，是为了让编译器以紧凑模式来分配内存
 * 如果没有这个属性，编译器可能会为struct的字段优化对齐，在其中填充空字节
 * 那样的话，就不能保证header和sds的数据部分紧紧前后相邻
 * 也不能按照固定向低地址方向偏移1个字节的方式来获取flag字段
 *  s1 是 sds字符串。通过s1[-1]就可以获取flag属性
 * 
 * 
 * 在各个header的定义中最后一个char buf[]。是一个没有指明长度的字符数组
 * 它在这里只是起到一个标记的作用，表示在flags字段后面的一个数组
 * 而程序在为header分配内存的时候，它并不占用内存空间。如果计算sizeof(struct sdshdr16)的值，那么结果是5字节，其中没有buf字段
 * 
 * sds字符串的header，其实隐藏在真正的字符串数据的前面(低地址位置)。这样的一个定义，有如下好处
 *  1. header和数据相邻，而不用分为两块内存空间来单独分配。这有利于减少内存碎片
 *  2. 虽然header有多个类型。但sds可以用统一的char *来表达。且它与传统的C语言字符串保持类型兼容
 *  3. 如果一个sds里面存储的是可打印字符串，那么我们可以直接把它传给C函数，比如使用strcmp比较字符串大小，或者使用printf打印
 */

struct __attribute__((__packed__)) sdshdr5 {
    unsigned char flags;    // 长度使用flags的高5位来存储
    char buf[];
};

struct __attribute__((__packed__)) sdshdr8 {
    uint8_t len;                // 长度
    uint8_t alloc;              // 最大容量
    unsigned char flags;        // 总占用一个字节，其中的最低3个bit用来表示header类型。header类型有5种
    char buf[];                 // 实际保存字符串数据的地方
};

struct __attribute__((__packed__)) sdshdr16 {
    uint16_t len;
    uint16_t alloc;
    unsigned char flags;
    char buf[];
};

struct __attribute__((__packed__)) sdshdr32 {
    uint32_t len;
    uint32_t alloc;
    unsigned char flags;
    char buf[];
};

struct __attribute__((__packed__)) sdshdr64 {
    uint64_t len;
    uint64_t alloc;
    unsigned char flags;
    char buf[];
};

#define SDS_TYPE_5 0
#define SDS_TYPE_8 1
#define SDS_TYPE_16 2
#define SDS_TYPE_32 3
#define SDS_TYPE_64 4

#define SDS_TYPE_MASK 7
#define SDS_TYPE_BITS 3

#define SDS_HDR_VAR(T, s) struct sdshdr##T *sh = (void*)(s - (sizeof(struct sdshdr##T)));

// SDS_HDR用来从sds字符串获取header起始位置指针，例如 SDS_HDR(8, s1)， SDS_HDR(16, 32)
#define SDS_HDR(T, s) ((struct sdshdr##T*)((s) - (sizeof(struct sdshdr##T))))

#define SDS_TYPE_5_LEN(f) ((f) >> SDS_TYPE_BITS)

// 返回SDS的已使用空间字节数
static inline size_t sdslen(const sds s) {

    unsigned char flags = s[-1];
    switch (flags & SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return SDS_TYPE_5_LEN(flags);
        case SDS_TYPE_8:
            return SDS_HDR(8, s)->len;
        case SDS_TYPE_16:
            return SDS_HDR(16, s)->len;
        case SDS_TYPE_32:
            return SDS_HDR(32,s)->len;
        case SDS_TYPE_64:
            return SDS_HDR(64,s)->len;
    }
    return 0;
}

// 返回SDS的未使用空间字节数
static inline size_t sdsavail(const sds s) {

    unsigned char flags = s[-1];
    switch (flags & SDS_TYPE_MASK) {
        case SDS_TYPE_5: {
            return 0;
        }

        case SDS_TYPE_8: {
            SDS_HDR_VAR(8, s);
            return sh->alloc - sh->len;
        }

        case SDS_TYPE_16: {
            SDS_HDR_VAR(16, s);
            return sh->alloc - sh->len;
        }

        case SDS_TYPE_32: {
            SDS_HDR_VAR(32, s);
            return sh->alloc - sh->len;
        }

        case SDS_TYPE_64: {
            SDS_HDR_VAR(64, s);
            return sh->alloc - sh->len;
        }
    }
    return 0;
}

// 设置len属性
static inline void sdssetlen(sds s, size_t newlen) {

    unsigned char flags = s[-1];
    switch (flags & SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            {
                unsigned char *fp = ((unsigned char*)s) - 1;
                *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);   // 高5位设置长度
            }
            break;
        case SDS_TYPE_8:
            SDS_HDR(8, s)->len = newlen;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->len = newlen;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->len = newlen;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->len = newlen;
            break;
    }
}

// 增加len数量
static inline void sdsinclen(sds s, size_t inc) {

    unsigned char flags = s[-1];
    switch (flags & SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            {
                unsigned char *fp = ((unsigned char*)s) -1;
                unsigned char newlen = SDS_TYPE_5_LEN(flags) + inc;
                *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
            }
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->len += inc;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->len += inc;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->len += inc;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->len += inc;
            break;
    }
}

// 获取最大容量
static inline size_t sdsalloc(const sds s) {

    unsigned char flags = s[-1];
    switch (flags & SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return SDS_TYPE_5_LEN(flags);

        case SDS_TYPE_8:
            return SDS_HDR(8,s)->alloc;

        case SDS_TYPE_16:
            return SDS_HDR(16,s)->alloc;

        case SDS_TYPE_32:
            return SDS_HDR(32,s)->alloc;
            
        case SDS_TYPE_64:
            return SDS_HDR(64,s)->alloc;
    }
    return 0;
}

// 设置alloc属性
static inline void sdssetalloc(sds s, size_t newlen) {

    unsigned char flags = s[-1];
    switch (flags & SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            // 无需要操作，因为没有alloc属性
            break;
        case SDS_TYPE_8:
            SDS_HDR(8, s)->alloc = newlen;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->alloc = newlen;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->alloc = newlen;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->alloc = newlen;
            break;
    }
}

// 创建一个指定长度的 sds
sds sdsnewlen(const void *init, size_t initlen);

// 创建一个包含给定C字符串的SDS
sds sdsnew(const char *init);

// 创建一个不包含任何内容的的空SDS
sds sdsempty(void);

// 创建一个给定SDS的副本(copy)
sds sdsdup(const sds s);

// 释放指定的SDS
void sdsfree(sds s);

// 用空字符串SDS扩展至给定的长度
sds sdsgrowzero(sds s, size_t len);

// 按长度 len 扩展 sds ，并将 t 拼接到 sds 的末尾
sds sdscatlen(sds s, const void *t, size_t len);

// 将给定C字符拼接到SDS字符串的末尾
sds sdscat(sds s, const char *t);

// 将给定的C字符串复制到SDS里面，覆盖SDS原有的字符串
sds sdscatsds(sds s, const sds t);

/**
 * 将一个 char 数组的前 len 个字节复制至 sds
 * 如果 sds 的 buf 不足以容纳要复制的内容，那么扩展 buf 的长度
 * 让 buf 的长度大于等于 len 。
 */
sds sdscpylen(sds s, const char *t, size_t len);

// 将给定的C字符复制到SDS里面，覆盖SDS原有的字符串
sds sdscpy(sds s, const char *t);

sds sdscatvprintf(sds s, const char *fmt, va_list ap);
#ifdef __GNUC__
sds sdscatprintf(sds s, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
sds sdscatprintf(sds s, const char *fmt, ...);
#endif

sds sdscatfmt(sds s, char const *fmt, ...);

// 接受一个SDS和一个C字符串作为参数，从SDS中移除所有在C字符串中出现的字符
sds sdstrim(sds s, const char *cset);

// 保留SDS给定区间内的数据，不在区间内的数据会被覆盖或清除
void sdsrange(sds s, ssize_t start, ssize_t end);

// 更新给定 sds 所对应的 sdshdr 结构的 free 和 len 属性
void sdsupdatelen(sds s);

// 清除SDS保存的字符串内容
void sdsclear(sds s);

// 对比两个SDS字符串是否相同
int sdscmp(const sds s1, const sds s2);

// 按长度切割字符串
sds *sdssplitlen(const char *s, ssize_t len, const char *sep, int seplen, int *count);

void sdsfreesplitres(sds *tokens, int count);

// 将给定 sds 中的字符全部转为小写
void sdstolower(sds s);

// 将给定 sds 中的字符全部转为大写
void sdstoupper(sds s);

sds sdsfromlonglong(long long value);

sds sdscatrepr(sds s, const char *p, size_t len);

sds *sdssplitargs(const char *line, int *argc);

sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);

sds sdsjoin(char **argv, int argc, char *sep);

sds sdsjoinsds(sds *argv, int argc, const char *sep, size_t seplen);

/* Low level functions exposed to the user API */
// 对 sds 的 buf 进行扩展，扩展的长度不少于 addlen 。
sds sdsMakeRoomFor(sds s, size_t addlen);

// 在 sds buf 的右端添加 incr 个位置。如果 incr 是负数时，可以作为 right-trim 使用
void sdsIncrLen(sds s, ssize_t incr);

/*
 * 在不改动 sds buf 内容的情况下，将 buf 内多余的空间释放出去。
 * 在对 sds 执行这个函数之后，下一次对这个 sds 的拼接操作必然需要一次内存分配。
 */
sds sdsRemoveFreeSpace(sds s);

// 计算给定 sds buf 的内存长度（包括已使用和未使用的）
size_t sdsAllocSize(sds s);

void *sdsAllocPtr(sds s);

/* Export the allocator used by SDS to the program using SDS.
 * Sometimes the program SDS is linked to, may use a different set of
 * allocators, but may want to allocate or free things that SDS will
 * respectively free or allocate. */
void *sds_malloc(size_t size);
void *sds_realloc(void *ptr, size_t size);
void sds_free(void *ptr);

#endif