#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "demo_sds_2.h"

int __failed_tests = 0;
int __test_num = 0;
#define test_cond(descr,_c) do { \
    __test_num++; printf("%d - %s: ", __test_num, descr); \
    if(_c) printf("成功\n"); else {printf("失败\n"); __failed_tests++;} \
} while(0);

#define test_report() do { \
    printf("%d tests, %d passed, %d failed\n", __test_num, \
                    __test_num-__failed_tests, __failed_tests); \
    if (__failed_tests) { \
        printf("=== WARNING === We have failed tests here...\n"); \
        exit(1); \
    } \
} while(0);


int main () {

    {
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

        x = sdsnew("--");
        x = sdscatfmt(x, "%u,%U--", UINT_MAX, ULLONG_MAX);
        test_cond("sdscatfmt() 处理无符号数字", sdslen(x) == 35 && memcmp(x, "--4294967295,18446744073709551615--", 35) == 0);
        sdsfree(x);

        x = sdsnewlen("aaa\0b", 5);
        test_cond("二进制安全", sdslen(x) == 5 && memcmp(x, "aaa\0b\0", 6) == 0);
        sdsfree(x);

        x = sdsnew(" x ");
        sdstrim(x, " x ");
        test_cond("sdstrim() 匹配所有字符", sdslen(x) == 0);
        sdsfree(x);

        x = sdsnew(" x ");
        sdstrim(x, " ");
        test_cond("sdstrim() 用于单个字符", sdslen(x) == 1 && x[0] == 'x');
        sdsfree(x);

        x = sdstrim(sdsnew("xxciaoyy"), "xy");
        test_cond("sdstrim() 正确修改字符", strlen(x) == 4 && memcmp(x, "ciao\0", 5) == 0);

        y = sdsdup(x);
        sdsrange(y, 1, 1);
        test_cond("sdsrange(..., 1, 1)", sdslen(y) == 1 && memcmp(y, "i\0", 2) == 0);
        sdsfree(y);
        sdsfree(x);

        x = sdsnew("foa");
        y = sdsnew("foa");
        test_cond("sdscmp(foo, foa)", sdscmp(x, y) > 0);
        sdsfree(y);
        sdsfree(x);

        x = sdsnew("bar");
        y = sdsnew("bar");
        test_cond("sdscmp(bar, bar)相等", sdscmp(x, y) == 0);
        sdsfree(x);
        sdsfree(y);

        x = sdsnew("aar");
        y = sdsnew("bar");
        test_cond("sdscmp(aar, bar)不相等", sdscmp(x, y) < 0);

        x = sdsnewlen("\a\n\0foo\r", 7);
        x = sdscatrepr(sdsempty(), x, sdslen(x));
        test_cond("sdscatrepr(...data...)", memcmp(y, "\"\\a\\n\\x00foo\\r\"", 15) == 0);

        {
            unsigned int oldfree;
            char *p;
            int step = 10, j, i;
            sdsfree(x);
            sdsfree(y);
            x = sdsnew("0");
            test_cond("sdsnew() free/len buffers", sdslen(x) == 1 && sdsavail(x) == 0);

            for (i = 0; i < 10; i++) {
                int oldlen = sdslen(x);
                x = sdsMakeRoomFor(x, step);
                int type = x[-1] & SDS_TYPE_MASK;

                test_cond("sdsMakeRoomFor() len", sdslen(x) == oldlen);
                if (type != SDS_TYPE_5) {
                    test_cond("sdsMakeRoomFor() free", sdsavail(x) >= step);
                    oldfree = sdsavail(x);
                }

                p = x + oldlen;
                for (j = 0; j < step; j++) {
                    p[j] = 'A' + j;
                }
                sdsIncrLen(x, step);
            }
            test_cond("sdsMakeRoomFor() content", memcmp("0ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ",x,101) == 0);

            test_cond("sdsMakeRoomFor() final length",sdslen(x)==101);

            sdsfree(x);
        }
    }

    test_report();

    return 0;
}