#include <iostream>
#include <libmemcached/memcached.h>

int main() {
    // Initialize the Memcached connection
    memcached_st *memc;
    memcached_server_st *servers;
    
    memc = memcached_create(NULL);
    servers = memcached_server_list_append(NULL, "127.0.0.1", 11211, NULL);
    memcached_server_push(memc, servers);
    memcached_server_free(servers);

    // Set a key-value pair
    const char *key = "my_key";
    const char *value = "Hello, Memcached!";
    size_t value_length = strlen(value);
    memcached_return_t rc;

    // Store the value (key, value, value_length, expiration_time)
    rc = memcached_set(memc, key, strlen(key), value, value_length, (time_t)0, (uint32_t)0);
    if (rc == MEMCACHED_SUCCESS) {
        std::cout << "Stored key: " << key << std::endl;
    } else {
        std::cerr << "Error storing key: " << memcached_strerror(memc, rc) << std::endl;
    }

    // Retrieve the value
    size_t ret_length;
    uint32_t flags;
    char *retrieved_value = memcached_get(memc, key, strlen(key), &ret_length, &flags, &rc);
    if (rc == MEMCACHED_SUCCESS) {
        std::cout << "Retrieved value: " << std::string(retrieved_value, ret_length) << std::endl;
        free(retrieved_value); // Free the retrieved value
    } else {
        std::cerr << "Error retrieving key: " << memcached_strerror(memc, rc) << std::endl;
    }

    // Delete the key
    rc = memcached_delete(memc, key, strlen(key), (time_t)0);
    if (rc == MEMCACHED_SUCCESS) {
        std::cout << "Deleted key: " << key << std::endl;
    } else {
        std::cerr << "Error deleting key: " << memcached_strerror(memc, rc) << std::endl;
    }

    // Cleanup
    memcached_free(memc);

    return 0;
}
