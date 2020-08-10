#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <string.h>
#include "demo_quicklist_2_ziplist.h"
#include "demo_quicklist_2.h"
#include "demo_quicklist_2_util.h"

#define assertx(_e)                                                                 \
    do {                                                                            \
        if (!(_e)) {                                                                \
            printf("\n\n== ASSERTION FAILED === \n");                               \
            printf("==> %s: %d '%s' is not true\n", __FILE__, __LINE__, #_e);       \
        }                                                                           \
    } while (0)                                                                     \

#define yell(str, ...) printf("ERROR! " str "\n\n", __VA_ARGS__);

#define OK printf("\tOK\n");

#define ERROR                   \
    do {                        \
        printf("\tERROR!\n");   \
    } while (0);                \

#define ERR(x, ...)                                                 \
    do {                                                            \
        printf("%s:%s:%d:\t", __FILE__, __FUNCTION__, __LINE__);    \
        printf("ERROR! " x "\n", __VA_ARGS__);                      \
    } while (0);                                                    \

#define TEST(name) printf("test -- %s\n", name)
#define TEST_DESC(name, ...) printf("test -- " name "\n", __VA_ARGS__);

#define QL_TEST_VERBOSE 0

#define UNUSED(x) (void)(x)

static void ql_info(quicklist *ql) {

#if QL_TEST_VERBOSE
    printf("Container length: %lu\n", ql->len);
    printf("Container size: %lu\n", ql->count);
    if (ql->head) {
        printf("\t(zsize head: %d)\n", ziplistLen(ql->head->zl));
    }

    if (ql->tail) {
        printf("\t(zsize tail: %d)\n", ziplistLen(ql->tail->zl));
    }

    printf("\n");
#else
    UNUSED(ql);
#endif
}

static long long ustime(void) {

    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long long)tv.tv_sec) * 1000000;
    ust += tv.tv_usec;
    return ust;
}

static long long mstime() {

    return ustime() / 1000;
}

// 返回通过迭代列表找到的元素的物理计数
static int _itrprintr(quicklist *ql, int print, int forward) {

    quicklistIter *iter = quicklistGetIterator(ql, forward ? AL_START_HEAD : AL_START_TAIL);
    quicklistEntry entry;

    int i = 0;
    int p = 0;
    quicklistNode *prev = NULL;

    while (quicklistNext(iter, &entry)) {
        if (entry.node != prev) {
            p++;
            prev = entry.node;
        }

        if (print) {
            printf("[%3d (%2d)]: [%.*s] (%lld)\n", i, p, entry.sz, (char *)entry.value, entry.longval);
        }
        i++;
    }

    quicklistReleaseIterator(iter);
    return i;
}

static int itrprintr(quicklist *ql, int print) {

    return _itrprintr(ql, print, 1);
}

static int itrprintr_rev(quicklist *ql, int print) {

    return _itrprintr(ql, print, 0);
}

#define ql_verify(a, b, c, d, e)                        \
    do {                                                \
        err += _ql_verify((a), (b), (c), (d), (e));     \
    } while (0);                                        

// 验证列表元数据与物理列表内容匹配
static int _ql_verify(quicklist *ql, uint32_t len, uint32_t count, uint32_t head_count, uint32_t tail_count) {

    int errors = 0;

    ql_info(ql);
    if (len != ql->len) {
        yell("qulicklist length wrong: expected %d, got %u", len, ql->len);
        errors++;
    }

    if (count != ql->count) {
        yell("quicklist count wrong: expected %d, got %lu", count, ql->count);
        errors++;
    }

    // 从左向右遍历，计算个数
    int loopr = itrprintr(ql, 0);
    if (loopr != (int)ql->count) {
        yell("quiclist cached count not match actual count: expected %lu, got %d", ql->count, loopr);
        errors++;
    }

    // 从右向做遍历，计算个数
    int rlooper = itrprintr_rev(ql, 0);
    // printf("looper:%d, rlooper:%d\n", loopr, rlooper);
    if (loopr != rlooper) {
        yell("quicklist has different forward count than reverse count! Forward count is %d, reverse count is %d.", loopr, rlooper);
        errors++;
    }

    if (ql->len == 0 && !errors) {
        OK;
        return errors;
    }

    // head_count 和  ql->head->count(quicklistNode) 数量 不匹配。 同时  head_count 和 ziplist的数量不匹配
    if (ql->head && head_count != ql->head->count && head_count != ziplistLen(ql->head->zl)) {
        yell("quicklist head count wrong: expected %d, "
             "got cached %d vs. actual %d",
             head_count, ql->head->count, ziplistLen(ql->head->zl));
        errors++;
    }

    // tail_count 和  ql->tail->count(quicklistNode) 数量 不匹配。 同时  tail_count 和 ziplist的数量不匹配
    if (ql->tail && tail_count != ql->tail->count && tail_count != ziplistLen(ql->tail->zl)) {
        yell("quicklist tail count wrong: expected %d, "
             "got cached %u vs. actual %d",
             tail_count, ql->tail->count, ziplistLen(ql->tail->zl));
        errors++;
    }

    // quicklist开启压缩的情况
    if (quicklistAllowsCompression(ql)) {
        quicklistNode *node = ql->head;
        unsigned int low_raw = ql->compress;
        unsigned int high_raw = ql->len - ql->compress;

        for (unsigned int at = 0; at < ql->len; at++, node = node->next) {
            if (node && (at < low_raw || at >= high_raw)) {
                // 未设置压缩标识
                if (node->encoding != QUICKLIST_NODE_ENCODING_RAW) {
                    yell("Incorrect compression: node %d is "
                         "compressed at depth %d ((%u, %u); total "
                         "nodes: %u; size: %u; recompress: %d)",
                         at, ql->compress, low_raw, high_raw, ql->len, node->sz,
                         node->recompress);
                    errors++;
                }
            } else {
                // 已经开始开启压缩标识
                if (node->encoding != QUICKLIST_NODE_ENCODING_LZF && !node->attempted_compress) {
                    yell("Incorrect non-compression: node %d is NOT "
                         "compressed at depth %d ((%u, %u); total "
                         "nodes: %u; size: %u; encoding: %d; recompress: %d; attempted: %d)",
                         at, ql->compress, low_raw, high_raw, ql->len, node->sz, node->encoding,
                         node->recompress, node->attempted_compress);
                    errors++;
                }
            }
        }
    }

    if (!errors) OK;

    return errors;
}

// 生成新字符串，根据字符串“prefix”连接整数i
static char *genstr(char *prefix, int i) {

    static char result[64] = {0};
    snprintf(result, sizeof(result), "%s%d", prefix, i);
    return result;
}


void test_case_1(int argc, char *argv[]) {

    UNUSED(argc);
    UNUSED(argv);

    unsigned int err = 0;
    int optimize_start = -(int)(sizeof(optimization_level) / sizeof(*optimization_level));


    printf("Starting optimization offset at: %d\n", optimize_start);

    int options[] = {0, 1, 2, 3, 4, 5, 6, 10};
    size_t option_count = sizeof(options) / sizeof(*options);
    long long runtime[option_count];

    for (int _i = 0; _i < (int)option_count; _i++) {
        printf("Testing Option %d\n", options[_i]);
        long long start = mstime();

        TEST("create list"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            ql_verify(ql, 0, 0, 0, 0);
            quicklistRelease(ql);
        }

        TEST("add to tail of empty list"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistPushTail(ql, "hello", 6);
            // 1 for head and 1 for tail because 1 node = head = tail
            ql_verify(ql, 1, 1, 1, 1);
            quicklistRelease(ql);
        }

        TEST("add to head of empty list"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistPushHead(ql, "hello", 6);
            // 1 for head and 1 for tail because 1 node = head = tail
            quicklistRelease(ql);
        }

        for (int f = optimize_start; f < 32; f++) {
            TEST_DESC("add to tail 5x at fill %d at compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);
                for (int i = 0; i < 5; i++) {
                    quicklistPushTail(ql, genstr("hello", i), 32);
                }

                if (ql->count != 5) {
                    ERROR;
                }

                if (f == 32) {
                    ql_verify(ql, 1, 5, 5, 5);
                }

                quicklistRelease(ql);
            }
        }

        for (int f = optimize_start; f < 32; f++) {
            TEST_DESC("add to head 5x at fill %d at compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);
                for (int i = 0; i < 5; i++) {
                    quicklistPushHead(ql, genstr("hello", i), 32);
                }

                if (ql->count != 5) {
                    ERROR;
                }

                if (f == 32) {
                    ql_verify(ql, 1, 5, 5, 5);
                }
            }
        }

        for (int f = optimize_start; f < 512; f++) {
            TEST_DESC("add to tail 500x at fill %d compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);
                for (int i = 0; i < 500; i++) {
                    quicklistPushTail(ql, genstr("hello", i), 64);
                }

                if (ql->count != 500) {
                    ERROR;
                }

                if (f == 32) {
                    ql_verify(ql, 16, 500, 32, 20);
                }

                quicklistRelease(ql);
            }
        }

        for (int f = optimize_start; f < 512; f++) {
            TEST_DESC("add to head 500x at fill %d at compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);

                for (int i = 0; i < 500; i++) {
                    quicklistPushHead(ql, genstr("hello", i), 64);
                }

                if (ql->count != 500) {
                    ERROR;
                }

                if (f == 32) {
                    ql_verify(ql, 16, 500, 32, 20);
                }

                quicklistRelease(ql);
            }
        }

        TEST("rotate empty"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistRotate(ql);
            ql_verify(ql, 0, 0, 0, 0);
            quicklistRelease(ql);
        }

        for (int f = optimize_start; f < 32; f++) {
            TEST("rotate one val once"); {
                quicklist *ql = quicklistNew(f, options[_i]);
                quicklistPushHead(ql, "hello", 6);
                quicklistRotate(ql);
                // 忽略压缩验证，因为ziplist太小而无法压缩
                ql_verify(ql, 1, 1, 1, 1);
                quicklistRelease(ql);
            }
        }

        for (int f = optimize_start; f < 3; f++) {
            TEST_DESC("rotate 500 val 5000 times at fill %d at compress %d", f, options[_i]);
            quicklist *ql = quicklistNew(f, options[_i]);
            quicklistPushHead(ql, "900", 3);
            quicklistPushHead(ql, "7000", 4);
            quicklistPushHead(ql, "-1200", 5);
            quicklistPushHead(ql, "42", 2);

            for (int i = 0; i < 500; i++) {
                quicklistPushHead(ql, genstr("hello", i), 64);
            }

            ql_info(ql);

            for (int i = 0; i < 5000; i++) {
                ql_info(ql);
                quicklistRotate(ql);
            }

            if (f == 1) {
                ql_verify(ql, 504, 504, 1, 1);
            } else if (f == 2) {
                ql_verify(ql, 252, 504, 2, 2);
            } else if (f == 32) {
                ql_verify(ql, 16, 504, 32, 24);
            }

            quicklistRelease(ql);
        }

        TEST("pop empty"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistPop(ql, QUICKLIST_HEAD, NULL, NULL, NULL);
            ql_verify(ql, 0, 0, 0, 0);
            quicklistRelease(ql);
        }

        TEST("pop 1 string from 1"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            char *populate = genstr("hello", 331);
            quicklistPushHead(ql, populate, 32);

            unsigned char *data;
            unsigned int sz;
            long long lv;
            ql_info(ql);

            quicklistPop(ql, QUICKLIST_HEAD, &data, &sz, &lv);
            assert(data != NULL);
            assert(sz == 32);

            if (strcmp(populate, (char *)data)) {
                ERR("Pop'd value (%.*s) didn't equal original value (%s)", sz, data, populate);
            }

            free(data);
            ql_verify(ql, 0, 0, 0, 0);
            quicklistRelease(ql);
        }

        TEST("pop head 1 number from 1"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistPushHead(ql, "55513", 5);
            unsigned char *data;
            unsigned int sz;
            long long lv;
            ql_info(ql);

            quicklistPop(ql, QUICKLIST_HEAD, &data, &sz, &lv);
            assert(data == NULL);
            assert(lv == 55513);
            ql_verify(ql, 0, 0, 0, 0);
            quicklistRelease(ql);
        }

        TEST("pop head 500 from 500"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            for (int i = 0; i < 500; i++) {
                quicklistPushHead(ql, genstr("hello", i), 32);
            }

            ql_info(ql);

            for (int i = 0; i < 500; i++) {
                unsigned char *data;
                unsigned int sz;
                long long lv;
                int ret = quicklistPop(ql, QUICKLIST_HEAD, &data, &sz, &lv);
                assert(ret == 1);
                assert(data != NULL);
                assert(sz == 32);
                if (strcmp(genstr("hello", 499 - i), (char *)data)) {
                    ERR("Pop'd value (%.*s) didn't equal original value (%s)", sz, data, genstr("hello", 499 - i));
                }
                free(data);
            }
            ql_verify(ql, 0, 0, 0, 0);
            quicklistRelease(ql);
        }

        TEST("pop head 5000 from 500"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            for (int i = 0; i < 500; i++) {
                quicklistPushHead(ql, genstr("hello", i), 32);
            }

            for (int i = 0; i < 5000; i++) {
                unsigned char *data;
                unsigned int sz;
                long long lv;

                int ret = quicklistPop(ql, QUICKLIST_HEAD, &data, &sz, &lv);
                if (i < 500) {
                    assert(ret == 1);
                    assert(data != NULL);
                    assert(sz == 32);
                    if (strcmp(genstr("hello", 499 - i), (char *)data)) {
                        ERR("Pop'd value (%.*s) didn't equal original value ", "(%s)", sz, data, genstr("hello", 499 - i));
                    }
                    free(data);
                } else {
                    assert(ret == 0);
                }
            }
            ql_verify(ql, 0, 0, 0, 0);
            quicklistRelease(ql);
        }

        TEST("iterate forward over 500 list"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistSetFill(ql, 32);
            for (int i = 0; i < 500; i++) {
                quicklistPushHead(ql, genstr("hello", i), 32);
            }
            quicklistIter *iter = quicklistGetIterator(ql, AL_START_TAIL);
            quicklistEntry entry;
            int i = 0;

            while (quicklistNext(iter, &entry)) {
                char *h = genstr("hello", i);
                if (strcmp((char *)entry.value, h)) {
                    ERR("value [%s] didn't match [%s] at position %d", entry.value, h, i);
                }
                i++;
            }

            if (i != 500) {
                ERR("Didn't iterate over exactly 500 elements (%d)", i);
            }

            ql_verify(ql, 16, 500, 20, 32);
            quicklistReleaseIterator(iter);
            quicklistRelease(ql);
        }

        TEST("insert before with 0 elements"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistEntry entry;

            quicklistIndex(ql, 0, &entry);
            quicklistInsertBefore(ql, &entry, "abc", 4);
            ql_verify(ql, 1, 1, 1, 1);
            quicklistRelease(ql);
        }

        TEST("insert after with 0 elements"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistEntry entry;
            quicklistIndex(ql, 0, &entry);
            quicklistInsertAfter(ql, &entry, "abc", 4);
            ql_verify(ql, 1, 1, 1, 1);
            quicklistRelease(ql);
        }

        TEST("insert before 1 element"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistPushHead(ql, "hello", 6);
            quicklistEntry entry;

            quicklistIndex(ql, 0, &entry);
            quicklistInsertBefore(ql, &entry, "abc", 4);
            ql_verify(ql, 1, 2, 2, 2);
            quicklistRelease(ql);
        }

        TEST("insert after 1 element"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistPushHead(ql, "hello", 6);
            quicklistEntry entry;

            quicklistIndex(ql, 0, &entry);
            quicklistInsertAfter(ql, &entry, "abc", 4);
            ql_verify(ql, 1, 2, 2, 2);
        }

        // for (int f = optimize_start; f < 12; f++) {
        //     TEST_DESC("insert once in elments while iterating at fill %d at " "compress %d\n", f, options[_i]); {
        //         quicklist *ql = quicklistNew(f, options[_i]);
        //         quicklistPushTail(ql, "abc", 3);
        //         quicklistSetFill(ql, 1);
        //         quicklistPushTail(ql, "def", 3);    // force to unique node

        //         quicklistSetFill(ql, f);
        //         quicklistPushTail(ql, "bob", 3);
        //         quicklistPushTail(ql, "foo", 3);
        //         quicklistPushTail(ql, "zoo", 3);
                
        //         itrprintr(ql, 0);

        //         quicklistIter *iter = quicklistGetIterator(ql, AL_START_HEAD);
        //         quicklistEntry entry;
        //         while (quicklistNext(iter, &entry)) {
        //             if (!strncmp((char *)entry.value, "bob", 3)) {
        //                 quicklistInsertBefore(ql, &entry, "bar", 3);
        //             }
        //         }

        //         itrprintr(ql, 0);

        //         quicklistIndex(ql, 0, &entry);
        //         if (strncmp((char *)entry.value, "abc", 3)) {
        //             ERR("Value 0 didn't match, instead got: %.*s", entry.sz, entry.value);
        //         }

        //         quicklistIndex(ql, 1, &entry);
        //         if (strncmp((char *)entry.value, "def", 3)) {
        //             ERR("Value 1 didn't match, instead got: %.*s", entry.sz, entry.value);
        //         }

        //         quicklistIndex(ql, 2, &entry);
        //         if (strncmp((char *)entry.value, "bar", 3)) {
        //             ERR("Value 2 didn't match, instead got: %.*s", entry.sz, entry.value);
        //         }

        //         // quicklistIndex(ql, 3, &entry);
        //         // printf("entry.value:%s\n", (char *)entry.value);
        //         // int ret = strncmp((char *)entry.value, "bob", 3);
        //         // printf("ret: %d\n", ret);
        //         // if (ret) {
        //         //     ERR("Value 3 didn't match, instead got: %.*s", entry.sz, entry.value);
        //         // }

        //         // quicklistIndex(ql, 4, &entry);
        //         // if (strncmp((char *)entry.value, "foo", 3)) {
        //         //     ERR("Value 4 didn't match, instead got: %.*s", entry.sz, entry.value);
        //         // }

        //         // quicklistIndex(ql, 5, &entry);
        //         // if (strncmp((char *)entry.value, "zoo", 3)) {
        //         //     ERR("Value 5 didn't match, instead got: %.*s", entry.sz, entry.value);
        //         // }

        //         quicklistReleaseIterator(iter);
        //         quicklistRelease(ql);
        //     }
        // }
        
        for (int f = optimize_start; f < 1024; f++) {
            TEST_DESC("insert [before] 250 new in middle of 500 elements at fill %d at compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);
                for (int i = 0; i < 500; i++) {
                    quicklistPushTail(ql, genstr("hello", i), 32);
                }

                for (int i = 0; i < 250; i++) {
                    quicklistEntry entry;
                    quicklistIndex(ql, 250, &entry);
                    quicklistInsertBefore(ql, &entry, genstr("abc", i), 32);
                }

                if (f == 32) {
                    ql_verify(ql, 25, 750, 32, 20);
                }
                quicklistRelease(ql);
            }
        }

        for (int f = optimize_start; f < 1024; f++) {
            TEST_DESC("insert [after] 250 new in middle of 500 elements at fill %d at compress %d", f, options[_i]);
            quicklist *ql = quicklistNew(f, options[_i]);
            for (int i = 0; i < 500; i++) {
                quicklistPushHead(ql, genstr("hello", i), 32);
            }

            for (int i = 0; i < 250; i++) {
                quicklistEntry entry;
                quicklistIndex(ql, 250, &entry);
                quicklistInsertAfter(ql, &entry, genstr("abc", i), 32);
            }

            if (ql->count != 750) {
                ERR("List size not 750, but rather %ld", ql->count);
            }

            if (f == 32) {
                ql_verify(ql, 26, 750, 20, 32);
            }
            quicklistRelease(ql);
        }

        TEST("duplicate empty list"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            ql_verify(ql, 0, 0, 0, 0);
            quicklist *copy = quicklistDup(ql);
            ql_verify(copy, 0, 0, 0, 0);
        }

        TEST("duplicate list of 1 element"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistPushHead(ql, genstr("hello", 3), 32);
            ql_verify(ql, 1, 1, 1, 1);

            quicklist *copy = quicklistDup(ql);
            ql_verify(copy, 1, 1, 1, 1);
            quicklistRelease(ql);
            quicklistRelease(copy);
        }

        TEST("duplicate list of 500"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistSetFill(ql, 32);

            for (int i = 0; i < 500; i++) {
                quicklistPushHead(ql, genstr("hello", i), 32);
            }
            ql_verify(ql, 16, 500, 20, 32);

            quicklist *copy = quicklistDup(ql);
            ql_verify(copy, 16, 500, 20, 32);
            quicklistRelease(ql);
            quicklistRelease(copy);
        }


        for (int f = optimize_start; f < 512; f++) {
            TEST_DESC("index 1,200 from 500 list at fill %d at compress %d", f,  options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);
                for (int i = 0; i < 500; i++) {
                    quicklistPushTail(ql, genstr("hello", i + 1), 32);
                }
                   
                quicklistEntry entry;
                quicklistIndex(ql, 1, &entry);
                if (!strcmp((char *)entry.value, "hello2")) {
                    OK;
                } else {
                    ERR("Value: %s", entry.value);
                }
                    
                quicklistIndex(ql, 200, &entry);
                if (!strcmp((char *)entry.value, "hello201")) {
                    OK;
                } else {
                    ERR("Value: %s", entry.value);
                }
                quicklistRelease(ql);
            }

            TEST_DESC("index -1,-2 from 500 list at fill %d at compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);
                for (int i = 0; i < 500; i++) {
                    quicklistPushTail(ql, genstr("hello", i + 1), 32);
                }
                    
                quicklistEntry entry;
                quicklistIndex(ql, -1, &entry);
                if (!strcmp((char *)entry.value, "hello500")) {
                    OK;
                } else {
                    ERR("Value: %s", entry.value);
                }

                quicklistIndex(ql, -2, &entry);
                if (!strcmp((char *)entry.value, "hello499")) {
                    OK;
                } else {
                    ERR("Value: %s", entry.value);
                }
                quicklistRelease(ql);
            }

            TEST_DESC("index -100 from 500 list at fill %d at compress %d", f, options[_i]) {
                quicklist *ql = quicklistNew(f, options[_i]);
                for (int i = 0; i < 500; i++) {
                    quicklistPushTail(ql, genstr("hello", i + 1), 32);
                }

                quicklistEntry entry;
                quicklistIndex(ql, -100, &entry);
                if (!strcmp((char *)entry.value, "hello401")) {
                    OK;
                } else {
                    ERR("Value: %s", entry.value);
                }
                quicklistRelease(ql);
            }

            TEST_DESC("index too big +1 from 50 list at fill %d at compress %d", f, options[_i]) {
                quicklist *ql = quicklistNew(f, options[_i]);
                for (int i = 0; i < 50; i++) {
                    quicklistPushTail(ql, genstr("hello", i + 1), 32);
                }

                quicklistEntry entry;
                if (quicklistIndex(ql, 50, &entry)) {
                    ERR("Index found at 50 with 50 list: %.*s", entry.sz, entry.value);
                } else {
                    OK;
                }
                quicklistRelease(ql);
            }
        }

        TEST("delete range empty list"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistDelRange(ql, 5, 20);
            ql_verify(ql, 0, 0, 0, 0);
            quicklistRelease(ql);
        }

        TEST("delete range of entire node in list of one node"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            for (int i = 0; i < 32; i++) {
                quicklistPushHead(ql, genstr("hello", i), 32);
            }
            ql_verify(ql, 1, 32, 32, 32);
            quicklistDelRange(ql, 0, 32);
            ql_verify(ql, 0, 0, 0, 0);
            quicklistRelease(ql);
        }

        TEST("delete range of entire node with overflow counts"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            for (int i = 0; i < 32; i++) {
                quicklistPushHead(ql, genstr("hello", i), 32);
            }
            ql_verify(ql, 1, 32, 32, 32);
            quicklistDelRange(ql, 0, 128);
            ql_verify(ql, 0, 0, 0, 0);
            quicklistRelease(ql);
        }

        TEST("delete middle 100 of 500 list"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistSetFill(ql, 32);
            for (int i = 0; i < 500; i++) {
                quicklistPushTail(ql, genstr("hello", i + 1), 32);
            }
            ql_verify(ql, 16, 500, 32, 20);
            quicklistDelRange(ql, 200, 100);
            ql_verify(ql, 14, 400, 32, 20);
            quicklistRelease(ql);
        }

        TEST("delete negative 1 from 500 list"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistSetFill(ql, 32);
            for (int i = 0; i < 500; i++) {
                quicklistPushTail(ql, genstr("hello", i + 1), 32);
            }
            ql_verify(ql, 16, 500, 32, 20);
            quicklistDelRange(ql, -1, 1);
            ql_verify(ql, 16, 499, 32, 19);
            quicklistRelease(ql);
        }

        TEST("delete negative 1 from 500 list with overflow counts"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistSetFill(ql, 32);
            for (int i = 0; i < 500; i++) {
                quicklistPushTail(ql, genstr("hello", i + 1), 32);
            }
            ql_verify(ql, 16, 500, 32, 20);
            quicklistDelRange(ql, -1, 128);
            ql_verify(ql, 16, 499, 32, 19);
            quicklistRelease(ql);
        }

        TEST("delete negative 100 from 500 list"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistSetFill(ql, 32);
            for (int i = 0; i < 500; i++) {
                quicklistPushTail(ql, genstr("hello", i + 1), 32);
            }

            quicklistDelRange(ql, -100, 100);
            ql_verify(ql, 13, 400, 32, 16);
            quicklistRelease(ql);
        }

        TEST("delete -10 count 5 from 50 list"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistSetFill(ql, 32);
            for (int i = 0; i < 50; i++) {
                quicklistPushTail(ql, genstr("hello", i + 1), 32);
            }
            ql_verify(ql, 2, 50, 32, 18);
            quicklistDelRange(ql, -10, 5);
            ql_verify(ql, 2, 45, 32, 13);
            quicklistRelease(ql);
        }

        TEST("numbers only list read"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistPushTail(ql, "1111", 4);
            quicklistPushTail(ql, "2222", 4);
            quicklistPushTail(ql, "3333", 4);
            quicklistPushTail(ql, "4444", 4);
            ql_verify(ql, 1, 4, 4, 4);

            quicklistEntry entry;
            quicklistIndex(ql, 0, &entry);
            if (entry.longval != 1111) {
                ERR("Not 1111, %lld", entry.longval);
            }

            quicklistIndex(ql, 1, &entry);
            if (entry.longval != 2222) {
                ERR("Not 2222, %lld", entry.longval);
            }

            quicklistIndex(ql, 2, &entry);
            if (entry.longval != 3333) {
                ERR("Not 3333, %lld", entry.longval);
            }

            quicklistIndex(ql, 3, &entry);
            if (entry.longval != 4444) {
                ERR("Not 4444, %lld", entry.longval);
            }

            if (quicklistIndex(ql, 4, &entry)) {
                ERR("Index past elements: %lld", entry.longval);
            }

            quicklistIndex(ql, -1, &entry);
            if (entry.longval != 4444) {
                ERR("Not 4444 (reverse), %lld", entry.longval);
            }

            quicklistIndex(ql, -2, &entry);
            if (entry.longval != 3333) {
                ERR("Not 3333 (reverse), %lld", entry.longval);
            }

            quicklistIndex(ql, -3, &entry);
            if (entry.longval != 2222) {
                ERR("Not 2222 (reverse), %lld", entry.longval);
            }

            quicklistIndex(ql, -4, &entry);
            if (entry.longval != 1111) {
                ERR("Not 1111 (reverse), %lld", entry.longval);
            }

            if (quicklistIndex(ql, -5, &entry)) {
                ERR("Index past elements (reverse), %lld", entry.longval);
            }
            quicklistRelease(ql);
        }

        TEST("numbers larger list read"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistSetFill(ql, 32);
            char num[32];
            long long nums[5000];
            for (int i = 0; i < 5000; i++) {
                nums[i] = -5157318210846258176 + i;
                int sz = ll2string(num, sizeof(num), nums[i]);
                quicklistPushTail(ql, num, sz);
            }
            
            quicklistPushTail(ql, "xxxxxxxxxxxxxxxxxxxx", 20);
            quicklistEntry entry;
            for (int i = 0; i < 5000; i++) {
                quicklistIndex(ql, i, &entry);
                if (entry.longval != nums[i]) {
                    ERR("[%d] Not longval %lld but rather %lld", i, nums[i], entry.longval);
                }
                entry.longval = 0xdeadbeef;
            }

            quicklistIndex(ql, 5000, &entry);
            if (strncmp((char *)entry.value, "xxxxxxxxxxxxxxxxxxxx", 20)) {
                ERR("String val not match: %s", entry.value);
            }
            ql_verify(ql, 157, 5001, 32, 9);
            quicklistRelease(ql);
        }

        TEST("numbers larger list read B"); {
            quicklist *ql = quicklistNew(-2, options[_i]);
            quicklistPushTail(ql, "99", 2);
            quicklistPushTail(ql, "98", 2);
            quicklistPushTail(ql, "xxxxxxxxxxxxxxxxxxxx", 20);
            quicklistPushTail(ql, "96", 2);
            quicklistPushTail(ql, "95", 2);
            quicklistReplaceAtIndex(ql, 1, "foo", 3);
            quicklistReplaceAtIndex(ql, -1, "bar", 3);
            quicklistRelease(ql);
            OK;
        }

        for (int f = optimize_start; f < 16; f++) {
            TEST_DESC("lrem test at fill %d at compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);

                char *words[] = {"abc", "foo", "bar",  "foobar", "foobared", "zap", "bar", "test", "foo"};
                char *result[] = {"abc", "foo",  "foobar", "foobared", "zap", "test", "foo"};
                char *resultB[] = {"abc", "foo", "foobar", "foobared", "zap", "test"};

                for (int i = 0; i < 9; i++) {
                    quicklistPushTail(ql, words[i], strlen(words[i]));
                }

                /* lrem 0 bar */
                quicklistIter *iter = quicklistGetIterator(ql, AL_START_HEAD);
                quicklistEntry entry;
                int i = 0;
                while (quicklistNext(iter, &entry)) {
                    if (quicklistCompare(entry.zi, (unsigned char *)"bar", 3)) {
                        quicklistDelEntry(iter, &entry);
                    }
                    i++;
                }
                quicklistReleaseIterator(iter);

                /* check result of lrem 0 bar */
                iter = quicklistGetIterator(ql, AL_START_HEAD);
                i = 0;
                int ok = 1;
                while (quicklistNext(iter, &entry)) {
                    /* Result must be: abc, foo, foobar, foobared, zap, test,
                     * foo */
                    if (strncmp((char *)entry.value, result[i], entry.sz)) {
                        ERR("No match at position %d, got %.*s instead of %s", i, entry.sz, entry.value, result[i]);
                        ok = 0;
                    }
                    i++;
                }
                quicklistReleaseIterator(iter);

                quicklistPushTail(ql, "foo", 3);

                /* lrem -2 foo */
                iter = quicklistGetIterator(ql, AL_START_TAIL);
                i = 0;
                int del = 2;
                while (quicklistNext(iter, &entry)) {
                    if (quicklistCompare(entry.zi, (unsigned char *)"foo", 3)) {
                        quicklistDelEntry(iter, &entry);
                        del--;
                    }
                    if (!del)
                        break;
                    i++;
                }
                quicklistReleaseIterator(iter);

                /* check result of lrem -2 foo */
                /* (we're ignoring the '2' part and still deleting all foo
                 * because
                 * we only have two foo) */
                iter = quicklistGetIterator(ql, AL_START_TAIL);
                i = 0;
                size_t resB = sizeof(resultB) / sizeof(*resultB);
                while (quicklistNext(iter, &entry)) {
                    /* Result must be: abc, foo, foobar, foobared, zap, test,
                     * foo */
                    if (strncmp((char *)entry.value, resultB[resB - 1 - i], entry.sz)) {
                        ERR("No match at position %d, got %.*s instead of %s", i, entry.sz, entry.value, resultB[resB - 1 - i]);
                        ok = 0;
                    }
                    i++;
                }

                quicklistReleaseIterator(iter);
                /* final result of all tests */
                if (ok) {
                    OK;
                }
                quicklistRelease(ql);
            }
        }

        for (int f = optimize_start; f < 16; f++) {
            TEST_DESC("iterate reverse + delete at fill %d at compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);
                quicklistPushTail(ql, "abc", 3);
                quicklistPushTail(ql, "def", 3);
                quicklistPushTail(ql, "hij", 3);
                quicklistPushTail(ql, "jkl", 3);
                quicklistPushTail(ql, "oop", 3);

                quicklistEntry entry;
                quicklistIter *iter = quicklistGetIterator(ql, AL_START_TAIL);
                int i = 0;
                while (quicklistNext(iter, &entry)) {
                    if (quicklistCompare(entry.zi, (unsigned char *)"hij", 3)) {
                        quicklistDelEntry(iter, &entry);
                    }
                    i++;
                }
                quicklistReleaseIterator(iter);

                if (i != 5) {
                    ERR("Didn't iterate 5 times, iterated %d times.", i);
                }

                /* Check results after deletion of "hij" */
                iter = quicklistGetIterator(ql, AL_START_HEAD);
                i = 0;
                char *vals[] = {"abc", "def", "jkl", "oop"};
                while (quicklistNext(iter, &entry)) {
                    if (!quicklistCompare(entry.zi, (unsigned char *)vals[i], 3)) {
                        ERR("Value at %d didn't match %s\n", i, vals[i]);
                    }
                    i++;
                }
                quicklistReleaseIterator(iter);
                quicklistRelease(ql);
            }
        }

        for (int f = optimize_start; f < 800; f++) {
            TEST_DESC("iterator at index test at fill %d at compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);
                char num[32];
                long long nums[5000];
                for (int i = 0; i < 760; i++) {
                    nums[i] = -5157318210846258176 + i;
                    int sz = ll2string(num, sizeof(num), nums[i]);
                    quicklistPushTail(ql, num, sz);
                }

                quicklistEntry entry;
                quicklistIter *iter = quicklistGetIteratorAtIdx(ql, AL_START_HEAD, 437);
                int i = 437;
                while (quicklistNext(iter, &entry)) {
                    if (entry.longval != nums[i])
                        ERR("Expected %lld, but got %lld", entry.longval, nums[i]);
                    i++;
                }
                quicklistReleaseIterator(iter);
                quicklistRelease(ql);
            }
        }

        for (int f = optimize_start; f < 40; f++) {
            TEST_DESC("ltrim test A at fill %d at compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);
                char num[32];
                long long nums[5000];
                for (int i = 0; i < 32; i++) {
                    nums[i] = -5157318210846258176 + i;
                    int sz = ll2string(num, sizeof(num), nums[i]);
                    quicklistPushTail(ql, num, sz);
                }
                if (f == 32) {
                    ql_verify(ql, 1, 32, 32, 32);
                }
                /* ltrim 25 53 (keep [25,32] inclusive = 7 remaining) */
                quicklistDelRange(ql, 0, 25);
                quicklistDelRange(ql, 0, 0);
                quicklistEntry entry;

                for (int i = 0; i < 7; i++) {
                    quicklistIndex(ql, i, &entry);
                    if (entry.longval != nums[25 + i]) {
                        ERR("Deleted invalid range!  Expected %lld but got "
                            "%lld", entry.longval, nums[25 + i]);
                    }
                }

                if (f == 32) {
                    ql_verify(ql, 1, 7, 7, 7);
                }
                quicklistRelease(ql);
            }
        }

        for (int f = optimize_start; f < 40; f++) {
            TEST_DESC("ltrim test B at fill %d at compress %d", f, options[_i]); {
                /* Force-disable compression because our 33 sequential
                 * integers don't compress and the check always fails. */
                quicklist *ql = quicklistNew(f, QUICKLIST_NOCOMPRESS);
                char num[32];
                long long nums[5000];
                for (int i = 0; i < 33; i++) {
                    nums[i] = i;
                    int sz = ll2string(num, sizeof(num), nums[i]);
                    quicklistPushTail(ql, num, sz);
                }

                if (f == 32) {
                    ql_verify(ql, 2, 33, 32, 1);
                }

                /* ltrim 5 16 (keep [5,16] inclusive = 12 remaining) */
                quicklistDelRange(ql, 0, 5);
                quicklistDelRange(ql, -16, 16);
                if (f == 32) {
                    ql_verify(ql, 1, 12, 12, 12);
                }

                quicklistEntry entry;
                quicklistIndex(ql, 0, &entry);
                if (entry.longval != 5) {
                    ERR("A: longval not 5, but %lld", entry.longval);
                } else {
                    OK;
                }

                quicklistIndex(ql, -1, &entry);
                if (entry.longval != 16) {
                    ERR("B! got instead: %lld", entry.longval);
                } else {
                    OK;
                }

                quicklistPushTail(ql, "bobobob", 7);
                quicklistIndex(ql, -1, &entry);
                if (strncmp((char *)entry.value, "bobobob", 7)) {
                    ERR("Tail doesn't match bobobob, it's %.*s instead", entry.sz, entry.value);
                }

                for (int i = 0; i < 12; i++) {
                    quicklistIndex(ql, i, &entry);
                    if (entry.longval != nums[5 + i]) {
                        ERR("Deleted invalid range!  Expected %lld but got "
                            "%lld", entry.longval, nums[5 + i]);
                    }
                }
                quicklistRelease(ql);
            }
        }

        for (int f = optimize_start; f < 40; f++) {
            TEST_DESC("ltrim test C at fill %d at compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);
                char num[32];
                long long nums[5000];
                for (int i = 0; i < 33; i++) {
                    nums[i] = -5157318210846258176 + i;
                    int sz = ll2string(num, sizeof(num), nums[i]);
                    quicklistPushTail(ql, num, sz);
                }

                if (f == 32) {
                    ql_verify(ql, 2, 33, 32, 1);
                }
                
                /* ltrim 3 3 (keep [3,3] inclusive = 1 remaining) */
                quicklistDelRange(ql, 0, 3);
                quicklistDelRange(ql, -29, 4000); /* make sure not loop forever */

                if (f == 32) {
                    ql_verify(ql, 1, 1, 1, 1);
                }

                quicklistEntry entry;
                quicklistIndex(ql, 0, &entry);
                if (entry.longval != -5157318210846258173) {
                    ERROR;
                } else {
                    OK;
                }

                quicklistRelease(ql);
            }
        }

        for (int f = optimize_start; f < 40; f++) {
            TEST_DESC("ltrim test D at fill %d at compress %d", f, options[_i]); {
                quicklist *ql = quicklistNew(f, options[_i]);
                char num[32];
                long long nums[5000];
                for (int i = 0; i < 33; i++) {
                    nums[i] = -5157318210846258176 + i;
                    int sz = ll2string(num, sizeof(num), nums[i]);
                    quicklistPushTail(ql, num, sz);
                }

                if (f == 32) {
                    ql_verify(ql, 2, 33, 32, 1);
                }

                quicklistDelRange(ql, -12, 3);
                if (ql->count != 30) {
                    ERR("Didn't delete exactly three elements!  Count is: %lu", ql->count);
                }

                quicklistRelease(ql);
            }
        }

        for (int f = optimize_start; f < 72; f++) {
            TEST_DESC("create quicklist from ziplist at fill %d at compress %d", f, options[_i]) {
                unsigned char *zl = ziplistNew();
                long long nums[64];
                char num[64];
                for (int i = 0; i < 33; i++) {
                    nums[i] = -5157318210846258176 + i;
                    int sz = ll2string(num, sizeof(num), nums[i]);
                    zl = ziplistPush(zl, (unsigned char *)num, sz, ZIPLIST_TAIL);
                }

                for (int i = 0; i < 33; i++) {
                    zl = ziplistPush(zl, (unsigned char *)genstr("hello", i), 32, ZIPLIST_TAIL);
                }

                quicklist *ql = quicklistCreateFromZiplist(f, options[_i], zl);

                if (f == 1) {
                    ql_verify(ql, 66, 66, 1, 1);
                } else if (f == 32) {
                    ql_verify(ql, 3, 66, 32, 2);
                } else if (f == 66) {
                    ql_verify(ql, 1, 66, 66, 66);
                }

                quicklistRelease(ql);
            }
        }


        long long stop = mstime();
        runtime[_i] = stop - start;
    }

    int list_sizes[] = {250, 251, 500, 999, 1000};
    long long start = mstime();

    for (int list = 0; list < (int)(sizeof(list_sizes) / sizeof(*list_sizes)); list++) {
        for (int f = optimize_start; f < 128; f++) {
            for (int depth = 1; depth < 40; depth++) {
                /* skip over many redundant test cases */
                TEST_DESC("verify specific compression of interior nodes with "
                          "%d list "
                          "at fill %d at compress %d",
                          list_sizes[list], f, depth) {
                    quicklist *ql = quicklistNew(f, depth);
                    for (int i = 0; i < list_sizes[list]; i++) {
                        quicklistPushTail(ql, genstr("hello TAIL", i + 1), 64);
                        quicklistPushHead(ql, genstr("hello HEAD", i + 1), 64);
                    }

                    quicklistNode *node = ql->head;
                    unsigned int low_raw = ql->compress;
                    unsigned int high_raw = ql->len - ql->compress;

                    for (unsigned int at = 0; at < ql->len;
                         at++, node = node->next) {
                        if (at < low_raw || at >= high_raw) {
                            if (node->encoding != QUICKLIST_NODE_ENCODING_RAW) {
                                ERR("Incorrect compression: node %d is "
                                    "compressed at depth %d ((%u, %u); total "
                                    "nodes: %u; size: %u)",
                                    at, depth, low_raw, high_raw, ql->len,
                                    node->sz);
                            }
                        } else {
                            if (node->encoding != QUICKLIST_NODE_ENCODING_LZF) {
                                ERR("Incorrect non-compression: node %d is NOT "
                                    "compressed at depth %d ((%u, %u); total "
                                    "nodes: %u; size: %u; attempted: %d)",
                                    at, depth, low_raw, high_raw, ql->len,
                                    node->sz, node->attempted_compress);
                            }
                        }
                    }
                    quicklistRelease(ql);
                }
            }
        }
    }

    long long stop = mstime();

    printf("\n");
    for (size_t i = 0; i < option_count; i++) {
        printf("Test Loop %02d: %0.2f seconds.\n", options[i], (float)runtime[i] / 1000);
    }
    printf("Compressions: %0.2f seconds.\n", (float)(stop - start) / 1000);
    printf("\n");

    if (!err) {
        printf("ALL TESTS PASSED!\n");
    } else {
        ERR("Sorry, not all tests passed!  In fact, %d tests failed.", err);
    }

    // return err;
}

int main(int argc, char *argv[]) {

    test_case_1(argc, argv);
    return 0;
}