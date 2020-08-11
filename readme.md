#### Redis 数据结构
| 数据结构 | 说明 |
| --- | --- |
| [adlist](/adlist/) | 链表 |
| [dict](/dict/) | 字典 |
| [intset](/intset/) | 整数集合 |
| [quicklist](/quicklist/)  | 快表 |
| [robj](/robj) | Redis 对象 |
| [sds](/sds/) | 字符串 |
| [ziplist](/ziplist/) | 压缩列表 |
| [zskiplist](/zskiplist/)|跳跃表|

#### 数据结构之间的关系
- ![avatar](images/redis_1.png)
- ![avatar](images/redis_2.png)

### 参考资料
- 《Redis设计与实现》
- [Redis系列文章——合集](https://mp.weixin.qq.com/s?__biz=MzA4NTg1MjM0Mg==&mid=509777776&idx=1&sn=e56f24bdf2de7e25515fe9f25ef57557&mpshare=1&scene=1&srcid=1010HdkIxon3icsWNmTyecI6#rd)
- [Redis 面试全攻略，读完这个就可以和面试官大战几个回合了](https://mp.weixin.qq.com/s/jLWKxQYOz6vA5aimh44OMg)
- [HyperLogLog 算法的原理讲解以及 Redis 是如何应用它的](https://juejin.im/post/6844903785744056333)