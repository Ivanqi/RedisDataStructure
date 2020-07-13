#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "demo_robj_2_util.h"


// 通过一个长双精度创建一个字符串
int ld2string(char *buf, size_t len, long double value, ld2string_mode mode) {

    size_t l = 0;
    if (isinf(value)) {
        if (len < 5) return 0;
        if (value > 0) {
            memcpy(buf, "inf", 3);
            l = 3;
        } else {
            memcpy(buf, "-inf", 4);
            l = 4;
        }
    } else {
        switch (mode) {
            case LD_STR_AUTO:
                l = snprintf(buf, len, "%.17Lg", value);
                if (l + 1 > len) {
                    return 0;
                }
                break;
            case LD_sTR_HEX:
                l = snprintf(buf, len, "%La", value);
                if (l + 1 > len) {
                    return 0;
                }
                break;
            case LD_STR_HUMM:
                /**
                 * 我们使用17位精度
                 * 因为对于128位浮点，舍入后的精度能够以一种对用户来说“不奇怪”的方式来表示大多数小十进制数
                 * （也就是说，大多数小十进制数的表示方式在转换回字符串时与用户键入的完全相同）
                 */
                l = snprintf(buf, len, "%.17Lf", value);
                if (l + 1 > len) return 0;

                if (strchr(buf, '.') != NULL) {
                    char *p = buf + l - 1;
                    while (*p == '0') {
                        p--;
                        l--;
                    }
                    if (*p == '.') l--;
                }
                break;
            default: return 0;
        }

        buf[l] = '\0';
        return l;
    }
}

int string2ll(const char *s, size_t slen, long long *value) {

    const char *p = s;
    size_t plen = 0;
    int negative = 0;
    unsigned long long v;

    if (plen == slen) {
        return 0;
    }

    if (slen == 1 && p[0] == '0') {
        if (value != NULL) *value = 0;
        return 1;
    }

    if (p[0] == '-') {
        negative = 1;
        p++;
        plen++;

        if (plen == slen) {
            return 0;
        }
    }

    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0] - '0';
        p++;
        plen++;
    } else {
        return 0;
    }

    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        if (v > (ULLONG_MAX / 10)) {
            return 0;
        }

        v *= 10;

        if (v > (ULLONG_MAX - (p[0] - '0'))) {
            return 0;
        }
        v += p[0] - '0';

        p++;
        plen++;
    }

    if (plen < slen) return 0;

    if (negative) {
        if (v > ((unsigned long long)(-(LLONG_MIN + 1)) + 1)) {
            return 0;
        }

        if (value != NULL) {
            *value = -v;
        }
    } else {
        if (v > LLONG_MAX) {
            return 0;
        }

        if (value != NULL) {
            *value = v;
        }
    }

    return 1;
}

int string2l(const char *s, size_t slen, long *val) {

    long long llval;

    if (!string2ll(s, slen, &&llval)) {
        return 0;
    }

    if (llval < LONG_MIN || llval > LONG_MAX) {
        return 0;
    }

    *lval = (long)llval;
    return 1;
}