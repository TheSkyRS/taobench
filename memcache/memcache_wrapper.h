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
  bool read_only;
  bool txn_op;
  MSGPACK_DEFINE(timestamp, operations, resp_port, read_only, txn_op);
};

struct DBRequest {
  MemcacheResponse resp;
  std::vector<DB::DB_Operation> operations;
  std::string resp_port;
  bool read_only;
  bool txn_op;
  MSGPACK_DEFINE(resp, operations, resp_port, read_only, txn_op);
};

const std::vector<std::string> zmq_read_ports = {"6100", "6101", "6102", "6103"};
const std::vector<std::string> zmq_read_txn_ports = {"6200", "6201"};
const std::vector<std::string> zmq_write_ports = {"6300", "6301"};
const std::vector<std::string> zmq_db_read_ports = {"6400", "6401"}; // this may not exceed read threads
const std::vector<std::string> zmq_db_write_ports = {"6500"}; // this may not exceed write threads

class MemcacheWrapper {
 public:
  MemcacheWrapper(std::vector<DB*>& dbr, std::vector<DB*>& dbw):
    dbr_(dbr), dbw_(dbw) {
    std::cout << "creating MemcacheWrapper" << std::endl;
  }
  ~MemcacheWrapper() {
    Reset();
  }

  void Start() {
    for (size_t i = 0; i < zmq_read_ports.size(); i++) {
      auto& db_port = zmq_db_read_ports[i % zmq_db_read_ports.size()];
      thread_pool_.push_back(
        std::async(std::launch::async, PollRead, zmq_read_ports[i], db_port)
      );
    }
    // for (size_t i = 0; i < zmq_read_txn_ports.size(); i++) {
    //   int j = i + zmq_read_ports.size();
    //   auto& = zmq_db_read_ports[j % zmq_db_read_ports.size()];
    //   thread_pool_.push_back(
    //     std::async(std::launch::async, PollReadTxn, zmq_read_txn_ports[i], db_port)
    //   );
    // }
    for (size_t i = 0; i < zmq_write_ports.size(); i++) {
      auto& db_port = zmq_db_write_ports[i % zmq_db_write_ports.size()];
      thread_pool_.push_back(
        std::async(std::launch::async, PollWrite, zmq_write_ports[i], db_port)
      );
    }
    for (size_t i = 0; i < zmq_db_read_ports.size(); i++) {
      thread_pool_.push_back(
        std::async(std::launch::async, DBRThread, dbr_[i], zmq_db_read_ports[i])
      );
    }
    for (size_t i = 0; i < zmq_db_write_ports.size(); i++) {
      thread_pool_.push_back(
        std::async(std::launch::async, DBWThread, dbw_[i], zmq_db_write_ports[i])
      );
    }
  }

  void Reset() {}

 private:
  static uint64_t getTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  }

  static void PollRead(std::string port, std::string db_port) {
    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port);
    std::unordered_map<std::string, WebQueuePush<MemcacheResponse>*> responses;
    WebQueuePush<DBRequest> db_queue(new zmq::context_t(1));
    db_queue.connect(db_port);

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
          db_queue.enqueue({resp, miss_ops, req.resp_port, true, true});
        }
      } else {
        resp.operation = operations[0].operation;
        resp.read_count += 1;

        if (memcache_get.get(operations[0], read_buffer)) {
          resp.s = Status::kOK;
          resp.hit_count += 1;
          ENQUEUE_RESPONSE(resp);
        } else {
          db_queue.enqueue({resp, operations, req.resp_port, true, false});
        }
      }
    }
  }

  static void PollWrite(std::string port, std::string db_port) {
    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port);
    std::unordered_map<std::string, WebQueuePush<MemcacheResponse>*> responses;
    WebQueuePush<DBRequest> db_queue(new zmq::context_t(1));
    db_queue.connect(db_port);

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
        db_queue.enqueue({resp, operations, req.resp_port, false, true});
      } else {
        resp.operation = operations[0].operation;
        db_queue.enqueue({resp, operations, req.resp_port, false, false});
      }
    }
  }

  static void DBRThread(DB *db, std::string db_port) {
    std::unordered_map<std::string, WebQueuePush<MemcacheResponse>*> responses;
    WebQueuePull<DBRequest> db_queue(new zmq::context_t(1), db_port);
    
    DBRequest req;
    MemcachedClient memcache_put;
    while (true) {
      if (!db_queue.dequeue(req)) {
        continue;
      }

      MemcacheResponse& resp = req.resp;
      const auto& operations = req.operations;
      auto& read_buffer = resp.read_buffer;

      if (req.txn_op) {
        std::vector<DB::TimestampValue> rsl_db;
        resp.s = db->ExecuteTransaction(operations, rsl_db, true);
        if (resp.s == Status::kOK) {
          size_t db_pos = 0;
          for (size_t i = 0; i < operations.size(); i++) {
            if (read_buffer[i].timestamp != -1){
              continue;
            }
            read_buffer[i] = rsl_db[db_pos];
            memcache_put.put(operations[i], rsl_db[db_pos]);
            db_pos++;
          } 
        } else {
          read_buffer.clear();
        }
      } else {
        resp.s = db->Execute(operations[0], read_buffer);
        if (resp.s == Status::kOK) {
          memcache_put.put(operations[0], read_buffer[0]);
        }
      }
      ENQUEUE_RESPONSE(resp);  
    }
  }

  static void DBWThread(DB *db, std::string db_port) {
    std::unordered_map<std::string, WebQueuePush<MemcacheResponse>*> responses;
    WebQueuePull<DBRequest> db_queue(new zmq::context_t(1), db_port);
    
    DBRequest req;
    MemcachedClient memcache_put;
    while (true) {
      if (!db_queue.dequeue(req)) {
        continue;
      }

      MemcacheResponse& resp = req.resp;
      const auto& operations = req.operations;
      auto& read_buffer = resp.read_buffer;

      if (req.txn_op) {
        resp.s = db->ExecuteTransaction(operations, read_buffer, false);
        if (resp.s == Status::kOK) {
          for (const DB::DB_Operation& op : operations) {
            memcache_put.invalidate(op);
          }
        }
      } else {
        resp.s = db->Execute(operations[0], read_buffer);
        if (resp.s == Status::kOK) {
          memcache_put.invalidate(operations[0]);
        }
      }
      ENQUEUE_RESPONSE(resp);  
    }
  }

  std::vector<DB*> dbr_, dbw_;
  std::vector<std::future<void>> thread_pool_;
};

} // benchmark

#endif // MEMCACHE_WRAPPER_H_
