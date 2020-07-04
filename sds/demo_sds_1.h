#ifndef __SDS_1_H
#define __SDS_1_H

#define SDS_MAX_PREALLOC (1024 * 1024)

#include <sys/types.h>
#include <stdarg.h>

int __failed_tests = 0;
int __test_num = 0;
#define test_cond(descr,_c) do { \
    __test_num++; printf("%d - %s: ", __test_num, descr); \
    if(_c) printf("成功\n"); else {printf("失败\n"); __failed_tests++;} \
} while(0);

// sds类型
typedef char *sds;

// sdshdr 结构体
struct sdshdr {
    // buf已占用长度
    int len;
    // buf剩余长度
    int free;

    // 实际保存字符串数据的地方
    char buf[];
};

// 返回sds buf的已占用长度
static inline size_t sdslen(const sds s) {
    struct sdshdr *sh = (void*) (s - (sizeof(struct sdshdr)));
    return sh->len;
}

// 返回 sds buf的可用长度
static inline size_t sdsavail(const sds s) {
    struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
    return sh->free;
}

// 创建一个指定长度的 sds
sds sdsnewlen(const void *init, size_t initlen);

// 创建一个包含给定C字符串的SDS
sds sdsnew(const char *init);

// 创建一个不包含任何内容的的空SDS
sds sdsempty();

// 返回SDS的已使用空间字节数
size_t sdslen(const sds s);

// 创建一个给定SDS的副本(copy)
sds sdsdup(const sds s);

// 释放指定的SDS
void sdsfree(sds s);

// 返回SDS的未使用空间字节数
size_t sdsavail(const sds s);

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

// 接受一个SDS和一个C字符串作为参数，从SDS中移除所有在C字符串中出现的字符
sds sdstrim(sds s, const char *cset);

// 保留SDS给定区间内的数据，不再区间内的数据会被覆盖或清除
sds sdsrange(sds s, int start, int end);

// 更新给定 sds 所对应的 sdshdr 结构的 free 和 len 属性
void sdsupdatelen(sds s);

// 清除SDS保存的字符串内容
void sdsclear(sds s);

// 对比两个SDS字符串是否相同
int sdscmp(const sds s1, const sds s2);

// 按长度切割字符串
sds *sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count);

void sdsfreesplitres(sds *tokens, int count);

// 将给定 sds 中的字符全部转为小写
void sdstolower(sds s);

// 将给定 sds 中的字符全部转为大写
void sdstoupper(sds s);

sds sdsfromlonglong(long long value);

sds sdscatrepr(sds s, const char *p, size_t len);

sds *sdssplitargs(const char *line, int *argc);

void sdssplitargs_free(sds *argv, int argc);

sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);

/* Low level functions exposed to the user API */
// 对 sds 的 buf 进行扩展，扩展的长度不少于 addlen 。
sds sdsMakeRoomFor(sds s, size_t addlen);

// 在 sds buf 的右端添加 incr 个位置。如果 incr 是负数时，可以作为 right-trim 使用
void sdsIncrLen(sds s, int incr);

/*
 * 在不改动 sds buf 内容的情况下，将 buf 内多余的空间释放出去。
 * 在对 sds 执行这个函数之后，下一次对这个 sds 的拼接操作必然需要一次内存分配。
 */
sds sdsRemoveFreeSpace(sds s);

// 计算给定 sds buf 的内存长度（包括已使用和未使用的）
size_t sdsAllocSize(sds s);

#endif