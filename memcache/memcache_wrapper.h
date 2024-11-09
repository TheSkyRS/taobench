#ifndef MEMCACHE_WRAPPER_H_
#define MEMCACHE_WRAPPER_H_

#include <string>
#include <vector>
#include <iostream>
#include <cassert>
#include <future>
#include <atomic>
#include <cstdlib>
#include <zmq.hpp>
#include <unordered_map>

#include "db.h"
#include "timer.h"
#include "utils.h"
#include "memcache.h"
#include "lock_free_queue.h"
#include "db_factory.h"

#define RTHREADS 2

#define ENQUEUE_RESPONSE(resp) \
    if (responses.find(req.resp_port) == responses.end()) { \
        responses[req.resp_port] = new WebQueuePush<MemcacheResponse>(new zmq::context_t(1)); \
        responses[req.resp_port]->connect(req.resp_port); \
    } \
    responses[req.resp_port]->enqueue(resp);

namespace benchmark {

template <typename T>
uintptr_t ptr2uint(T* const& v) {
  return reinterpret_cast<uintptr_t>(v);
}

template <typename T>
T* uint2ptr(uintptr_t const& v) {
  return reinterpret_cast<T*>(v);
}

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
  std::string resp_port;
  bool txn_op;
  bool read_only;
  MSGPACK_DEFINE(timestamp, operations, resp_port, txn_op, read_only);
};

struct DBRequest {
  std::vector<DB::DB_Operation> operations;
  MemcacheResponse resp;
  bool read_only;
  bool txn_op;
  MSGPACK_DEFINE(operations, resp, read_only, txn_op);
};

const std::vector<std::string> zmq_read_ports = {"6100", "6101"};
const std::vector<std::string> zmq_write_ports = {"6200", "6201"};
const std::string zmq_db_port = "6400";

class MemcacheWrapper {
 public:
  MemcacheWrapper(DB *db): db_(db) {
    std::cout << "creating MemcacheWrapper" << std::endl;
  }
  ~MemcacheWrapper() {
    Reset();
  }

  void Start() {
    for (size_t i = 0; i < zmq_read_ports.size(); i++) {
      thread_pool_.push_back(
        std::async(std::launch::async, PollRead, zmq_read_ports[i])
      );
    }
    for (size_t i = 0; i < zmq_write_ports.size(); i++) {
      thread_pool_.push_back(
        std::async(std::launch::async, PollWrite, zmq_write_ports[i])
      );
    }
    thread_pool_.push_back(
      std::async(std::launch::async, DBThread, db_)
    );
  }

  void Reset() {}

 private:
  static void PollRead(std::string port) {
    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port);
    std::unordered_map<std::string, WebQueuePush<MemcacheResponse>*> responses;
    WebQueuePush<DBRequest> db_queue(new zmq::context_t(1));
    db_queue.connect(zmq_db_port);

    MemcacheRequest req;
    MemcachedClient memcache_get;
    while (true) {
      if (!requests.dequeue(req)) {
        continue;
      }

      MemcacheResponse resp;
      resp.timestamp = req.timestamp;
      const auto& operations = req.operations;
      auto& read_buffer = resp.read_buffer;
      
      if (req.txn_op) {
        resp.operation = Operation::READTRANSACTION;
        resp.read_count += operations.size();

        std::vector<DB::DB_Operation> miss_ops;
        for (size_t i = 0; i < operations.size(); i++) {
          if (memcache_get.get(operations[i], read_buffer)) {
            resp.hit_count++;
          } else {
            read_buffer.emplace_back(-1, "");
            miss_ops.push_back(operations[i]);
          }
        }

        if (miss_ops.empty()) {
          resp.s = Status::kOK; 
          ENQUEUE_RESPONSE(resp);
        } else {
          db_queue->enqueue({miss_ops, resp, true, true});
        }
      } else {
        resp.operation = operations[0].operation;
        resp.read_count += 1;

        if (memcache_get->get(operations[0], read_buffer)) {
          resp.s = Status::kOK;
          resp.hit_count += 1;
          ENQUEUE_RESPONSE(resp);
        } else {
          db_queue->enqueue({miss_ops, resp, true, false});
        }
      }
    }
  }

  static void PollWrite(std::string port) {
    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port);
    std::unordered_map<std::string, WebQueuePush<MemcacheResponse>*> responses;
    WebQueuePush<DBRequest> db_queue(new zmq::context_t(1));
    db_queue.connect(zmq_db_port);

    MemcacheRequest req;
    while (true) {
      if (!requests.dequeue(req)) {
        continue;
      }

      MemcacheResponse resp;
      resp.timestamp = req.timestamp;
      const auto& operations = req.operations;
      auto& read_buffer = resp.read_buffer;

      if (req.txn_op) {
        resp.operation = Operation::WRITETRANSACTION;
        db_queue->enqueue({operations, resp, false, true});
      } else {
        resp.operation = operations[0].operation;
        db_queue->enqueue({operations, resp, false, false});
      }
    }
  }

  // Has segmentation fault, and db + loop wait is slow (when 0 hit)
  static void DBThread(DB *db) {
    WebQueuePull<DBRequest> db_queue(new zmq::context_t(1), zmq_db_port);
    MemcacheRequest req;
    MemcachedClient memcache_put;
    while (true) {
      if (!db_queue.dequeue(req)) {
        continue;
      }

      MemcacheResponse& resp = req.resp;
      const auto& operations = req.operations;
      auto& read_buffer = resp.read_buffer;

      if (req.read_only) {
        if (req.txn_op) {
          std::vector<DB::TimestampValue> rsl_db;
          resp.s = db->ExecuteTransaction(operations, rsl_db, true);
          if (resp.s != Status::kOK) {
            continue;
          }

          size_t db_pos = 0;
          for (size_t i = 0; i < operations.size(); i++) {
            if (read_buffer[i].timestamp == -1){
              read_buffer[i] = rsl_db[db_pos];
              memcache_put.put(operations[i], rsl_db[db_pos]);
              db_pos++;
            } 
          }  
        } else {
          resp.s = db->Execute(operations[0], read_buffer);
          if (resp.s == Status::kOK) {
            memcache_put.put(operations[0], read_buffer[0]);
          }
        }
      } else {
        if (req.txn_op) {
          resp.s = db->ExecuteTransaction(operations, rsl_db, false);
          if (resp.s == Status::kOK) {
            for (const DB::DB_Operation& op : operations) {
              memcache_put->invalidate(op);
            }
          }
        } else {
          resp.s = db->Execute(operations[0], read_buffer);
          if (resp.s == Status::kOK) {
            memcache_put.invalidate(operations[0]);
          }
        }
      }
      ENQUEUE_RESPONSE(resp);
    }
  }

} // benchmark

#endif // MEMCACHE_WRAPPER_H_
