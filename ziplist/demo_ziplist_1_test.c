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
    int pos = where == ZIPLIST_HEAD ? 0 : -1;

    p = ziplistIndex(zl, pos);

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
        ziplistDeleteRange(zl, pos, 1);
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

    pop(zl, ZIPLIST_TAIL);
    ziplistRepr(zl);

    pop(zl, ZIPLIST_HEAD);
    ziplistRepr(zl);

    printf("Get element at index 3: ");
    {
        zl = createList();
        p = ziplistIndex(zl, 3);

        if (!ziplistGet(p, &entry, &elen, &value)) {
            printf("ERROR: Could not access index 3\n");
            exit(-1);
        }

        if (entry) {
            if (elen && fwrite(entry, elen, 1, stdout) == 0) perror("fwrite");
            printf("\n");
        } else {
            printf("%lld\n", value);
        }
        printf("\n");
    }

    printf("Get element at index 4 (out of range): ");
    {
        zl = createList();
        p = ziplistIndex(zl, 4);
        
        if (p == NULL) {
            printf("No entry\n");
        } else {
            printf("ERROR: Out of range index should return NULL, returned offset: %ld\n", p - zl);
            exit(-1);
        }
        printf("\n");
    }

    printf("Get element at index -1 (last element): ");
    {
        zl = createList();
        p = ziplistIndex(zl, -1);

        if (!ziplistGet(p, &entry, &elen, &value)) {
            printf("ERROR: Could not access index -1\n");
            exit(-1);
        }

        if (entry) {
            if (elen && fwrite(entry, elen, 1, stdout) == 0) perror("fwrite");
            printf("\n");
        } else {
            printf("%lld\n", value);
        }
        printf("\n");
    }

    printf("Get element at index -4 (first element): ");
    {
        zl = createList();
        p = ziplistIndex(zl, -4);
        if (!ziplistGet(p, &entry, &elen, &value)) {
            printf("ERROR: Could not access index -4\n");
            exit(-1);
        }

        if (entry) {
            if (elen && fwrite(entry, elen, 1, stdout) == 0) perror("fwrite");
            printf("\n");
        } else {
            printf("%lld\n", value);
        }

        printf("\n");
    }

    printf("Get element at index -5 (reverse out of range): ");
    {
        zl = createList();
        p = ziplistIndex(zl, -5);
        if (p == NULL) {
            printf("No entry\n");
        } else {
            printf("ERROR: Out of range index should return NULL, returned offset: %ld\n", p - zl);
            exit(-1);
        }
        printf("\n");
    }

    printf("Iterate list from 0 to end:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, 0);

        while (ziplistGet(p, &entry, &elen, &value)) {
            printf("Entry: ");
            if (entry) {
                if (elen && fwrite(entry, elen, 1, stdout) == 0) perror("fwrite");
            } else {
                printf("%lld", value);
            }
            p = ziplistNext(zl, p);
            printf("\n");
        }
        printf("\n");
    }

    printf("Iterate list from 1 to end:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, 1);

        while (ziplistGet(p, &entry, &elen, &value)) {
            printf("Entry: ");
            if (entry) {
                if (elen && fwrite(entry, elen, 1, stdout) == 0) perror("fwrite");
            } else {
                printf("%lld", value);
            }
            p = ziplistNext(zl, p);
            printf("\n");
        }
        printf("\n");
    }

    printf("Interate list from 2 to end:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, 2);

        while (ziplistGet(p, &entry, &elen, &value)) {
            printf("Entry: ");
            if (entry) {
                if (elen && fwrite(entry, elen, 1, stdout) == 0) perror("fwrite");
            } else {
                printf("%lld", value);
            }
            p = ziplistNext(zl, p);
            printf("\n");
        }
        printf("\n");
    }

    printf("Iterate from back to front:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, -1);

        while (ziplistGet(p, &entry, &elen, &value)) {
            printf("Entry: ");
            if (entry) {
                if (elen && fwrite(entry, elen, 1, stdout) == 0) perror("fwrite");
            } else {
                printf("%lld", value);
            }
            p = ziplistPrev(zl, p);
            printf("\n");
        }
        printf("\n");
    }

    printf("Iterate from back to front, deleting all items\n");
    {
        zl = createList();
        p = ziplistIndex(zl, -1);

        while (ziplistGet(p, &entry, &elen, &value)) {
            printf("Entry: ");
            if (entry) {
                if (elen && fwrite(entry, elen, 1, stdout) == 0) perror("fwrite");
            } else {
                printf("%lld", value);
            }

            zl = ziplistDelete(zl, &p);
            p = ziplistPrev(zl, p);
            printf("\n");
        }
        printf("\n");
    }

    printf("Delete inclusive range 0, 0:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 0, 1);
        ziplistRepr(zl);
    }

    printf("Delete inclusive range 0, 1:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 0, 2);
        ziplistRepr(zl);
    }

    printf("Delete inclusive range 1, 2:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 1, 2);
        ziplistRepr(zl);
    }

    printf("Delete with start index out of range:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 5, 1);
        ziplistRepr(zl);
    }

    printf("Delete with num overflow:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 1, 5);
        ziplistRepr(zl);
    }

    printf("Delete foo while iterating:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, 0);
        while (ziplistGet(p, &entry, &elen, &value)) {
            if (entry && strncmp("foo", (char*)entry, elen) == 0) {
                printf("Delete foo\n");
                zl = ziplistDelete(zl, &p);
            } else {
                printf("Entry: ");
                if (entry) {
                    if (elen && fwrite(entry, elen, 1, stdout) == 0) perror("fwrite");
                } else {
                    printf("%lld", value);
                }
                p = ziplistNext(zl, p);
                printf("\n");
            }
        }
        printf("\n");
        ziplistRepr(zl);
    }
}

int main() {

    test_case_1();
    return 0;
}