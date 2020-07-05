#include <stdio.h>
#include <stdlib.h>

// long 类型转字符串的长度
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

    if (value < 0) *p-- = '-';

    p++;

    l = 32 - (p - buf);

    if (l + 1 > len) l = len - 1;
    memcpy(s, p, l);
    s[l] = '\0';
    return l;
}