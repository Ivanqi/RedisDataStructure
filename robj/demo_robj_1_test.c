#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "demo_robj_1.h"

void test_case_1() {

    char string[20] = "hello ivan";

    robj *stringObj = createStringObject(string, strlen(string));
    assert(stringObj != NULL);

    printf("stringObj->ptr:%s\n", stringObj->ptr);

    robj *encodeObj = tryObjectEncoding(stringObj);
    if (!encodeObj) {
        printf("不是数字字符串\n");
    }

    robj *longObj_1 = createStringObjectFromLongLong(1000);
    assert(longObj_1 != NULL);

    robj *longObj_2 = createStringObjectFromLongLong(20000);
    assert(longObj_2 != NULL);

    robj *doubleObj = createStringObjectFromLongDouble(943.343);
    assert(doubleObj != NULL);

    robj *dupStringObj = dupStringObject(stringObj);
    assert(dupStringObj != NULL);

    long long llval;
    int ret = isObjectRepresentableAsLongLong(longObj_2, &llval);
    if (ret == 0) {
        printf("是long long 类型, llval:%lld\n", llval);
    } else {
        printf("不是long long 类型\n");
    }

    freeStringObject(stringObj);
    freeStringObject(longObj_1);
    freeStringObject(longObj_2);
    freeStringObject(doubleObj);
    freeStringObject(dupStringObj);
}

void test_case_2() {

    char string[20] = "123456789";
    robj *numObject = createStringObject(string, strlen(string));
    assert(numObject != NULL);

    size_t len = stringObjectLen(numObject);
    printf("len:%d\n", len);

    robj *encodeObj = tryObjectEncoding(numObject);
    assert(encodeObj != NULL);

    printf("ptr:%lld, encoding: %d, refcount:%d\n", encodeObj->ptr, encodeObj->encoding, encodeObj->refcount);
    size_t len2 = stringObjectLen(encodeObj);
    printf("len2:%d\n", len2);


    char string2[20] = "1000";
    robj *numObject2 = createStringObject(string2, strlen(string2));
    assert(numObject2 != NULL);

    robj *encodeObj2 = tryObjectEncoding(numObject2);
    assert(encodeObj2 != NULL);

    printf("ptr:%lld, encoding: %d, refcount:%d\n", encodeObj2->ptr, encodeObj2->encoding, encodeObj2->refcount);

    int ret = compareStringObjects(numObject2, encodeObj2); 
    if (ret){
        printf("相等\n");
    } else {
        printf("不相等\n");
    }

    freeStringObject(numObject);
    freeStringObject(encodeObj);
    freeStringObject(numObject2);
    freeStringObject(encodeObj2);
}

int main() {
    createSharedObjects();

    test_case_2();
    return 0;
}