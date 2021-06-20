#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "demo_zskiplist_2.h"
#include "demo_sds_2.h"

struct sharedObjectsStruct shared;

void createSharedObjects(void) {

    shared.minstring = sdsnew("minstring");
    shared.maxstring = sdsnew("maxstring");
}

/**
 *  创建并返回一个跳跃表节点
 */
zskiplistNode *zslCreateNode(int level, double score, sds ele) {

    // 分配器
    zskiplistNode *zn = malloc(sizeof(*zn) + level * sizeof(struct zskiplistLevel));

    // 点数
    zn->score = score;

    // 对象
    zn->ele = ele;

    return zn;
}

// 创建一个跳跃表
zskiplist *zslCreate(void) {

    int j;
    zskiplist *zsl;

    zsl = malloc(sizeof(*zsl));

    zsl->level = 1;
    zsl->length = 0;

    // 初始化头节点, O(1)
    zsl->header = zslCreateNode(ZSKIPLIST_MAXLEVEL, 0, NULL);

    // 初始化层指针, O(1)
    for (j = 0; j < ZSKIPLIST_MAXLEVEL; j++) {
        zsl->header->level[j].forward = NULL;
        zsl->header->level[j].span = 0;
    }

    zsl->header->backward = NULL;

    zsl->tail = NULL;

    return zsl;
}

// 释放跳跃表
void zslFreeNode(zskiplistNode *node) {

    sdsfree(node->ele);
    free(node);
}

// 释放整个跳跃表
void zslFree(zskiplist *zsl) {

    if (zsl == NULL || zsl->header == NULL) {
        return ;
    }

    // 遍历删除, O(N)
    zskiplistNode *node = zsl->header->level[0].forward, *next;
    free(zsl->header);

    while (node != NULL) {
        next = node->level[0].forward;
        zslFreeNode(node);
        node = next;
    }

    free(zsl);
}

/**
 * 返回一个介于1和ZSKIPLIST_MAXLEVEL 之间的随机值，作为节点的层数
 * 
 * 根据幂次定律(power law), 数值越大，函数生成它的几率越小
 */
int zslRandomLevel(void) {

    int level = 1;
    while ((random() & 0xFFFF) < (ZSKIPLIST_P * 0xFFFF)) {
        level += 1;
    }

    return (level < ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
}

/**
 * 将包含给定 score的对象 obj 添加到 skiplist 里
 * T_worst = O(N), T_average = O(log N)
 */
zskiplistNode *zslInsert(zskiplist *zsl, double score, sds ele) {

    // 记录寻找元素过程中，每层能到达的最右节点
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;

    // 记录寻找元素过程中，每层所跨越的节点数
    unsigned int rank[ZSKIPLIST_MAXLEVEL];

    int i, level;

    assert(!isnan(score));

    x = zsl->header;
    /**
     * 记录沿途访问的节点，并计数span等属性
     * 平均 O(logN)， 最快O(N)
     */
    for (i = zsl->level - 1; i >= 0; i--) {
        rank[i] = i == (zsl->level - 1) ? 0 : rank[i + 1]; 

        // 右节点不为空
        while (x->level[i].forward && 
            // 右节点的 score 比给定 score小
            (x->level[i].forward->score < score || 
                // 右节点的score相同，但节点的member比输入member 要小
                (x->level[i].forward->score == score && 
                sdscmp(x->level[i].forward->ele, ele) < 0))) {
            
            // 记录跨越多少个元素
            rank[i] += x->level[i].span;
            // 继续向右前进
            x = x->level[i].forward;
        }

        // 保存访问节点
        update[i] = x;
    }

    /**
     * 因为这个函数不可能处理两个元素的member和score都相同的情况
     * 所以直接创建新节点，不用检查存在性
     */

    // 计算新的随机层数
    level = zslRandomLevel();
    printf("level:%d, %d\n", zsl->level, level);

    /**
     * 如果level比当前 skiplist 的最大层数还要大
     * 那么更新 zsl->level 参数
     * 并且初始化update 和 rank参数相应的层的数据
     */
    if (level > zsl->level) {
        for (i = zsl->level; i < level; i++) {
            rank[i] = 0;
            update[i] = zsl->header;
            update[i]->level[i].span = zsl->length;
        }
        zsl->level = level;
    }

    // 创建新节点
    x = zslCreateNode(level, score, ele);
    assert(x != NULL);
    // 根据update和rank 两个数组的资料，初始化新节点
    // 并设置相应的指针 O(N)
    for (i = 0; i < level; i++) {
        // 设置指针
        x->level[i].forward = update[i]->level[i].forward;
        update[i]->level[i].forward = x;

        // 设置 span
        x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);
        update[i]->level[i].span = (rank[0] - rank[i]) + 1;
    }

    // 更新沿途访问节点的 span值
    for (i = level; i < zsl->level; i++) {
        update[i]->level[i].span++;
    }

    // 设置后退指针
    x->backward = (update[0] == zsl->header) ? NULL : update[0];
    // 设置 x 的前进指针
    if (x->level[0].forward) {
        x->level[0].forward->backward = x;
    } else {
        // 这个是新的表尾节点
        zsl->tail = x;
    }

    // 更新跳跃表节点数量
    zsl->length++;

    return x;
}

void zslDump(zskiplist *zsl) {

    assert(zsl != NULL);

    printf("zsl level:%d, length:%d\n", zsl->level, zsl->length);

    int i;
    zskiplistNode *tmp;

    for (i = zsl->level - 1; i >= 0; i--) {
        tmp = zsl->header->level[i].forward;
        printf("\n level[%d]: ", i);
        while (tmp != NULL) {
            printf("score:%0.2lf, val:%s, span:%d  ", tmp->score, tmp->ele, tmp->level[i].span);
            tmp = tmp->level[i].forward;
        }
    }
    printf("\n -------------- \n");
}

// 节点删除函数
void zslDeleteNode(zskiplist *zsl, zskiplistNode *x, zskiplistNode **update) {

    int i;
    
    // 修改相应的指针和 span， O(N)
    for (i = 0; i < zsl->level; i++) {
        if (update[i]->level[i].forward == x) {
            update[i]->level[i].span += x->level[i].span - 1;
            update[i]->level[i].forward = x->level[i].forward;
        } else {
            update[i]->level[i].span -= 1;
        }
    }

    // 处理表头和表尾节点
    if (x->level[0].forward) {
        x->level[0].forward->backward = x->backward;
    } else {
        zsl->tail = x->backward;
    }

    // 收缩 level 的值, O(N)
    while (zsl->level > 1 && zsl->header->level[zsl->level - 1].forward == NULL) {
        zsl->level--;
    }
    zsl->length--;
}

/**
 *  从skiplist 中删除和给定obj以及score匹配元素
 */
int zslDelete(zskiplist *zsl, double score, sds ele, zskiplistNode **node) {

    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    int i;

    x = zsl->header;

    // 遍历所有层，记录删除节点需要被修改的节点到 update 数组
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward &&
            (x->level[i].forward->score < score ||
                (x->level[i].forward->score == score && 
                sdscmp(x->level[i].forward->ele, ele) < 0))) {
            
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    /**
     * 因为多个不同的member可能有相同的score
     * 所以要确保x的member和score都匹配时，才进行删除
     */
    x = x->level[0].forward;
    if (x && score == x->score && sdscmp(x->ele, ele) == 0) {
        zslDeleteNode(zsl, x, update);
        if (!node) {
            zslFreeNode(x);
        } else {
            *node = x;
        }
        return 1;
    } else {
        return 0;   // not found
    }
    return 0; // not found
}

zskiplistNode *zslUpdateScore(zskiplist *zsl, double curscore, sds ele, double newscore) {

    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    int i;

    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward && 
            (x->level[i].forward->score < curscore ||
            (x->level[i].forward->score == curscore && 
            sdscmp(x->level[i].forward->ele, ele) < 0))) {
                x = x->level[i].forward;
        }
        update[i] = x;    
    }

    x = x->level[0].forward;
    assert(x && curscore == x->score && sdscmp(x->ele, ele) == 0);

    /**
     * 如果更新后的score位置不变，直接替换值
     */
    if ((x->backward == NULL || x->backward->score < newscore) && 
        (x->level[0].forward == NULL || x->level[0].forward->score > newscore)){
            x->score = newscore;
            return x;
    }

    // score位置变化了，应该移除并重新插入
    zslDeleteNode(zsl, x, update);
    zskiplistNode *newnode = zslInsert(zsl, newscore, x->ele);

    x->ele = NULL;
    zslFreeNode(x);
    return newnode;
}

// 检查 value是否属于 spec 指定的范围内
static int zslValueGteMin(double value, zrangespec *spec) {

    return spec->minex ? (value > spec->min) : (value >= spec->min);
}

// 检查value是否属于 spce指定的范围内
static int zslValueLteMax(double value, zrangespec *spec) {

    return spec->maxex ? (value < spec->max) : (value <= spec->max);
}

/**
 * 检查zset中的元素是否在给定范围之内
 */
int zslIsInRange(zskiplist *zsl, zrangespec *range) {

    zskiplistNode *x;

    if (range->min > range->max || (range->min == range->max && (range->minex || range->maxex))) {
        return 0;
    }

    /**
     * 如果zset 的最大节点的 score比范围的最小值要小
     * 那么zset 不在范围内
     */
    x = zsl->tail;
    if (x == NULL || !zslValueGteMin(x->score, range)) {
        return 0;
    }

    /**
     * 如果zset的最小节点的score比范围的最大值要大
     * 那么zset不在范围内
     */
    x = zsl->header->level[0].forward;
    if (x == NULL || !zslValueLteMax(x->score, range)) {
        return 0;
    }

    return 1;   // 在范围内
}

// 找到跳跃表中第一个符合给定范围的元素
zskiplistNode *zslFirstInRange(zskiplist *zsl, zrangespec *range) {

    zskiplistNode *x;
    int i;

    // 如果超过范围，返回NULL
    if (!zslIsInRange(zsl, range)) return NULL;

    // 找到第一个score值大于给定范围最小值的节点 O(N)
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward && 
            !zslValueGteMin(x->level[i].forward->score, range)) {
                x = x->level[i].forward;
        }
    }

    x = x->level[0].forward;
    assert(x != NULL);

    // 检查score值是否小于 max范围
    if (!zslValueLteMax(x->score, range)) return NULL;
    return x;
}

// 找到跳跃表中最后一个符合给定范围的元素
zskiplistNode *zslLastInRange(zskiplist *zsl, zrangespec *range) {

    zskiplistNode *x;
    int i;

    if (!zslIsInRange(zsl, range)) return NULL;

    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward && zslValueLteMax(x->level[i].forward->score, range)) {
            x = x->level[i].forward;
        }
    }

    assert(x != NULL);

    if (!zslValueGteMin(x->score, range)) return NULL;
    return x;
}


// 删除给定范围内的score的元素
unsigned long zslDeleteRangeByScore(zskiplist *zsl, zrangespec *range) {

    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    unsigned long removed = 0;
    int i;

    // 记录沿途的节点
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward && (range->minex ?
            x->level[i].forward->score <= range->min :
            x->level[i].forward->score < range->min)) {
                x = x->level[i].forward;
        }
        update[i] = x;
    }

    // Current node is the last with score < or <= min.
    x = x->level[0].forward;

    // 一直向右删除，直接到达 range 的底为止。 O(N ^ 2)
    while (x && (range->maxex ? x->score < range->max : x->score <= range->max)) {
        // 保存后继指针
        zskiplistNode *next = x->level[0].forward;
        // 在跳跃表中删除 O(N)
        zslDeleteNode(zsl, x, update);
        // 释放
        zslFreeNode(x);

        removed++;

        x = next;
    }

    return removed;
}

// 删除给定排序范围内的所有节点
unsigned long zslDeleteRangeByRank(zskiplist *zsl, unsigned int start, unsigned int end) {

    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    unsigned long traversed = 0, removed = 0;
    int i;

    // 统计计算rank， 移动删除开始的地方
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward && (traversed + x->level[i].span) < start) {
            traversed += x->level[i].span;
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    // 算上start节点
    traversed++;

    /**
     * 从start开始，删除直到到达索引end，或者末尾
     * O(N ^ 2)
     */
    x = x->level[0].forward;
    while (x && traversed <= end) {
        // 保存后一节点的指针
        zskiplistNode *next = x->level[0].forward;
        // 删除skiplist 节点, O(N)
        zslDeleteNode(zsl, x, update);
        // 删除节点
        zslFreeNode(x);
        // 删除计数
        removed++;
        traversed++;
        x = next;
    }

    return removed;
}

/**
 * 返回目标元素在有序集合中的rank
 * 如果元素不存在与有序集合，那么返回 0
 */
unsigned long zslGetRank(zskiplist *zsl, double score, sds ele) {

    zskiplistNode *x;
    unsigned long rank = 0;
    int i;

    x = zsl->header;
    /**
     * 遍历zskiplist,并累积沿途的span到rank，找到目标元素时返回 rank
     * O(N)
     */
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward && (x->level[i].forward->score < score || 
            (x->level[i].forward->score == score && sdscmp(x->level[i].forward->ele, ele) <= 0))) {
            // 累积
            rank += x->level[i].span;    
            // 前进
            x = x->level[i].forward;
        }

        // 找到目标元素
        if (x->ele && sdscmp(x->ele, ele) == 0) {
            return rank;
        }
    }

    return 0;
}

/**
 * 根据给定的 rank 查找元素
 */
zskiplistNode *zslGetElementByRank(zskiplist *zsl, unsigned long rank) {

    zskiplistNode *x;
    unsigned long traversed = 0;
    int i;

    // 沿着指针前进，直到累积的步数 traversed 等于 rank为止 O(N)
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward && (traversed + x->level[i].span) <= rank) {
            traversed += x->level[i].span;
            x = x->level[i].forward;
        }

        if (traversed == rank) {
            return x;
        }
    }

    // 没有找到
    return NULL;
}

int sdscmplex(sds a, sds b) {

    if (a == b) return 0;
    if (a == shared.minstring || b == shared.maxstring) return -1;
    if (a == shared.maxstring || b == shared.minstring) return 1;
    int ret =  sdscmp(a, b);
    return ret;
}

int zslLexValueGteMin(sds value, zlexrangespec *spec) {

    return spec->minex ?
        (sdscmplex(value, spec->min) > 0) :
        (sdscmplex(value, spec->min) >= 0);
}

int zslLexValueLteMax(sds value, zlexrangespec *spec) {

    return spec->maxex ? 
        (sdscmplex(value, spec->max) < 0):
        (sdscmplex(value, spec->max) <= 0);
}

unsigned long zslDeleteRangeByLex(zskiplist *zsl, zlexrangespec *range) {

    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    unsigned long removed = 0;
    int i;

    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward &&
            !zslLexValueGteMin(x->level[i].forward->ele, range)) {
                x = x->level[i].forward;
        }
        update[i] = x;
    }

    x = x->level[0].forward;

    while (x && zslLexValueLteMax(x->ele, range)) {
        zskiplistNode *next = x->level[0].forward;
        zslDeleteNode(zsl, x, update);
        zslFreeNode(x);
        removed++;
        x = next;
    }

    return removed;
}

void zslFreeLexRange(zlexrangespec *spec) {

    if (spec->min != shared.minstring &&
        spec->min != shared.maxstring) sdsfree(spec->min);
    
    if (spec->max != shared.minstring &&
        spec->max != shared.maxstring) sdsfree(spec->max);
}

int zslIsInLexRange(zskiplist *zsl, zlexrangespec *range) {

    zskiplistNode *x;

    int cmp = sdscmplex(range->min, range->max);
    if (cmp > 0 || (cmp == 0 && (range->minex || range->maxex))) {
        return 0;
    }
    x = zsl->tail;
    if (x == NULL || !zslLexValueGteMin(x->ele, range)) {
        return 0;
    }

    x = zsl->header->level[0].forward;
    if (x == NULL || !zslLexValueLteMax(x->ele, range)) {
        return 0;
    }
    return 1;
}

zskiplistNode *zslFirstInLexRange(zskiplist *zsl, zlexrangespec *range) {

    zskiplistNode *x;
    int i;

    if (!zslIsInLexRange(zsl, range)) return NULL;

    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward &&
            !zslLexValueGteMin(x->level[i].forward->ele, range)) {
            x = x->level[i].forward;
        }
    }

    x = x->level[0].forward;
    assert(x != NULL);

    if (!zslLexValueLteMax(x->ele, range)) return NULL;

    return x;
}

zskiplistNode *zslLastInLexRange(zskiplist *zsl, zlexrangespec *range) {

    zskiplistNode *x;
    int i;

    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward &&
            zslLexValueLteMax(x->level[i].forward->ele, range)) {
                x = x->level[i].forward;
        }
    }

    assert(x != NULL);

    if (!zslLexValueGteMin(x->ele, range)) return NULL;

    return x;
}

