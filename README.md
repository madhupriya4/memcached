# Implementation of Berkeley DB as an in-memory cache using Memcached

Berkeley DB is a data store intended to provide a
high-performance embedded database for key/value data. In order to make
Berkeley DB function as a cache, an eviction strategy such as LRU must
be implemented which will discard old cache entries to make room for
new ones. In addition, various commands like get, set, add, replace, insert
need to be implemented on top of Berkeley DB to make it a complete
cache. In order to simplify this, we have used the networking layer of Memcached to
map the commands along with its tried and tested LRU. After this is implemented,
the Berkeley DB as a cache has been compared to the existing Memcached
using YCSB.
