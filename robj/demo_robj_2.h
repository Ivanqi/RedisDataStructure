#ifndef __ROBJ_2_H
#define __ROBJ_2_H

#include <limits.h>
#include "demo_sds_2.h"

#define LRU_BITS 24
#define LRU_CLOCK_RESOLUTION 1000
#define LRU_CLOCK_MAX (( 1 << LRU_BITS ) - 1)

#define OBJ_SHARED_REFCOUNT INT_MAX

#define OBJ_ENCODING_RAW 0      // string采用原生表示方式，即用sds
#define OBJ_ENCODING_INT 1      // string采用数字的表示方式，实际是long类型
#define OBJ_ENCODING_EMBSTR 8   // string采用一种特殊的嵌入式的sds来表示

#define OBJ_ENCODING_EMBSTR_SIZE_LIMIT 14


#define CONFIG_DEFAULT_HZ 10

/* The actual Redis Object */
#define OBJ_STRING 0    /* String object. */
#define OBJ_LIST 1      /* List object. */
#define OBJ_SET 2       /* Set object. */
#define OBJ_ZSET 3      /* Sorted set object. */
#define OBJ_HASH 4      /* Hash object. */

#define C_OK 0
#define C_ERR -1

#define OBJ_SHARED_INTEGERS 10000

#define sdsEncodedObject(objptr) (objptr->encoding == OBJ_ENCODING_RAW || objptr->encoding == OBJ_ENCODING_EMBSTR)

#define REDIS_COMPARE_BINARY (1 << 0)
#define REDIS_COMPARE_COLL (1 << 1)

/**
 * robj的作用
 *  1. 为多种数据类型提供一种统一的表示方式
 *  2. 允许同一类型的数据采用不同的内部表示，从而在某些情况下尽量节省内存
 *  3. 支持对象共享和引用计数。当对象被共享的时候，只占用一份内存拷贝，进一步节省内存
 */
typedef struct redisObject {
    unsigned type:4;            // 对象数据类型， 占4bit
    unsigned encoding:4;        // 对象内部表示方式，占4bit
    unsigned lru:LRU_BITS;      // 做LRU替换算法用，占24bit
    int refcount;               // 引用计数
    void *ptr;                  // 数据指针
} robj;

struct redisServer {
    int hz;                  // serverCron（）调用频率
    unsigned int lruclock;   // LRU驱逐时钟
};

/*
 * 共享对象
 */
struct sharedObjectsStruct {
    robj *integers[OBJ_SHARED_INTEGERS];
    sds minstring, maxstring;
};

void initServerConfig();

static long long mstime(void);

unsigned int getLRUClock(void);

unsigned int LRU_CLOCK(void);

robj *createObject(int type, void *ptr);

robj *makeObjectShared(robj *o);

robj *createRawStringObject(const char *ptr, size_t len);

robj *createEmbeddedStringObject(const char *ptr, size_t len);

robj *createStringObject(const char *ptr, size_t len);

robj *createStringObjectFromLongLongWithOptions(long long value, int valueobj);

robj *createStringObjectFromLongLong(long long value);

robj *createStringObjectFromLongLongForValue(long long value);

robj *createStringObjectFromLongDouble(long double value, int humanfriendly);

robj *dupStringObject(const robj *o);

void freeStringObject(robj *o);

void decrRefCountVoid(void *o);

robj *resetRefCount(robj *obj);

int isSdsRepresentableAsLongLong(sds s, long long *llval);

int isObejctReresentableAsLongLong(robj *o, long long *llval);

void trimStringObjectIfNeeded(robj *o);

robj *tryObjectEncoding(robj *o);

robj *getDecodedObject(robj *o);

int compareStringObjectsWithFlags(robj *a, robj *b, int flags);

int compareStringObjects(robj *a, robj *b);

int collateStringObjects(robj *a, robj *b);

int equalStringObjects(robj *a, robj *b);

size_t stringObjectLen(robj *o);

int getDoubleFromObject(const robj *o, double *target);

int getLongDoubleFromObject(robj *o, long double *target);

int getLongLongFromObject(robj *o, long long *targe);



#endif
