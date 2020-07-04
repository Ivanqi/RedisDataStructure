#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/time.h>
#include <assert.h>

#include "demo_dict_2.h"

static int dict_can_resize = 1;
static unsigned int dict_force_resize_ratio = 5;

static int _dictExpandIfNeeded(dict *ht);
static unsigned long _dictNextPower(unsigned long size);
static long _dictKeyIndex(dict *ht, const void *key, uint64_t hash, dictEntry **existing);
static int _dictInit(dict *ht, dictType *type, void *privDataPtr);

static uint8_t dict_hash_function_seed[16];

void dictSetHashFunctionSeed(uint8_t *seed) {

    memcpy(dict_hash_function_seed, seed, sizeof(dict_hash_function_seed));
}

uint8_t *dictGetHashFunctionSeed(void) {

    return dict_hash_function_seed;
}

uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);
uint64_t siphash_nocase(const uint8_t *in, const size_t inlen, const uint8_t *k);

uint64_t dictGenHashFunction(const void *key, int len) {

    return siphash(key, len, dict_hash_function_seed);
}

uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len) {

    return siphash_nocase(buf, len, dict_hash_function_seed);
}

// 重置哈希表的各项属性
static void _dictReset(dictht *ht) {

    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

// 创建一个新字典
dict *dictCreate(dictType *type, void *privDataPtr) {

    // 分配空间
    dict *d = malloc(sizeof(*d));

    // 初始化字典
    _dictInit(d, type, privDataPtr);

    return d;
}

// 初始化字典
int _dictInit(dict *d, dictType *type, void *privDataPtr) {

    // 初始化 ht[0]
    _dictReset(&d->ht[0]);

    // 初始化 ht[1]
    _dictReset(&d->ht[1]);

    // 初始化字典属性
    d->type = type;
    d->privdata = privDataPtr;
    d->rehashidx = -1;
    d->iterators = 0;

    return DICT_OK;
}

// 对字典进行紧缩，让节点数/桶数的比率接近 <= 1
int dictResize(dict *d) {

    int minimal;
    
    // 不能在 dict_can_resize 为假
    // 或者字典正在 rehash时调用
    if (!dict_can_resize || dictIsRehashing(d)) return DICT_ERR;

    minimal = d->ht[0].used;

    if (minimal < DICT_HT_INITIAL_SIZE) {
        minimal = DICT_HT_INITIAL_SIZE;
    }

    return dictExpand(d, minimal);
}

/**
 * 创建一个新哈希表，并视情况，进行以下动作之一
 *  1. 如果字典里的ht[0]为空，将新哈希表赋值给它
 *  2. 如果字典里的ht[0]不为空，那么将新哈希表赋值给ht[1],并打开rehash标识
 */
int dictExpand(dict *d, unsigned long size) {

    if (dictIsRehashing(d) || d->ht[0].used > size) {
        return DICT_ERR;
    }

    dictht n;   // 新的哈希表

    // 计算哈希表的真实大小
    unsigned long realsize = _dictNextPower(size);

    if (realsize == d->ht[0].size) return DICT_ERR;

    // 创建并初始化新哈希表
    n.size = realsize;
    n.sizemask = realsize - 1;
    n.table = malloc(realsize * sizeof(dictEntry*));
    n.used = 0;

    // 如果ht[0]为空，那么这就是一次创建新哈希表行为
    // 将新哈希表设置为 ht[0], 然后返回
    if (d->ht[0].table == NULL) {
        d->ht[0] = n;
        return DICT_OK;
    }

    // 如果ht[0]不为空， 那么这就是一次扩展字典的行为
    // 将新哈希表设置为ht[1]， 并打开rehash标识
    d->ht[1] = n;
    d->rehashidx = 0;
    return DICT_OK;
}

/**
 * 执行 N 步渐进式 rehash
 * 
 * 如果执行之后哈希表还有元素需要rehash， 那么返回-1
 * 如果哈希表里面所有元素已经迁移完毕，那么返回0
 * 
 * 每步 rehash 都会移动哈希表数组内某个索引上的整个链表节点
 * 所有从 ht[0] 迁移都 ht[1] 的key 可能不止一个
 */
int dictRehash(dict *d, int n) {

    int empty_vists = n * 10;   // 访问的最大空桶数
    if (!dictIsRehashing(d)) return 0;

    while (n-- && d->ht[0].used != 0) {
        dictEntry *de, *nextde;

        assert(d->ht[0].size > (unsigned) d->rehashidx);

        // 移动到数组中首个不为NULL链表的索引上
        while (d->ht[0].table[d->rehashidx] == NULL) {
            d->rehashidx++;
            if (--empty_vists == 0) return 1;
        }

        // 指向链表头
        de = d->ht[0].table[d->rehashidx];

        /**
         * 将链表内的所有元素从ht[0]迁移到ht[1]
         * 因为桶内的元素通常只有一个，或者不多于某个特定比率
         * 所以可以将这个操作看作O(1)
         */
        while (de) {
            uint64_t h;

            nextde = de->next;

            // 计算元素在ht[1]的哈希值
            h = dictHashKey(d, de->key) & d->ht[1].sizemask;

            // 添加节点到 ht[1]，调整指针
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;

            // 更新计数器
            d->ht[0].used--;
            d->ht[1].used++;

            de = nextde;
        }

        // 设置指针为NULL， 方便下次rehash时跳过
        d->ht[0].table[d->rehashidx] = NULL;

        // 前进至下一索引
        d->rehashidx++;
    }

    // 如果 ht[0]已经为空，那么迁移完毕
    // 用 ht[1] 代替原有的 ht[0]
    if (d->ht[0].used == 0) {

        // 释放 ht[0]的哈希表数组
        free(d->ht[0].table);

        // 将ht[0]指向ht[1]
        d->ht[0] = d->ht[1];

        // 清空ht[1]的指针
        _dictReset(&d->ht[1]);

        // 关闭 rehash标识
        d->rehashidx = -1;

        // 通知调用者， rehash完毕
        return 0;
    }

    // 通知调用者，还有元素等待 rehash
    return 1;
}

/**
 * 以毫秒为单位，返回当时时间
 */
long long timeInMilliseconds(void) {

    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (((long long)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

/**
 * 在给定毫秒数内，以100步为单位，对字典进行rehash
 * T = O(N)， N为被 rehash的 key-value 对数量
 */
int dictRehashMilliseconds(dict *d, int ms) {

    long long start = timeInMilliseconds();
    int rehashs = 0;

    while (dictRehash(d, 100)) {
        rehashs += 100;
        if (timeInMilliseconds() - start > ms) break;
    }

    return rehashs;
}

/**
 * 如果允许条件的话，将一个元素从ht[0]迁移至ht[1]
 * 
 * 这个函数被其他查找和更新函数所调用，从而实现渐进式 rehash
 */
static void _dictRehashStep(dict *d) {

    // 只在没有安全迭代器的时候，才能进行迁移
    // 否则可能产生重复元素，或者丢失元素
    if (d->iterators == 0) {
        dictRehash(d, 1);
    }
}

// 添加给定 key-value对 到字典
int dictAdd(dict *d, void *key, void *val) {
    // 添加key到哈希表，返回包含该key节点
    dictEntry *entry = dictAddRaw(d, key, NULL);

    // 添加失败
    if (!entry) { 
        return DICT_ERR;
    }

    // 设置节点的值
    dictSetVal(d, entry, val);

    return DICT_OK;
}

/**
 * 添加key到字典的底层实现， 完成之后返回新节点
 * 如果key已经存在，返回NULL
 */
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing) {

    int index;
    dictEntry *entry;
    dictht *ht;

    // 尝试渐进式地 rehash 一个元素
    if (dictIsRehashing(d)) _dictRehashStep(d);

    // 查找可容纳新元素的索引位置
    // 如果元素已存在，index为-1
    index = _dictKeyIndex(d, key, dictHashKey(d, key), existing);
    if (index == -1) {
        return NULL;
    }
    // 决定该把新元素放在那个哈希表
    ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];

    // 为新元素分配节点空间
    entry = malloc(sizeof(*entry));

    // 新节点的后继指针指向旧的表头节点
    entry->next = ht->table[index];

    // 设置新节点为表头
    ht->table[index] = entry;

    // 更新已有节点数量
    ht->used++;

    // 关联起节点和key
    dictSetKey(d, entry, key);

    // 返回新节点
    return entry;
}


/**
 * 返回给定key的哈希表存放索引
 * 
 * 如果key已经存在于哈希表，返回-1
 * 
 * 当正在执行rehash的时候
 * 返回的 index 总是应用于第二个（新的）哈希表
 */
static long _dictKeyIndex(dict *d, const void *key, uint64_t hash, dictEntry **existing) {

    unsigned long idx, table;
    dictEntry *he;
    if (existing) {
        *existing = NULL;
    }

    if (_dictExpandIfNeeded(d) == DICT_ERR) return -1;

    for (table = 0; table <= 1; table++) {
        idx = hash & d->ht[table].sizemask;
        he = d->ht[table].table[idx];

        while (he) {
            if (key == he->key || dictCompareKeys(d, key, he->key)) {
                if (existing) {
                    *existing = he;
                }
                return -1;
            }
            he = he->next;
        }

        if (!dictIsRehashing(d)) break;
    }

    return idx;
}

/**
 * 根据需要，扩展字典的大小
 * （也即是对 ht[0] 进行 rehash）
 */
static int _dictExpandIfNeeded(dict *d) {

    // 已经在渐进式 rehash 当中，直接返回
    if (dictIsRehashing(d)) return DICT_OK;

    // 如果哈希表为空，那么将它扩展为初始大小 O(N)
    if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);

    /**
     * 如果哈希表的已用节点数 >= 哈希表的大小
     * 并且以下条件任一个为真
     *  1. dict_can_resize 为真
     *  2. 已用节点数除以哈希表大小之比大于 dict_force_resize_ratio
     * 
     * 那么调用dictExpand 对哈希表进行扩展
     * 扩展的体积至少为已使用节点数的两倍
     */
    if (d->ht[0].used >= d->ht[0].size && (dict_can_resize || d->ht[0].used / d->ht[0].size > dict_force_resize_ratio)) {
        return dictExpand(d, d->ht[0].used * 2);
    }

    return DICT_OK;
}

/**
 * 计算哈希表的真实体积
 * 
 * 如果size 小于等于 DICT_HT_INITIAL_SIZE
 * 那么返回 DICT_HT_INITIAL_SIZE
 * 否则这个值为第一个 >= size的二次幂
 */
static unsigned long _dictNextPower(unsigned long size) {

    unsigned long i = DICT_HT_INITIAL_SIZE;

    if (size >= LONG_MAX) return LONG_MAX;

    while (1) {
        if (i >= size) {
            return i;
        }
        i *= 2;
    }
}

// 销毁给定哈希表
int _dictClear(dict *d, dictht *ht, void(callback)(void *)) {

    unsigned long i;

    // 遍历哈希表数组
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        dictEntry *he, *nextHe;

        if (callback && (i & 65535) == 0) callback(d->privdata);

        if ((he = ht->table[i]) == NULL) continue;

         /**
         * 释放整个链表上的元素
         * 因为链表的元素数量通常为1，或者维持在一个很小的比率
         * 因此可以将这个操作看作O(1)
         */
        while (he) {
            nextHe = he->next;

            dictFreeKey(d, he);
            dictFreeVal(d, he);

            free(he);

            ht->used--;

            he = nextHe;
        }

        free(ht->table);

        _dictReset(ht);

        return DICT_OK;
    }
}

void dictEmpty(dict *d, void(callback)(void*)) {

    _dictClear(d, &d->ht[0], callback);
    _dictClear(d, &d->ht[1], callback);
    d->rehashidx = -1;
    d->iterators = 0;
}

// 打开rehash标识
void dictEnableResize(void) {

    dict_can_resize = 1;
}

// 关闭rehash标识
void dictDisableResize(void) {
    dict_can_resize = 0;
}

dictEntry *dictFind(dict *d, const void *key) {

    dictEntry *he;
    uint64_t h, idx, table;

    if (d->ht[0].used + d->ht[1].used == 0) return NULL;    // dict为空

    if (dictIsRehashing(d)) _dictRehashStep(d);

    h = dictHashKey(d, key);

    for (table = 0; table <= 1; table++) {
        idx = h & d->ht[table].sizemask;
        he = d->ht[table].table[idx];

        while (he) {
            if (key == he->key || dictCompareKeys(d, key, he->key)) {
                return he;
            }
            he = he->next;
        }

        if (!dictIsRehashing(d)) return NULL;
    }

    return NULL;
}

uint64_t dictGetHash(dict *d, const void *key) {
    
    return dictHashKey(d, key);
}

dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash) {

    dictEntry *he, **heref;
    unsigned long idx, table;

    if (d->ht[0].used + d->ht[1].used == 0) return NULL;
    
    for (table = 0; table <= 1; table++) {
        idx = hash & d->ht[table].sizemask;
        heref = &d->ht[table].table[idx];
        he = *heref;

        while (he) {
            if (oldptr == he->key) {
                return heref;
            }
            heref = &he->next;
            he = *heref;
        }

        if (!dictIsRehashing(d)) return NULL;
    }

    return NULL;
}

#define DICT_STATS_VECTLEN 50
size_t _dictGetStatsHt(char *buf, size_t bufsize, dictht *ht, int tableid) {

    unsigned long i, slots = 0, chainlen, maxchainlen = 0;
    unsigned long totchainlen = 0;
    unsigned long clvector[DICT_STATS_VECTLEN];
    size_t l = 0;

    if (ht->used == 0) {
        return snprintf(buf, bufsize, "No stats available for empty dictionaries \n");
    }

    for (i = 0; i < DICT_STATS_VECTLEN; i++) clvector[i] = 0;
    
    for (i = 0; i < ht->size; i++) {
        dictEntry *he;

        if (ht->table[i] == NULL) {
            clvector[0]++;
            continue;
        }

        slots++;
        chainlen = 0;
        he = ht->table[i];

        while (he) {
            chainlen++;
            he = he->next;
        }

        clvector[(chainlen < DICT_STATS_VECTLEN) ? chainlen : (DICT_STATS_VECTLEN - 1)]++;

        if (chainlen > maxchainlen) {
            maxchainlen = chainlen;
        }
        totchainlen += chainlen;
    }

    l += snprintf(buf + 1, bufsize - 1, 
        "Hash table %d stats (%s): \n"
        " table size: %ld\n"
        " number of elements: %d\n"
        " different slots: %ld\n"
        " max chain length: %ld\n"
        " avg chain length (counted): %.02f\n"
        " avg chain length (computed): %.02f\n"
        " Chain length distribution: \n",
        tableid, (tableid == 0) ? "main hash table" : "rehashing target",
        ht->size, ht->used, slots, maxchainlen,
        (float) totchainlen / slots, (float) ht->used / slots);
    
    for (i = 0; i < DICT_STATS_VECTLEN - 1; i++) {
        if (clvector[i] == 0) continue;
        if (l >= bufsize) break;

        l += snprintf(buf + l, bufsize - l,
            "   %s%ld: %ld (%.02f%%)\n",
            (i == DICT_STATS_VECTLEN - 1) ? ">= " : "",
            i, clvector[i], ((float)clvector[i] / ht->size) * 100);
    }

    if (bufsize) buf[bufsize - 1] = '\0';
    return strlen(buf);
}

void dictGetStats(char *buf, size_t bufsize, dict *d) {

    size_t l;
    char *orig_buf = buf;
    size_t orig_bufsize = bufsize;

    l = _dictGetStatsHt(buf, bufsize, &d->ht[0], 0);
    buf += l;
    bufsize -= l;

    if (dictIsRehashing(d) && bufsize > 0) {
        _dictGetStatsHt(buf, bufsize, &d->ht[1], 1);
    }

    if (orig_bufsize) orig_buf[orig_bufsize - 1] = '\0';
}

long long dictFingerprint(dict *d) {

    long long integers[6], hash = 0;
    int j;

    integers[0] = (long) d->ht[0].table;
    integers[1] = d->ht[0].size;
    integers[2] = d->ht[0].used;
    integers[3] = (long) d->ht[1].table;
    integers[4] = d->ht[1].size;
    integers[5] = d->ht[1].used;

    /* We hash N integers by summing every successive integer with the integer
     * hashing of the previous sum. Basically:
     *
     * Result = hash(hash(hash(int1)+int2)+int3) ...
     *
     * This way the same set of integers in a different order will (likely) hash
     * to a different number. */
    for (j = 0; j < 6; j++) {
        hash += integers[j];
        /* For the hashing step we use Tomas Wang's 64 bit integer hash. */
        hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
    }
    return hash;
}

dictIterator *dictGetIterator(dict *d) {

    dictIterator *iter = malloc(sizeof(*iter));

    iter->d = d;
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    iter->nextEntry = NULL;
    return iter;
}

dictIterator *dictGetSafeIterator(dict *d) {

    dictIterator *i = dictGetIterator(d);

    i->safe = 1;
    return i;
}

dictEntry *dictNext(dictIterator *iter) {

    while (1) {
        if (iter->entry == NULL) {
            dictht *ht = &iter->d->ht[iter->table];

            if (iter->index == -1 && iter->table == 0) {
                if (iter->safe) {
                    iter->d->iterators++;
                } else {
                    iter->fingerprint = dictFingerprint(iter->d);
                }
            }

            iter->index++;

            if (iter->index >= (long) ht->size) {
                if (dictIsRehashing(iter->d) && iter->table == 0) {
                    iter->table++;
                    iter->index = 0;
                    ht = &iter->d->ht[1];
                } else {
                    break;
                }
            }

            iter->entry = ht->table[iter->index];

        } else {

            iter->entry = iter->nextEntry;

        }

        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            iter->nextEntry = iter->entry->next;
            return iter->entry;
        }
    }
    return NULL;
}

void dictReleaseIterator(dictIterator *iter)
{
    if (!(iter->index == -1 && iter->table == 0)) {
        if (iter->safe) {
            iter->d->iterators--;
        } else {
            assert(iter->fingerprint == dictFingerprint(iter->d));
        } 
    }
    free(iter);
}

// 按key 查找并删除节点
static dictEntry *dictGenericDelete(dict *d, const void *key, int nofree) {

    uint64_t h, idx;
    dictEntry *he, *prevHe;
    int table;

    // 空表?
    if (d->ht[0].used == 0 && d->ht[1].used == 0) return NULL;

    // 渐进式 rehash
    if (dictIsRehashing(d)) _dictRehashStep(d);

    // 计算哈希值
    h = dictHashKey(d, key);

    // 在两个哈希表中查找
    for (table = 0; table <= 1; table++) {
        // 索引值
        idx = h & d->ht[table].sizemask;
        // 索引在数组中对应的表头
        he = d->ht[table].table[idx];
        prevHe = NULL;

        /**
         * 遍历链表
         * 因为链表的元素数量通常为1，或者维持在一个很小的比率
         * 因此可以将这个操作看作O(1)
         */
        while (he) {
            // 对比
            if (key == he->key || dictCompareKeys(d, key, he->key)) {
                if (prevHe) {
                    prevHe->next = he->next;
                } else {
                    d->ht[table].table[idx] = he->next;
                }

                // 释放节点的键和值
                if (!nofree) {
                    dictFreeKey(d, he);
                    dictFreeVal(d, he);
                    free(he);
                }
                d->ht[table].used--;
                return he;
            }
            prevHe = he;
            he = he->next;
        }

        /**
         * 如果不是正在进行 rehash
         * 那么无须遍历ht[1]
         */
        if (!dictIsRehashing(d)) break;
    }

    return NULL;    // 没有找到
}


// 删除哈希表中的key，并且释放保存这个key的节点
int dictDelete(dict *ht, const void *key) {

    return dictGenericDelete(ht, key, 0) ? DICT_OK : DICT_ERR;
}