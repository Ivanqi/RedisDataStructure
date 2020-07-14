#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "demo_sds_2.h"
#include "demo_robj_2.h"
#include "demo_robj_2_util.h"

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

    robj *doubleObj = createStringObjectFromLongDouble(943.343, LD_STR_HUMM);
    assert(doubleObj != NULL);

    robj *dupStringObj = dupStringObject(stringObj);
    assert(dupStringObj != NULL);

    freeStringObject(stringObj);
    freeStringObject(longObj_1);
    freeStringObject(longObj_2);
    freeStringObject(doubleObj);
    freeStringObject(dupStringObj);
}
int main() {

    return 0;
}