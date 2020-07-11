#ifndef __ROBJ_1_H
#define __ROBJ_1_H

#define REDIS_ENCODING_RAW 0    // 简单动态字符串
#define REDIS_ENCODING_INT 1    // long 类型的整数
#define REDIS_SHARED_INTEGERS 10000

/*
 * 对象类型
 */
#define REDIS_STRING 0  // 字符串对象
#define REDIS_LIST 1    // 列表对象
#define REDIS_SET 2     // 集合对象
#define REDIS_ZSET 3    // 有序集合对象
#define REDIS_HASH 4    // 哈希对象

#define REDIS_ERR -1
#define REDIS_OK 0

// redis 对象
typedef struct redisObject {

    // 类型
    unsigned type:4;

    // 不使用(对齐位)
    unsigned notused:2;

    // 编码方式
    unsigned encoding:4;

    // LRU时间 (相当于server.lrulock)
    unsigned lru:22;

    // 引用计数
    int refcount;

    // 指向对象的值
    void *ptr;
} robj;

/*
 * 共享对象
 */
struct sharedObjectsStruct {
    robj *integers[REDIS_SHARED_INTEGERS];
};

robj *createObject(int type, void *ptr);

robj *createStringObject(char *ptr, size_t len);


void incrRefCount(robj *o);

void createSharedObjects(void);

robj *createStringObjectFromLongLong(long long value);

robj *createStringObjectFromLongDouble(long double value);

robj *dupStringObject(robj *o);

void freeStringObject(robj *o);

robj *resetRefCount(robj *obj);

int isObjectRepresentableAsLongLong(robj *o, long long *llval);

robj *tryObjectEncoding(robj *o);

robj *getDecodedObject(robj *o);

int compareStringObjects(robj *a, robj *b);

int equalStringObjects(robj *a, robj *b);

size_t stringObjectLen(robj *o);

int getDoubleFromObject(robj *o, double *target);

int getLongDoubleFromObject(robj *o, long double *target);

#endif