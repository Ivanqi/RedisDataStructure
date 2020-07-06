#include <stdio.h>
#include <stdlib.h>

#include "demo_zskiplist_1.h"

void test_case_1() {

    zskiplist *zsk =  zslCreate();
    zslFree(zsk);
}

int main() {

    test_case_1();
    return 0;
}