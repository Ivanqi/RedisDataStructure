#include <stdint.h>

#ifndef __DICT_2_H
#define __DICT_2_H

// 操作返回状态
#define DICT_OK 0
#define DICT_ERR 1

#define DICT_NOTUSED(V) ((void) V)

// 哈希表节点
typedef struct dictEntry {
    // 键
    void *key;
    // 值
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
    // 链往后继节点
    struct dictEntry *next;
} dictEntry;

// 特定于类型的一簇处理函数
typedef struct dictType {
    // 计算键的哈希值函数，计算key在 hash table中的存储位置，不同的dict可以有不同的hash function
    uint64_t (*hashFunction)(const void *key);

    // 复制键的函数
    void *(*keyDup)(void *privdata, const void *key);

    // 复制值的函数
    void *(*valDup)(void *privdata, const void *obj);

    // 对比两个键的函数
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);

    // 键的析构函数
    void (*keyDestructor)(void *privdata, void *key);

    // 值的析构函数
    void (*valDestructor)(void *privdata, void *key);
} dictType;

// 哈希表
typedef struct dictht {
    // 哈希表节点指针数组(俗称桶，bucket)
    dictEntry **table;

    // 指数数组的大小
    unsigned long size;

    // 指针数组的长度掩码
    unsigned long sizemask;
    
    // 哈希表现在的节点数量
    unsigned long used;
} dictht;

/**
 * 字典
 * 
 * 每个字典使用两个哈希表，用于实现渐进式 rehash
 */
typedef struct dict {
    // 特定于类型的处理函数
    dictType *type;

    // 类型处理函数的私有数量
    void *privdata;

    // 哈希表(2个)
    dictht ht[2];

    // 记录rehash 进度的标志，值为-1，表示rehash 未进行
    long rehashidx;

    // 当前正在运行的安全迭代器数量
    unsigned long iterators;
} dict;


/**
 * 字典迭代器
 * 如果 safe 属性的值为1，那么表示这个迭代器是一个安全的迭代器
 * 当安全迭代器正在迭代一个字典时，该字典仍然可以调用dictAdd, dictFind和其他函数
 * 
 * 如果 safe 属性的值为 0， 那么表示这不是一个安全迭代器
 * 如果正在运行的迭代器是不安全迭代器，那么它只可以对字段调用 dictNex函数
 */
typedef struct dictIterator {
    // 正在迭代的字典
    dict *d;

    // 正在迭代的哈希表数组索引
    long index;
    int table, safe;                // table: 正在迭代的哈希表的号码(0或者1), safe: 是否安全?
    dictEntry *entry, *nextEntry;   // entry: 当前哈希节点, nextEntry: 当前哈希节点的后继节点
    // 用于误用检测的不安全迭代器的指纹
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);
typedef void (dictScanBucketFunction)(void *privdata, dictEntry **bucketref);

// 所有哈希表的起始大小
#define DICT_HT_INITIAL_SIZE 4

#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

#define dictSetSignedIntegerVal(entry, _val_) \
    do { (entry)->v.s64 = _val_; } while(0)

#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { (entry)->v.u64 = _val_; } while(0)

#define dictSetDoubleVal(entry, _val_) \
    do { (entry)->v.d = _val_; } while(0)

#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))


#define dictHashKey(d, key) (d)->type->hashFunction(key)

#define dictGetKey(he) ((he)->key)

#define dictGetVal(he) ((he)->v.val)

#define dictGetSignedIntegerVal(he) ((he)->v.s64)

#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)

#define dictGetDoubleVal(he) ((he)->v.d)

#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)

#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)

#define dictIsRehashing(d) ((d)->rehashidx != -1)

// 创建一个新的字典，O(1)
dict *dictCreate(dictType *type, void *privDataPtr);

int dictExpand(dict *d, unsigned long size);

// 将给定的键值对添加到字段里面, O(1)
int dictAdd(dict *d, void *key, void *val);

dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing);

dictEntry *dictAddOrFind(dict *d, void *key);

// 将给定的键值对添加到字典里面，如果键已经存在于字典，那么用新值取代原有的值
int dictReplace(dict *d, void *key, void *val);

// 从字典中删除给定键所对应的键值对
int dictDelete(dict *d, const void *key);

dictEntry *dictUnlink(dict *ht, const void *key);

void dictFreeUnlinkedEntry(dict *d, dictEntry *he);

void dictRelease(dict *d);

dictEntry * dictFind(dict *d, const void *key);

// 返回给定键的值, O(1)
void *dictFetchValue(dict *d, const void *key);

int dictResize(dict *d);

dictIterator *dictGetIterator(dict *d);

dictIterator *dictGetSafeIterator(dict *d);

dictEntry *dictNext(dictIterator *iter);

void dictReleaseIterator(dictIterator *iter);

// 从字典中随机返回一个键值对，O(1)
dictEntry *dictGetRandomKey(dict *d);

dictEntry *dictGetFairRandomKey(dict *d);

unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);

void dictGetStats(char *buf, size_t bufsize, dict *d);

uint64_t dictGenHashFunction(const void *key, int len);

uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len);

void dictEmpty(dict *d, void(callback)(void*));

void dictEnableResize(void);

void dictDisableResize(void);

int dictRehash(dict *d, int n);

int dictRehashMilliseconds(dict *d, int ms);

void dictSetHashFunctionSeed(uint8_t *seed);

uint8_t *dictGetHashFunctionSeed(void);

unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, dictScanBucketFunction *bucketfn, void *privdata);

uint64_t dictGetHash(dict *d, const void *key);

dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash);


extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif
