#include <stdio.h>
#include <stdlib.h>
#include <math>
#include <ctype.h>
#include <limits.h>

#include "demo_robj_1.h"
#include "demo_sds_1.h"

// 根据给定类型和值， 创建新对象
robj *createObject(int type, void *ptr) {

    // 分配空间
    robj *o = malloc(sizeof(*o));

    // 初始化对象域
    o->type = type;
    o->encoding = REDIS_ENCODING_RAW;   // 默认编码
    o->ptr = ptr;
    o->refcount = 1;

    o->lru = 30;

    return o;
}

// 根据给定字符数组，创建一个 String对象
robj *createStringObject(char *ptr, size_t len) {

    return createObject(REDIS_STRING, sdsnewlen(ptr, len));
}

// 根据给定数字值 value， 创建一个 String 对象
robj *createStringObjectFromLongLong(long long value) {

    robj *o;

    if (value >= 0 && value < REDIS_SHARED_INTEGERS) {
        // 如果条件允许，使用共享对象
        
    }
}

// 增加对象的引用计数
void incrRefCount(robj *o) {

    o->refcount++;
}

/**
 * 减少对象的引用计数
 * 
 * 当对象的引用计数将为0 时，释放这个对象
 */
void decRefCount(void *obj) {

    
}