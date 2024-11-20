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
  FullAddr resp_addr;
  FullAddr prev_addr;
  MSGPACK_DEFINE(timestamp, read_buffer, operation, s, hit_count, resp_addr, prev_addr);
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
  FullAddr server_addr;
  bool read_only;
  bool txn_op;
  MSGPACK_DEFINE(resp, operations, server_addr, read_only, txn_op);
};

struct InvalidRequest {
  int shard_id = 0;
  int hash_id = 0;
  std::vector<DB::DB_Operation> wops;
  std::vector<DB::DB_Operation> wops_txn;
  MSGPACK_DEFINE(shard_id, hash_id, wops, wops_txn);
};

const std::vector<std::string> zmq_router_ports = {"6000", "6002"};
const std::vector<std::string> zmq_router_rports = {"6001", "6003"};

const std::vector<std::string> zmq_read_ports = {"6100", "6102", "6104", "6106"};
const std::vector<std::string> zmq_read_rports = {"6101", "6103", "6105", "6107"};
const std::vector<std::string> zmq_read_txn_ports = {"6200", "6202"};
const std::vector<std::string> zmq_read_txn_rports = {"6201", "6203"};
const std::vector<std::string> zmq_write_ports = {"6300"};
const std::vector<std::string> zmq_write_rports = {"6301"};

const std::vector<std::string> zmq_dbr_ports = {"6400", "6401"};
const std::vector<std::string> zmq_dbw_ports = {"6500"};
const std::string zmq_invalid_port = "6600";

} // benchmark

#endif // MEMCACHE_STRUCT_H_