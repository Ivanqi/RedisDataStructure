#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "demo_sds_2.h"
#include "demo_zskiplist_2.h"

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

    zskiplistNode **node;
    int ret = zslDelete(zsk, 12.0, twelve, node);
    if (ret) {
        printf("删除成功\n");
    } else {
        printf("删除失败\n");
    }

    zslDump(zsk);

    zslFree(zsk);
}

int main() {

    createSharedObjects();
    test_case_1();
    return 0;
}