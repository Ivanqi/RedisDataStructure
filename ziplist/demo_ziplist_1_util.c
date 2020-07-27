#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <float.h>

#include "demo_ziplist_1_util.h"

// 将long long转换为字符串。返回表示数字所需的字符
int ll2string(char *s, size_t len, long long value) {

    char buf[32], *p;
    unsigned long long v;
    size_t l;

    if (len == 0) return 0;

    v = (value < 0) ? -value : value;
    p = buf + 31;
    do {
        *p-- = '0' + (v % 10);
        v /= 10;
    } while (v);

    if (value < 0) {
        *p-- = '-';
    }

    p++;
    l = 32 - (p - buf);
    if (l + 1 > len) {
        l = len - 1;
    }

    memcpy(s, p, l);
    s[l] = '\0';
    return l;
}

/**
 * 将一个字符串转换为 long long 整数值
 * 
 * 复杂度: O(N)
 * 
 * 返回值
 *  转换成功返回1 失败返回0
 *  转换成功时，将value的值设为转换所得的 long long 值
 */
int string2ll(const char *s, size_t slen, long long *value) {

    const char *p = s;
    size_t plen = 0;
    int negative = 0;
    unsigned long long v;

    // 空字符串
    if (plen == slen) {
        return 0;
    }

    // 值为0
    if (slen == 1 && p[0] == '0') {
        if (value != NULL) *value = 0;
        return 1;
    }

    // 值为负数
    if (p[0] == '-') {
        negative = 1;
        p++; plen++;

        // 只有负号，停止
        if (plen == slen) {
            return 0;
        }
    }

    // 第一个数字必须不为0，否则值为0
    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0] - '0';
        p++; plen++;
    } else if (p[0] == '0' && slen == 1) {
        *value = 0;
        return 1;
    } else {
        return 0;
    }

    // 遍历整个字符串
    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        // 如果 v * 10 > ULLONG_MAX
        // 那么值溢出
        if (v > (ULLONG_MAX / 10)) {    // Overflow
            return 0;
        }
        v *= 10;

        /**
         * 如果 v + (p[0] - '0') > ULLONG_MAX
         * 那么值溢出
         */
        if (v > (ULLONG_MAX - (p[0] - '0'))) {  // Overflow
            return 0;
        }

        v += p[0] - '0';

        p++; plen++;
    }

    // 并非整个字符串都能转换成整数，返回0
    if (plen < slen) {
        return 0;
    }

    // 处理返回值的负数情况
    if (negative) {
        if (v > ((unsigned long long) (-(LLONG_MIN + 1)) + 1)) {    // Overflow
            return 0;
        }
        if (value != NULL) *value = -v;
    } else {
        if (v > LLONG_MAX) {    // Overflow
            return 0;
        }
        if (value != NULL) *value = v;
    }

    return 1;
}

int string2l(const char *s, size_t slen, long *lval) {

    long long llval;

    if (!string2ll(s, slen, &llval)) {
        return 0;
    }

    if (llval < LONG_MIN || llval > LONG_MAX) {
        return 0;
    }

    *lval = (long)llval;
    return 1;
}