#### 疑问
- 什么时候触发hash 拓展内存？
  - ht[0].size == 0 
    - 通过 调用 dictExpand函数申请内存
    - 同时把新的内存地址赋予 ht[0], ht[1]
  - 哈希表的已用节点数 >= 哈希表的大小(d->ht[0].used >= d->ht[0].size)
    - 申请 哈希表的已用节点数(d->ht[0].used) * 2 的内存
    - 同时因为 ht[0]内存不为空，所以把新内存地址赋予 ht[1]
- 什么时候触发hash rehash？
  - （(ht[0].used + ht[1].used) / (ht[0].size + ht[1].size) < 0.1
    - 意味着 hash扩展过大，而所使用的元素过少，需要减缩
  -  哈希表的已用节点数 >= 哈希表的大小(d->ht[0].used >= d->ht[0].size)
     -  一边扩展ht[1]的内存
     -  一边把打开rehash标识

#### 参考资料
- [漫谈非加密哈希算法（MurMurHash，CRC32，FNV，SipHash，xxHash）](http://www.xiaojiejing.com/2018/12/06/%e6%bc%ab%e8%b0%88%e9%9d%9e%e5%8a%a0%e5%af%86%e5%93%88%e5%b8%8c%e7%ae%97%e6%b3%95%ef%bc%88murmurhash%ef%bc%8ccrc32%ef%bc%8cfnv%ef%bc%8csiphash%ef%bc%8cxxhash%ef%bc%89/)
- [什么是哈希洪水攻击（Hash-Flooding Attack）？](https://www.zhihu.com/question/286529973)
- [SipHash 算法流程](https://my.oschina.net/tigerBin/blog/3038044)