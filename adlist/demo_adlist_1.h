#ifndef __ADLIST_1_H__
#define __ADLIST_1_H__

// 链表节点
typedef struct listNode {
    // 前驱节点
    struct listNode *prev;

    // 后继节点
    struct listNode *next;

    // 值
    void *value
} listNode;

// 链表迭代器
typedef struct listIter {
    // 下一个节点
    listNode *next;

    // 迭代方向
    int direction;
} listIter;

//链表
typedef struct list {
    // 表头指针
    listNode *head;

    // 表尾指针
    listNode *tail;

    // 节点数量
    unsigned long len;

    // 复制函数
    void *(*dup)(void *ptr);

    // 释放函数
    void *(*free)(void *ptr);

    // 比对函数
    int (*match)(void *ptr, void *key);
} list;

// 返回链表的节点数量, O(1)
#define listLength(l) ((l)->len)

// 返回链表的表头节点, O(1)
#define listFirst(l) ((l)->head)

// 返回链表的表尾节点, O(1)
#define listLast(l) ((l)->tail)

// 返回给定节点的前一个节点, O(1)
#define listPrevNode(n) ((n)->prev)

// 返回给定节点的后一个节点, O(1)
#define listNextNode(n) ((n)->next)

// 返回给定节点的值, O(1)
#define listNodeValue(n) ((n)->value)

// 将给定的函数为链表的节点值复制函数, O(1)
#define listSetDupMethod(l, m) ((l)->dup = (m))

// 将给定的函数设置为链表的节点值对比函数, O(1)
#define listSetFreeMethod(l, m) ((l)->free = (m))

// 将给定的函数设置为链表的节点值的对比函数, O(1)
#define listSetMatchMethod(l, m) ((l)->match = (m));

// 返回链表当前正在使用的节点值复制函数, O(1)
#define listGetDupMethod(l) ((l)->dup);

// 返回链表当前正在使用的节点值释放函数, O(1)
#define listGetFree(l) ((l)->free);

// 返回链表当前正在使用的节点值对比函数, O(1)
#define listGetMatchMethod(l) ((l)->match);

// 创建一个不包含任何节点的新链表
list *listCreate();

// 释放给定链表，以及链表中的所有节点. O(N), N为链表长度
void listRelease(list *list);

// 将一个包含给定值的新节点添加到给定链表的表头, O(1)
list *listAddNodeHead(list *list, void *value);

// 将一个包含给定的新节点添加到给定链表的表尾, O(1)
list *listAddNodeTail(list *list, void *value);

// 将一个包含给定值的新节点添加到给定节点的之前或之后, O(1)
list *listInsertNode(list *list, listNode *old_node, void *value, int after);

// 从链表中删除给定节点，O(N), N为链表长度
void listDelNode(list *list, listNode *node);

listIter *listGetIterator(list *list, int direction);

listNode *listNext(listIter *iter);

void listReleaseIterator(listIter *iter);

// 复制一个给定链表的副本, O(N), N为链表长度
list *listDup(int *orig);

// 查找并返回链表中包含给定值的节点, O(N), N为链表长度
listNode *listSearchKey(list *list, void *key);

// 返回链表给定索引上的节点, O(N), N为链表长度
listNode *listIndex(list *list, long index);

void listRwind(list *list, listIter *li);

void listRewindTail(list *list, listIter *li);

void listRotate(list *list);

#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif