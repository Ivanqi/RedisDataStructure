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

unsigned char *createList() {

    unsigned char *zl = ziplistNew();
    zl = ziplistPush(zl, (unsigned char *)"foo", 3, ZIPLIST_TAIL);
    
    zl = ziplistPush(zl, (unsigned char *)"quux", 4, ZIPLIST_TAIL);

    zl = ziplistPush(zl, (unsigned char *)"hello", 5, ZIPLIST_HEAD);

    zl = ziplistPush(zl, (unsigned char *)"1024", 4, ZIPLIST_TAIL);

    return zl;
}

void pop(unsigned char *zl, int where) {

    unsigned char *p, *vstr;
    unsigned int vlen;
    long long vlong;

    p = ziplistIndex(zl, where == ZIPLIST_HEAD ? 0 : -1);

    int ret = ziplistGet(p, &vstr, &vlen, &vlong);
    if (ret) {
        if (where == ZIPLIST_HEAD) {
           printf("Pop head: "); 
        } else {
            printf("Pop tail: ");
        }

        if (vstr) {
            if (vlen && fwrite(vstr, vlen, 1, stdout) == 0) {
                perror("fwrite");
            }
        } else {
            printf("%lld", vlong);
        }

        printf("\n");
        ziplistDeleteRange(zl, -1, 1);
    } else {
        printf("ERROR: Could not pop \n");
        exit(1);
    }
}

void test_case_1() {

    unsigned char *zl, *p;
    unsigned char *entry;
    unsigned int elen;
    long long value;

    zl = createIntList();
    ziplistRepr(zl);

    zl = createList();
    ziplistRepr(zl);

    pop(zl, ZIPLIST_TAIL);
    ziplistRepr(zl);

    pop(zl, ZIPLIST_HEAD);
    ziplistRepr(zl);

    // pop(zl, ZIPLIST_TAIL);
    // ziplistRepr(zl);

    // pop(zl, ZIPLIST_HEAD);
    // ziplistRepr(zl);
}

int main() {

    test_case_1();
    return 0;
}