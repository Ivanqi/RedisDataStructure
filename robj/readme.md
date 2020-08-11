### REDIS5 ROBJ
#### ROBJ 的 作用
- 从Redis的使用的角度
  - 一个Redis节点包含多个database(非cluster默认是16个, cluster模式下只能是1)
  - 而一个database维护了从 key space 到 object  space的映射关系
  - 这个映射关系的key是string类型，而value可以是多种数据类型，比如：string, list, hash等
  - 我们可以看到，key的类型固定是string，而value可能的类型是多个
- 从Redis内部实现的角度
  - 一个database内的这个映射关系是用一个dict来维护的
  - dict的key固定用一种数据结构来表达就够了，这就是动态字符串sds
  - 而value则比较复杂，为了在同一个dict内能够存储不同类型的value，就需要一个通过的数据结构，这个通用的数据结构就是robj(redisObject)
    - 举个列子
      - 如果value是一个list，那么它的内部存储结构是一个quicklist
      - 如果value是一个string，那么它的内部结构一般情况下是一个sds
      - 如果它的是一个数字，那么Redis内部还会把它转成long型存储，从而减小内存使用

#### ROBJ 的数据结构
```
/* The actual Redis Object */
#define OBJ_STRING 0    /* String object. */
#define OBJ_LIST 1      /* List object. */
#define OBJ_SET 2       /* Set object. */
#define OBJ_ZSET 3      /* Sorted set object. */
#define OBJ_HASH 4      /* Hash object. */

#define OBJ_ENCODING_RAW 0     /* Raw representation */
#define OBJ_ENCODING_INT 1     /* Encoded as integer */
#define OBJ_ENCODING_HT 2      /* Encoded as hash table */
#define OBJ_ENCODING_ZIPMAP 3  /* Encoded as zipmap */
#define OBJ_ENCODING_LINKEDLIST 4 /* No longer used: old list encoding. */
#define OBJ_ENCODING_ZIPLIST 5 /* Encoded as ziplist */
#define OBJ_ENCODING_INTSET 6  /* Encoded as intset */
#define OBJ_ENCODING_SKIPLIST 7  /* Encoded as skiplist */
#define OBJ_ENCODING_EMBSTR 8  /* Embedded sds string encoding */
#define OBJ_ENCODING_QUICKLIST 9 /* Encoded as linked list of ziplists */

typedef struct redisObject {
    unsigned type:4;
    unsigned encoding:4;
    unsigned lru:LRU_BITS; 
    int refcount;
    void *ptr;
} robj;
```
##### 对象的类型
| 类型常量 | 对象名称 |
| --- | --- |
| OBJ_STRING | 字符串对象 |
| OBJ_LIST | 列表对象 |
| OBJ_HASH |哈希对象  |
| OBJ_SET | 集合对象 |
| OBJ_ZSET | 有序集合对象 |

##### 对象的编码
| 编码常量 | 编码所对应的底层数据结构 |
| --- | --- |
| OBJ_ENCODING_INT | long 类型的整数 |
| OBJ_ENCODING_EMBSTR | embstr 编码的简单动态 |
| OBJ_ENCODING_RAW |简单动态字符串  |
| OBJ_ENCODING_HT | 字典 |
| OBJ_ENCODING_LINKEDLIST | 双端链表 |
|OBJ_ENCODING_ZIPLIST|压缩列表|
|OBJ_ENCODING_INTSET|整数集合|
|OBJ_ENCODING_SKIPLIST|跳跃表|
|OBJ_ENCODING_QUICKLIST|跳表：压缩列表的链表|

##### 不同类型和编码的对象
| 类型 | 编码 | 对象|
| --- | --- | --- |
|OBJ_STRING|OBJ_ENCODING_INT|使用整数数值实现的字符串对象|
|OBJ_STRING|OBJ_ENCODING_EMBSTR|使用emstr编码(SDS)的简单字符串实现的字符串对象|
|OBJ_STRING|OBJ_ENCODING_RAW|使用简单动态字符串实现的字符串对象|
|OBJ_LIST|OBJ_ENCODING_ZIPLIST|使用压缩列表实现的列表对象|
|OBJ_LIST|OBJ_ENCODING_LINKEDLIST|使用双端链表实现的列表对象|
|OBJ_HASH|OBJ_ENCODING_ZIPLIST|使用压缩列表实现的哈希对象|
|OBJ_HASH|OBJ_ENCODING_HT|使用字典实现的哈希对象|
|OBJ_SET|OBJ_ENCODING_INTSET|使用整数集合实现集合对象|
|OBJ_SET|OBJ_ENCODING_HT|使用字典实现集合对象|
|OBJ_ZSET|OBJ_ENCODING_ZIPLIST|使用压缩列表实现的有序集合对象|
|OBJ_ZSET|OBJ_ENCODING_SKIPLIST|使用跳跃表和字典实现的有序集合|
