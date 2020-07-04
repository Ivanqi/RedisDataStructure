#include <stdlib.h>
#include "demo_adlist_1.h"
#include <string.h>
#include <stdio.h>

/**
 * 创建一个新列表
 * 创建成功时返回列表，创建失败返回NULL
 */
list *listCreate(void) {

    struct list *list;

    // 为列表结构分配内存
    if ((list = malloc(sizeof(*list))) == NULL) {
        return NULL;
    }

    // 初始化属性
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;

    return list;
}

/**
 * 释放整个列表(以及列表包含的节点)
 */
void listRelease(list *list) {

    unsigned long len;
    listNode *current, *next;

    current = list->head;
    len = list->len;

    while (len--) {
        next = current->next;
        // 如果列表有自带的free方法，那么先对节点值调用它
        if (list->free) list->free(current->value);
        // 之后再释放节点
        free(current);
        current = next;
    }
    free(list);
}

/**
 * 新建一个包含给定value的节点，并将它加入到列表的表头
 * 出错时，返回NULL，不执行动作
 * 成功时，返回传入列表
 */
list *listAddNodeHead(list *list, void *value) {

    listNode *node;

    if ((node = malloc(sizeof(*node))) == NULL) {
        return NULL;
    }

    node->value = value;

    if (list->len == 0) {
        // 第一个节点
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        // 不是第一个节点
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }

    list->len++;
    return list;
}

/**
 * 创建一个包含给定 value的节点，并将它加入到列表的表尾
 * 出错时，返回NULL，不执行动作
 * 成功时，返回传入的列表
 */
list *listAddNodeTail(list *list, void *value) {

    listNode *node;
    if ((node = malloc(sizeof(*node))) == NULL) {
        return NULL;
    }

    node->value = value;

    if (list->len == 0) {
        // 第一个节点
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        // 不是第一个节点
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }

    list->len++;
    return list;
}

/**
 * 创建一个包含值 value 的节点
 * 并根据after参数的指示，将新节点插入到old_node的之前或之后
 */
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {

    listNode *node;

    if ((node = malloc(sizeof(*node))) == NULL) {
        return NULL;
    }

    node->value = value;

    if (after) {
        // 插到old_node之后
        node->prev = old_node;
        node->next = old_node->next;
        // 处理尾节点
        if (list->tail == old_node) {
            list->tail = node;
        }
    } else {
        // 插到old_node之前
        node->next = old_node;
        node->prev = old_node->prev;
        // 处理表头节点
        if (list->head == old_node) {
            list->head = node;
        }
    }

    // 更新前置节点和后继节点的指针
    if (node->prev != NULL) {
        node->prev->next = node;
    }

    if (node->next != NULL){
        node->next->prev = node;
    }

    // 更新列表节点数量 
    list->len++;
    return list;
}

/**
 * 释放列表给定的节点
 * 清楚节点私有值(private value)的工作有调用者完成
 */
void listDelNode(list *list, listNode *node) {

    // 处理前驱节点的指针
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }

    // 处理后继节点的指针
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

    // 释放节点值
    if (list->free) list->free(node->value);

    // 释放节点
    free(node);

    // 更新列表节点数量
    list->len--;
}

/**
 * 创建列表 list的一个迭代器，迭代方向由参数direction决定
 * 每次对迭代器调用 listNext()，迭代器就返回列表的下一个节点
 * 这个函数不处理失败情形
 */
listIter *listGetIterator(list *list, int direction) {

    listIter *iter;

    if ((iter = malloc(sizeof(*iter))) == NULL) return NULL;

    // 根据迭代方向，将迭代器的指针指向表头或表尾
    if (direction == AL_START_HEAD) {
        iter->next = list->head;
    } else {
        iter->next = list->tail;
    }

    // 记录方向
    iter->direction = direction;

    return iter;
}

/**
 * 释放迭代器
 */
void listReleaseIterator(listIter *iter) {

    free(iter);
}

/**
 * 将迭代器iter的迭代指针倒回list的表头
 */
void listRewind(list *list, listIter *li) {

    li->next = list->head;
    li->direction = AL_START_HEAD;
}

/**
 * 将迭代器 iter 迭代指针倒回list的表尾
 */
void listRewindTail(list *list, listIter *li) {

    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/**
 * 返回迭代器的当前节点
 * 可以使用listDelNode()删除当前节点，但是不可以删除其他节点 
 * 函数要么返回当前节点，要么返回NULL，因此，常见的用法是
 * 
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *  doSomethingWith(listNodeValue(node));
 * }
 */
listNode *listNext(listIter *iter) {

    listNode *current = iter->next;

    if (current != NULL) {
        // 根据迭代方向，选择节点
        if (iter->direction == AL_START_HEAD) {
            iter->next = current->next;
        } else {
            iter->next = current->prev;
        }
    }

    return current;
}

/**
 * 复制整个列表，成功返回列表的副本，内存不足而失败时返回NULL
 * 
 * 无论复制是成功或失败，输入列表都不会被修改
 */

list *listDup(list *orig) {

    list *copy;
    listIter *iter;
    listNode *node;

    if ((copy = listCreate()) == NULL) {
        return NULL;
    }

    // 复制属性
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;

    // 复制节点
    iter = listGetIterator(orig, AL_START_HEAD);
    while ((node = listNext(iter)) != NULL) {

        // 复制节点值
        int *value;

        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                listRelease(copy);
                listReleaseIterator(iter);
                return NULL;
            } else {
                value = node->value;
            }
        }

        // 将新节点添加到新列表
        if (listAddNodeTail(copy, value) == NULL) {
            listRelease(copy);
            listReleaseIterator(iter);
            return NULL;
        }
    }

    listReleaseIterator(iter);

    return copy;
}

/**
 * 在列表中查找和key匹配的节点
 * 
 * 如果列表带有匹配器，那么匹配通过匹配器进行
 * 如果列表没有带匹配器，那么直接将key和节点的值进行比较
 * 
 * 匹配从头开始，第一个匹配成功的节点回被返回
 * 如果匹配不成功，返回NULL
 */
listNode *listSearchKey(list *list, void *key) {

    listIter *iter;
    listNode *node;

    // 使用迭代器
    iter = listGetIterator(list, AL_START_HEAD);
    while ((node = listNext(iter)) != NULL) {
        if (list->match) {
            // 使用列表自带的匹配器进行比对
            if (list->match(node->value, key)) {
                listReleaseIterator(iter);
                return node;
            }
        } else {
            // 直接使用列表的值来比对
            if (key == node->value) {
                listReleaseIterator(iter);
                return node;
            }
        }
    }

    // 没找到
    listReleaseIterator(iter);
    return NULL;
}

/**
 * 根据给定索引，返回列表中对应的节点
 * 
 * 索引可以是正数，也可以是负数
 * 正数从0开始计数，由表头开始；负数从-1开始计数，由表尾开始
 * 
 * 如果给定索引超出列表的返回，返回NULL
 */
listNode *listIndex(list *list, long index) {

    listNode *n;

    if (index < 0) {
        index = (-index) - 1;
        n = list->tail;
        while (index-- && n) n = n->prev;
    } else {
        n = list->head;
        while (index-- && n) n = n->next;
    }

    return n;
}

/**
 * 取出列表的尾节点，将它插入到表头，成为新的表头节点
 */
void listRotate(list *list) {

    listNode *tail = list->tail;

    // 列表只有一个元素
    if (listLength(list) < 1) return;

    // 取出尾节点
    list->tail = tail->prev;
    list->tail->next = NULL;

    // 将它插入到表头
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}