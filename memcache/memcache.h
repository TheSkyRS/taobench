#ifndef MEMCACHE_H
#define MEMCACHE_H

#include <libmemcached/memcached.h>
#include <string>
#include <cassert>
#include <sstream>
#include <iostream>

#include "db.h"

namespace benchmark {

class MemcachedClient {
public:
    MemcachedClient(const std::string &server = "127.0.0.1", in_port_t port = 11211);
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
