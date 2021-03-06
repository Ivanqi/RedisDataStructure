#ifndef __ZSKIPLIST_1_H
#define __ZSKIPLIST_1_H

#define ZSKIPLIST_MAXLEVEL 32
#define REDIS_ENCODING_SKIPLIST 7
#define ZSKIPLIST_P 0.25
#define REDIS_ENCODING_RAW 0    // 简单动态字符串
#define REDIS_ENCODING_INT 1    // long 类型的整数

/*
 * 对象类型
 */
#define REDIS_STRING 0  // 字符串对象
#define REDIS_LIST 1    // 列表对象
#define REDIS_SET 2     // 集合对象
#define REDIS_ZSET 3    // 有序集合对象
#define REDIS_HASH 4    // 哈希对象

#define REDIS_ERR -1
#define REDIS_OK 0


// Redis对象
typedef struct redisObject {
    // 类型
    unsigned type:4;

    // 不使用(对齐位)
    unsigned notused:2;

    // 编码方式
    unsigned encoding:4;

    // LRU 时间(相对于 server.lrulock)
    unsigned lru:22;

    // 引用计数
    int refcount;

    // 指向对象的值
    void *ptr;
} robj;

// 跳跃表节点
typedef struct zskiplistNode {
    // member 对象
    robj *obj;
    
    // 分值
    double score;

    // 后退指针
    struct zskiplistNode *backward;

    // 层
    struct zskiplistLevel {
        // 前进指针
        struct zskiplistNode *forward;

        // 这个层跨越的节点数量
        unsigned int span;
    } level[];
} zskiplistNode;

// 跳跃表
typedef struct zskiplist {
    // 头节点，尾节点
    struct zskiplistNode *header, *tail;

    // 节点数量
    unsigned long length;

    // 目前表内节点的最大层数
    int level;
} zskiplist;

// 用于保存范围值的结构
typedef struct {
    double min, max;
    // min 和 max
    int minex, maxex;   // are min or max exclusive
} zrangespec;

zskiplistNode *zslCreateNode(int level, double score, robj *obj);

// 创建一个新的跳跃表, O(1)
zskiplist *zslCreate(void);

void zslFreeNode(zskiplistNode *node);

// 释放给定跳跃表，以及表中包含的所有节点, O(N)， N为跳跃表长度
void zslFree(zskiplist *zsl);

int zslRandomLevel(void);

// 将包含给定成员和分值的新节点添加到跳跃表中。平均O(logN)，最坏O(N)， N为跳跃表长度
zskiplistNode *zslInsert(zskiplist *zsl, double score, robj *obj);

void zslDump(zskiplist *zsl);

void zslDeleteNode(zskiplist *zsl, zskiplistNode *x, zskiplistNode **update);

// 删除跳跃表中包含给定成员和分值的节点。平均O(logN),最坏O(N)， N为跳跃表长度
int zslDelete(zskiplist *zsl, double score, robj *obj);

static int zslValueGteMin(double value, zrangespec *spec);

static int zslValueLteMax(double value, zrangespec *spec);

int zslIsInRange(zskiplist *zsl, zrangespec *range);

zskiplistNode *zslFirstInRange(zskiplist *zsl, zrangespec range);

zskiplistNode *zslLastInRange(zskiplist *zsl, zrangespec range);

unsigned long zslDeleteRangeByScore(zskiplist *zsl, zrangespec range);

unsigned long zslDeleteRangeByRank(zskiplist *zsl, unsigned int start, unsigned int end);

unsigned long zslGetRank(zskiplist *zsl, double score, robj *o);

zskiplistNode *zslGetElementByRank(zskiplist *zsl, unsigned long rank);



#endif