#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "demo_adlist_1.h"

void test_case_1() {

    list *link = listCreate();
    int num[] = {2, 3, 4, 5, 6, 7, 8, 9};

    int first = 1;
    int tenth = 10;
    int i;

    listAddNodeHead(link, (void*)&first);
    for (i = 0; i < 8; i++) {
        listAddNodeHead(link, (void*)&num[i]);
    }

    listAddNodeTail(link, (void*)&tenth);

    listIter *iter;
    listNode *node;
    iter = listGetIterator(link, AL_START_HEAD);

    while ((node = listNext(iter)) != NULL) {
        printf("node:%d\n", *(void **)node->value);
    }

    listNode *n = listIndex(link, 4);
    printf("n:%d\n", *(void **)n->value);
    int eleventh = 11;
    listInsertNode(link, n, (void *)&eleventh, 1);

    iter = listGetIterator(link, AL_START_HEAD);
    while ((node = listNext(iter)) != NULL) {
        printf("node2:%d\n", *(void **)node->value);
    }

    listRelease(link);
}

int keycmp(void *key1, void *key2) {
    return strcmp(key1, key2) == 0 ? 1 : 0;
}

void *keydup(void *ptr) {

    int len = sizeof(ptr);
    void *dest = (void*)malloc(len);
    void *ret = memcpy(dest, ptr, len);
    return ret;
}

void test_case_2() {

    list *link = listCreate();
    char first[] = "apple";
    char tenth[] = "juice";
    int i;
    char string[][8] = {
        "banana", "corrent", "dress", "eat", "flower", "guy", "high",
        "ice"
    };

    for (i = 0; i < 8; i++) {
        listAddNodeHead(link, string[i]);
    }

    listAddNodeHead(link, first);

    listAddNodeTail(link, (void*)&tenth);

    listIter *iter;
    listNode *node;
    iter = listGetIterator(link, AL_START_HEAD);

    while ((node = listNext(iter)) != NULL) {
        printf("node:%s\n", (char *)node->value);
    }

    listSetDupMethod(link, keydup);
    list *newlink = listDup(link);
    char searchIndex[] = "eat";

    listSetMatchMethod(newlink, keycmp);
    listNode *searchNode = listSearchKey(newlink, searchIndex);

    printf("searchNode:%s\n", (char *)searchNode->value);

    listRelease(link);
    listRelease(newlink);
}

int main() {

    test_case_2();
    return 0;
}