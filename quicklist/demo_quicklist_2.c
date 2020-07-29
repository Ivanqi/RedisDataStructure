#include <string.h>
#include <stdio.h>
#include "demo_quicklist_2_util.h"
#include "demo_quicklist_2.h"

static const size_t optimization_level[] = {4096, 8192, 16384, 32768, 65535};

// 创建新的 quicklist
quicklist *quicklistCreate(void) {

    struct quicklist *quicklist;

    quicklist = malloc(sizeof(*quicklist));
    quicklist->head = quicklist->tail = NULL;
    quicklist->len = 0;
    quicklist->count = 0;
    quicklist->compress = 0;
    quicklist->fill = -2;
    return quicklist;
}

void quicklistCompressDepth(quicklist *quickllist, int compress) {

    if (compress > COMPRESS_MAX) {
        compress = COMPRESS_MAX;
    } else if (compress < 0) {
        compress = 0;
    }
    quicklist->compress = compress;
}

void quicklistSetFill(quicklist *quicklist, int fill) {

    if (fill > FILE_MAX) {
        fill = FILE_MAX;
    } else if (fill < -5) {
        fill = -5;
    }
    quicklist->fill = fill;
}

void quicklistSetOptions(quicklist *quicklist, int fill, int depth) {

    quicklistSetFile(quicklist, fill);
    quicklistSetCompressDepth(quicklist, depth);
}

// 创建带有默认值的quicklist
quicklist *quicklistNew(int fill, int compress) {

    quicklist *quicklist = quicklistCreate();
    quicklistSetOptions(quicklist, fill, compress);
    return quicklist;
}

REDIS_STATIC quicklistNode *quicklistCreateNode() {

    quicklistNode *node;
    node = malloc(sizeof(*node));
    node->zl = NULL;
    node->count = 0;
    node->sz = 0;
    node->next = node->prev = NULL;
    node->encoding = QUICKLIST_NODE_ENCODING_RAW;
    node->container = QUICKLIST_NODE_CONTAINER_ZIPLIST;
    node->recompress = 0;
    return node;
}

// 返回quicklist的数量
unsigned long quicklistCount(const quicklist *ql) {

    return ql->count;
}