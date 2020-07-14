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
            case LD_STR_HEX:
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

//  将long long 转换为字符串。返回表示数字所需的字符
int ll2string(char *dst, size_t dstlen, long long svalue) {

    static const char digits[201] = 
        "0001020304050607080910111213141516171819"
        "2021222324252627282930313233343536373839"
        "4041424344454647484950515253545556575859"
        "6061626364656667686970717273747576777879"
        "8081828384858687888990919293949596979899";

    int negative;
    unsigned long long value;

    if (svalue < 0) {
        if (svalue != LLONG_MIN) {
            value = -svalue;
        } else {
            value = ((unsigned long long) LLONG_MAX) + 1;
        }
        negative = 1;
    } else {
        value = svalue;
        negative = 0;
    }

    uint32_t const length = digits10(value) + negative;
    if (length >= dstlen) {
        return 0;
    }

    uint32_t next = length;
    dst[next] = '\0';
    next--;

    while (value >= 100) {
        int const i = (value % 100) * 2;
        value /= 100;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
        next -= 2; 
    }

    if (value < 10) {
        dst[next] = '0' + (uint32_t) value;
    } else {
        int i = (uint32_t) value * 2;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
    }

    if (negative) {
        dst[0] = '-';
    }
    return length;
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

uint32_t digits10(uint64_t v) {

    if (v < 10) return 1;
    if (v < 100) return 2;
    if (v < 1000) return 3;

    if (v < 1000000000000UL) {
        if (v < 100000000UL) {
            if (v < 1000000) {
                if (v < 10000) return 4;
                return 5 + (v >= 100000);
            }
            return 7 + (v >= 10000000UL);
        }

        if (v < 10000000000UL) {
            return 9 + (v >= 1000000000UL);
        }
        return 11 + (v >= 100000000000UL);
    }
    return 12 + digits10(v / 1000000000000UL);
}

uint32_t sdigits10(int64_t v) {

    if (v < 0) {
        uint64_t uv = (v != LLONG_MIN) ? (uint64_t) - v : ((uint64_t) LLONG_MAX) + 1;
        return digits10(uv) + 1;
    } else {
        return digits10(v);
    }
}