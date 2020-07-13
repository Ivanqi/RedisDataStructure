#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include "demo_robj_2.h"
#include "demo_sds_2.h"
#include "demo_robj_2_util.h"

struct redisServer server;
struct sharedObjectsStruct shared;


void initServerConfig() {

    server.hz = CONFIG_DEFAULT_HZ;
    server.lruclock = getLRUClock();
}

void createSharedObjects(void) {

    int j;
    for (j = 0; j < OBJ_SHARED_INTEGERS; j++) {
        shared.integers[j] = makeObjectShared(createObject(OBJ_STRING, (void *)(long)j));
    }

    shared.minstring = sdsnew("minstring");
    shared.maxstring = sdsnew("maxstring");
}

unsigned int getLRUClock(void) {

    return (mstime() / LRU_CLOCK_RESOLUTION) & LRU_CLOCK_MAX;
}

unsigned int LRU_CLOCK(void) {

    unsigned int lruclock;
    if (1000 / server.hz <= LRU_CLOCK_RESOLUTION) {
        lruclock = server.lruclock;
    } else {
        lruclock = getLRUClock();
    }
    return lruclock;
}

void freeStringObject(robj *o) {

    if (o->encoding == OBJ_ENCODING_RAW) {
        sdsfree(o->ptr);
    }
}

void increRefCount(robj *o) {

    if (o->refcount != OBJ_SHARED_REFCOUNT) {
        o->refcount++;
    }
}

void decrRefCount(robj *o) {

    if (o->refcount == 1) {
        switch (o->type) {
            case OBJ_STRING:
                freeStringObject(o);
                break;
            default: printf("Unknown object type\n"); break;
        }
        free(o);
    } else {
        if (o->refcount <= 0) {
            printf("decrRefCount against refcount <= 0");
            return;
        }

        if (o->refcount != OBJ_SHARED_REFCOUNT) o->refcount--;
    }
}

robj *createObject(int type, void *ptr) {

    robj *o = malloc(sizeof(*o));
    o->type = type;
    o->encoding = OBJ_ENCODING_RAW;
    o->ptr = ptr;
    o->refcount = 1;

    o->lru = LRU_CLOCK();
}

robj *makeObjectShared(robj *o) {
    assert(o->refcount == 1);
    o->refcount = OBJ_SHARED_REFCOUNT;
    return o;
}

robj *createRawStringObject(const char *ptr, size_t len) {

    return createObject(OBJ_STRING, sdsnewlen(ptr, len));
}

robj *createEmbeddedStringObject(const char *ptr, size_t len) {

    robj *o = malloc(sizeof(robj) + sizeof(struct sdshdr8) + len + 1);
    struct sdshdr8 *sh = (void *)(o + 1);

    o->type = OBJ_STRING;
    o->encoding = OBJ_ENCODING_EMBSTR;
    o->ptr = sh + 1;
    o->refcount = 1;
    o->lru = LRU_CLOCK();

    sh->len = len;
    sh->alloc = len;
    sh->flags = SDS_TYPE_8;

    if (ptr == SDS_NOINIT) {
        sh->buf[len] = '\0';
    } else if (ptr) {
        memcpy(sh->buf, ptr, len);
        sh->buf[len] = '\0';
    } else {
        memset(sh->buf, 0, len + 1);
    }

    return o;
}

robj *createStringObject(const char *ptr, size_t len) {

    if (len <= OBJ_ENCODING_EMBSTR_SIZE_LIMIT) {
        return createEmbeddedStringObject(ptr, len);
    } else {
        return createRawStringObject(ptr, len);
    }
}

robj *createStringObjectFromLongLongWithOptions(long long value, int valueobj) {

    robj *o;

    if (value >= 0; value < OBJ_SHARED_REFCOUNT && valueobj == 0) {
        increRefCount(shared.integers[value]);
        o = shared.integers[value];
    } else {
        if (value >= LONG_MIN && value <= LONG_MAX) {
            o = createObject(OBJ_STRING, NULL);
            o->encoding = OBJ_ENCODING_INT;
            o->ptr = (void *)((long)value);
        } else {
            o = createObject(OBJ_STRING, sdsfromlonglong(value));
        }
    }

    return o;
}

// 通过 createStringObjectFromLongLong 创建共享对象
robj *createStringObjectFromLongLong(long long value) {

    return createStringFromLongLongWithOptions(value, 0);
}

// 通过 createStringObjectFromLongLongForValue 避免共享
robj *createStringObjectFromLongLongForValue(long long value) {

    return createStringObjectFromLongLongWithOptions(value, 1);
}

// 从一个长双精度对象创建一个字符串对象
robj *createStringObjectFromLongDouble(long double value, int humanfriendly) {

    char buf[MAX_LONG_DOUBLE_CHARS];
    int len = ld2string(buf, sizeof(buf), value, humanfriendly ? LD_STR_HUMM : LD_STR_AUTO);
    return createStringObject(buf, len);
}

robj *dupStringObject(const robj *o) {

    robj *d;

    assert(o->type == OBJ_STRING);

    switch (o->encoding) {
        case OBJ_ENCODING_RAW:
            return createRawStringObject(o->ptr, sdslen(p->ptr));

        case OBJ_ENCODING_EMBSTR:
            return createEmbeddedStringObject(o->ptr, sdslen(o->ptr));

        case OBJ_ENCODING_INT:
            d = createObject(OBJ_STRING, NULL);
            d->encoding = OBJ_ENCODING_INT;
            d->ptr = o->ptr;
            return d;
        default:
            printf("Wrong encoding.");
            break;
    }
}

void freeStringObject(robj *o) {

    if (o->encoding == OBJ_ENCODING_RAW) {
        sdsfree(o->ptr);
    }
}

// void类型，引用减少函数
void decrRefCountVoid(void *o) {
    decrRefCount(o);
}

robj *resetRefCount(robj *obj) {
    obj->refcount = 0;
    return obj;
}

int isSdsRepresentableAsLongLong(sds s, long long *llval) {

    return string2ll(s, sdslen(s), llval) ? C_OK : C_ERR;;
}