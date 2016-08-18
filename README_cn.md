# redisbase proxy

redisbase(redisbase.com):

* 支持 redis 协议和数据类型 (set/list/kv)
* 采用 一致性 hash 对数据分片, 预分片 1024(?)
* 每一个分片采用 raft (基于 logcabin hack) 协议冗余 3 份
* raft 的 state machine 采用两层设计, 一个 cache 层(redis), 一个 db 层 (基于 rocksdb 实现, 兼容 redis 协议)。用 redisbase proxy 模块封装成单一接口
* raft state machine cache 层支持 LRU 淘汰, 通过淘汰冷数据, 限制 cache 的内存使用量。访问冷数据 不命中时, 从 db 读取 (set/list 的写操作允许 key 不存在, 需要特殊处理)
* 扩容/缩容 采用搬迁一个 raft 节点的方法, 不中断业务, 迁移一个 raft 节点到新的物理机
