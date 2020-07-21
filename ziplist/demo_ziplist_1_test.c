#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "demo_ziplist_1.h"

unsigned char *createIntList() {

    unsigned char *zl = ziplistNew();
    char buf[32];
    
    sprintf(buf, "100");
    zl = ziplistPush(zl, (unsigned char *)buf, strlen(buf), ZIPLIST_TAIL);

    sprintf(buf, "128000");
    zl = ziplistPush(zl, (unsigned char *)buf, strlen(buf), ZIPLIST_TAIL);
}

void test_case_1() {

    unsigned char *zl, *p;
    unsigned char *entry;
    unsigned int elen;
    long long value;

    createIntList();
}

int main() {

    test_case_1();
    return 0;
}