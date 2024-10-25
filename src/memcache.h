#ifndef MEMCACHE_H
#define MEMCACHE_H

#include <libmemcached/memcached.h>
#include <string>
#include <cassert>
#include <sstream>
#include <iostream>
#include <vector>
#include <utility>
#include "db.h"

namespace benchmark {

class MemcachedClient {
public:
    // MemcachedClient(const std::string &server = "127.0.0.1", in_port_t port = 11211);
    // MemcachedClient(const std::vector<std::pair<std::string, in_port_t>> &servers);
    // 修改构造函数以接受多个服务器
    MemcachedClient(const std::vector<std::pair<std::string, in_port_t>> &servers);

    // 保留单服务器的默认构造函数，调用多服务器的构造函数
    // MemcachedClient(const std::string &server = "127.0.0.1", in_port_t port = 11211)
        // : MemcachedClient(std::vector<std::pair<std::string, in_port_t>>{{server, port}}) {}

    ~MemcachedClient();

    bool get(const DB::DB_Operation &operation, std::vector<DB::TimestampValue> &read_buffer);
    bool put(const DB::DB_Operation &operation, std::vector<DB::TimestampValue> &read_buffer);
    bool invalidate(const DB::DB_Operation &operation);

private:
    std::string fields2Str(std::vector<DB::Field> const & k);
    std::string timeval2Str(DB::TimestampValue const & tv);
    DB::TimestampValue str2Timeval(const std::string &str);

    bool storeValue(const std::string &key, const std::string &value, time_t expiration = 0);
    std::string readValue(const std::string &key);
    bool deleteValue(const std::string &key);

    memcached_st *memc;
};

} // benchmark

#endif // MEMCACHE_H
