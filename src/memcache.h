#ifndef MEMCACHE_H
#define MEMCACHE_H

#include <libmemcached/memcached.h>
#include <string>

class MemcachedClient {
public:
    MemcachedClient(const std::string &server = "127.0.0.1", in_port_t port = 11211);
    ~MemcachedClient();

    bool storeValue(const std::string &key, const std::string &value, time_t expiration = 0);
    std::string readValue(const std::string &key);
    bool deleteValue(const std::string &key);

private:
    memcached_st *memc;
};

#endif // MEMCACHE_H
