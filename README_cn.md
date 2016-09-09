# Redisbase Proxy

* RedisbaseProxy 把一个 Redis 和一个持久化的、兼容 Redis 的 key value store (RedisDB) 封装成一个单一的 DB, 对外提供 Redis 兼容的接口
* RedisbaseProxy 写操作会写通 Redis/RedisDB
* Redis 支持 LRU 淘汰算法, Redis 可以使用的内存可配置
* 读写时, Redis 如果没命中, 先从 RedisDB 提升数据到 Redis
* 用 redis db 来分库，扩容、缩容 通过移动 db 来实现。根据 key 的 hash 值，把 key 散列到不同的 db

# Redisbase

redisbase(redisbase.com):

* 支持 redis 协议和数据类型 (set/list/kv)
* 采用 一致性 hash 对数据分片, 预分片 1024(?)
* 每一个分片采用 raft (基于 logcabin hack) 协议冗余 3 份
* raft 的 state machine 采用两层设计, 一个 cache 层(redis), 一个 db 层 (基于 rocksdb 实现, 兼容 redis 协议)。用 redisbase proxy 模块封装成单一接口
* raft state machine cache 层支持 LRU 淘汰, 通过淘汰冷数据, 限制 cache 的内存使用量。访问冷数据 不命中时, 从 db 读取 (set/list 的写操作允许 key 不存在, 需要特殊处理)
* 扩容/缩容 采用搬迁一个 raft 节点的方法, 不中断业务, 迁移一个 raft 节点到新的物理机
