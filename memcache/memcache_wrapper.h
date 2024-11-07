#ifndef MEMCACHE_WRAPPER_H_
#define MEMCACHE_WRAPPER_H_

#include <string>
#include <vector>
#include <iostream>
#include <cassert>
#include <future>
#include <atomic>
#include <cstdlib>
#include <ctime>

#include "db.h"
#include "timer.h"
#include "utils.h"
#include "memcache.h"
#include "lock_free_queue.h"
#include "db_factory.h"

#define RTHREADS 2

#define DB_REQUEST(operations, read_buffer, txn_op, read_only, ret) \
    do { \
        std::atomic<bool> flag{false}; \
        DBRequest db_req{operations, ptr2uint(&read_buffer), txn_op, read_only, ptr2uint(&ret), ptr2uint(&flag)}; \
        db_queue->enqueue(db_req); \
        while (!flag); \
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
  std::vector<DB::TimestampValue> read_buffer;
  Status s;
  int hit_count = 0;
  int read_count = 0;
  MSGPACK_DEFINE(read_buffer, s, hit_count, read_count);
};

struct MemcacheRequest {
  std::vector<DB::DB_Operation> operations;
  uintptr_t result_queue; // LockFreeQueue<MemcacheResponse>*
  bool txn_op;
  bool read_only;
  MSGPACK_DEFINE(operations, result_queue, txn_op, read_only);
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

class MemcacheWrapper {
 public:
  MemcacheWrapper(DB *db): db_(db) {
    std::srand(static_cast<unsigned>(std::time(0)));
  }
  ~MemcacheWrapper() {}
  void Start() {
    for (size_t i = 0; i < RTHREADS; i++) {
      thread_pool_.push_back(
        std::async(std::launch::async, PollRead, &read_queues_[i], &db_queue_, i)
      );
      thread_pool_.push_back(
        std::async(std::launch::async, PollWrite, &write_queues_[i], &db_queue_)
      );
    }
    thread_pool_.push_back(
      std::async(std::launch::async, DBThread, &db_queue_, db_)
    );
  }

  void SendCommand(MemcacheRequest req, int idx=0) {
    if (req.read_only) {
      read_queues_[idx].enqueue(req);
    } else {
      write_queues_[idx].enqueue(req);
    }
  }

 private:
  static void PollRead(LockFreeQueue<MemcacheRequest> *requests, 
                       LockFreeQueue<DBRequest> *db_queue, int tid) {
    MemcachedClient *memcache_get = new MemcachedClient();
    MemcachedClient *memcache_put = new MemcachedClient();
    MemcacheRequest req;
    MemcacheResponse resp;
    while (true) {
      if (!requests->dequeue(req)) {
        continue;
      }
      // std::cout << "read" << tid << std::endl;
      if (req.txn_op) {
        resp = ReadTxn(req, memcache_get, memcache_put, db_queue);
      } else {
        resp = Read(req, memcache_get, memcache_put, db_queue);
      }
      auto* rq = uint2ptr<LockFreeQueue<MemcacheResponse>>(req.result_queue);
      rq->enqueue(resp);
    }
  }

  static void PollWrite(LockFreeQueue<MemcacheRequest> *requests,
                        LockFreeQueue<DBRequest> *db_queue) {
    MemcachedClient *memcache_put = new MemcachedClient();
    MemcacheRequest req;
    MemcacheResponse resp;
    while (true) {
      if (!requests->dequeue(req)) {
        continue;
      }
      if (req.txn_op) {
        resp = WriteTxn(req, memcache_put, db_queue);
      } else {
        resp = Write(req, memcache_put, db_queue);
      }
      auto* rq = uint2ptr<LockFreeQueue<MemcacheResponse>>(req.result_queue);
      rq->enqueue(resp);
    }
  }

  static void DBThread(LockFreeQueue<DBRequest> *requests, DB *db) {
    DBRequest req;
    while (true) {
      if (!requests->dequeue(req)) {
        continue;
      }
      auto* s = uint2ptr<Status>(req.s);
      auto* read_buffer = uint2ptr<std::vector<DB::TimestampValue>>(req.read_buffer);
      auto* finished = uint2ptr<bool>(req.finished);
      if (req.txn_op) {
        *s = db->ExecuteTransaction(req.operations, *read_buffer, req.read_only);
      } else {
        *s = db->Execute(req.operations[0], *read_buffer);
      }
      *finished = true;
    }
  }

  static MemcacheResponse Read(MemcacheRequest &req,
                               MemcachedClient *memcache_get,
                               MemcachedClient *memcache_put,
                               LockFreeQueue<DBRequest> *db_queue) {
    MemcacheResponse resp;
    const auto& operations = req.operations;
    auto& read_buffer = resp.read_buffer;

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
                                LockFreeQueue<DBRequest> *db_queue) {
    MemcacheResponse resp;
    const auto& operations = req.operations;
    auto& read_buffer = resp.read_buffer;
    DB_REQUEST(operations, read_buffer, false, false, resp.s);
    memcache_put->invalidate(operations[0]);
    return resp;
  }

  static MemcacheResponse ReadTxn(MemcacheRequest &req,
                                  MemcachedClient *memcache_get,
                                  MemcachedClient *memcache_put,
                                  LockFreeQueue<DBRequest> *db_queue) {
    MemcacheResponse resp;
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
                                   LockFreeQueue<DBRequest> *db_queue) {
    MemcacheResponse resp;
    const auto& operations = req.operations;
    auto& read_buffer = resp.read_buffer;
    
    DB_REQUEST(operations, read_buffer, true, false, resp.s);
    for (const DB::DB_Operation& op : operations) {
      memcache_put->invalidate(op);
    }
    return resp;
  }
  
  DB *db_;
  LockFreeQueue<MemcacheRequest> read_queues_[RTHREADS];
  LockFreeQueue<MemcacheRequest> write_queues_[RTHREADS];
  LockFreeQueue<DBRequest> db_queue_;
  std::vector<std::future<void>> thread_pool_;
  std::atomic<uint64_t> cmd_count_ = 0;
};

} // benchmark

#endif // MEMCACHE_WRAPPER_H_
