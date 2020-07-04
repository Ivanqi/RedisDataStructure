#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "demo_sds_1.h"

/**
 * 根据给定初始化值 init, 创建sds
 * 如果init 为 NULL, 那么创建一个buf只包含 \0 的sds结构体
 */
sds sdsnew(const char *init) {

    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

/**
 * 创建一个指定长度的sds
 * 如果给定了初始化值 init 的话，
 */
sds sdsnewlen(const void *init, size_t initlen) {

    struct sdshdr *sh;

    if (init) {
        sh = malloc(sizeof(struct sdshdr) + initlen + 1);
    } else {
        sh = calloc(initlen + 1, sizeof(struct sdshdr));
    }

    // 内存不足，分配失败
    if (sh == NULL) return NULL;

    sh->len = initlen;
    sh->free = 0;

    /**
     * 如果给定了 init 且 initlen不为0的话
     * 那么将init的内容复制至 sds buf
     */
    if (initlen && init) {
        memcpy(sh->buf, init, initlen);
    }

    // 加上终结符
    sh->buf[initlen] = '\0';

    // 返回buf ，而不是整个sdshdr
    return (char*)sh->buf;
}

// 创建一个不包含任何内容的的空SDS
sds sdsempty() {

    return sdsnewlen("", 0);
}

// 创建一个给定SDS的副本(copy)
sds sdsdup(const sds s) {

    return sdsnewlen(s, strlen(s));
}

// 释放指定的SDS
void sdsfree(sds s) {

    if (s == NULL) return;
    free(s - sizeof(struct sdshdr));
}

// 更新给定 sds 所对应的 sdshdr 结构的 free 和 len 属性
void sdsupdatelen(sds s) {

    struct sdshdr *sh = (void*)(s - (sizeof(struct sdshdr)));

    // 计算正确的buf长度
    int reallen = strlen(s);

    // 更新属性
    sh->free += (sh->len - reallen);
    sh->len = reallen;
}

// 清除SDS保存的字符串内容
void sdsclear(sds s) {

    struct sdshdr *sh = (void*)(s - (sizeof(struct sdshdr)));

    sh->free += sh->len;
    sh->len = 0;
    sh->buf[0] = '\0';
}

// 按长度 len 扩展 sds ，并将 t 拼接到 sds 的末尾
sds sdscatlen(sds s, const void *t, size_t len) {
    
    struct sdshdr *sh;

    size_t curlen = sdslen(s);

    s = sdsMakeRoomFor(s, len);
    if (s == NULL) {
        return NULL;
    }

    // 复制
    memcpy(s + curlen, t, len);

    // 更新 len 和 free属性
    sh = (void *)(s - (sizeof(struct sdshdr)));
    sh->len = curlen + len;
    sh->free = sh->free - len;

    // 终结符
    s[curlen + len] = '\0';
    return s;
}

// 将给定C字符拼接到SDS字符串的末尾
sds sdscat(sds s, const char *t) {
    return sdscatlen(s, t, strlen(t));
}

// 对 sds 的 buf 进行扩展，扩展的长度不少于 addlen 。
sds sdsMakeRoomFor(sds s, size_t addlen) {

    size_t free = sdsavail(s);
    // 剩余空间可以满足需求，无须扩展
    if (free >= addlen) return s;

    size_t len, newlen;
    // 目前len
    len = sdslen(s);
    // 新buf长度
    newlen = (len + addlen);

    struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
    struct sdshdr *newsh;

    /**
     * 如果新buf 长度小于 SDS_MAX_PREALLOC长度
     * 那么将buf 的长度设为新buf长度的两倍
     */
    if (newlen < SDS_MAX_PREALLOC) {
        newlen *= 2;
    } else {
        newlen += SDS_MAX_PREALLOC;
    }

    // 扩展长度
    newsh = realloc(sh, sizeof(struct sdshdr) + newlen + 1);
    if (newsh == NULL) {
        return NULL;
    }

    newsh->free = newlen - len;
    return newsh->buf;
}

/**
 * 将一个 char 数组的前 len 个字节复制至 sds
 * 如果 sds 的 buf 不足以容纳要复制的内容，那么扩展 buf 的长度
 * 让 buf 的长度大于等于 len 。
 */
sds sdscpylen(sds s, const char *t, size_t len) {

    struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
    // 是否需要扩展buf?
    size_t totlen = sh->free + sh->len;
    if (totlen < len) {
        s = sdsMakeRoomFor(s, len);
        if (s == NULL) {
            return NULL;
        }
        sh = (void *)(s - (sizeof(struct sdshdr)));
        totlen = sh->free + sh->len;
    }

    memcpy(s, t, len);
    s[len] = '\0';

    sh->len = len;
    sh->free = totlen - len;

    return s;
}

// 将给定的C字符复制到SDS里面，覆盖SDS原有的字符串
sds sdscpy(sds s, const char *t) {
    return sdscpylen(s, t, strlen(t));    
}

sds sdscatprintf(sds s, const char *fmt, ...) {

    va_list ap;
    char *t;
    va_start(ap, fmt);
    t = sdscatvprintf(s, fmt, ap);
    va_end(ap);
    return t;
}

sds sdscatvprintf(sds s, const char *fmt, va_list ap) {

    va_list cpy;
    char *buf, *t;
    size_t buflen = 16;

    while (1) {
        buf = malloc(buflen);
        if (buf == NULL) {
            return NULL;
        }

        buf[buflen - 2] = '\0';
        va_copy(cpy, ap);
        vsnprintf(buf, buflen, fmt, cpy);
        if (buf[buflen - 2] != '\0') {
            free(buf);
            buflen *= 2;
            continue;
        }
        break;
    }

    t = sdscat(s, buf);
    free(buf);
    return t;
}

// 接受一个SDS和一个C字符串作为参数，从SDS中移除所有在C字符串中出现的字符
sds sdstrim(sds s, const char *cset) {

    struct sdshdr *sh = (void*)(s - (sizeof(struct sdshdr)));
    char *start, *end, *sp, *ep;
    size_t len;

    sp = start = s;
    ep = end = s + sdslen(s) - 1;

    // 从头开始遍历，如果无法匹配cset中的值，就停下
    while (sp <= end && strchr(cset, *sp)) {
        sp++;
    }

    // 从尾开始往前遍历，如果无法匹配cset中的值，就停下
    while (ep > start && strchr(cset, *ep)) {
        ep--;
    }

    len = (sp > ep) ? 0 : ((ep - sp) + 1);
    if (sh->buf != sp) {
        memmove(sh->buf, sp, len);
    }

    sh->buf[len] = '\0';
    sh->free = sh->free + (sh->len - len);
    sh->len = len;

    return s;
}

// 保留SDS给定区间内的数据，不在区间内的数据会被覆盖或清除
sds sdsrange(sds s, int start, int end) {

    struct sdshdr *sh = (void *)(s - sizeof(struct sdshdr));
    size_t newlen, len = sdslen(s);

    if (len == 0) {
        return s;
    }

    // 如果start 为负，字符串长度 + start
    if (start < 0) {
        start = len + start;
        if (start < 0) {
            start = 0;
        }
    }

    // 如果start 为负，字符串长度 + end
    if (end < 0) {
        end = len + end;
        if (end < 0) {
            end = 0;
        }
    }

    newlen = (start > end) ? 0 : (end - start) + 1;

    if (newlen != 0) {
        // start 大于 len的情况
        if (start >= (signed)len) {
            newlen = 0;
        } else if (end >= (signed)len) { // end 大于len的情况
            newlen = len - 1;
            newlen = (start > end) ? 0 : (end - start) + 1;
        }
    } else {
        start = 0;
    }

    if (start && newlen) {
        memmove(sh->buf, sh->buf + start, newlen);
    }

    sh->buf[newlen] = '\0';
    sh->free = sh->free + (sh->len - newlen);
    sh->len = newlen;
    return s;
}

// 对比两个SDS字符串是否相同
int sdscmp(const sds s1, const sds s2) {

    size_t l1, l2, minlen;
    int cmp;

    l1 = sdslen(s1);
    l2 = sdslen(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1, s2, minlen);

    if (cmp == 0) {
        return l1 - l2;
    }
    return cmp;
}

// 在 sds buf 的右端添加 incr 个位置。如果 incr 是负数时，可以作为 right-trim 使用
void sdsIncrLen(sds s, int incr) {

    struct sdshdr *sh = (void*) (s - (sizeof(struct sdshdr)));

    assert(sh->free >= incr);
    sh->len += incr;
    sh->free -= incr;
    assert(sh->free >= 0);
    s[sh->len] = '\0';
}

int main() {
    {
        struct sdshdr *sh;
        sds x = sdsnew("foo"), y;

        test_cond("创建字符串并获取长度", sdslen(x) == 3 && memcmp(x,"foo\0",4) == 0);
        sdsfree(x);

        x = sdsnewlen("foo", 2);
        test_cond("创建指定长度的字符串", sdslen(x) == 2 && memcmp(x,"fo\0",2) == 0);

        x = sdscat(x, "bar");
        test_cond("字符串连接", sdslen(x) == 5 && memcmp(x, "fobar\0", 6) == 0);

        x = sdscpy(x, "a");
        test_cond("sdscpy() 复制一个短字符串", sdslen(x) == 1 && memcmp(x, "a\0", 2) == 0);

        x = sdscpy(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk");
        test_cond("sdscpy() 复制一个长字符串", sdslen(x) == 33 && memcmp(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk\0", 34) == 0);
        sdsfree(x);

        x = sdscatprintf(sdsempty(), "%d", 123);
        test_cond("sdscatprintf() 基础例子", strlen(x) == 3 && memcmp(x, "123\0", 4) == 0);
        sdsfree(x);

        x = sdstrim(sdsnew("xxciaoyy"), "xy");
        test_cond("sdstrim() 正确修改字符", strlen(x) == 4 && memcmp(x, "ciao\0", 5) == 0);

        y = sdsrange(sdsdup(x), 1, 1);
        test_cond("sdsrange(..., 1, 1)", sdslen(y) == 1 && memcmp(y, "i\0", 2) == 0);
        sdsfree(y);

        y = sdsrange(sdsdup(x), -2, -1);
        test_cond("sdsrange(..., -2, -1)", sdslen(y) == 3 && memcmp(y, "iao\0", 4) == 0);
        sdsfree(y);

        y = sdsrange(sdsdup(x), 2, 1);
        test_cond("sdsrange(..., 2, 1)", sdslen(y) == 0 && memcmp(y, "\0", 1) == 0);
        sdsfree(y);

        y = sdsrange(sdsdup(x), 1, 100);
        test_cond("sdsrange(..., 1, 100)", sdslen(y) == 3 && memcmp(y, "iao\0", 4) == 0);
        sdsfree(y);

        y = sdsrange(sdsdup(x), 100, 100);
        test_cond("sdsrange(..., 100, 100)", sdslen(y) == 0 && memcmp(y, "\0", 1) == 0);
        sdsfree(y);
        sdsfree(x);

        x = sdsnew("foo");
        y = sdsnew("foa");
        test_cond("sdscmp(foo, foa)不相等", sdscmp(x, y) > 0);
        sdsfree(x);
        sdsfree(y);

        x = sdsnew("bar");
        y = sdsnew("bar");
        test_cond("sdscmp(bar, bar)相等", sdscmp(x, y) == 0);
        sdsfree(x);
        sdsfree(y);

        x = sdsnew("aar");
        y = sdsnew("bar");
        test_cond("sdscmp(aar, bar)不相等", sdscmp(x, y) < 0);

        {
            int oldfree;
            sdsfree(x);
            x = sdsnew("0");
            sh = (void *) (x - (sizeof(struct sdshdr)));
            test_cond("sdsnew() free/len buffers", sh->len == 1 && sh->free == 0);

            x = sdsMakeRoomFor(x, 1);
            sh = (void*) (x - (sizeof(struct sdshdr)));
            test_cond("sdsMakeRoomFor()", sh->len == 1 && sh->free > 0);
            oldfree = sh->free;

            x[1] = '1';
            sdsIncrLen(x, 1);
            test_cond("sdsIncrLen() -- content", x[0] == '0' && x[1] == '1');
            test_cond("sdsIncrLen() -- len", sh->len == 2);
            test_cond("sdsIncrLen() -- free", sh->free == oldfree - 1);
        }
    }

    test_report();
    return 0;
}