#ifndef MEMCACHE_STRUCT_H_
#define MEMCACHE_STRUCT_H_

#include <string>
#include <vector>
#include "db.h"

namespace benchmark {

struct MemcacheResponse {
  uint64_t timestamp = 0;
  std::vector<DB::TimestampValue> read_buffer;
  Operation operation = Operation::INVALID;
  Status s = Status::kOK;
  int hit_count = 0;
  int read_count = 0;
  MSGPACK_DEFINE(timestamp, read_buffer, operation, s, hit_count, read_count);
};

struct MemcacheRequest {
  uint64_t timestamp;
  std::vector<DB::DB_Operation> operations;
  std::string resp_addr;
  std::string resp_port;
  bool read_only;
  bool txn_op;
  MSGPACK_DEFINE(timestamp, operations, resp_addr, resp_port, read_only, txn_op);
};

struct DBRequest {
  MemcacheResponse resp;
  std::vector<DB::DB_Operation> operations;
  std::string resp_addr;
  std::string resp_port;
  bool read_only;
  bool txn_op;
  MSGPACK_DEFINE(resp, operations, resp_port, read_only, txn_op);
};

const std::vector<std::string> zmq_read_ports = {"6100", "6101", "6102", "6103"};
const std::vector<std::string> zmq_read_txn_ports = {"6200", "6201"};
const std::vector<std::string> zmq_write_ports = {"6300"};
const std::vector<std::string> zmq_db_ports = {"6400", "6401"};

} // benchmark

#endif // MEMCACHE_STRUCT_H_