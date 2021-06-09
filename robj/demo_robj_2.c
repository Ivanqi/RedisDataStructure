#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
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

static long long mstime(void) {

    struct timeval tv;
    long long mst;

    gettimeofday(&tv, NULL);
    mst = ((long long)tv.tv_sec) * 1000;
    mst += tv.tv_usec / 1000;
    return mst;
}

void createSharedObjects(void) {

    int j;
    for (j = 0; j < OBJ_SHARED_INTEGERS; j++) {
        shared.integers[j] = makeObjectShared(createObject(OBJ_STRING, (void*)(long)j));
        shared.integers[j]->encoding = OBJ_ENCODING_INT;
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

void incrRefCount(robj *o) {

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

/**
 * createEmbeddedStringObject对sds重新分配内存
 * 将robj和sds放在一个连续的内存块中分配，这样对于短字符串的存储有利于减少内存碎片
 * 这个连续的内存块包含如下几部分
 *  16个字节的robj结构
 *  3个字节的sdshdr8头
 *  最多14个字节的sds字符数组
 *  1个NULL结束符
 * 加起来一共不超过34字节（16+3+14+1）
 */
robj *createEmbeddedStringObject(const char *ptr, size_t len) {

    robj *o = malloc(sizeof(robj) + sizeof(struct sdshdr8) + len + 1);
    // o + 1相当于 o + sizeof(robj)， robj为16字节
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

    if (value >= 0 && value < OBJ_SHARED_INTEGERS && valueobj == 0) {
        incrRefCount(shared.integers[value]);
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

    return createStringObjectFromLongLongWithOptions(value, 0);
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
            return createRawStringObject(o->ptr, sdslen(o->ptr));

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

int isObejctReresentableAsLongLong(robj *o, long long *llval) {

    assert(o != NULL);
    assert(o->type == OBJ_STRING);

    if (o->encoding == OBJ_ENCODING_INT) {
        if (llval) {
            *llval = (long)o->ptr;
        }
        return C_OK;
    } else {
        return isSdsRepresentableAsLongLong(o->ptr, llval);
    }
}

// 优化string对象的SDS字符串，使其只需要很少的空间
void trimStringObjectIfNeeded(robj *o) {

    if (o->encoding == OBJ_ENCODING_RAW && sdsavail(o->ptr) > sdslen(o->ptr) / 10) {
        o->ptr = sdsRemoveFreeSpace(o->ptr);
    }
}

/**
 * 第1步：检查type。确保只对string类型的对象进行操作
 * 第2步：检查encoding。 使用sdsEncodedObject宏，确保只对OBJ_ENCODING_RAW和OBJ_ENCODING_EMBSTR编码的string操作
 * 第3步：检查refcount
 *  引用计数大于1的共享对象，在多处被引用
 *  由于编码过程结束后robj对象指针可能会变化，这样对引用计数大于1的对象，就需要更新所有地方的引用，这不容易做到。因此，对于计数大于1的不做编码处理
 *  试图将字符串转成64位long。64位long所能表达的数据范围是 -2 ^ 63 到 2 ^ 63 - 1，用十进制表达出来最长是20位数
 * 第4步：在转成long成功时，又分成两种情况
 *  1. 第一种情况：如果Redis的配置不要求运行LRU替换算法，且转成的long型数字的值又比较小
 *      值小于小于OBJ_SHARED_INTEGERS，在目前的实现中这个值是10000
 *      那么会使用共享数字对象来表示。之所以这里的判断跟LRU有关，是因为LRU算法要求每个robj有不同的lru字段值，所以用了LRU就不能共享robj
 *      shared.integers是一个长度为10000的数组，里面预存了10000个小的数字对象
 *      这些小数字对象都是encoding = OBJ_ENCODING_INT的string robj对象
 *  2. 第二种情况：如果前一步不能使用共享小对象来表示，那么将原来的robj编码成encoding = OBJ_ENCODING_INT，这时ptr字段直接存成这个long型的值
 *      注意ptr字段本来是一个void*指针（即存储的是内存地址），因此在64位机器上有64位宽度，正好能存储一个64位的long型值
 *      这样，除了robj本身之外，它就不再需要额外的内存空间来存储字符串值
 * 第5步：对于哪些不能转成64位long的字符串进行处理。最后再做两步处理
 *  1. 如果字符串长度足够小（小于等于OBJ_ENCODING_EMBSTR_SIZE_LIMIT，定义14）
 *      那么调用createEmbeddedStringObject 编码成encoding = OBJ_ENCODING_EMBSTR
 *  2. 如果前面所有的编码尝试都没有成功（仍然是OBJ_ENCODING_RAW），且sds里空余字节过多
 *      做最后一次努力，调用sds的sdsRemoveFreeSpace接口来释放空余字节。     
 */
robj *tryObjectEncoding(robj *o) {

    long value;
    sds s = o->ptr;
    size_t len;

    assert(o != NULL);
    assert(o->type == OBJ_STRING);

    if (!sdsEncodedObject(o)) {
        return o;
    }

    if (o->refcount > 1) return o;

    len = sdslen(s);

    if (len <= 20 && string2l(s, len, &value)) {
        if (value >= 0 && value < OBJ_SHARED_INTEGERS) {
            decrRefCount(o);
            incrRefCount(shared.integers[value]);
            return shared.integers[value];
        } else {
            if (o->encoding == OBJ_ENCODING_RAW) {
                sdsfree(o->ptr);
                o->encoding = OBJ_ENCODING_INT;
                o->ptr = (void *) value;
                return o;
            } else if (o->encoding == OBJ_ENCODING_EMBSTR) {
                decrRefCount(o);
                return createStringObjectFromLongLongForValue(value);
            }
        }
    }

    if (len <= OBJ_ENCODING_EMBSTR_SIZE_LIMIT) {
        robj *emb;

        if (o->encoding == OBJ_ENCODING_EMBSTR) {
            return o;
        }

        emb = createEmbeddedStringObject(s, sdslen(s));
        decrRefCount(o);
        return emb;
    }

    trimStringObjectIfNeeded(o);

    return o;
}

/**
 * 编码为OBJ_ENCODING_RAW和OBJ_ENCODING_EMBSTR的字符串robj对象，不做变化，原封不动返回
 *  站在使用者的角度，这两种编码没有什么区别，内部都是封装的sds
 * 编码为数字的字符串robj对象，将long重新转为十进制字符串的形式，然后调用createStringObject转为sds的表示
 *  这里由long转成的sds字符串长度肯定不超过20，而根据createStringObject的实现，它们肯定会被编码成OBJ_ENCODING_EMBSTR的对象
 */
robj *getDecodedObject(robj *o) {

    robj *dec;

    if (sdsEncodedObject(o)) {
        incrRefCount(o);
        return o;
    }

    if (o->type == OBJ_STRING && o->encoding == OBJ_ENCODING_INT) {
        char buf[32];

        ll2string(buf, 32, (long)o->ptr);
        dec = createStringObject(buf, strlen(buf));
        return dec;
    } else {
        printf("Unknown encoding type");
        exit(1);
    }
}

int compareStringObjectsWithFlags(robj *a, robj *b, int flags) {

    assert(a != NULL);
    assert(a->type == OBJ_STRING);
    assert(b->type == OBJ_STRING);

    char bufa[128], bufb[128], *astr, *bstr;
    size_t alen, blen, minlen;

    if (a == b) return 0;

    if (sdsEncodedObject(a)) {
        astr = a->ptr;
        alen = sdslen(astr);
    } else {
        alen = ll2string(bufa, sizeof(bufa), (long) a->ptr);
        astr = bufa;
    }

    if (sdsEncodedObject(b)) {
        bstr = b->ptr;
        blen = sdslen(bstr);
    } else {
        blen = ll2string(bufb, sizeof(bufb), (long)b->ptr);
        bstr = bufb;
    }

    if (flags & REDIS_COMPARE_COLL) {
        return strcoll(astr, bstr);
    } else {
        int cmp;

        minlen = (alen < blen) ? alen : blen;
        cmp = memcmp(astr, bstr, minlen);
        if (cmp == 0) {
            return alen - blen;
        }
        return cmp;
    }
}

int compareStringObjects(robj *a, robj *b) {

    return compareStringObjectsWithFlags(a, b, REDIS_COMPARE_BINARY);
}

int collateStringObjects(robj *a, robj *b) {

    return compareStringObjectsWithFlags(a, b, REDIS_COMPARE_COLL);
}

int equalStringObjects(robj *a, robj *b) {

    if (a->encoding == OBJ_ENCODING_INT && b->encoding == OBJ_ENCODING_INT) {
        return a->ptr == b->ptr;
    } else {
        return compareStringObjects(a, b) == 0;
    }
}

size_t stringObjectLen(robj *o) {

    assert(o != NULL);
    assert(o->type == OBJ_STRING);

    if (sdsEncodedObject(o)) {
        return sdslen(o->ptr);
    } else {
        return sdigits10((long)o->ptr);
    }
}

int getDoubleFromObject(const robj *o, double *target) {

    double value;
    char *eptr;

    if (o == NULL) {
        value = 0;
    } else {
        assert(o != NULL);
        assert(o->type == OBJ_STRING);

        if (sdsEncodedObject(o)) {
            errno = 0;
            value = strtod(o->ptr, &eptr);
            if (sdslen(o->ptr) == 0 || isspace(((const char*)o->ptr)[0]) || 
            (size_t)(eptr - (char *)o->ptr) != sdslen(o->ptr) ||
            (errno == ERANGE && (value == HUGE_VAL || value == -HUGE_VAL || value == 0)) ||
            isnan(value)) {
                return C_ERR;
            }
        } else if (o->encoding == OBJ_ENCODING_INT) {
            value = (long)o->ptr;
        } else {
            printf("Unknown string encoding");
            exit(1);
        }
    }

    *target = value;
    return C_OK;
}

int getLongDoubleFromObject(robj *o, long double *target) {

    long double value;
    char *eptr;

    if (o == NULL) {
        value = 0;
    } else {
        assert(o != NULL);
        assert(o->type == OBJ_STRING);
        if (sdsEncodedObject(o)) {
            errno = 0;
            value = strtold(o->ptr, &eptr);
            if (sdslen(o->ptr) == 0 ||
                isspace(((const char*)o->ptr)[0]) ||
                (size_t)(eptr-(char*)o->ptr) != sdslen(o->ptr) ||
                (errno == ERANGE &&
                    (value == HUGE_VAL || value == -HUGE_VAL || value == 0)) ||
                isnan(value))
                return C_ERR;
        } else if (o->encoding == OBJ_ENCODING_INT) {
            value = (long)o->ptr;
        } else {
            printf("Unknown string encoding");
            exit(1);
        }
    }
    *target = value;
    return C_OK;
}

int getLongLongFromObject(robj *o, long long *target) {

    long long value;

    if (o == NULL) {
        value = 0;
    } else {
        assert(o != NULL);
        assert(o->type == OBJ_STRING);

        if (sdsEncodedObject(o)) {
            if (string2ll(o->ptr, sdslen(o->ptr), &value) == 0) {
                return C_ERR;
            }
        } else if (o->encoding == OBJ_ENCODING_INT) {
            value = (long)o->ptr;
        } else {
            printf("Unknown string encoding");
        }
    }

    if (target) {
        *target = value;
    }
    return C_OK;
}