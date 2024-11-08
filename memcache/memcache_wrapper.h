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

// #define DB_REQUEST(operations, read_buffer, txn_op, read_only, ret) \
//     do { \
//         std::atomic<bool> flag{false}; \
//         DBRequest db_req{operations, ptr2uint(&read_buffer), txn_op, read_only, ptr2uint(&ret), ptr2uint(&flag)}; \
//         db_queue->enqueue(db_req); \
//         while (!flag); \
//     } while (0)

#define DB_REQUEST(operations, read_buffer, txn_op, read_only, ret) \
    do { \
        for (int i = 0; i < operations.size(); i++) \
          read_buffer.push_back({-1, ""}); \
    } while (0)

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
  uintptr_t read_buffer; // std::vector<DB::TimestampValue>*
  bool txn_op;
  bool read_only;
  uintptr_t s; // Status*
  uintptr_t finished; // std::atomic<bool>*
  MSGPACK_DEFINE(operations, read_buffer, txn_op, read_only, s, finished);
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
    std::unordered_map<std::string, WebQueuePush<MemcacheResponse>*> responses;

    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port);
    MemcachedClient memcache_get;
    MemcachedClient memcache_put;

    WebQueuePush<DBRequest> db_queue(new zmq::context_t(1));
    db_queue.connect(zmq_db_port);

    MemcacheRequest req;
    MemcacheResponse resp;
    while (true) {
      if (!requests.dequeue(req)) {
        continue;
      }
      if (req.txn_op) {
        resp = ReadTxn(req, &memcache_get, &memcache_put, &db_queue);
      } else {
        resp = Read(req, &memcache_get, &memcache_put, &db_queue);
      }
      resp.timestamp = req.timestamp;

      if (responses.find(req.resp_port) == responses.end()) {
        responses[req.resp_port] = new WebQueuePush<MemcacheResponse>
                                  (new zmq::context_t(1));
        responses[req.resp_port]->connect(req.resp_port);
      }
      responses[req.resp_port]->enqueue(resp);
    }
  }

  static void PollWrite(std::string port) {
    std::unordered_map<std::string, WebQueuePush<MemcacheResponse>*> responses;

    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port);
    MemcachedClient memcache_put;

    WebQueuePush<DBRequest> db_queue(new zmq::context_t(1));
    db_queue.connect(zmq_db_port);

    MemcacheRequest req;
    MemcacheResponse resp;
    while (true) {
      if (!requests.dequeue(req)) {
        continue;
      }
      if (req.txn_op) {
        resp = WriteTxn(req, &memcache_put, &db_queue);
      } else {
        resp = Write(req, &memcache_put, &db_queue);
      }

      resp.timestamp = req.timestamp;
      if (responses.find(req.resp_port) == responses.end()) {
        responses[req.resp_port] = new WebQueuePush<MemcacheResponse>
                                  (new zmq::context_t(1));
        responses[req.resp_port]->connect(req.resp_port);
      }
      responses[req.resp_port]->enqueue(resp);
    }
  }

  // Has segmentation fault, and db + loop wait is slow (when 0 hit)
  static void DBThread(DB *db) {
    WebQueuePull<DBRequest> db_queue(new zmq::context_t(1), zmq_db_port);
    DBRequest req;
    while (true) {
      if (!db_queue.dequeue(req)) {
        continue;
      }
      auto* s = uint2ptr<Status>(req.s);
      auto* read_buffer = uint2ptr<std::vector<DB::TimestampValue>>(req.read_buffer);
      auto* finished = uint2ptr<bool>(req.finished);
      for (int i = 0; i < req.operations.size(); i++)
        read_buffer->push_back({-1, ""});
      // if (req.txn_op) {
      //   *s = db->ExecuteTransaction(req.operations, *read_buffer, req.read_only);
      // } else {
      //   *s = db->Execute(req.operations[0], *read_buffer);
      // }
      *finished = true;
    }
  }

  static MemcacheResponse Read(MemcacheRequest &req,
                               MemcachedClient *memcache_get,
                               MemcachedClient *memcache_put,
                               WebQueuePush<DBRequest> *db_queue) {
    MemcacheResponse resp;
    const auto& operations = req.operations;
    auto& read_buffer = resp.read_buffer;
    resp.operation = operations[0].operation;

    resp.read_count += 1;
    if (memcache_get->get(operations[0], read_buffer)) {
      resp.s = Status::kOK;
      resp.hit_count += 1;
    } else {
      DB_REQUEST(operations, read_buffer, false, true, resp.s);
      if (resp.s == Status::kOK) {
        memcache_put->put(operations[0], read_buffer[0]);
      }
    }
    return resp;
  }

  static MemcacheResponse Write(MemcacheRequest &req,
                                MemcachedClient *memcache_put,
                                WebQueuePush<DBRequest> *db_queue) {
    MemcacheResponse resp;
    const auto& operations = req.operations;
    auto& read_buffer = resp.read_buffer;
    resp.operation = operations[0].operation;

    DB_REQUEST(operations, read_buffer, false, false, resp.s);
    memcache_put->invalidate(operations[0]);
    return resp;
  }

  static MemcacheResponse ReadTxn(MemcacheRequest &req,
                                  MemcachedClient *memcache_get,
                                  MemcachedClient *memcache_put,
                                  WebQueuePush<DBRequest> *db_queue) {
    MemcacheResponse resp;
    resp.operation = Operation::READTRANSACTION;
    const auto& operations = req.operations;
    auto& read_buffer = resp.read_buffer;
    
    std::vector<DB::DB_Operation> miss_ops;
    std::vector<DB::TimestampValue> rsl_db;
      
    resp.read_count += operations.size();
    for (size_t i = 0; i < operations.size(); i++) {
      if (memcache_get->get(operations[i], read_buffer)) {
        resp.hit_count++;
      } else {
        read_buffer.emplace_back(-1, "");
        miss_ops.push_back(operations[i]);
      }
    }
    assert(read_buffer.size() == operations.size());

    if (miss_ops.empty()) {
      resp.s = Status::kOK; 
      return resp;
    }

    DB_REQUEST(operations, rsl_db, true, true, resp.s);
    if (resp.s == Status::kOK) {
      size_t db_pos = 0;
      for (size_t i = 0; i < operations.size(); i++) {
        if (read_buffer[i].timestamp == -1){
          read_buffer[i] = rsl_db[db_pos];
          memcache_put->put(operations[i], rsl_db[db_pos]);
          db_pos++;
        } 
      }
    } else {
      read_buffer.clear();
    }
    return resp;
  }

  static MemcacheResponse WriteTxn(MemcacheRequest &req,
                                   MemcachedClient *memcache_put,
                                   WebQueuePush<DBRequest> *db_queue) {
    MemcacheResponse resp;
    resp.operation = Operation::WRITETRANSACTION;
    const auto& operations = req.operations;
    auto& read_buffer = resp.read_buffer;
    
    DB_REQUEST(operations, read_buffer, true, false, resp.s);
    for (const DB::DB_Operation& op : operations) {
      memcache_put->invalidate(op);
    }
    return resp;
  }
  
  DB *db_;
  std::vector<std::future<void>> thread_pool_;
};

} // benchmark

#endif // MEMCACHE_WRAPPER_H_
