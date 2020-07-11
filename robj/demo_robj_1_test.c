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
    assert(encodeObj != NULL);
    printf("stringObj->ptr:%s\n", encodeObj->ptr);

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

int main() {
    createSharedObjects();

    test_case_1();
    return 0;
}