#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "demo_dict_1.h"

unsigned int hashFunc_1(const void *key) {
    return dictIntHashFuncion((unsigned int *)key);
}

unsigned int hashFunc_2(const void *key) {
    return dictGenHashFunction(key, sizeof(key));
}

unsigned int hashFunc_3(const void *key) {
    const unsigned char *data = (const unsigned char *)key;
    int len = strlen(data);
    return dictGenCaseHashFunction(data, len);
}

void test_case_1() {
    dictType *type = (dictType *)malloc(sizeof(dictType));

    char privDataPtr[] = "privDataPtr";
    dict *d;

    type->hashFunction = hashFunc_2;

    d = dictCreate(type, privDataPtr);

    int one = 1;
    int two = 2;

    dictAdd(d, (void *)&one, (void *)&one);
    dictAdd(d, (void *)&two, (void *)&two);

    dictIterator *dIter = dictGetIterator(d);
    dictEntry *node;

    while ((node = dictNext(dIter)) != NULL) {
        printf("node key:%d, val:%d\n", *(void **)node->key, *(void **)node->v.val);
    }
    dictRelease(d);
}

void test_case_2() {
    dictType *type = (dictType *)malloc(sizeof(dictType));

    char privDataPtr[] = "privDataPtr";
    dict *d;

    type->hashFunction = hashFunc_1;

    d = dictCreate(type, privDataPtr);

    int num[] = {1, 2, 3, 4, 5, 6, 7};
    int i;
    for (i = 0; i < 7; i++) {
        dictAdd(d, (void*)&num[i], (void*)&num[i]);
    }

    dictIterator *dIter = dictGetIterator(d);
    dictEntry *node;

    while ((node = dictNext(dIter)) != NULL) {
        printf("node key:%d, val:%d\n", *(void **)node->key, *(void **)node->v.val);
    }

    int find = 2;

    dictEntry *entry = dictFind(d, &find);
    if (entry) {
        printf("entry key:%d, val:%d\n", *(void **)entry->key, *(void **)entry->v.val);
    } else {
        printf("没找到对应的值\n");
    }

    dictRelease(d);
}

int strCompare (void *privdata, const void *key1, const void *key2) {

    return strcmp(key1, key2) == 0 ? 1 : 0;
}

void test_case_3() {
    dictType *type = (dictType *)malloc(sizeof(dictType));

    char privDataPtr[] = "privDataPtr";
    dict *d;

    type->hashFunction = hashFunc_3;
    type->keyCompare = strCompare;


    d = dictCreate(type, privDataPtr);

    printf("\n================= add =================\n");
    dictAdd(d, "one", "one");
    dictAdd(d, "two", "two");
    dictAdd(d, "three", "three");
    dictAdd(d, "four", "four");
    dictAdd(d, "five", "five");
    dictAdd(d, "six", "six");

    printf("\n================= fetch =================\n");
    dictIterator *dIter = dictGetIterator(d);
    dictEntry *node;
    while ((node = dictNext(dIter)) != NULL) {
        printf("node key:%s, val:%s\n", (char *)node->key, (char *)node->v.val);
    }

    printf("\n================= find =================\n");
    dictEntry *entry = dictFind(d, "two");
    if (entry) {
       printf("find key:%s, val:%s\n", (char *)entry->key, (char *)entry->v.val);
    } else {
        printf("没找到对应的值\n");
    }

    printf("\n================= fetch val =================\n");
    void *ret = dictFetchValue(d, "six");
    if (ret) {
        printf("fetch val:%s\n", (char *)ret);
    }

    printf("\n================= replace val =================\n");
    int res = dictReplace(d, "three", "ten");
    if (res) {
        printf("新增成功\n");
    } else {
        printf("替换成功\n");
    }

    printf("\n================= delete =================\n");
    dictDelete(d, "four");
    dIter = dictGetIterator(d);
     while ((node = dictNext(dIter)) != NULL) {
        printf("node key:%s, val:%s\n", (char *)node->key, (char *)node->v.val);
    }

    printf("\n================= random =================\n");
    entry = dictGetRandomKey(d);
     if (entry) {
       printf("find key:%s, val:%s\n", (char *)entry->key, (char *)entry->v.val);
    } else {
        printf("没找到对应的值\n");
    }

    dictRelease(d);
}


int main() {

    test_case_3();
    return 0;
}