#ifndef MEMCACHE_WRAPPER_H_
#define MEMCACHE_WRAPPER_H_

#include <string>
#include <vector>
#include <iostream>
#include <cassert>
#include <future>

#include "db.h"
#include "timer.h"
#include "utils.h"
#include "memcache.h"
#include "lock_free_queue.h"


namespace benchmark {

struct MemcacheResponse {
  std::vector<DB::TimestampValue> read_buffer;
  Status s;
  int hit_count = 0;
  int read_count = 0;
};

struct MemcacheRequest {
  std::vector<DB::DB_Operation> operations;
  LockFreeQueue<MemcacheResponse> *result_queue;
  bool txn_op;
  bool read_only;
};

class MemcacheWrapper {
 public:
  MemcacheWrapper(DB *db) : db_(db) {
    memcache_ = new MemcachedClient();
  }
  ~MemcacheWrapper() {
    delete db_;
  }
  void Start() {
    poll_thread_ = std::async(std::launch::async, PollThread, this);
  }
  void Cleanup() {
    db_->Cleanup();
  }

  void SendCommand(MemcacheRequest req) {
    request_queue_.enqueue(req);
  }

 private:
  static void PollThread(MemcacheWrapper* obj) {
    MemcacheRequest req;
    MemcacheResponse resp;
    while (true) {
      if (!obj->request_queue_.dequeue(req)) {
        continue;
      }
      if (req.txn_op) {
        resp = obj->ExecuteTransaction(req);
      } else {
        resp = obj->Execute(req);
      }
      req.result_queue->enqueue(resp);
    }
  }

  MemcacheResponse Execute(MemcacheRequest req) {
    assert(req.txn_op == false);
    MemcacheResponse resp;
    const auto& operation = req.operations[0];
    auto& read_buffer = resp.read_buffer;
    if (req.read_only) {
        resp.read_count += 1;
      if (memcache_->get(operation, read_buffer)) {
        resp.s = Status::kOK;
        resp.hit_count += 1;
      } else {
        resp.s = db_->Execute(operation, read_buffer, false);
        if (resp.s == Status::kOK) {
          memcache_->put(operation, read_buffer[0]);
        }
      }
    } else {
      resp.s = db_->Execute(operation, read_buffer, false);
      memcache_->invalidate(operation);
    }
    return resp;
  }

  MemcacheResponse ExecuteTransaction(MemcacheRequest req) {
    assert(req.txn_op == true);
    MemcacheResponse resp;
    const auto& operations = req.operations;
    auto& read_buffer = resp.read_buffer;
    if (req.read_only) {
      std::vector<DB::DB_Operation> miss_ops;
      std::vector<DB::TimestampValue> rsl_db;
      
      resp.read_count += operations.size();
      for (size_t i = 0; i < operations.size(); i++) {
        if (memcache_->get(operations[i], read_buffer)) {
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

      resp.s = db_->ExecuteTransaction(miss_ops, rsl_db, true);
      if (resp.s == Status::kOK) {
        size_t db_pos = 0;
        for (size_t i = 0; i < operations.size(); i++) {
          if (read_buffer[i].timestamp == -1){
            read_buffer[i] = rsl_db[db_pos];
            memcache_->put(operations[i], rsl_db[db_pos]);
            db_pos++;
          } 
        }
      } else {
        read_buffer.clear();
      }
    } else {
      resp.s = db_->ExecuteTransaction(operations, read_buffer, false);
      for (const DB::DB_Operation& op : operations) {
        memcache_->invalidate(op);
      }
    }
    assert(!operations.empty());

    return resp;
  }

  DB *db_;
  MemcachedClient *memcache_;
  LockFreeQueue<MemcacheRequest> request_queue_;
  std::future<void> poll_thread_;
};

} // benchmark

#endif // MEMCACHE_WRAPPER_H_
