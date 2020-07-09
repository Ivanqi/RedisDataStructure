#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "demo_zskiplist_1.h"

robj *createRobj(char *string) {

    robj *o = (robj *)malloc(sizeof(robj));
    
    o->type = REDIS_STRING;
    o->encoding = REDIS_ENCODING_RAW;
    o->ptr = string;
    o->refcount = 1;

    assert(o != NULL);

    return o;
}

zrangespec createRangeObj(double min, double max) {

    zrangespec range;
    range.min = min;
    range.max = max;
    range.minex = range.maxex = 1;

    return range;
}

void test_case_1() {

    zskiplist *zsk = zslCreate();

    robj *o1 = createRobj("one");
    robj *o2 = createRobj("two");
    robj *o3 = createRobj("three");
    robj *o4 = createRobj("four");
    robj *o5 = createRobj("five");
    robj *o6 = createRobj("six");
    robj *o7 = createRobj("seven");
    robj *o8 = createRobj("eight");
    robj *o9 = createRobj("nine");
    robj *o10 = createRobj("ten");
    robj *o11 = createRobj("eleven");
    robj *o12 = createRobj("twelve");
    robj *o13 = createRobj("thirteen");
    robj *o14 = createRobj("fourteen");
    robj *o15 = createRobj("fitteen");

    zslInsert(zsk, 1.0, o1);
    zslInsert(zsk, 2.0, o2);
    zslInsert(zsk, 3.0, o3);
    zslInsert(zsk, 4.0, o4);
    zslInsert(zsk, 5.0, o5);
    zslInsert(zsk, 6.0, o6);
    zslInsert(zsk, 7.0, o7);
    zslInsert(zsk, 8.0, o8);
    zslInsert(zsk, 9.0, o9);
    zslInsert(zsk, 10.0, o10);
    zslInsert(zsk, 11.0, o11);
    zslInsert(zsk, 12.0, o12);
    zslInsert(zsk, 13.0, o13);
    zslInsert(zsk, 14.0, o14);
    zslInsert(zsk, 15.0, o15);

    int ret = zslDelete(zsk, 12.0, o12);
    if (ret) {
        printf("删除成功\n");
    } else {
        printf("删除失败\n");
    }

    zslDump(zsk);

    zslFree(zsk);
}

void test_case_2() {

    zskiplist *zsk = zslCreate();
    int i;

    for (i = 0; i < 50; i++) {

        char *buff = (char *)malloc(15);
        snprintf(buff, 15, "k%d", i + 1);

        robj *o = createRobj(buff);
        zslInsert(zsk, i + 1, o);
    }

    printf("\n ============== dump =========== \n");
    zslDump(zsk);

    printf("\n ============= range =========== \n");

    zrangespec range = createRangeObj(10, 25);
    int ret = zslIsInRange(zsk, &range);
    if (ret) {
        zskiplistNode *firstNode = zslFirstInRange(zsk, range);
        zskiplistNode *lastNode = zslLastInRange(zsk, range);
        printf("第一个节点搜索成功, firstNode score:%0.2lf, val:%s\n", firstNode->score, firstNode->obj->ptr);
        printf("最后一个节点搜索成功, lastNode score:%0.2lf, val:%s\n", lastNode->score, lastNode->obj->ptr);

        int deleteNum = zslDeleteRangeByScore(zsk, range);
        printf("范围删除数量:%d\n", deleteNum);
    } else {
        printf("范围搜索失败\n");
    }

    printf("\n ============== dump =========== \n");
    zslDump(zsk);


    printf("\n ============== rank =========== \n");

    robj *o = createRobj("k46");
    double score = 46;
    unsigned long rank = zslGetRank(zsk, score, o);
    printf("rank:%u\n", rank);

    zskiplistNode *rankNode = zslGetElementByRank(zsk, rank);
    printf("rankNode score:%0.2lf, val:%s\n", rankNode->score, rankNode->obj->ptr);

    unsigned long dNum = zslDeleteRangeByRank(zsk, 20, 30);
    printf("删除节点: %d\n", dNum);

    printf("\n ============== dump =========== \n");
    zslDump(zsk);

    zslFree(zsk);
}

int main() {

    test_case_2();
    return 0;
}