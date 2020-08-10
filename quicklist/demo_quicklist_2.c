#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "demo_quicklist_2_util.h"
#include "demo_quick_2_lzf.h"
#include "demo_quicklist_2_ziplist.h"
#include "demo_quicklist_2.h"

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

void quicklistSetCompressDepth(quicklist *quicklist, int compress) {

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

    quicklistSetFill(quicklist, fill);
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

void quicklistRelease(quicklist *quicklist) {

    unsigned long len;
    quicklistNode *current, *next;

    current = quicklist->head;
    len = quicklist->len;
    
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

#ifdef REDIS_TEST
    node->attempted_compress = 1;
#endif

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
        // new_node->prev指针 指向 old_node
        new_node->prev = old_node;

        // 如果old_node不为空
        if (old_node) {
            // new_node 的 next 指针 指向 old_node->next 指针。new_node 替换 old_node的位置
            new_node->next = old_node->next;
            // old_node->next指针不为空
            if (old_node->next) {
                // old_node->next->prev 指针，指向new_node。 new_node 替换 old_node的为止
                old_node->next->prev = new_node;
            }
            //old_node->next指针指向new_node
            old_node->next = new_node;
        }

        if (quicklist->tail == old_node) {
            // 把 new_node 指针，更新到tail
            quicklist->tail = new_node;
        }
            
    } else {
        // new_node->next指针 指向 old_node
        new_node->next = old_node;

        // 如果old_node不为空
        if (old_node) {
            // new_node->prev指针，接管 old_node->prev指针的内容
            new_node->prev = old_node->prev;
            // 如果old_node->prev指针不为空
            if (old_node->prev) {
                // old_node->prev->next指针为 new_node
                old_node->prev->next = new_node;
            }
            // old_node->prev 指针指向 new_node
            old_node->prev = new_node;
        }

        if (quicklist->head == old_node) {
            // 把 new_node 指针，更新到 head中
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
    // offset 小于 optimization_level 元素个数
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

/*
 * 判断是否允许插入数据
 * 判断标准
 *  1. node 不能为NULL
 *  2. fill < 0 && new_sz 小于 optimization_level
 *  3. new_sz 大于 SIZE_SAFETY_LIMIT(8192)
 *  4. fill > 0 && quicklistNode 节点数量数量小于 fill的值
 */
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

/**
 * 判断是否被允许合并
 * 允许合并的规则
 *  1. a 或者 b 不能为NULL
 *  2. fill < 0 && merge_sz 不能大于 optimization_level的值
 *  3. new_sz 大于 SIZE_SAFETY_LIMIT(8192)
 *  4. fille > 0 && a->count + b->count <= fill
 */
REDIS_STATIC int _quicklistNodeAllowMerge(const quicklistNode *a, const quicklistNode *b, const int fill) {

    if (!a || !b) {
        return 0;
    }

    // 合并的ziplist的大致大小（-11可删除一个ziplist标头/尾部
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

// 更新ziplist 的 数量
#define quicklistNodeUpdateSz(node)                                            \
    do {                                                                       \
        (node)->sz = ziplistBlobLen((node)->zl);                               \
    } while (0)


/**
 * 如何向快表的头节点添加新条目
 *   如果判断quicklist 的 node 节点不被允许插入数据
 *   则会新增加一个 node 节点
 *   最后才会往zl追加新数据
 * 
 * 如果使用了现有头，则返回0
 * 如果创建了新的头，则返回1
 */
int quicklistPushHead(quicklist *quicklist, void *value, size_t sz) {

    quicklistNode *orig_head = quicklist->head;

    if (likely(_quicklistNodeAllowInsert(quicklist->head, quicklist->fill, sz))) {
        quicklist->head->zl = ziplistPush(quicklist->head->zl, value, sz, ZIPLIST_HEAD);
        quicklistNodeUpdateSz(quicklist->head);
    } else {
        quicklistNode *node = quicklistCreateNode();
        node->zl = ziplistPush(ziplistNew(), value, sz, ZIPLIST_HEAD);

        quicklistNodeUpdateSz(node);
        _quicklistInsertNodeBefore(quicklist, quicklist->head, node);
    }

    quicklist->count++;
    quicklist->head->count++;
    return (orig_head != quicklist->head);
}


/**
 * 向快表的尾部节点添加新条目
 *  如果判断quicklist 的 node 节点不被允许插入数据
 *  则会新增加一个 node 节点
 *  最后才会往zl追加新数据
 * 
 * 如果使用现有尾部，则返回0
 * 如果创建了新的尾部，则返回1
 */
int quicklistPushTail(quicklist *quicklist, void *value, size_t sz) {

    quicklistNode *orig_tail = quicklist->tail;
    if (likely(_quicklistNodeAllowInsert(quicklist->tail, quicklist->fill, sz))) {
        quicklist->tail->zl = ziplistPush(quicklist->tail->zl, value, sz, ZIPLIST_TAIL);
        quicklistNodeUpdateSz(quicklist->tail);
    } else {
        quicklistNode *node = quicklistCreateNode();
        node->zl = ziplistPush(ziplistNew(), value, sz, ZIPLIST_TAIL);

        quicklistNodeUpdateSz(node);
        _quicklistInsertNodeAfter(quicklist, quicklist->tail, node);
    }

    quicklist->count++;
    quicklist->tail->count++;
    return (orig_tail != quicklist->tail);
}

/**
 * 创建新节点， 该节点包含一个预先形成的ziplist
 * 
 * 用于加载RDB，其中存储了整个ziplist以供以后检索
 */
void quicklistAppendZiplist(quicklist *quicklist, unsigned char *zl) {

    quicklistNode *node = quicklistCreateNode();

    node->zl = zl;
    node->count = ziplistLen(node->zl);
    node->sz = ziplistBlobLen(zl);

    _quicklistInsertNodeAfter(quicklist, quicklist->tail, node);
    quicklist->count += node->count;
}

/**
 * 将ziplist “zl”的所有值单独附加到"quicklist"中
 * 
 * 这使我们能够将旧的RDB ziplist 还原成比保存的RDB ziplist更小的ziplist大小的快表
 * 
 *  Returns 'quicklist' argument. Frees passed-in ziplist 'zl'
 */
quicklist *quicklistAppendValuesFromZiplist(quicklist *quicklist, unsigned char *zl) {

    unsigned char *value;
    unsigned int sz;
    long long longval;
    char longstr[32] = {0};

    unsigned char *p = ziplistIndex(zl, 0);
    while (ziplistGet(p, &value, &sz, &longval)) {
        if (!value) {
            /* Write the longval as a string so we can re-add it */
            sz = ll2string(longstr, sizeof(longstr), longval);
            value = (unsigned char *)longstr;
        }
        quicklistPushTail(quicklist, value, sz);
        p = ziplistNext(zl, p);
    }
    free(zl);
    return quicklist;
}

/**
 * 从单个现在的ziplist创建新的(可能是多节点)快表
 * 
 *  Returns new quicklist.  Frees passed-in ziplist 'zl'
 */
quicklist *quicklistCreateFromZiplist(int fill, int compress, unsigned char *zl) {
    return quicklistAppendValuesFromZiplist(quicklistNew(fill, compress), zl);
}

#define quicklistDeleteIfEmpty(ql, n)                                          \
    do {                                                                       \
        if ((n)->count == 0) {                                                 \
            __quicklistDelNode((ql), (n));                                     \
            (n) = NULL;                                                        \
        }                                                                      \
    } while (0)



REDIS_STATIC void __quicklistDelNode(quicklist *quicklist, quicklistNode *node) {

    if (node->next) {
        node->next->prev = node->prev;
    }
        
    if (node->prev) {
        node->prev->next = node->next;
    }
    
    if (node == quicklist->tail) {
        quicklist->tail = node->prev;
    }

    if (node == quicklist->head) {
        quicklist->head = node->next;
    }

    /* If we deleted a node within our compress depth, we
     * now have compressed nodes needing to be decompressed. */
    __quicklistCompress(quicklist, NULL);

    quicklist->count -= node->count;

    free(node->zl);
    free(node);
    quicklist->len--;
}

 /**
  * 从列表中删除一个条目，给定条目的节点和指向节点中条目的指针
  * 
  * 注意：quicklistDelIndex() 需要未压缩的节点，因此你已经从某个未压缩的节点获取了 *p
  * 
  * 如果整个节点已删除，则返回1
  * 如果节点仍然存在，则返回0
  * 
  * 同时用ziplist中的下一个偏移量更小in/out参数'p'
  */
REDIS_STATIC int quicklistDelIndex(quicklist *quicklist, quicklistNode *node, unsigned char **p) {

    int gone = 0;

    node->zl = ziplistDelete(node->zl, p);
    node->count--;
    if (node->count == 0) {
        gone = 1;
        __quicklistDelNode(quicklist, node);
    } else {
        quicklistNodeUpdateSz(node);
    }
    quicklist->count--;
    /* If we deleted the node, the original node is no longer valid */
    return gone ? 1 : 0;
}

/* Delete one element represented by 'entry'
 *
 * 'entry' stores enough metadata to delete the proper position in
 * the correct ziplist in the correct quicklist node. */

/**
 * 删除一个由 "entry" 表示的元素
 * 
 * "entry" 存储足够的元数据，以删除正确的快表列表节点中正确的ziplist列表中适当的位置
 */
void quicklistDelEntry(quicklistIter *iter, quicklistEntry *entry) {

    quicklistNode *prev = entry->node->prev;
    quicklistNode *next = entry->node->next;
    int deleted_node = quicklistDelIndex((quicklist *)entry->quicklist, entry->node, &entry->zi);

    // 删除后，zi现在对将来的使用无效
    iter->zi = NULL;

    // 如果当前节点被删除，我们必须更新迭代器节点和偏移量
    if (deleted_node) {
        if (iter->direction == AL_START_HEAD) {
            iter->current = next;
            iter->offset = 0;
        } else if (iter->direction == AL_START_TAIL) {
            iter->current = prev;
            iter->offset = -1;
        }
    }

    /**
     * 否则，如果（！deleted_node），则无需更改
     * 我们已经在上面重置了iter-> zi，并且现有的iter-> offset不会再移动，因为
     *  - [1、2、3] =>删除偏移量1 => [1、3]：下一个元素仍偏移1
     *  - [1、2、3] =>删除偏移量0 => [2、3]：下一个元素仍偏移0
     * 如果我们删除了offet N处的最后一个元素，并且此ziplist的长度现在为N-1，则对quicklistNext（）的下一次调用将跳至下一个节点。
     */
}

/**
 * 将偏移量 “索引” 处的快速列表条目替换为长度为 “sz” 的 “数据”
 * 
 * 如果发生替换，则返回1
 * 如果替换失败且未发生任何更改，则返回0
 */
int quicklistReplaceAtIndex(quicklist *quicklist, long index, void *data, int sz) {

    quicklistEntry entry;
    if (likely(quicklistIndex(quicklist, index, &entry))) {
        /* quicklistIndex provides an uncompressed node */
        entry.node->zl = ziplistDelete(entry.node->zl, &entry.zi);
        entry.node->zl = ziplistInsert(entry.node->zl, entry.zi, data, sz);
        quicklistNodeUpdateSz(entry.node);
        quicklistCompress(quicklist, entry.node);
        return 1;
    } else {
        return 0;
    }
}

/**
 * 给定两个节点，请尝试合并其ziplist
 * 如果我们的填充因子可以处理更高级别的内容，这将有助于我们避免使用包含3个元素ziplist的快速列表
 * 
 * 注意：“ a”必须在“ b”的左边
 * 
 * 调用此函数后，“ a”和“ b”都应视为不可用. 必须使用此函数的返回值，而不是重新使用任何quicklistNode输入参数
 * 
 * 返回选择要合并的输入节点；如果不可能合并，则返回NULL 
 */

REDIS_STATIC quicklistNode *_quicklistZiplistMerge(quicklist *quicklist, quicklistNode *a, quicklistNode *b) {

    D("Requested merge (a,b) (%u, %u)", a->count, b->count);

    quicklistDecompressNode(a);
    quicklistDecompressNode(b);
    
    if ((ziplistMerge(&a->zl, &b->zl))) {
        // 我们合并了ziplist！现在删除未使用的quicklistNode
        quicklistNode *keep = NULL, *nokeep = NULL;
        if (!a->zl) {
            nokeep = a;
            keep = b;
        } else if (!b->zl) {
            nokeep = b;
            keep = a;
        }
        keep->count = ziplistLen(keep->zl);
        quicklistNodeUpdateSz(keep);

        nokeep->count = 0;
        __quicklistDelNode(quicklist, nokeep);
        quicklistCompress(quicklist, keep);
        return keep;
    } else {
        /* else, the merge returned NULL and nothing changed. */
        return NULL;
    }
}

/**
 * 尝试在 'center' 两侧的两个节点内合并ziplist
 * 我们尝试合并
 *  (center->prev->prev, center->prev)
 *  (center->next, center->next->next)
 *  (center->prev, center)
 *  (center, center->next)
 */
REDIS_STATIC void _quicklistMergeNodes(quicklist *quicklist, quicklistNode *center) {

    int fill = quicklist->fill;
    quicklistNode *prev, *prev_prev, *next, *next_next, *target;
    prev = prev_prev = next = next_next = target = NULL;

    if (center->prev) {
        prev = center->prev;
        if (center->prev->prev) {
            prev_prev = center->prev->prev;
        }
    }

    if (center->next) {
        next = center->next;
        if (center->next->next) {
            next_next = center->next->next;
        }
    }

    // 尝试合并prev_prev 和 prev
    // 判断是否被允许合并
    if (_quicklistNodeAllowMerge(prev, prev_prev, fill)) {
        _quicklistZiplistMerge(quicklist, prev_prev, prev);
        prev_prev = prev = NULL; /* they could have moved, invalidate them. */ 
    }

    // 尝试合并next 和 next_next
    // 判断是否被允许合并
    if (_quicklistNodeAllowMerge(next, next_next, fill)) {
        _quicklistZiplistMerge(quicklist, next, next_next);
        next = next_next = NULL; /* they could have moved, invalidate them. */
    }

    // 尝试合并中心节点和上一个节点
    if (_quicklistNodeAllowMerge(center, center->prev, fill)) {
        target = _quicklistZiplistMerge(quicklist, center->prev, center);
        center = NULL; /* center could have been deleted, invalidate it. */
    } else {
        /* else, we didn't merge here, but target needs to be valid below. */
        target = center;
    }

    // 使用中心合并(或原始)的结果与下一个节点合并
    if (_quicklistNodeAllowMerge(target, target->next, fill)) {
        _quicklistZiplistMerge(quicklist, target, target->next);
    }
}

/**
 * 将“节点”分为两部分，分别由“偏移”和“之后”参数化
 * 
 * 'after'参数控制返回哪个quicklistNode
 * 
 * 如果'after'== 1，则返回的节点的元素在'offset'之后
 *  输入节点使元素保持“偏移”，包括“偏移”
 * 
 * 如果'after'== 0，则返回的节点具有直到'offset'的元素，包括'offset'
 *  输入节点在“偏移”之后保留元素
 * 
 * 如果'after'== 1，则返回的节点将具有_after_'offset'元素
 *  返回的节点将具有元素[OFFSET + 1，END]
 *  输入节点保留元素[0，OFFSET]
 * 
 * 如果'after'== 0，则返回的节点将使元素保持在'offset'以内，包括'offset'
 *  返回的节点将具有元素[0，OFFSET]
 *  输入节点保留元素[OFFSET + 1，END]
 * 
 * 输入节点保留返回节点未使用的所有元素
 * 
 * 返回新创建的节点；如果无法拆分，则返回NULL
 * 
 */
REDIS_STATIC quicklistNode *_quicklistSplitNode(quicklistNode *node, int offset, int after) {

    size_t zl_sz = node->sz;

    quicklistNode *new_node = quicklistCreateNode();
    new_node->zl = malloc(zl_sz);

    // 复制原始ziplist，以便我们对其进行拆分
    memcpy(new_node->zl, node->zl, zl_sz);

    // -1在这里表示“继续删除，直到列表结束”
    // orig_start 和 orig_extent 为要在 node->sz 删除的开始结束地址。为了把多余的数据切分出去
    int orig_start = after ? offset + 1 : 0;
    int orig_extent = after ? -1 : offset;

    // new_start 和 new_extent 为要new_node->zl删除的开始结束地址。这部分是不需要的
    int new_start = after ? 0 : offset;
    int new_extent = after ? offset + 1 : -1;

    D("After %d (%d); ranges: [%d, %d], [%d, %d]", after, offset, orig_start, orig_extent, new_start, new_extent);

    // 删除被切分的数据的数据
    node->zl = ziplistDeleteRange(node->zl, orig_start, orig_extent);
    node->count = ziplistLen(node->zl);
    // 更新ziplist长度
    quicklistNodeUpdateSz(node);

    // 得到被切分出的数据
    new_node->zl = ziplistDeleteRange(new_node->zl, new_start, new_extent);
    new_node->count = ziplistLen(new_node->zl);
    // 更新ziplist长度
    quicklistNodeUpdateSz(new_node);

    D("After split lengths: orig (%d), new (%d)", node->count, new_node->count);
    return new_node;
}

/**
 * 在现有条目“条目”之前或之后插入一个新条目
 * 
 * 如果after == 1，则将新值插入“ entry”之后，否则将新值插入“ entry”之前
 * quicklistInsert 逻辑
 *  1. 如果当前节点未空，那么在quiclist创建新节点
 * 
 *  2. 数据不满且数据往后插入的情况
 *      1. 如果 ziplist next 为NULL,就ziplist的尾部插入数据
 *      2. 如果 ziplist next 不为NULL, 就从 当前的ziplist next 后插入数据
 * 
 *  3. 数据不满且数据往前插入情况
 *      1. 往当前的 ziplist 头部插入数据
 *  
 *  4. 数据满了 且 往后插入 且 node->next节点不为空 且 当前 node->next的数据未满
 *      1. 往下一个quicklistNode 的 ziplist头部插入数据
 * 
 *  5. 数据满了 且 往前插入 且 node->prev 不为空 且 node-prev的数据未满
 *      1. 往上一个quicklistNode 的 ziplist尾部插入数据
 * 
 *  6. 数据满了 且 上一个或下一个quicklistNode的数据也满了
 *      1. 创建一个新quicklistNode 和 ziplist并填入数据
 *      2. 把新的quiclistNode 从后面插入到 quicklist中去
 * 
 *  7. 数据全满了
 *      1. 对当前的quicklistNode 的 ziplist 按照 当前给的offset 拆分新和旧两个 quicklistNode
 *      2. 把新的quiclistNode 从后面插入到 quicklist中去
 *      3. 合并数据
 *          1. 当前节点的上一个和上上个节点尝试合并
 *          2. 当前节点的下一个和下下个节点尝试合并
 *          3. 当前节点和上一个节点尝试合并
 *          4. 当前节点和下一个节点尝试合并
 */
REDIS_STATIC void _quicklistInsert(quicklist *quicklist, quicklistEntry *entry, void *value, const size_t sz, int after) {

    int full = 0, at_tail = 0, at_head = 0, full_next = 0, full_prev = 0;
    int fill = quicklist->fill;
    quicklistNode *node = entry->node;
    quicklistNode *new_node = NULL;

    // node节点为空，在列表中创建新节点
    if (!node) {
        D("No node given!");
        new_node = quicklistCreateNode();
        new_node->zl = ziplistPush(ziplistNew(), value, sz, ZIPLIST_HEAD);
        __quicklistInsertNode(quicklist, NULL, new_node, after);
        new_node->count++;
        quicklist->count++;
        return;
    }

    // 判断是否允许插入数据
    if (!_quicklistNodeAllowInsert(node, fill, sz)) {
        D("Current node is full with count %d with requested fill %lu", node->count, fill);
        full = 1;
    }

    if (after && (entry->offset == node->count)) {
        D("At Tail of current ziplist");
        at_tail = 1;
        // 判断下一个节点是否允许插入数据
        if (!_quicklistNodeAllowInsert(node->next, fill, sz)) {
            D("Next node is full too.");
            full_next = 1;
        }
    }

    if (!after && (entry->offset == 0)) {
        D("At Head");
        at_head = 1;
        // 判断上一个节点是否允许插入数据
        if (!_quicklistNodeAllowInsert(node->prev, fill, sz)) {
            D("Prev node is full too.");
            full_prev = 1;
        }
    }

    // 现在确定在哪里以及如何插入新元素
    if (!full && after) {   // 数据不满且数据往后插入的情况
        D("Not full, inserting after current position.");
        // 解密，且设置recompress为1
        quicklistDecompressNodeForUse(node);

        unsigned char *next = ziplistNext(node->zl, entry->zi);
        if (next == NULL) {
            node->zl = ziplistPush(node->zl, value, sz, ZIPLIST_TAIL);
        } else {
            node->zl = ziplistInsert(node->zl, next, value, sz);
        }
        node->count++;
        quicklistNodeUpdateSz(node);
        
        // 加密
        quicklistRecompressOnly(quicklist, node);

    } else if (!full && !after) {   // 数据不满且数据往前插入
        D("Not full, inserting before current position.");
        // 解密，且设置recompress为1
        quicklistDecompressNodeForUse(node);

        node->zl = ziplistInsert(node->zl, entry->zi, value, sz);

        node->count++;
        quicklistNodeUpdateSz(node);

        // 加密
        quicklistRecompressOnly(quicklist, node);

    } else if (full && at_tail && node->next && !full_next && after) {  // 数据满了 且 往后插入 且 node->next节点不为空 且 当前 node->next的数据未满
        /**
         * 如果是：尾部，下一个具有可用空间，并在以下位置插入：在下一个节点的头部插入条目
         */
        D("Full and tail, but next isn't full; inserting next node head");
        new_node = node->next;
        // 解密，且设置recompress为1
        quicklistDecompressNodeForUse(new_node);

        // 往下一个quicklistNode 的 ziplist头部插入数据
        new_node->zl = ziplistPush(new_node->zl, value, sz, ZIPLIST_HEAD);
        new_node->count++;

        quicklistNodeUpdateSz(new_node);

        // 加密
        quicklistRecompressOnly(quicklist, new_node);

    } else if (full && at_head && node->prev && !full_prev && !after) { // 数据满了 且 往前插入 且 node->prev 不为空 且 node-prev的数据未满

        /**
         * 如果是：头部, 上一个具有可用空间，并在以下位置插入：在上一个节点的末尾插入条目
         */
        D("Full and head, but prev isn't full, inserting prev node tail");
        new_node = node->prev;
        // 解密，且设置recompress为1
        quicklistDecompressNodeForUse(new_node);

        // 往 上一个quicklistNode 的 ziplist尾部插入数据
        new_node->zl = ziplistPush(new_node->zl, value, sz, ZIPLIST_TAIL);
        new_node->count++;
        quicklistNodeUpdateSz(new_node);

        // 加密
        quicklistRecompressOnly(quicklist, new_node);

    } else if (full && ((at_tail && node->next && full_next && after) || (at_head && node->prev && full_prev && !after))) {
        // 数据满了 且 上一个或下一个quicklistNode的数据也满了
        /**
         * 如果是：已满，并且上一个/下一个已满，则: 创建新的节点并附加到快表
         */
        D("\tprovisioning new node...");
        new_node = quicklistCreateNode();
        new_node->zl = ziplistPush(ziplistNew(), value, sz, ZIPLIST_HEAD);
        new_node->count++;
        quicklistNodeUpdateSz(new_node);
        __quicklistInsertNode(quicklist, node, new_node, after);
        
    } else if (full) {  

        /**
         * 否则，节点已满，我们需要将其拆分
         * 涵盖after 和 !after
         */
        D("\tsplitting node...");
        // 解密，且设置recompress为1
        quicklistDecompressNodeForUse(node);

        new_node = _quicklistSplitNode(node, entry->offset, after);
        new_node->zl = ziplistPush(new_node->zl, value, sz, after ? ZIPLIST_HEAD : ZIPLIST_TAIL);
        new_node->count++;

        quicklistNodeUpdateSz(new_node);
        // 把new_node 插入到 quicklist中去
        __quicklistInsertNode(quicklist, node, new_node, after);

        // 合并节点。四种方式合并
        _quicklistMergeNodes(quicklist, node);
    }

    quicklist->count++;
}

void quicklistInsertBefore(quicklist *quicklist, quicklistEntry *entry, void *value, const size_t sz) {
    _quicklistInsert(quicklist, entry, value, sz, 0);
}

void quicklistInsertAfter(quicklist *quicklist, quicklistEntry *entry, void *value, const size_t sz) {
    _quicklistInsert(quicklist, entry, value, sz, 1);
}

/**
 * 从快表中删除一系列元素
 * 元素可能跨越多个quicklistNodes, 因此我们必须谨慎跟踪起点和终点 
 * 
 * 如果删除了条目，则返回1： 如果未删除任何内容，则返回0
 */
int quicklistDelRange(quicklist *quicklist, const long start,
                      const long count) {
    if (count <= 0) {
        return 0;
    }

    // 范围包括起始位置
    unsigned long extent = count;

    if (start >= 0 && extent > (quicklist->count - start)) {
        // 如果请求删除不存在的元素，请限制列表大小
        extent = quicklist->count - start;
    } else if (start < 0 && extent > (unsigned long)(-start)) {
        // 否则，如果偏移量为负，则将最大大小限制为列表的其余部分
        extent = -start; /* c.f. LREM -29 29; just delete until end. */
    }

    quicklistEntry entry;
    if (!quicklistIndex(quicklist, start, &entry)) {
        return 0;
    }

    D("Quicklist delete request for start %ld, count %ld, extent: %ld", start, count, extent);
    quicklistNode *node = entry.node;

    // 遍历下一个节点，直到所有内容都被删除
    while (extent) {
        quicklistNode *next = node->next;

        unsigned long del;
        int delete_entire_node = 0;
        if (entry.offset == 0 && extent >= node->count) {
            /**
             * 如果要删除的节点数超过此节点的数目，则可以删除整个节点而无需 ziplist math
             */
            delete_entire_node = 1;
            del = node->count;
        } else if (entry.offset >= 0 && extent >= node->count) {
            /**
             * 如果要在此之后删除更多节点，请根据当前节点的大小来计算删除
             */
            del = node->count - entry.offset;
        } else if (entry.offset < 0) {
            /**
             * 如果offset为负，则在此循环的第一次运行中，我们将删除从 start offset 到 list 末尾整个范围
             * 
             * 由于负偏移量是到列表末尾的元素数量，因此直接将其用作删除计数 
             */
            del = -entry.offset;

            /**
             * 如果正偏移量大于剩余范围，则仅删除剩余范围，而不删除整个偏移量
             */
            if (del > extent) {
                del = extent;
            }
        } else {
            /**
             * 否则，我们删除的范围小于此节点的范围，因此请直接使用范围
             */
            del = extent;
        }

        D("[%ld]: asking to del: %ld because offset: %d; (ENTIRE NODE: %d), "
          "node count: %u",
          extent, del, entry.offset, delete_entire_node, node->count);

        if (delete_entire_node) {
            __quicklistDelNode(quicklist, node);
        } else {
            quicklistDecompressNodeForUse(node);
            node->zl = ziplistDeleteRange(node->zl, entry.offset, del);
            quicklistNodeUpdateSz(node);
            node->count -= del;
            quicklist->count -= del;
            quicklistDeleteIfEmpty(quicklist, node);
            if (node) {
                quicklistRecompressOnly(quicklist, node);
            }
        }

        extent -= del;

        node = next;

        entry.offset = 0;
    }

    return 1;
}

/* Passthrough to ziplistCompare() */
int quicklistCompare(unsigned char *p1, unsigned char *p2, int p2_len) {
    return ziplistCompare(p1, p2, p2_len);
}

/**
 * 返回快速列表迭代器“ iter”
 * 初始化后，每次对quicklistNext（）的调用都将返回快速列表的下一个元素。
 */
quicklistIter *quicklistGetIterator(const quicklist *quicklist, int direction) {

    quicklistIter *iter;

    iter = malloc(sizeof(*iter));

    if (direction == AL_START_HEAD) {
        iter->current = quicklist->head;
        iter->offset = 0;
    } else if (direction == AL_START_TAIL) {
        iter->current = quicklist->tail;
        iter->offset = -1;
    }

    iter->direction = direction;
    iter->quicklist = quicklist;

    iter->zi = NULL;

    return iter;
}


/**
 * 在特定偏移量“ idx”处初始化迭代器，并使迭代器返回“方向”方向的节点
 */
quicklistIter *quicklistGetIteratorAtIdx(const quicklist *quicklist, const int direction, const long long idx) {

    quicklistEntry entry;

    if (quicklistIndex(quicklist, idx, &entry)) {
        quicklistIter *base = quicklistGetIterator(quicklist, direction);
        base->zi = NULL;
        base->current = entry.node;
        base->offset = entry.offset;
        return base;
    } else {
        return NULL;
    }
}

/**
 * 释放迭代器
 * 如果我们仍然有一个有效的当前节点，则重新编码当前节点
 */
void quicklistReleaseIterator(quicklistIter *iter) {

    if (iter->current) {
        quicklistCompress(iter->quicklist, iter->current);
    }

    free(iter);
}

/**
 * 获取迭代器中的下一个元素
 * 注意：遍历列表时，请勿插入列表
 * 您可以*在使用quicklistDelEntry（）函数进行迭代时从列表中删除
 * 如果在迭代时插入快速列表，则添加后应重新创建迭代器
 * 
 * iter = quicklistGetIterator(quicklist,<direction>);
 * quicklistEntry entry;
 * while (quicklistNext(iter, &entry)) {
 *     if (entry.value)
 *          [[ use entry.value with entry.sz ]]     
 *      else
 *          [[ use entry.longval ]]
 * }
 * 
 * 使用此迭代的值填充“条目”
 * 迭代完成或无法迭代时返回0
 * 如果返回值为0，则“ entry”的内容无效
 */
int quicklistNext(quicklistIter *iter, quicklistEntry *entry) {

    initEntry(entry);

    if (!iter) {
        D("Returning because no iter!");
        return 0;
    }

    entry->quicklist = iter->quicklist;
    entry->node = iter->current;

    if (!iter->current) {
        D("Returning because current node is NULL");
        return 0;
    }

    unsigned char *(*nextFn)(unsigned char *, unsigned char *) = NULL;
    int offset_update = 0;

    if (!iter->zi) {
        // 如果 !zi, 则使用当前索引
        quicklistDecompressNodeForUse(iter->current);
        iter->zi = ziplistIndex(iter->current->zl, iter->offset);
    } else {
        // 否则，使用现有的迭代器偏移量并根据需要获取上一个/下一个
        if (iter->direction == AL_START_HEAD) {
            nextFn = ziplistNext;
            offset_update = 1;
        } else if (iter->direction == AL_START_TAIL) {
            nextFn = ziplistPrev;
            offset_update = -1;
        }
        iter->zi = nextFn(iter->current->zl, iter->zi);
        iter->offset += offset_update;
    }

    entry->zi = iter->zi;
    entry->offset = iter->offset;

    // 如果迭代器的zi不为NULL，从ziplist中获取值
    if (iter->zi) {
        // 从现有的ziplist位置填充值
        ziplistGet(entry->zi, &entry->value, &entry->sz, &entry->longval);
        return 1;
    } else {
        /**
         * 我们用完来 ziplist 条目
         * 选择下一个节点，更新偏移量，然后重新运行检索
         */
        quicklistCompress(iter->quicklist, iter->current);
        if (iter->direction == AL_START_HEAD) {
            // 向前遍历
            D("Jumping to start of next node");
            iter->current = iter->current->next;
            iter->offset = 0;
        } else if (iter->direction == AL_START_TAIL) {
            // 反向遍历
            D("Jumping to end of previous node");
            iter->current = iter->current->prev;
            iter->offset = -1;
        }
        iter->zi = NULL;
        return quicklistNext(iter, entry);
    }
}

/**
 * 复制快速列表
 * 成功后，将返回原始快速列表的副本
 * 
 * 无论成功还是错误，原始快速列表都不会被修改
 * 返回新分配的快速列表
 */
quicklist *quicklistDup(quicklist *orig) {

    quicklist *copy;

    copy = quicklistNew(orig->fill, orig->compress);

    for (quicklistNode *current = orig->head; current;
         current = current->next) {
        quicklistNode *node = quicklistCreateNode();

        if (current->encoding == QUICKLIST_NODE_ENCODING_LZF) {
            quicklistLZF *lzf = (quicklistLZF *)current->zl;
            size_t lzf_sz = sizeof(*lzf) + lzf->sz;
            node->zl = malloc(lzf_sz);
            memcpy(node->zl, current->zl, lzf_sz);
        } else if (current->encoding == QUICKLIST_NODE_ENCODING_RAW) {
            node->zl = malloc(current->sz);
            memcpy(node->zl, current->zl, current->sz);
        }

        node->count = current->count;
        copy->count += node->count;
        node->sz = current->sz;
        node->encoding = current->encoding;

        _quicklistInsertNodeAfter(copy, copy->tail, node);
    }

    /* copy->count must equal orig->count here */
    return copy;
}

/**
 * 使用指定的从零开始的索引处的元素填充“条目”，其中0是head，1是head旁边的元素，依此类推
 * 为了从尾数开始使用负整数，-1是最后一个元素，-2是倒数第二个，依此类推
 * 
 * 如果索引超出范围，则返回0
 * 
 * 如果找到元素，则返回1
 * 如果找不到元素，则返回0
 */
int quicklistIndex(const quicklist *quicklist, const long long idx, quicklistEntry *entry) {

    quicklistNode *n;
    unsigned long long accum = 0;
    unsigned long long index;
    int forward = idx < 0 ? 0 : 1; /* < 0 -> reverse, 0+ -> forward */

    initEntry(entry);
    entry->quicklist = quicklist;

    if (!forward) {
        index = (-idx) - 1;
        n = quicklist->tail;
    } else {
        index = idx;
        n = quicklist->head;
    }

    if (index >= quicklist->count) {
        return 0;
    }

    while (likely(n)) {
        if ((accum + n->count) > index) {
            break;
        } else {
            D("Skipping over (%p) %u at accum %lld", (void *)n, n->count, accum);
            accum += n->count;
            n = forward ? n->next : n->prev;
        }
    }

    if (!n) {
        return 0;
    }

    D("Found node: %p at accum %llu, idx %llu, sub+ %llu, sub- %llu", (void *)n, accum, index, index - accum, (-index) - 1 + accum);

    entry->node = n;
    if (forward) {
        // 前进 = 正常(forward = normal) 头到尾偏移
        entry->offset = index - accum;
    } else {
        // reverse = need, 尾部到头部的负偏移量，因此撤消原始if（index <0）的结果
        entry->offset = (-index) - 1 + accum;
    }

    // 解码， recompresss设置为1
    quicklistDecompressNodeForUse(entry->node);

    entry->zi = ziplistIndex(entry->node->zl, entry->offset);
    ziplistGet(entry->zi, &entry->value, &entry->sz, &entry->longval);
   
    /**
     * 调用者将使用我们的结果，因此我们在这里不重新压缩
     * 调用方可以根据需要重新压缩或删除该节点。
     */
    return 1;
}

// 通过将尾部元素移到头部来旋转快表
void quicklistRotate(quicklist *quicklist) {

    if (quicklist->count <= 1) {
        return;
    }

    // 首先，获取尾部条目
    unsigned char *p = ziplistIndex(quicklist->tail->zl, -1);
    unsigned char *value;
    long long longval;
    unsigned int sz;
    char longstr[32] = {0};
    ziplistGet(p, &value, &sz, &longval);

    // 如果找到的值为NULL，则使用ziplistGet填充longval代替
    // 不是字符串而是数字的情况
    if (!value) {
        /* Write the longval as a string so we can re-add it */
        sz = ll2string(longstr, sizeof(longstr), longval);
        value = (unsigned char *)longstr;
    }

    // 将尾部条目添加到头部（必须在删除尾部之前发生）
    quicklistPushHead(quicklist, value, sz);

    /**
     * 如果quicklist只有一个节点，则头ziplist也是尾部ziplist
     * 而PushHead() 可能已重新分配了我们的单个ziplist
     * 这将使我们先前存在的'p'无法使用。
     */
    if (quicklist->len == 1) {
        p = ziplistIndex(quicklist->tail->zl, -1);
    }

    // 删除 taiil entry
    quicklistDelIndex(quicklist, quicklist->tail, &p);
}

/**
 * 从快表中弹出并在 “data” ptr中返回结果
 * 如果数据不是数字，则'data'的值是'saver'函数指针的返回值
 * 
 * 如果quicklist元素 是 long long 类型，则返回值以'sval'返回
 * 
 * 返回值0表示没有可用元素
 * 返回值1表示检查“data”和 “sval” 的值
 * 如果设置了“data”，则使用“data”和 “sz” ,否则，请使用“ sval”。
 */
int quicklistPopCustom(quicklist *quicklist, int where, unsigned char **data, unsigned int *sz, long long *sval,
                       void *(*saver)(unsigned char *data, unsigned int sz)) {

    unsigned char *p;
    unsigned char *vstr;
    unsigned int vlen;
    long long vlong;
    int pos = (where == QUICKLIST_HEAD) ? 0 : -1;

    if (quicklist->count == 0) {
        return 0;
    }

    if (data) {
        *data = NULL;
    }

    if (sz) {
        *sz = 0;
    }

    if (sval) {
        *sval = -123456789;
    }

    quicklistNode *node;
    
    if (where == QUICKLIST_HEAD && quicklist->head) {
        node = quicklist->head;
    } else if (where == QUICKLIST_TAIL && quicklist->tail) {
        node = quicklist->tail;
    } else {
        return 0;
    }

    p = ziplistIndex(node->zl, pos);

    if (ziplistGet(p, &vstr, &vlen, &vlong)) {
        if (vstr) {
            if (data) {
                *data = saver(vstr, vlen);
            }

            if (sz) {
                *sz = vlen;
            }

        } else {
            if (data) {
                *data = NULL;                
            }

            if (sval) {
                *sval = vlong;
            }
        }

        quicklistDelIndex(quicklist, node, &p);
        return 1;
    }
    return 0;
}

// 返回传入的数据的malloc副本
REDIS_STATIC void *_quicklistSaver(unsigned char *data, unsigned int sz) {

    unsigned char *vstr;
    if (data) {
        vstr = malloc(sz);
        memcpy(vstr, data, sz);
        return vstr;
    }
    return NULL;
}


/**
 * 默认弹出功能
 * 从快速列表返回分配的值
 */
int quicklistPop(quicklist *quicklist, int where, unsigned char **data, unsigned int *sz, long long *slong) {

    unsigned char *vstr;
    unsigned int vlen;
    long long vlong;
    if (quicklist->count == 0) {
        return 0;
    }
    int ret = quicklistPopCustom(quicklist, where, &vstr, &vlen, &vlong, _quicklistSaver);

    if (data) {
        *data = vstr;
    }

    if (slong) {
        *slong = vlong;
    }

    if (sz) {
        *sz = vlen;
    }

    return ret;
}


/**
 * 包装程序允许在 HEAD/TAIL pop 之间进行基于参数的切换
 */
void quicklistPush(quicklist *quicklist, void *value, const size_t sz, int where) {

    if (where == QUICKLIST_HEAD) {
        quicklistPushHead(quicklist, value, sz);
    } else if (where == QUICKLIST_TAIL) {
        quicklistPushTail(quicklist, value, sz);
    }
}


