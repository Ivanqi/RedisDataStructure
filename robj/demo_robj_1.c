#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "demo_robj_1.h"
#include "demo_sds_1.h"
#include "demo_robj_util_1.h"

// 共用共享数组
struct sharedObjectsStruct shared;

void createSharedObjects(void) {

    int j;
    for (j = 0; j < REDIS_SHARED_INTEGERS; j++) {
        shared.integers[j] = createObject(REDIS_STRING, (void*)(long)j);
        shared.integers[j]->encoding = REDIS_ENCODING_INT;
    }
}

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
        incrRefCount(shared.integers[value]);
        o = shared.integers[value];
    } else {
        // 否则，创建新String 对象
        if (value >= LONG_MIN && value <= LONG_MAX) {
            // long 类型的数字值以long类型保存
            o = createObject(REDIS_STRING, NULL);
            o->encoding = REDIS_ENCODING_INT;   // 设置编码
            o->ptr = (void *)((long)value);
        } else {
            // long long 类型的数字值编码成字符串来保存
            o = createObject(REDIS_STRING, sdsfromlonglong(value));
        }
    }

    return o;
}

// 根据给定 long double值value，创建string 对象
robj *createStringObjectFromLongDouble(long double value) {

    char buf[256];
    int len;

    // 将 long double值转换成字符串
    len = snprintf(buf, sizeof(buf), "%.17LF", value);
    // 现在删除“.”后面的零
    if (strchr(buf, '.') != NULL) {
        char *p = buf + len - 1;
        while (*p == '0') {
            p--;
            len--;
        }

        if (*p == '.') len--;
    }

    return createStringObject(buf, len);
}

// 复制一个String对象的副本
robj *dupStringObject(robj *o) {

    assert(o != NULL);
    assert(o->encoding == REDIS_ENCODING_RAW);

    return createStringObject(o->ptr, sdslen(o->ptr));
}

// 释放string 对象
void freeStringObject(robj *o) {

    if (o->encoding == REDIS_ENCODING_RAW) {
        sdsfree(o->ptr);
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
            case REDIS_STRING: 
                freeStringObject(o);
                break;
            default: printf("Unknown object type\n"); break;
        }

        // 释放对象本身
        free(o);
    } else {
        // 否则，只降低引用数
        o->refcount--;
    }
}

// 将对象的引用数设置为0，但不释放对象
robj *resetRefCount(robj *obj) {

    obj->refcount = 0;
    return obj;
}

// 检查给定的string 对象是否表示为 long long 类型值
int isObjectRepresentableAsLongLong(robj *o, long long *llval) {

    assert(o != NULL);
    assert(o->type == REDIS_STRING);

    if (o->encoding == REDIS_ENCODING_INT) {
        if (llval) {
            *llval = (long) o->ptr; 
        }
        return REDIS_OK;
    } else {
        return string2ll(o->ptr, sdslen(o->ptr), llval) ? REDIS_OK : REDIS_ERR;
    }
}

// 尝试将对象o编码成整数，并尝试将它加入到共享对象里面
robj *tryObjectEncoding(robj *o) {

    long value;
    sds s = o->ptr;

    // 对象已编码
    if (o->encoding != REDIS_ENCODING_RAW) {
        return 0;
    }

    // 不对共享对象进行编码
    if (o->refcount > 1) return 0;

    // 只尝试对 string 对象进行编码
    assert(o != NULL);
    assert(o->type == REDIS_STRING);

    /**
     * 尝试将字符串值转换为 long函数
     * 转换失败直接返回0， 转换成功时继续执行
     */
    if (!string2l(s, sdslen(s), &value)) {
        return 0;
    }

    // 执行到这一行时，value保存的已经是一个long整数
    if (value >= 0 && value < REDIS_SHARED_INTEGERS) {
        /**
         * 看看value是否属于可共享值的范围
         * 如果是的化就用共享对象代替这个对象 o
         */
        

        // 将共享对象返回
        return shared.integers[value];
    } else {
        // value 不属于共享范围，将它保存到对象 o中
        o->encoding = REDIS_ENCODING_INT;   // 更新编码方式
        sdsfree(o->ptr);            // 释放旧值
        o->ptr = (void *) value;    // 设置新值

        return o;
    }
}

/**
 * 返回一个对象的未编码版本
 * 
 * 如果输入对象是已编码的，那么返回的对象是输入对象的新对象副本
 * 如果输入对象是未编码的，那么为它的引用计数增一，然后返回它
 */
robj *getDecodedObject(robj *o) {

    robj *dec;

    // 返回未编码对象
    if (o->encoding == REDIS_ENCODING_RAW) {
        incrRefCount(o);
        return 0;
    }

    // 返回已编码对象的未编码版本
    if (o->type == REDIS_STRING && o->encoding == REDIS_ENCODING_INT) {
        char buf[32];

        // 将整数值转换回一个字符串对象
        ll2string(buf, 32, (long)o->ptr);
        dec = createStringObject(buf, strlen(buf));

        return dec;
    } else {
        printf("Unknown encoding type");
    }
}

// 两个字符串对象比较
int compareStringObjects(robj *a, robj *b) {

    assert(a != NULL);
    assert(a->type == REDIS_STRING);
    assert(b->type == REDIS_STRING);

    char bufa[128], bufb[128], *astr, *bstr;
    int bothsds = 1;

    if (a == b) return 0;
    if (a->encoding != REDIS_ENCODING_RAW) {
        ll2string(bufa, sizeof(bufa), (long) a->ptr);
        astr = bufa;
        bothsds = 0;
    } else {
        astr = a->ptr;
    }

    if (b->encoding != REDIS_ENCODING_RAW) {
        ll2string(bufb, sizeof(bufb), (long)b->ptr);
        bstr = bufb;
        bothsds = 0;
    } else {
        bstr = b->ptr;
    }

    return bothsds ? sdscmp(astr, bstr) : strcmp(astr, bstr);
}

int equalStringObjects(robj *a, robj *b) {

    if (a->encoding != REDIS_ENCODING_RAW && b->encoding != REDIS_ENCODING_RAW) {
        return a->ptr == b->ptr;
    } else {
        return compareStringObjects(a, b) == 0;
    }
}

size_t stringObjectLen(robj *o) {

    assert(o != NULL);
    assert(o->type == REDIS_STRING);

    if (o->encoding == REDIS_ENCODING_RAW) {
       return sdslen(o->ptr);
    } else {
        char buf[32];
        return ll2string(buf, 32, (long)o->ptr);
    }
}

int getDoubleFromObject(robj *o, double *target) {

    double value;
    char *eptr;

    if (o == NULL) {
        value = 0;
    } else {
        assert(o != NULL);
        assert(o->type == REDIS_STRING);

        if (o->encoding == REDIS_ENCODING_RAW) {
            errno = 0;
            value = strtod(o->ptr, &eptr);;

            if (isspace(((char*)o->ptr)[0]) || eptr[0] != '\0' || errno == ERANGE || isnan(value)) {
                return REDIS_ERR;
            }
        } else if (o->encoding == REDIS_ENCODING_INT) {
            value = (long)o->ptr;
        } else {
            printf("Unknown string encoding");
            exit(1);
        }
    }

    *target = value;
    return REDIS_OK;
}

int getLongDoubleFromObject(robj *o, long double *target) {

    long double value;
    char *eptr;

    if (o == NULL) {
        value = 0;
    } else {
        assert(o != NULL);
        assert(o->type != REDIS_STRING);
        if (o->encoding == REDIS_ENCODING_RAW) {
            errno = 0;
            value = strtold(o->ptr, &eptr);
            if (isspace(((char*)o->ptr)[0]) || eptr[0] != '\0' || errno == ERANGE || isnan(value)) {
                return REDIS_ERR;
            }
        } else if (o->encoding == REDIS_ENCODING_INT) {
            value = (long)o->ptr;
        } else {
            printf("Unknown string encoding");
        }
    }
    *target = value;
    return REDIS_OK;
}

/**
 * 从对象o中提取 long long 值，并使用指针保存所提取的值
 * robj *o: 提取对象
 * long long *target: 保存值的指针
 * */

int getLongLongFromObject(robj *o, long long *target) {

    long long value;
    char *eptr;

    if (o == NULL) {
        value = 0;
    } else {
        assert(o != NULL);
        assert(o->type == REDIS_STRING);

        // 根据不同编码，取出值
        if (o->encoding == REDIS_ENCODING_RAW) {
            errno = 0;
            value = strtoll(o->ptr, &eptr, 10);
            if (isspace(((char*)o->ptr)[0]) || eptr[0] != '\0' || errno == ERANGE) {
                return REDIS_ERR;
            }
        } else if (o->encoding == REDIS_ENCODING_INT) {
            value = (long)o->ptr;
        } else {
            printf("Unknown string encoding");
            exit(1);
        }
    }

    if (target) *target = value;
    return REDIS_OK;
}