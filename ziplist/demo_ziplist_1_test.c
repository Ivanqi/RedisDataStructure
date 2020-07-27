#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
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

void verify(unsigned char *zl, zlentry *e) {

    int i;
    int len = ziplistLen(zl);
    
    zlentry _e;

    for (i = 0; i < len; i++) {
        memset(&e[i], 0, sizeof(zlentry));
        e[i] = zipEntryGet(ziplistIndex(zl, i));

        memset(&_e, 0, sizeof(zlentry));
        _e = zipEntryGet(ziplistIndex(zl, -len + i));

        assert(memcmp(&e[i], &_e, sizeof(zlentry)) == 0);
    }
}

long long usec(void) {

    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (((long long)tv.tv_sec) * 1000000) + tv.tv_usec;
}

void stress(int pos, int num, int maxsize, int dnum) {

    int i, j, k;
    unsigned char *zl;
    char posstr[2][5] = {"HEAD", "TAIL"};
    long long start;

    for (i = 0; i < maxsize; i += dnum) {
        zl = ziplistNew();
        for (j = 0; j < i; j++) {
            zl = ziplistPush(zl, (unsigned char *)"quux", 4, ZIPLIST_TAIL);
        }

        start = usec();

        for (k = 0; k < num; k++) {
            zl = ziplistPush(zl, (unsigned char *)"quux", 4, pos);
            zl = ziplistDeleteRange(zl, 0, 1);
        }

        printf("List size: %8d, bytes: %8d, %dx push + pop (%s): %6lld usec\n",
                i, intrev32ifbe(ZIPLIST_BYTES(zl)), num, posstr[pos], usec() - start);
        
        free(zl);
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

    printf("Regression test for > 255 byte string: ");
    {
        char v1[257], v2[257];
        memset(v1, 'x', 256);
        memset(v2, 'y', 256);
        zl = ziplistNew();
        zl = ziplistPush(zl,(unsigned char*)v1, strlen(v1), ZIPLIST_TAIL);
        zl = ziplistPush(zl,(unsigned char*)v2, strlen(v2), ZIPLIST_TAIL);

        p = ziplistIndex(zl, 0);
        assert(ziplistGet(p, &entry, &elen, &value));
        assert(strncmp(v1, (char *)entry, elen) == 0);

        p = ziplistIndex(zl, 1);
        assert(ziplistGet(p, &entry, &elen, &value));
        assert(strncmp(v2, (char *)entry, elen) == 0);
        printf("SUCCESS\n\n");
    }

    printf("Regression test deleting next to last entries: ");
    {
        char v[3][257];
        zlentry e[3];
        int i;

        for (i = 0; i < (sizeof(v) / sizeof(v[0])); i++) {
            memset(v[i], 'a' + i, sizeof(v[0]));
        }

        v[0][256] = '\0';
        v[1][  1] = '\0';
        v[2][256] = '\0';

        zl = ziplistNew();
        for (i = 0; i < (sizeof(v) / sizeof(v[0])); i++) {
            zl = ziplistPush(zl, (unsigned char *) v[i], strlen(v[i]), ZIPLIST_TAIL);
        }

        verify(zl, e);

        assert(e[0].prevrawlensize == 1);
        assert(e[1].prevrawlensize == 5);
        assert(e[2].prevrawlensize == 1);

        unsigned char *p = e[1].p;
        zl = ziplistDelete(zl, &p);

        verify(zl, e);

        assert(e[0].prevrawlensize == 1);
        assert(e[1].prevrawlensize == 5);

        printf("SUCCESS\n\n");
    }

    printf("Create long list and check indices: ");
    {
        zl = ziplistNew();
        char buf[32];
        int i, len;

        for (i = 0; i < 1000; i++) {
            len = sprintf(buf, "%d", i);
            zl = ziplistPush(zl, (unsigned char *)buf, len, ZIPLIST_TAIL);
        }


        for (i = 0; i < 1000; i++) {
            p = ziplistIndex(zl, i);
            int ret = ziplistGet(p, NULL, NULL, &value);
            assert(i == value);

            p = ziplistIndex(zl, -i - 1);
            assert(ziplistGet(p, NULL, NULL, &value));
            assert(999 - i == value);
        }

        printf("SUCCESS\n\n");
    }

    printf("Compare strings with ziplist entries: ");
    {
        zl = createList();
        p = ziplistIndex(zl, 0);
        if (!ziplistCompare(p, (unsigned char*)"hello", 5)) {
            printf("ERROR: not \"hello\"\n");
            exit(-1);
        }

        if (ziplistCompare(p, (unsigned char *)"hella", 5)) {
            printf("ERROR: \"hella\"\n");
            exit(-1);
        }

        p = ziplistIndex(zl, 3);
        if (!ziplistCompare(p, (unsigned char *)"1024", 4)) {
            printf("ERROR: not \"1024\"\n");
            exit(-1);
        }

        if (ziplistCompare(p, (unsigned char *)"1025", 4)) {
            printf("ERROR: \"1025\"\n");
            exit(-1);
        }
        printf("SUCCESS\n\n");
    }

    printf("Stress with variable ziplist size:\n");
    {
        stress(ZIPLIST_HEAD, 100000, 16384, 256);
        stress(ZIPLIST_TAIL, 100000, 16384, 256);
    }
}

int main() {

    test_case_1();
    return 0;
}