#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "demo_sds_2.h"
#include "demo_zskiplist_2.h"

zrangespec createRangeObj(double min, double max) {

    zrangespec range;
    range.min = min;
    range.max = max;
    range.minex = range.maxex = 1;

    return range;
}

zlexrangespec createLexRangeObj(sds min, sds max) {
    
    zlexrangespec range;
    range.min = min;
    range.max = max;

    range.minex = range.maxex = 1;

    return range;
}

void test_case_1() {

    zskiplist *zsk = zslCreate();
    sds one = sdsnew("one");
    sds two = sdsnew("two");
    sds threed = sdsnew("three");
    sds four = sdsnew("four");
    sds five = sdsnew("five");
    sds six = sdsnew("six");
    sds sevent = sdsnew("seven");
    sds eight = sdsnew("eight");
    sds nine = sdsnew("nine");
    sds ten = sdsnew("ten");
    sds eleven = sdsnew("eleven");
    sds twelve = sdsnew("twelve");
    sds thirteen = sdsnew("thirteen");
    sds fourteen = sdsnew("fourteen");
    sds fitteen = sdsnew("fitteen");

    zslInsert(zsk, 1.0, one);
    zslInsert(zsk, 2.0, two);
    zslInsert(zsk, 3.0, threed);
    zslInsert(zsk, 4.0, four);
    zslInsert(zsk, 5.0, five);
    zslInsert(zsk, 6.0, six);
    zslInsert(zsk, 7.0, sevent);
    zslInsert(zsk, 8.0, eight);
    zslInsert(zsk, 9.0, nine);
    zslInsert(zsk, 10.0, ten);
    zslInsert(zsk, 11.0, eleven);
    zslInsert(zsk, 12.0, twelve);
    zslInsert(zsk, 13.0, thirteen);
    zslInsert(zsk, 14.0, fourteen);
    zslInsert(zsk, 15.0, fitteen);

    int ret = zslDelete(zsk, 12.0, twelve, NULL);
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

       sds o = sdsnew(buff);
        zslInsert(zsk, i + 1, o);
    }

    printf("\n ============== dump =========== \n");
    zslDump(zsk);

    printf("\n ============= range =========== \n");

    zrangespec range = createRangeObj(10, 25);
    int ret = zslIsInRange(zsk, &range);
    if (ret) {
        zskiplistNode *firstNode = zslFirstInRange(zsk, &range);
        zskiplistNode *lastNode = zslLastInRange(zsk, &range);
        printf("第一个节点搜索成功, firstNode score:%0.2lf, val:%s\n", firstNode->score, firstNode->ele);
        printf("最后一个节点搜索成功, lastNode score:%0.2lf, val:%s\n", lastNode->score, lastNode->ele);

        int deleteNum = zslDeleteRangeByScore(zsk, &range);
        printf("范围删除数量:%d\n", deleteNum);
    } else {
        printf("范围搜索失败\n");
    }

    printf("\n ============== update score =========== \n");
    sds s = sdsnew("k45");
    double oldscore = 45;
    double newscore = 100;
    zslUpdateScore(zsk, oldscore, s, newscore);


    printf("\n ============== dump =========== \n");
    zslDump(zsk);


    printf("\n ============== rank =========== \n");

    sds o = sdsnew("k46");
    double score = 46;
    unsigned long rank = zslGetRank(zsk, score, o);
    printf("rank:%u\n", rank);

    zskiplistNode *rankNode = zslGetElementByRank(zsk, rank);
    printf("rankNode score:%0.2lf, val:%s\n", rankNode->score, rankNode->ele);

    unsigned long dNum = zslDeleteRangeByRank(zsk, 20, 30);
    printf("删除节点: %d\n", dNum);

    printf("\n ============== dump =========== \n");
    zslDump(zsk);

    zslFree(zsk);
}


void test_case_3() {

    zskiplist *zsk = zslCreate();

    sds apple = sdsnew("apple");
    sds banana = sdsnew("banana");
    sds cat = sdsnew("cat");
    sds draw = sdsnew("draw");
    sds even = sdsnew("even");
    sds five = sdsnew("five");
    sds get = sdsnew("get");
    sds high = sdsnew("high");
    sds ice = sdsnew("ice");
    sds juice = sdsnew("juice");
    sds kill = sdsnew("kill");
    sds level = sdsnew("level");
    sds moon = sdsnew("moon");
    sds nine = sdsnew("nine");
    sds oil = sdsnew("oil");

    zslInsert(zsk, 1.0, apple);
    zslInsert(zsk, 2.0, banana);
    zslInsert(zsk, 3.0, cat);
    zslInsert(zsk, 4.0, draw);
    zslInsert(zsk, 5.0, even);
    zslInsert(zsk, 6.0, five);
    zslInsert(zsk, 7.0, get);
    zslInsert(zsk, 8.0, high);
    zslInsert(zsk, 9.0, ice);
    zslInsert(zsk, 10.0, juice);
    zslInsert(zsk, 11.0, kill);
    zslInsert(zsk, 12.0, level);
    zslInsert(zsk, 13.0, moon);
    zslInsert(zsk, 14.0, nine);
    zslInsert(zsk, 15.0, oil);

    printf("\n ============== dump =========== \n");
    zslDump(zsk);

    zlexrangespec range = createLexRangeObj(kill, nine);
    int ret = zslIsInLexRange(zsk, &range);
    if (ret) {
        zskiplistNode *firstNode = zslFirstInLexRange(zsk, &range);
        zskiplistNode *lastNode = zslLastInLexRange(zsk, &range);
        printf("第一个节点搜索成功, firstNode score:%0.2lf, val:%s\n", firstNode->score, firstNode->ele);
        printf("最后一个节点搜索成功, lastNode score:%0.2lf, val:%s\n", lastNode->score, lastNode->ele);

        int deleteNum = zslDeleteRangeByLex(zsk, &range);
        printf("范围删除数量:%d\n", deleteNum);

    } else {
        printf("范围搜索失败\n");
    }

    printf("\n ============== dump =========== \n");
    zslDump(zsk);
    zslFreeLexRange(&range);

    zslFree(zsk);
}

int main() {

    createSharedObjects();
    test_case_3();
    return 0;
}