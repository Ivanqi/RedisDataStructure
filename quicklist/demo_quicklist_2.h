#ifndef __QUICKLIST_2_H__
#define __QUICKLIST_2_H__
#include <stdio.h>

/**
 * quicklistNode是一个32字节的结构，用于描述快速列表的ziplist
 * 我们使用位字段将quicklistNode保持在32字节
 * prev: 指向链表前一个节点的指针
 * 
 * next: 指向链表后一个节点的指针
 * 
 * zl: 数据指针。如果当前节点的数据没有压缩，那么它指向一个ziplist结构；否则，它指向一个quicklistZF结构
 * 
 * sz: 表示zl指向的ziplist的总大小(包括 zlbytes, zltail, zllen, zlend和各个数据项)
 *  需要注意的是：如果ziplist被压缩了，那么这个sz的值仍然是压缩前的ziplist大小
 * 
 * count: 表示ziplist里面包含的数据项个数。16位，最大65536（最大zl字节为65k，因此最大计数实际上 < 32k）
 * 
 * encoding: 表示ziplist是否压缩了（以及用了哪个压缩算法，2位，RAW = 1，没有压缩, LZF = 2，使用LZF算法压缩
 * 
 * container: 是一个预留字段，2位，NONE = 1, ZIPLIST = 2
 *  本来设计是用来表明一个quicklist节点下面是直接存数据，还是使用ziplist存数据，或者用其它的结构来存数据（用作一个数据容器，所以叫container）
 *  但是，在目前的实现中，这个值是一个固定的值2，表示使用ziplist作为数据容器
 * 
 * recompress: 1位，bool值，如果节点临时解压缩以供使用，则为true
 *  当我们使用类似lindex这样的命令查看了某一项本来压缩的数据时，需要把数据暂时解压
 *  这时就设置recompress=1做一个标记，等有机会再把数据重新压缩
 * 
 * attempted_compress：1位 ,bool值，用于测试期间的验证
 *  这个值只对Redis的自动化测试程序有用
 * 
 * extra：10位，免费供将来使用；将剩下的32位补出来
 *  其它扩展字段
 */
typedef struct quicklistNode {
    struct quicklistNode *prev;
    struct quicklistNode *next;
    unsigned char *zl;
    unsigned int sz;
    unsigned int count: 16;
    unsigned int encoding: 2;
    unsigned int container: 2;
    unsigned int recompress: 1;
    unsigned int attempted_compress: 1;
    unsigned int extra: 10;
} quicklistNode;

/**
 * quicklistLZF是一个4+N字节的结构，包含“sz”和“compressed”
 * sz：compressed字段的字节长度
 * compressed: 总长度为“sz”的LZF数据
 *  柔性数组（flexible array member），存放压缩后的ziplist字节数组  
 * 注意
 *  未压缩的长度存储在quicklistNode->sz中
 *  当quicklistNode->zl被压缩时，node->zl指向一个quicklistLZF
 */
typedef struct quicklistLZF {
    unsigned int sz;
    char compressed[];
} quicklistLZF;

/**
 * quicklist是一个40字节的结构（在64位系统上）
 * head: 指向头节点的指针
 * tail: 指向尾节点的指针
 * count: 所有ziplist数据项的个数总和
 * len: quicklist 节点的个数
 * compress：16bit, ziplist大小设置，存放list-max-ziplist-size参数的值
 *  值为-1，如果禁用压缩，则为在快速列表末尾保留未压缩的quickListNode数
 * fill: 16bit,ziplist大小设置，存放list-compress-depth参数的值
 *  是用户请求的（或默认）填充因子
 */
typedef struct quicklist {
    quicklistNode *head;
    quicklistNode *tail;
    unsigned long count;
    unsigned long len;
    int fill: 16;
    unsigned int compress: 16; /* depth of end nodes not to compress;0=off */
} quicklist;

typedef struct quicklistIter {
    const quicklist *quicklist;
    quicklistNode *current;
    unsigned char *zi;  // 存储通过 ziplistIndex() 获取的ziplist的值
    long offset;
    int direction;
} quicklistIter;

typedef struct quicklistEntry {
    const quicklist *quicklist;
    quicklistNode *node;
    unsigned char *zi;  // 存储通过 ziplistIndex() 获取的ziplist的值
    unsigned char *value;
    long long longval;
    unsigned int sz;
    int offset;
} quicklistEntry;

#define QUICKLIST_HEAD 0
#define QUICKLIST_TAIL -1

#define QUICKLIST_NODE_ENCODING_RAW 1
#define QUICKLIST_NODE_ENCODING_LZF 2

#define QUICKLIST_NOCOMPRESS 0

#define QUICKLIST_NODE_CONTAINER_NONE 1
#define QUICKLIST_NODE_CONTAINER_ZIPLIST 2

#define quicklistNodeIsCompressed(node) ((node)->encoding == QUICKLIST_NODE_ENCODING_LZF)

#ifndef REDIS_STATIC
#define REDIS_STATIC static
#endif

// 优化级别
static const size_t optimization_level[] = {4096, 8192, 16384, 32768, 65535};

// 任何多元素ziplist的最大字节大小
#define SIZE_SAFETY_LIMIT 8192

// 尝试压缩的最小ziplist大小（字节）
#define MIN_COMPRESS_BYTES 48

/**
 * 存储压缩quicklistNode数据的最小字节大小
 * 这也防止我们存储压缩如果压缩
 * 导致比原始数据更大的大小
 */
#define MIN_COMPRESS_IMPROVE 8

/* If not verbose testing, remove all debug printing. */
#ifndef REDIS_TEST_VERBOSE
#define D(...)
#else
#define D(...)                                                                 \
    do {                                                                       \
        printf("%s:%s:%d:\t", __FILE__, __FUNCTION__, __LINE__);               \
        printf(__VA_ARGS__);                                                   \
        printf("\n");                                                          \
    } while (0);
#endif

// quicklistEntry构造方法
#define initEntry(e)                                                           \
    do {                                                                       \
        (e)->zi = (e)->value = NULL;                                           \
        (e)->longval = -123456789;                                             \
        (e)->quicklist = NULL;                                                 \
        (e)->node = NULL;                                                      \
        (e)->offset = 123456789;                                               \
        (e)->sz = 0;                                                           \
    } while (0)

#if __GNUC__ >= 3   // GCC3.0以上
// __builtin_expect(!!(x), 1)  这个是说，如果x == 0，那么结果就是 0
// 如果x == 1， 那么结果就是1，使用了！！是为了让x转化成bool型的
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#define COMPRESS_MAX (1 << 16)

#define FILE_MAX (1 << 15)

#define quicklistAllowsCompression(_ql) ((_ql)->compress != 0)

// // 接口
quicklist *quicklistCreate(void);

quicklist *quicklistNew(int fill, int compress);

void quicklistSetCompressDepth(quicklist *quicklist, int depth);

void quicklistSetFill(quicklist *quicklist, int fill);

void quicklistSetOptions(quicklist *quicklist, int fill, int depth);

void quicklistRelease(quicklist *quicklist);

int quicklistPushHead(quicklist *quicklist, void *value, const size_t sz);

int quicklistPushTail(quicklist *quicklist, void *value, const size_t sz);

void quicklistPush(quicklist *quicklist, void *value, const size_t sz, int where);

void quicklistAppendZiplist(quicklist *quicklist, unsigned char *zl);

quicklist *quicklistAppendValuesFromZiplist(quicklist *quicklist, unsigned char *zl);

quicklist *quicklistCreateFromZiplist(int fill, int compress, unsigned char *zl);

void quicklistInsertAfter(quicklist *quicklist, quicklistEntry *node, void *value, const size_t sz);

void quicklistInsertBefore(quicklist *quicklist, quicklistEntry *node, void *value, const size_t sz);

void quicklistDelEntry(quicklistIter *iter, quicklistEntry *entry);

int quicklistReplaceAtIndex(quicklist *quicklist, long index, void *data,int sz);

int quicklistDelRange(quicklist *quicklist, const long start, const long stop);

quicklistIter *quicklistGetIterator(const quicklist *quicklist, int direction);

quicklistIter *quicklistGetIteratorAtIdx(const quicklist *quicklist, int direction, const long long idx);

int quicklistNext(quicklistIter *iter, quicklistEntry *node);

void quicklistReleaseIterator(quicklistIter *iter);

quicklist *quicklistDup(quicklist *orig);

int quicklistIndex(const quicklist *quicklist, const long long index, quicklistEntry *entry);

void quicklistRewind(quicklist *quicklist, quicklistIter *li);

void quicklistRewindTail(quicklist *quicklist, quicklistIter *li);

void quicklistRotate(quicklist *quicklist);

int quicklistPopCustom(quicklist *quicklist, int where, unsigned char **data, unsigned int *sz, long long *sval, void *(*saver)(unsigned char *data, unsigned int sz));

int quicklistPop(quicklist *quicklist, int where, unsigned char **data, unsigned int *sz, long long *slong);

unsigned long quicklistCount(const quicklist *ql);

int quicklistCompare(unsigned char *p1, unsigned char *p2, int p2_len);

size_t quicklistGetLzf(const quicklistNode *node, void **data);

#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif