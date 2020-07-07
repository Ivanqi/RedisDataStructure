#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "demo_zskiplist_1.h"

// 释放zset对象
// void freeZsetObject(robj *o) {

//     zset *zs;
//     switch (o->encoding) {
//         // skiplist 表示
//         case REDIS_ENCODING_SKIPLIST:
//             zs = o->ptr;
//             // dictRelease(zs->dict)
//             zslFree(zs->zsl);
//             free(zs);
//             break;
//         default:
//             printf("Unknown sotred set encoding");
//     }
// }

/**
 * 减少对象的引用计数
 * 
 * 当对象的引用计数降为0时，释放这个对象
 */
void decrRefCount(void *obj) {

    robj *o = obj;

    if (o->refcount <= 0) {
        printf("dercRefCount aginst refcount <= 0\n");
        exit(1);
    }

    if (o->refcount == 1) {
        // 如果引用数降为0
        // 根据对象类型，调用相应的对象释放函数来释放对象的值
        switch (o->type) {
            case REDIS_STRING: printf("释放 STRING 类型\n"); break;
            // case REDIS_ZSET: freeZsetObject(o); break;
            default: printf("Unknown object type\n"); break;
        }

        // 释放对象本身
        free(o);
    } else {
        // 否则，只降低引用数
        o->refcount--;
    }
}

int compareStringObjects(robj *a, robj *b) {

    assert(a != NULL);
    assert(a->type == REDIS_STRING);
    assert(b->type == REDIS_STRING);
    char bufa[128], bufb[128], *astr, *bstr;
    int bothsds = 1;

    if (a == b) return 0;
    if (a->encoding != REDIS_ENCODING_RAW) {
        ll2string(bufa, sizeof(bufa), (long)a->ptr);
        astr = bufa;
        bothsds = 0;
    } else {
        astr = a->ptr;
    }

    if (b->encoding != REDIS_ENCODING_RAW) {
        ll2string(bufa, sizeof(bufa), (long) a->ptr);
        astr = bufa;
        bothsds = 0;
    } else {
        bstr = b->ptr;
    }

    // return bothsds ? sdscmp(astr, bstr) : strcmp(astr, bstr);
    return strcmp(astr, bstr);
}

int equalStringObjects(robj *a, robj *b) {

    if (a->encoding != REDIS_ENCODING_RAW && b->encoding != REDIS_ENCODING_RAW) {
        return a->ptr == b->ptr;
    } else {
        return compareStringObjects(a, b) == 0;
    }
}