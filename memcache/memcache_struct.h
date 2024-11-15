#ifndef MEMCACHE_STRUCT_H_
#define MEMCACHE_STRUCT_H_

#include <string>
#include <vector>
#include "db.h"

namespace benchmark {

struct FullAddr {
  std::string addr;
  std::string port;
  MSGPACK_DEFINE(addr, port);

  bool operator==(const FullAddr& r) const {
    return addr == r.addr && port == r.port;
  }
};

struct FullAddrHash {
  std::size_t operator()(const FullAddr& f) const {
    return std::hash<std::string>{}(f.addr) ^ std::hash<std::string>{}(f.port);
  }
};

struct HitCount {
  int hit = 0;
  int read = 0;
  MSGPACK_DEFINE(hit, read);
};

struct MemcacheResponse {
  uint64_t timestamp = 0;
  std::vector<DB::TimestampValue> read_buffer;
  Operation operation = Operation::INVALID;
  Status s = Status::kOK;
  HitCount hit_count;
  FullAddr prev_addr;
  MSGPACK_DEFINE(timestamp, read_buffer, operation, s, hit_count, prev_addr);
};

struct MemcacheRequest {
  uint64_t timestamp;
  std::vector<DB::DB_Operation> operations;
  FullAddr resp_addr;
  FullAddr prev_addr;
  bool read_only;
  bool txn_op;
  MSGPACK_DEFINE(timestamp, operations, resp_addr, prev_addr, read_only, txn_op);
};

struct DBRequest {
  MemcacheResponse resp;
  std::vector<DB::DB_Operation> operations;
  FullAddr resp_addr;
  FullAddr prev_addr;
  bool read_only;
  bool txn_op;
  MSGPACK_DEFINE(resp, operations, resp_addr, prev_addr, read_only, txn_op);
};

const std::vector<std::string> zmq_router_ports = {"6000", "6002"};
const std::vector<std::string> zmq_router_resp_ports = {"6001", "6003"};

const std::vector<std::string> zmq_read_ports = {"6100", "6101", "6102", "6103"};
const std::vector<std::string> zmq_read_txn_ports = {"6200", "6201"};
const std::vector<std::string> zmq_write_ports = {"6300"};
const std::vector<std::string> zmq_db_ports = {"6400", "6401"};

} // benchmark

#endif // MEMCACHE_STRUCT_H_