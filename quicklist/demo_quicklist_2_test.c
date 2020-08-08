#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include "demo_quicklist_2_ziplist.h"
#include "demo_quicklist_2.h"

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
                if (node->encoding != QUICKLIST_NODE_ENCODING_RAW) {
                    yell("Incorrect compression: node %d is "
                         "compressed at depth %d ((%u, %u); total "
                         "nodes: %u; size: %u; recompress: %d)",
                         at, ql->compress, low_raw, high_raw, ql->len, node->sz,
                         node->recompress);
                    errors++;
                }
            } else {
                if (node->encoding != QUICKLIST_NODE_ENCODING_LZF && !node->attempted_compress) {
                    yell("Incorrect non-compression: node %d is NOT "
                         "compressed at depth %d ((%u, %u); total "
                         "nodes: %u; size: %u; recompress: %d; attempted: %d)",
                         at, ql->compress, low_raw, high_raw, ql->len, node->sz,
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
    }
}

int main(int argc, char *argv[]) {

    test_case_1(argc, argv);
    return 0;
}