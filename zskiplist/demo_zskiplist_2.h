#ifndef __ZSKIPLIST_2_H
#define __ZSKIPLIST_2_H

#include "demo_sds_2.h"

#define ZSKIPLIST_MAXLEVEL 32
#define ZSKIPLIST_P 0.25

typedef struct zskiplistNode {
    sds ele;
    double score;                       // 分值
    struct zskiplistNode *backward;     // 后退指针. backward字段是指向链表前一个节点的指针（前向指针）,节点只有1个前向指针，所以只有第1层链表是一个双向链表

    struct zskiplistLevel {
        struct zskiplistNode *forward;  // 前进指针
        unsigned long span;             // 这个层跨越的节点数量
    } level[];
} zskiplistNode;

typedef struct zskiplist {
    struct zskiplistNode *header, *tail;    // 头节点，尾节点
    unsigned long length;                   // 节点数量
    int level;                              // 目前表内节点的最大层数
} zskiplist;

// 用于保存范围值的结构
typedef struct {
    double min, max;
    int minex, maxex;
} zrangespec;

typedef struct {
    sds min, max;
    int minex, maxex;
} zlexrangespec;

struct sharedObjectsStruct {
    sds minstring, maxstring;
};

void createSharedObjects(void);

zskiplistNode *zslCreateNode(int level, double score, sds ele);

zskiplist *zslCreate(void);

void zslFree(zskiplist *zsl);

int zslRandomLevel(void);

zskiplistNode *zslInsert(zskiplist *zsl, double score, sds ele);

void zslDump(zskiplist *zsl);

void zslDeleteNode(zskiplist *zsl, zskiplistNode *x, zskiplistNode **update);

int zslDelete(zskiplist *zsl, double score, sds ele, zskiplistNode **node);

zskiplistNode *zslUpdateScore(zskiplist *zsl, double curscore, sds ele, double newscore);

static int zslValueGteMin(double value, zrangespec *spec);

static int zslValueLteMax(double value, zrangespec *spec);

int zslIsInRange(zskiplist *zsl, zrangespec *range);

zskiplistNode *zslFirstInRange(zskiplist *zsl, zrangespec *range);

zskiplistNode *zslLastInRange(zskiplist *zsl, zrangespec *range);

unsigned long zslDeleteRangeByScore(zskiplist *zsl, zrangespec *range);

unsigned long zslDeleteRangeByRank(zskiplist *zsl, unsigned int start, unsigned int end);

unsigned long zslGetRank(zskiplist *zsl, double score, sds ele);

zskiplistNode *zslGetElementByRank(zskiplist *zsl, unsigned long rank);

int sdscmplex(sds a, sds b);

int zslLexValueGteMin(sds value, zlexrangespec *spec);

int zslLexValueLteMax(sds value, zlexrangespec *spec);

unsigned long zslDeleteRangeByLex(zskiplist *zsl, zlexrangespec *range);

void zslFreeLexRange(zlexrangespec *spec);

int zslIsInLexRange(zskiplist *zsl, zlexrangespec *range);

zskiplistNode *zslFirstInLexRange(zskiplist *zsl, zlexrangespec *range);

zskiplistNode *zslLastInLexRange(zskiplist *zsl, zlexrangespec *range);

#endif