### REDIS5 列表对象
#### 列表对象的构成
- 列表对象的编码可以是ziplist或者linkedlist

#### 编码转换
- 当对象列表可以同时满足以下两个条件时，列表对象使用ziplist编码
  - 列表对象保存的所有字符串元素的长度都小于64字节
  - 列表对象保存元素数量小于512个
  - 不能满足这两个条件的列表对象需要使用linkedlist对象
  
#### 列表命令的实现
| 命令 | ziplist编码的实现方式 | linkedlist编码的实现方式 |
| --- | --- | --- |
| LPUSH | 调用ziplistPush函数，将新元素推入到压缩列表的表头 | 调用listAddNodeHead函数，将新元素推入到双端链表的表头 |
| RPUSH | 调用ziplistPush函数，将新元素推入到压缩列表的表尾 | 调用listAddNodeTail函数，将新元素推入到双端链表的表尾 |
| LPOP | 调用ziplistIndex函数定位压缩列表的表头节点，在向用户返回节点所保存的元素之后，调用ziplistDelete函数删除表头节点 | 调用listFirst函数定位双端链表的表头节点，在向用户返回节点所保存的元素之后，调用listDelNode函数删除表头节点|
| RPOP | 调用ziplistIndex函数定位压缩列表的表头节点，在向用户返回节点所保存的元素之后，调用ziplistDelete函数删除表尾节点 | 调用listLast函数定位双端链表的表头节点，在向用户返回节点所保存的元素之后，调用listDelNode函数删除表尾节点 |
| LINDEX | 调用ziplistIndex函数定位压缩列表的指定节点，然后返回节点所保存的元素 | 调用listIndex函数定位双端链表中指定节点，然后返回节点所保存的元素 |
| LLEN | 调用ziplistLen函数返回压缩列表长度 | 调用listLength函数返回双端链表的长度 |
| LINSERT | 插入新节点到压缩列表的表头或表尾时，使用ziplistPush函数;插入新节点到压缩列表的其他位置时，使用ziplistInsert函数 |调用listInsertNode函数，将新节点插入到双端链表的指定位置  |
| LREM | 遍历压缩列表节点，并调用ziplistDelete函数删除包含了给定元素的节点 | 遍历双端链表节点，并调用listDelNode函数删除包含了给定元素的节点 |
| LTRIM | 调用ziplistDelete函数，删除压缩列表中所有不在指定索引范围内的节点 | 遍历双端链表，并调用listDelNode函数删除链表中所有不在指定索引范围内的节点 |
| LSET | 调用ziplistDelete函数，先删除压缩列表指定上的节点，然后调用ziplistInsert函数，将一个包含给定元素的新节点插入到相同索引上面 | 调用listIndex函数，定位到双端链表指定的索引上的节点，然后通过赋值操作更新节点的值 |