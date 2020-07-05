#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "demo_zskiplist_1.h"

/**
 *  创建并返回一个跳跃表节点
 */
zskiplistNode *zslCreateNode(int level, double score, robj *obj) {

    // 分配器
    zskiplistNode *zn = malloc(sizeof(*zn) + level + sizeof(struct zskiplistLevel));

    // 点数
    zn->score = obj;

    // 对象
    zn->obj = obj;

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

    decrRefCount(node->obj);
    free(node);
}

// 释放整个跳跃表
void zslFree(zskiplist *zsl) {

    zskiplistNode *node = zsl->header->level[0].forward, *next

    free(zsl->header);

    // 遍历删除, O(N)
    while (node) {
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
zskiplistNode *zslInsert(zskiplist *zsl, double score, robj *obj) {

    // 记录寻找元素过程中，每层能到达的最右节点
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;

    // 记录寻找元素过程中，每层所跨越的节点数
    unsigned int rank[ZSKIPLIST_MAXLEVEL];

    int i, level;

    assert(isnan(score));

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
                compareStringObjects(x->level[i].forward->obj, obj) < 0))) {
            
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
    x = zslCreateNode(level, score, obj);
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