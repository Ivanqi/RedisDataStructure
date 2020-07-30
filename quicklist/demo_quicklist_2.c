#include <string.h>
#include <stdio.h>
#include "demo_quicklist_2_util.h"
#include "demo_quicklist_2.h"
#include "demo_quick_2_lzf.h"

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

void quicklistRelease(quicklist *quicklilst) {

    unsigned long len;
    quicklistNode *current, *next;

    current = quicklilst->head;
    len = quicklilst->len;
    
    while (len--) {
        next = current->next;

        free(current->zl);
        quicklist->count -= current->count;

        free(current);

        quicklist->len--;
        current = next;
    }
    free(quicklist);
}

/**
 * 在“节点”中压缩ziplist并更新编码详细信息
 * 如果ziplist成功压缩，则返回1
 * 如果压缩失败或ziplist太小而无法压缩，则返回0
 */
REDIS_STATIC int __quicklistCompressNode(quicklistNode *node) {

    // 小于压缩最小值，不压缩
    if (node->sz < MIN_COMPRESS_BYTES) return 0;

    quicklistLZF *lzf = malloc(sizeof(*lzf) + node->sz);

    // 如果压缩失败或压缩不够小，请取消
    if (((lzf->sz = lzf_compress(node->zl, node->sz, lzf->compressed, node->sz)) == 0) || lzf->sz + MIN_COMPRESS_IMPROVE >= node->sz) {
        /* 如果值不可压缩，lzf_compress中止/拒绝压缩. */
        free(lzf);
        return 0;
    }

    lzf = realloc(lzf, sizeof(*lzf) + lzf->sz);
    free(node->zl);
    node->zl = (unsigned char *)lzf;
    node->encoding = QUICKLIST_NODE_ENCODING_LZF;
    node->recompress = 0;
    return 1;
}

// 仅压缩未压缩的节点
#define quicklistCompressNode(_node)                                        \
    do {                                                                    \
        if ((_node) && (_node)->encoding == QUICKLIST_NODE_ENCODING_RAW) {  \
            __quicklistCompressNode((_node));                               \
        }                                                                   \
    } while (0)

/**
 * 解压缩'node'中的ziplist并更新编码详细信息
 * 解码成功返回1，解码失败返回0
 */
REDIS_STATIC int __quicklistDecompressNode(quicklistNode *node) {

    void *decompressed = malloc(node->sz);
    quicklistLZF *lzf = (quicklistLZF *)node->zl;
    if (lzf_decompress(lzf->compressed, lzf->sz, decompressed, node->sz) == 0) {
        free(decompressed);
        return 0;
    }
    free(lzf);
    node->zl = decompressed;
    node->encoding = QUICKLIST_NODE_ENCODING_RAW;
    return 1;
}            

// 仅解压压缩点
#define quicklistDecompressNode(_node)                                      \
    do {                                                                    \
        if ((_node) && (_node)->encoding == QUICKLIST_NODE_ENCODING_LZF) {  \
            __quicklistDecompressNode((_node));                             \
        }                                                                   \
    } while (0);                                                            \

// 强制节点不能立即压缩
#define quicklistDecompressNodeForUse(_node)                                \
    do {                                                                    \
        if ((_node) && (_node)->encoding == QUICKLIST_NODE_ENCODING_LZF) {  \
            __quicklistDecompressNode((_node));                             \
            (_node)->recompress = 1;                                        \
        }                                                                   \
    } while (0)                                                             \

/**
 * 从这个quicklistNode提取原始的LZF数据
 * 指向LZF data的指针被分配给“*data”
 * 返回值是压缩的LZF数据的长度
 */
size_t quicklistGetLzf(const quicklistNode *node, void **data) {

    quicklistLZF *lzf = (quicklistLZF *)node->zl;
    *data = lzf->compressed;
    return lzf->sz;
}

#define quicklistAllowsCompression(_ql) ((_ql)->compress != 0)


/**
 * 强制“快表”满足压缩深度设置的压缩准则
 * 保证内部节点得到压缩的唯一方法是迭代到我们的“内部”压缩深度，然后压缩找到的下一个节点
 * 如果压缩深度大于整个列表，则立即返回
 */
REDIS_STATIC void __quicklistCompress(const quicklist *quicklist, quicklistNode *node) {
    
    // 如果长度小于我们的压缩深度（从两侧），我们就不能压缩任何东西
    if (!quicklistAllowsCompression(quicklist) || quicklist->len < (unsigned int)(quicklist->compress * 2)) {
        return;
    }

#if 0
    /* Optimized cases for small depth counts */
    if (quicklist->compress == 1) {
        quicklistNode *h = quicklist->head, *t = quicklist->tail;
        quicklistDecompressNode(h);
        quicklistDecompressNode(t);
        if (h != node && t != node)
            quicklistCompressNode(node);
        return;
    } else if (quicklist->compress == 2) {
        quicklistNode *h = quicklist->head, *hn = h->next, *hnn = hn->next;
        quicklistNode *t = quicklist->tail, *tp = t->prev, *tpp = tp->prev;
        quicklistDecompressNode(h);
        quicklistDecompressNode(hn);
        quicklistDecompressNode(t);
        quicklistDecompressNode(tp);
        if (h != node && hn != node && t != node && tp != node) {
            quicklistCompressNode(node);
        }
        if (hnn != t) {
            quicklistCompressNode(hnn);
        }
        if (tpp != h) {
            quicklistCompressNode(tpp);
        }
        return;
    }
#endif

    /**
     * 迭代，直到我们达到列表两边的压缩深度
     * 注意：因为我们在这个函数的*顶部*进行长度检查，所以我们可以跳过下面的显式空检查
     * 一切都存在
     */
    quicklistNode *forward = quicklist->head;
    quicklistNode *reverse = quicklist->tail;
    int depth = 0;
    int in_depth = 0;
    while (depth++ < quicklist->compress) {
        quicklistDecompressNode(forward);
        quicklistDecompressNode(reverse);

        if (forward == node || reverse == node) {
            in_depth = 1;
        }

        if (forward == reverse) {
            return;
        }

        forward = forward->next;
        reverse = reverse->prev;
    }

    if (!in_depth) {
        quicklistCompressNode(node);
    }
        

    if (depth > 2) {
        // 在这一点上，前进和后退是一个超出深度的节点
        quicklistCompressNode(forward);
        quicklistCompressNode(reverse);
    }
}

#define quicklistCompress(_ql, _node)                                          \
    do {                                                                       \
        if ((_node)->recompress)                                               \
            quicklistCompressNode((_node));                                    \
        else                                                                   \
            __quicklistCompress((_ql), (_node));                               \
    } while (0)

// 如果我们以前使用了quicklistDecompressNodeForUse（），只需重新压缩
#define quicklistRecompressOnly(_ql, _node)                                    \
    do {                                                                       \
        if ((_node)->recompress)                                               \
            quicklistCompressNode((_node));                                    \
    } while (0)


/**
 * 如果 after 为 1， 请在 old_node 后插入 new_node
 * 如果 after 为 0， 请在 old_node 前插入 new_node
 * 
 * 注意：new_node 是未压缩的，因此如果我们将其指定给head或tail，则不需要解压缩它
 */
REDIS_STATIC void __quicklistInsertNode(quicklist *quicklist, quicklistNode *old_node, quicklistNode *new_node, int after) {

    if (after) {
        new_node->prev = old_node;

        if (old_node) {
            new_node->next = old_node->next;
            if (old_node->next) {
                old_node->next->prev = new_node;
            }
            old_node->next = new_node;
        }

        if (quicklist->tail == old_node) {
            quicklist->tail = new_node;
        }
            
    } else {
        new_node->next = old_node;

        if (old_node) {
            new_node->prev = old_node->prev;
            if (old_node->prev) {
                old_node->prev->next = new_node;
            }
            old_node->prev = new_node;
        }

        if (quicklist->head == old_node) {
            quicklist->head = new_node;
        }
            
    }
    /* 如果这个insert创建到目前为止唯一的元素，初始化head/tail. */
    if (quicklist->len == 0) {
        quicklist->head = quicklist->tail = new_node;
    }

    if (old_node) {
        quicklistCompress(quicklist, old_node);
    }
       
    quicklist->len++;
}

// 在现有节点周围插入节点的包装器
REDIS_STATIC void _quicklistInsertNodeBefore(quicklist *quicklist, quicklistNode *old_node, quicklistNode *new_node) {

    __quicklistInsertNode(quicklist, old_node, new_node, 0);
}

REDIS_STATIC void _quicklistInsertNodeAfter(quicklist *quicklist, quicklistNode *old_node, quicklistNode *new_node) {
    
    __quicklistInsertNode(quicklist, old_node, new_node, 1);
}

REDIS_STATIC int _quicklistNodeSizeMeetsOptimizationRequirement(const size_t sz, const int fill) {

    if (fill >= 0) {
        return 0;
    }

    size_t offset = (-fill) - 1;
    if (offset < (sizeof(optimization_level) / sizeof(*optimization_level))) {
        if (sz <= optimization_level[offset]) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

#define sizeMeetsSafetyLimit(sz) ((sz) <= SIZE_SAFETY_LIMIT)

REDIS_STATIC int _quicklistNodeAllowInsert(const quicklistNode *node, const int fill, const size_t sz) {

    if (unlikely(!node)) {
        return 0;
    }

    int ziplist_overhead;
    /* size of previous offset */
    if (sz < 254) {
        ziplist_overhead = 1;
    } else {
        ziplist_overhead = 5;
    }
       
    /* size of forward offset */
    if (sz < 64) {
        ziplist_overhead += 1;
    } else if (likely(sz < 16384)) {
        ziplist_overhead += 2;
    } else {
        ziplist_overhead += 5;
    }

    /* new_sz overestimates if 'sz' encodes to an integer type */
    unsigned int new_sz = node->sz + sz + ziplist_overhead;

    if (likely(_quicklistNodeSizeMeetsOptimizationRequirement(new_sz, fill))) {
        return 1;
    } else if (!sizeMeetsSafetyLimit(new_sz)) {
        return 0;
    } else if ((int)node->count < fill) {
        return 1;
    } else {
        return 0;
    }
}


REDIS_STATIC int _quicklistNodeAllowMerge(const quicklistNode *a, const quicklistNode *b, const int fill) {

    if (!a || !b) {
        return 0;
    }

    /* approximate merged ziplist size (- 11 to remove one ziplist
     * header/trailer) */
    unsigned int merge_sz = a->sz + b->sz - 11;
    if (likely(_quicklistNodeSizeMeetsOptimizationRequirement(merge_sz, fill))) {
        return 1;
    } else if (!sizeMeetsSafetyLimit(merge_sz)) {
        return 0;
    } else if ((int)(a->count + b->count) <= fill) {
        return 1;
    } else {
        return 0;
    }
}
