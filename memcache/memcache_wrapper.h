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
#include "memcache_struct.h"

#define RTHREADS 2

#define ENQUEUE_RESPONSE(resp) \
    if (responses.find(req.resp_addr) == responses.end()) { \
        responses[req.resp_addr] = new WebQueuePush<MemcacheResponse>(new zmq::context_t(1)); \
        responses[req.resp_addr]->connect(req.resp_addr.port, req.resp_addr.addr); \
    } \
    resp.prev_addr = req.prev_addr; \
    responses[req.resp_addr]->enqueue(resp);

namespace benchmark {

class MemcacheWrapper {
 public:
  MemcacheWrapper(std::vector<DB*>& dbr, std::vector<DB*>& dbw, std::string self_addr="127.0.0.1"):
    dbr_(dbr), dbw_(dbw), self_addr_(self_addr) {
    std::cout << "creating MemcacheWrapper" << std::endl;
  }
  ~MemcacheWrapper() {
    Reset();
  }

  void Start() {
    for (size_t i = 0; i < zmq_read_ports.size(); i++) {
      auto& db_port = zmq_db_ports[i % zmq_db_ports.size()];
      thread_pool_.push_back(std::async(std::launch::async, 
        &MemcacheWrapper::PollRead, this, zmq_read_ports[i], db_port
      ));
    }
    for (size_t i = 0; i < zmq_read_txn_ports.size(); i++) {
      int j = i + zmq_read_ports.size();
      auto& db_port = zmq_db_ports[j % zmq_db_ports.size()];
      thread_pool_.push_back(std::async(std::launch::async, 
        &MemcacheWrapper::PollReadTxn, this, zmq_read_txn_ports[i], db_port
      ));
    }
    for (size_t i = 0; i < zmq_write_ports.size(); i++) {
      thread_pool_.push_back(std::async(std::launch::async,
        &MemcacheWrapper::PollWrite, this, zmq_write_ports[i], dbw_[i], 3000
      ));
    }
    for (size_t i = 0; i < zmq_db_ports.size(); i++) {
      thread_pool_.push_back(std::async(std::launch::async, 
        &MemcacheWrapper::DBThread, this, dbr_[i], zmq_db_ports[i]
      ));
    }
    std::cout << "starting MemcacheWrapper" << std::endl;
  }

  void Reset() {}

 private:
  static uint64_t getTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  }

  void PollRead(std::string port, std::string db_port) {
    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port, self_addr_);
    std::unordered_map<FullAddr, WebQueuePush<MemcacheResponse>*, FullAddrHash> responses;
    WebQueuePush<uintptr_t> db_queue(new zmq::context_t(1));
    db_queue.connect(db_port);

    MemcacheRequest req;
    MemcachedClient memcache_get;
    while (true) {
      if (write_txn_flag.load() || !requests.dequeue(req)) {
        continue;
      }
      // std::cout << "read from " << req.resp_addr.addr << ":" << req.resp_addr.port << std::endl; 

      MemcacheResponse resp;
      resp.timestamp = req.timestamp;
      const auto& operations = req.operations;
      auto& read_buffer = resp.read_buffer;
      
      resp.operation = operations[0].operation;
      resp.hit_count.read += 1;

      if (memcache_get.get(operations[0], read_buffer)) {
        resp.s = Status::kOK;
        resp.hit_count.hit += 1;
        ENQUEUE_RESPONSE(resp);
      } else {
        auto* db_req = new DBRequest{resp, operations, req.resp_addr, req.prev_addr, true, false};
        db_queue.enqueue(reinterpret_cast<uintptr_t>(db_req));
      }
    }
  }

  void PollReadTxn(std::string port, std::string db_port) {
    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port, self_addr_);
    std::unordered_map<FullAddr, WebQueuePush<MemcacheResponse>*, FullAddrHash> responses;
    WebQueuePush<uintptr_t> db_queue(new zmq::context_t(1));
    db_queue.connect(db_port);

    MemcacheRequest req;
    MemcachedClient memcache_get;
    while (true) {
      if (write_flag.load() || !requests.dequeue(req)) {
        continue;
      }

      MemcacheResponse resp;
      resp.timestamp = req.timestamp;
      const auto& operations = req.operations;
      auto& read_buffer = resp.read_buffer;
      
      resp.operation = Operation::READTRANSACTION;
      read_buffer.reserve(operations.size());
      resp.hit_count.read += operations.size();

      std::vector<DB::DB_Operation> miss_ops;
      for (size_t i = 0; i < operations.size(); i++) {
        if (!memcache_get.get(operations[i], read_buffer)) {
          read_buffer.emplace_back(-1, "");
          miss_ops.push_back(operations[i]);
        }
      }

      if (miss_ops.empty()) {
        resp.s = Status::kOK; 
        ENQUEUE_RESPONSE(resp);
      } else {
        resp.hit_count.hit += operations.size() - miss_ops.size();
        auto* db_req = new DBRequest{resp, miss_ops, req.resp_addr, req.prev_addr, true, true};
        db_queue.enqueue(reinterpret_cast<uintptr_t>(db_req));
      }
    }
  }

  void PollWrite(std::string port, DB *db, int interval) {
    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port, self_addr_);
    requests.setup(0);
    std::unordered_map<FullAddr, WebQueuePush<MemcacheResponse>*, FullAddrHash> responses;

    MemcacheRequest req;
    MemcachedClient memcache_put;

    uint64_t last_time = getTimestamp(), cur_time = 0;
    std::vector<DB::DB_Operation> wops, wops_txn;
    while (true) {
      cur_time = getTimestamp();
      if (cur_time - last_time > interval) {
        write_flag.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (const auto& op: wops) {
          memcache_put.invalidate(op);
        }
        write_txn_flag.store(true);
        for (const auto& op: wops_txn) {
          memcache_put.invalidate(op);
        }
        write_txn_flag.store(false);
        write_flag.store(false);

        std::cout << "write back " << wops.size() << " scala and " << wops_txn.size() << " txn" << std::endl;
        wops.clear();
        wops_txn.clear();
        last_time = cur_time;
      }

      if (!requests.dequeue(req)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      }
      
      MemcacheResponse resp;
      resp.timestamp = req.timestamp;
      const auto& operations = req.operations;
      auto& read_buffer = resp.read_buffer;

      if (req.txn_op) {
        resp.operation = Operation::WRITETRANSACTION;
        resp.s = db->ExecuteTransaction(operations, read_buffer, false);
        if (resp.s == Status::kOK) {
          wops_txn.insert(wops_txn.end(), operations.begin(), operations.end());
        }
      } else {
        resp.operation = operations[0].operation;
        resp.s = db->Execute(operations[0], read_buffer);
        if (resp.s == Status::kOK) {
          wops.push_back(operations[0]);
        }
      }
      ENQUEUE_RESPONSE(resp); 
    }
  }

  void DBThread(DB *db, std::string db_port) {
    std::unordered_map<FullAddr, WebQueuePush<MemcacheResponse>*, FullAddrHash> responses;
    WebQueuePull<uintptr_t> db_queue(new zmq::context_t(1), db_port);
    
    uintptr_t req_ptr;
    MemcachedClient memcache_put;
    while (true) {
      if (!db_queue.dequeue(req_ptr)) {
        continue;
      }

      DBRequest& req = *reinterpret_cast<DBRequest*>(req_ptr);
      MemcacheResponse& resp = req.resp;
      const auto& operations = req.operations;
      auto& read_buffer = resp.read_buffer;

      if (req.txn_op) {
        std::vector<DB::TimestampValue> rsl_db;
        resp.s = db->ExecuteTransaction(operations, rsl_db, true);
        if (resp.s == Status::kOK) {
          size_t db_pos = 0;
          for (size_t i = 0; i < read_buffer.size(); i++) {
            if (read_buffer[i].timestamp != -1){
              continue;
            }
            read_buffer[i] = rsl_db[db_pos];
            memcache_put.put(operations[db_pos], rsl_db[db_pos]);
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
      delete reinterpret_cast<DBRequest*>(req_ptr);
    }
  }

  std::vector<DB*> dbr_, dbw_;
  std::vector<std::future<void>> thread_pool_;
  const std::string self_addr_;
  
  std::atomic<bool> write_flag{false};
  std::atomic<bool> write_txn_flag{false};
};

} // benchmark

#endif // MEMCACHE_WRAPPER_H_
