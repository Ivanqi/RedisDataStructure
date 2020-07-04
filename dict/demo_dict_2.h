#include <stdint.h>

#ifndef __DICT_2_H
#define __DICT_2_H

#define DICT_OK 0
#define DICT_ERR 1

#define DICT_NOTUSED(V) ((void) V)

typedef struct dictEntry {
    void *key;
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
    struct dictEntry *next;
} dictEntry;

typedef struct dictType {
    uint64_t (*hashFunction)(const void *key);
    void *(*keyDup)(void *privdata, const void *key);
    void *(*valDup)(void *privdata, const void *obj);
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    void (*keyDestructor)(void *privdata, void *key);
    void (*valDestructor)(void *privdata, void *key);
} dictType;

typedef struct dictht {
    dictEntry **table;
    unsigned long size;
    unsigned long sizemask;
    unsigned long used;
} dictht;

typedef struct dict {
    dictType *type;
    void *privdata;
    dictht ht[2];
    long rehashidx;
    unsigned long iterators;
} dict;

typedef struct dictIterator {
    dict *d;
    long index;
    int table, safe;
    dictEntry *entry, *nextEntry;
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);
typedef void (dictScanBucketFunction)(void *privdata, dictEntry **bucketref);

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
