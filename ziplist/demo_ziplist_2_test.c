#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "demo_ziplist_2.h"

static unsigned char *createList() {

    unsigned char *zl = ziplistNew();
    zl = ziplistPush(zl, (unsigned char*)"foo", 3, ZIPLIST_TAIL);

    zl = ziplistPush(zl, (unsigned char*)"quux", 4, ZIPLIST_TAIL);

    zl = ziplistPush(zl, (unsigned char*)"hello", 5, ZIPLIST_HEAD);

    zl = ziplistPush(zl, (unsigned char*)"1024", 4, ZIPLIST_TAIL);

    return zl;
}

static unsigned char *createIntList() {

    unsigned char *zl = ziplistNew();
    char buf[32];

    sprintf(buf, "100");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_TAIL);

    sprintf(buf, "128000");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_TAIL);

    sprintf(buf, "-100");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_HEAD);

    sprintf(buf, "4294967296");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_HEAD);

    sprintf(buf, "non integer");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_TAIL);

    sprintf(buf, "much much longer non integer");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_TAIL);

    return zl;
}

static void verify(unsigned char *zl, zlentry *e) {

    int len = ziplistLen(zl);
    zlentry _e;

    ZIPLIST_ENTRY_ZERO(&_e);

    for (int i = 0; i < len; i++) {
        memset(&e[i], 0, sizeof(zlentry));
        zipEntry(ziplistIndex(zl, i), &e[i]);

        memset(&_e, 0, sizeof(zlentry));
        zipEntry(ziplistIndex(zl, -len + i), &_e);

        assert(memcmp(&e[i], &_e, sizeof(zlentry)) == 0);
    }
}

static unsigned char *pop(unsigned char *zl, int where) {

    unsigned char *p, *vstr;
    unsigned int vlen;
    long long vlong;

    p = ziplistIndex(zl, where == ZIPLIST_HEAD ? 0 : -1);

    if (ziplistGet(p, &vstr, &vlen, &vlong)) {
        if (where == ZIPLIST_HEAD) {
            printf("Pop head: ");
        } else {
            printf("Pop tail: ");
        }

        if (vstr) {
            if (vlen && fwrite(vstr, vlen, 1, stdout) == 0) perror("fwrite");
        } else {
            printf("%lld", vlong);
        }

        printf("\n");
        return ziplistDelete(zl, &p);
    } else {
        printf("ERROR: Could not pop\n");
        exit(-1);
    }
}

static long long usec(void) {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec) * 1000000) + tv.tv_usec;
}

static void stress(int pos, int num, int maxsize, int dnum) {

    int i,j,k;
    unsigned char *zl;
    char posstr[2][5] = { "HEAD", "TAIL" };
    long long start;
    for (i = 0; i < maxsize; i+=dnum) {
        zl = ziplistNew();
        for (j = 0; j < i; j++) {
            zl = ziplistPush(zl, (unsigned char*)"quux", 4, ZIPLIST_TAIL);
        }

        /* Do num times a push+pop from pos */
        start = usec();
        for (k = 0; k < num; k++) {
            zl = ziplistPush(zl, (unsigned char*)"quux", 4, pos);
            zl = ziplistDeleteRange(zl, 0, 1);
        }
        printf("List size: %8d, bytes: %8d, %dx push + pop (%s): %6lld usec\n",
            i,intrev32ifbe(ZIPLIST_BYTES(zl)), num, posstr[pos], usec() - start);
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
    free(zl);

    zl = createList();
    ziplistRepr(zl);

    zl = pop(zl, ZIPLIST_TAIL);
    ziplistRepr(zl);

    zl = pop(zl, ZIPLIST_HEAD);
    ziplistRepr(zl);

    zl = pop(zl, ZIPLIST_TAIL);
    ziplistRepr(zl);

    zl = pop(zl, ZIPLIST_TAIL);
    ziplistRepr(zl);

    free(zl);

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
        free(zl);
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
        free(zl);
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
        free(zl);
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
        free(zl);
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
        free(zl);
    }

    printf("Iterate list from 0 to end: \n");
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
            p = ziplistNext(zl,p);
            printf("\n");
        }
        printf("\n");
        free(zl);
    }

    printf("Iterate list from 1 to end: \n");
    {
        zl = createList();
        p = ziplistIndex(zl, 1);
        while (ziplistGet(p, &entry, &elen, &value)) {
            printf("Entry: ");
            if (entry) {
                if (elen && fwrite(entry ,elen, 1, stdout) == 0) perror("fwrite");
            } else {
                printf("%lld", value);
            }
            p = ziplistNext(zl,p);
            printf("\n");
        }
        printf("\n");
        free(zl);
    }

    printf("Iterate list from 2 to end:\n");
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
            p = ziplistNext(zl,p);
            printf("\n");
        }
        printf("\n");
        free(zl);
    }

    printf("Iterate starting out of range:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, 4);
        if (!ziplistGet(p, &entry, &elen, &value)) {
            printf("No entry\n");
        } else {
            printf("ERROR\n");
        }
        printf("\n");
        free(zl);
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
            p = ziplistPrev(zl,p);
            printf("\n");
        }
        printf("\n");
        free(zl);
    }

    printf("Iterate from back to front, deleting all items:\n");
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
            zl = ziplistDelete(zl,&p);
            p = ziplistPrev(zl,p);
            printf("\n");
        }
        printf("\n");
        free(zl);
    }

    printf("Delete inclusive range 0,0:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 0, 1);
        ziplistRepr(zl);
        free(zl);
    }

    printf("Delete inclusive range 0,1:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 0, 2);
        ziplistRepr(zl);
        free(zl);
    }

    printf("Delete inclusive range 1,2:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 1, 2);
        ziplistRepr(zl);
        free(zl);
    }

    printf("Delete with start index out of range:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 5, 1);
        ziplistRepr(zl);
        free(zl);
    }

    printf("Delete with num overflow:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 1, 5);
        ziplistRepr(zl);
        free(zl);
    }

    printf("Delete foo while iterating:\n");
    {
        zl = createList();
        p = ziplistIndex(zl ,0);
        while (ziplistGet(p, &entry, &elen, &value)) {
            if (entry && strncmp("foo", (char*)entry, elen) == 0) {
                printf("Delete foo\n");
                zl = ziplistDelete(zl,&p);
            } else {
                printf("Entry: ");
                if (entry) {
                    if (elen && fwrite(entry, elen, 1, stdout) == 0)
                        perror("fwrite");
                } else {
                    printf("%lld",value);
                }
                p = ziplistNext(zl,p);
                printf("\n");
            }
        }
        printf("\n");
        ziplistRepr(zl);
        free(zl);
    }

    printf("Regression test for > 255 byte strings: ");
    {
        char v1[257] = {0}, v2[257] = {0};
        memset(v1, 'x', 256);
        memset(v2, 'y', 256);

        zl = ziplistNew();
        zl = ziplistPush(zl, (unsigned char*)v1, strlen(v1), ZIPLIST_TAIL);
        zl = ziplistPush(zl, (unsigned char*)v2, strlen(v2), ZIPLIST_TAIL);

        /* Pop values again and compare their value. */
        p = ziplistIndex(zl, 0);
        assert(ziplistGet(p, &entry, &elen, &value));
        assert(strncmp(v1, (char*)entry, elen) == 0);

        p = ziplistIndex(zl, 1);
        assert(ziplistGet(p, &entry, &elen, &value));
        assert(strncmp(v2, (char*)entry, elen) == 0);
        printf("SUCCESS\n\n");
        free(zl);
    }

    printf("Regression test deleting next to last entries: ");
    {
        char v[3][257] = {{0}};
        zlentry e[3] = {{.prevrawlensize = 0, .prevrawlen = 0, .lensize = 0,
                         .len = 0, .headersize = 0, .encoding = 0, .p = NULL}};
        size_t i;

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

        /* Deleting entry 1 will increase `prevrawlensize` for entry 2 */
        unsigned char *p = e[1].p;
        zl = ziplistDelete(zl, &p);

        verify(zl, e);

        assert(e[0].prevrawlensize == 1);
        assert(e[1].prevrawlensize == 5);

        printf("SUCCESS\n\n");
        free(zl);
    }

    printf("Compare strings with ziplist entries: ");
    {
        zl = createList();
        p = ziplistIndex(zl, 0);
        if (!ziplistCompare(p, (unsigned char*)"hello", 5)) {
            printf("ERROR: not \"hello\"\n");
            exit(-1);
        }

        if (ziplistCompare(p, (unsigned char*)"hella", 5)) {
            printf("ERROR: \"hella\"\n");
            exit(-1);
        }

        p = ziplistIndex(zl, 3);
        if (!ziplistCompare(p, (unsigned char*)"1024", 4)) {
            printf("ERROR: not \"1024\"\n");
            exit(-1);
        }

        if (ziplistCompare(p, (unsigned char*)"1025", 4)) {
            printf("ERROR: \"1025\"\n");
            exit(-1);
        }

        printf("SUCCESS\n\n");
        free(zl);
    }

    printf("Merge test: \n");
    {
        zl = createList();
        unsigned char *zl2 = createList();
        unsigned char *zl3 = ziplistNew();
        unsigned char *zl4 = ziplistNew();

        if (ziplistMerge(&zl4, &zl4)) {
            printf("ERROR: Allowed merging of one ziplist into itself.\n");
            exit(-1);
        }
        
        zl4 = ziplistMerge(&zl3, &zl4);
        ziplistRepr(zl4);
        if (ziplistLen(zl4)) {
            printf("ERROR: Merging two empty ziplists created entries. \n");
            exit(-1);
        }
        free(zl4);

        zl2 = ziplistMerge(&zl, &zl2);
        ziplistRepr(zl2);

        if (ziplistLen(zl2) != 8) {
            printf("ERROR: Merged length not 8, but: %u\n", ziplistLen(zl2));
            exit(-1);
        }

        p = ziplistIndex(zl2,0);
        if (!ziplistCompare(p,(unsigned char*)"hello", 5)) {
            printf("ERROR: not \"hello\"\n");
            exit(-1);
        }

        if (ziplistCompare(p,(unsigned char*)"hella", 5)) {
            printf("ERROR: \"hella\"\n");
            exit(-1);
        }

        p = ziplistIndex(zl2, 3);
        if (!ziplistCompare(p, (unsigned char*)"1024", 4)) {
            printf("ERROR: not \"1024\"\n");
            exit(-1);
        }

        if (ziplistCompare(p, (unsigned char*)"1025", 4)) {
            printf("ERROR: \"1025\"\n");
            exit(-1);
        }

        p = ziplistIndex(zl2, 4);
        if (!ziplistCompare(p, (unsigned char*)"hello", 5)) {
            printf("ERROR: not \"hello\"\n");
            exit(-1);
        }

        if (ziplistCompare(p, (unsigned char*)"hella", 5)) {
            printf("ERROR: \"hella\"\n");
            exit(-1);
        }

        p = ziplistIndex(zl2, 7);
        if (!ziplistCompare(p, (unsigned char*)"1024", 4)) {
            printf("ERROR: not \"1024\"\n");
            exit(-1);
        }

        if (ziplistCompare(p, (unsigned char*)"1025", 4)) {
            printf("ERROR: \"1025\"\n");
            exit(-1);
        }

        printf("SUCCESS\n\n");
        free(zl);
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