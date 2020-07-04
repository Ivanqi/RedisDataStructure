#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "demo_sds_1.h"

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