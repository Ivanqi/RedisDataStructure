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

    sprintf(buf, "-100");
    zl = ziplistPush(zl, (unsigned char *)buf, strlen(buf), ZIPLIST_HEAD);

    sprintf(buf, "4294967296");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_HEAD);

    sprintf(buf, "non integer");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_TAIL);

    sprintf(buf, "much much longer non integer");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_TAIL);

    return zl;
}

void test_case_1() {

    unsigned char *zl, *p;
    unsigned char *entry;
    unsigned int elen;
    long long value;

    zl = createIntList();
}

int main() {

    test_case_1();
    return 0;
}