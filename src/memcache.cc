#include "memcache.h"
#include <iostream>

MemcachedClient::MemcachedClient(const std::string &server, in_port_t port) {
    // Create a Memcached instance
    memc = memcached_create(nullptr);
    memcached_server_st *servers = memcached_server_list_append(nullptr, server.c_str(), port, nullptr);
    memcached_server_push(memc, servers);
    memcached_server_free(servers);
}

MemcachedClient::~MemcachedClient() {
    memcached_free(memc);
}

bool MemcachedClient::storeValue(const std::string &key, const std::string &value, time_t expiration) {
    memcached_return_t rc = memcached_set(memc, key.c_str(), key.length(), value.c_str(), value.length(), expiration, 0);
    return rc == MEMCACHED_SUCCESS;
}

std::string MemcachedClient::readValue(const std::string &key) {
    size_t value_length;
    uint32_t flags;
    memcached_return_t rc;
    
    char *value = memcached_get(memc, key.c_str(), key.length(), &value_length, &flags, &rc);
    
    if (rc == MEMCACHED_SUCCESS) {
        std::string result(value, value_length);
        free(value); // Free the retrieved value
        return result;
    } else {
        return ""; // Return empty string on failure
    }
}

bool MemcachedClient::deleteValue(const std::string &key) {
    memcached_return_t rc = memcached_delete(memc, key.c_str(), key.length(), 0);
    return rc == MEMCACHED_SUCCESS;
}
