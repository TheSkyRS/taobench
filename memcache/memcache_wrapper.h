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

namespace benchmark {

class MemcacheWrapper {
  typedef std::unordered_map<FullAddr, WebQueuePush<MemcacheResponse>*, FullAddrHash> ResponseMap;

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
      auto& db_port = zmq_dbr_ports[i % zmq_dbr_ports.size()];
      thread_pool_.push_back(std::async(std::launch::async, &MemcacheWrapper::PollRead, 
        this, zmq_read_ports[i], db_port, zmq_read_rports[i]
      ));
    }
    for (size_t i = 0; i < zmq_read_txn_ports.size(); i++) {
      int j = i + zmq_read_ports.size();
      auto& db_port = zmq_dbr_ports[j % zmq_dbr_ports.size()];
      thread_pool_.push_back(std::async(std::launch::async, &MemcacheWrapper::PollReadTxn, 
        this, zmq_read_txn_ports[i], db_port, zmq_read_txn_rports[i]
      ));
    }
    for (size_t i = 0; i < zmq_write_ports.size(); i++) {
      auto& db_port = zmq_dbw_ports[i % zmq_dbw_ports.size()];
      thread_pool_.push_back(std::async(std::launch::async, &MemcacheWrapper::PollWrite, 
        this, zmq_write_ports[i], db_port, zmq_write_rports[i]
      ));
    }
    for (size_t i = 0; i < zmq_dbr_ports.size(); i++) {
      thread_pool_.push_back(std::async(std::launch::async, &MemcacheWrapper::DBRThread, 
        this, dbr_[i], zmq_dbr_ports[i]
      ));
    }
    for (size_t i = 0; i < zmq_dbw_ports.size(); i++) {
      thread_pool_.push_back(std::async(std::launch::async, &MemcacheWrapper::DBWThread, 
        this, dbw_[i], zmq_dbw_ports[i], 3000
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

  inline void SendResponse(MemcacheResponse& resp, FullAddr& addr, ResponseMap& responses) {
    if (responses.find(addr) == responses.end()) {
      responses[addr] = new WebQueuePush<MemcacheResponse>(new zmq::context_t(1));
      responses[addr]->connect(addr.port, addr.addr);
    }
    responses[addr]->enqueue(resp);
  }

  inline void InvalidCache(InvalidRequest& req, MemcachedClient& memcache_put) {
    write_flag.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    for (const auto& op: req.wops) {
      memcache_put.invalidate(op);
    }
    write_txn_flag.store(true);
    for (const auto& op: req.wops_txn) {
      memcache_put.invalidate(op);
    }
    write_txn_flag.store(false);
    write_flag.store(false);

    std::cout << "write back " << req.wops.size() << " scala and " << req.wops_txn.size() 
              << " txn" << std::endl;
  }


  void PollRead(std::string port, std::string db_port, std::string ans_port) {
    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port, self_addr_);
    WebQueuePull<MemcacheResponse> answers(new zmq::context_t(1), ans_port, self_addr_);
    ResponseMap responses;
    WebQueuePush<DBRequest> db_queue(new zmq::context_t(1));
    db_queue.connect(db_port, self_addr_);

    MemcacheRequest req;
    MemcacheResponse ans;
    MemcachedClient memcache_get;
    while (true) {
      if (answers.dequeue(ans)) {
        SendResponse(ans, ans.resp_addr, responses);
      }

      if (write_txn_flag.load() || !requests.dequeue(req)) {
        continue;
      }

      MemcacheResponse resp;
      resp.timestamp = req.timestamp;
      const auto& operations = req.operations;
      auto& read_buffer = resp.read_buffer;
      
      // std::cout << "poll read ops " << operations.size() << std::endl;
      resp.operation = operations[0].operation;
      resp.hit_count.read += 1;
      resp.prev_addr = req.prev_addr;
      resp.resp_addr = req.resp_addr;

      if (memcache_get.get(operations[0], read_buffer)) {
        resp.s = Status::kOK;
        resp.hit_count.hit += 1;
        SendResponse(resp, resp.resp_addr, responses);
      } else {
        db_queue.enqueue({resp, operations, {self_addr_, ans_port}, true, false});
      }
    }
  }

  void PollReadTxn(std::string port, std::string db_port, std::string ans_port) {
    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port, self_addr_);
    WebQueuePull<MemcacheResponse> answers(new zmq::context_t(1), ans_port, self_addr_);
    ResponseMap responses;
    WebQueuePush<DBRequest> db_queue(new zmq::context_t(1));
    db_queue.connect(db_port, self_addr_);

    MemcacheRequest req;
    MemcacheResponse ans;
    MemcachedClient memcache_get;
    while (true) {
      if (answers.dequeue(ans)) {
        SendResponse(ans, ans.resp_addr, responses);
      }

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
      resp.prev_addr = req.prev_addr;
      resp.resp_addr = req.resp_addr;

      std::vector<DB::DB_Operation> miss_ops;
      for (size_t i = 0; i < operations.size(); i++) {
        if (!memcache_get.get(operations[i], read_buffer)) {
          read_buffer.emplace_back(-1, "");
          miss_ops.push_back(operations[i]);
        }
      }

      if (miss_ops.empty()) {
        resp.s = Status::kOK; 
        SendResponse(resp, resp.resp_addr, responses);
      } else {
        resp.hit_count.hit += operations.size() - miss_ops.size();
        db_queue.enqueue({resp, miss_ops, {self_addr_, ans_port}, true, true});
      }
    }
  }

  void PollWrite(std::string port, std::string db_port, std::string ans_port) {
    WebQueuePull<MemcacheRequest> requests(new zmq::context_t(1), port, self_addr_);
    WebQueuePull<MemcacheResponse> answers(new zmq::context_t(1), ans_port, self_addr_);
    ResponseMap responses;
    
    WebQueuePush<DBRequest> db_queue(new zmq::context_t(1));
    db_queue.connect(db_port, self_addr_);
    WebSubscribe<InvalidRequest> invalid_sub(new zmq::context_t(1), zmq_invalid_port, self_addr_);

    MemcacheRequest req;
    MemcacheResponse ans;
    InvalidRequest invalid_req;
    MemcachedClient memcache_put;
    while (true) {
      if (invalid_sub.poll(invalid_req)) {
        InvalidCache(invalid_req, memcache_put);
      }

      if (answers.dequeue(ans)) {
        SendResponse(ans, ans.resp_addr, responses);
      }

      if (!requests.dequeue(req)) {
        continue;
      }
      
      MemcacheResponse resp;
      resp.timestamp = req.timestamp;
      resp.prev_addr = req.prev_addr;
      resp.resp_addr = req.resp_addr;
      const auto& operations = req.operations;

      db_queue.enqueue({resp, operations, {self_addr_, ans_port}, req.read_only, req.txn_op});
    }
  }

  void DBRThread(DB *db, std::string db_port) {
    ResponseMap responses;
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
      SendResponse(resp, resp.resp_addr, responses);  
    }
  }

  void DBWThread(DB *db, std::string db_port, int interval) {
    ResponseMap responses;
    WebQueuePull<DBRequest> db_queue(new zmq::context_t(1), db_port, self_addr_);
    WebPublish<InvalidRequest> invalid_pub(new zmq::context_t(1), zmq_invalid_port, self_addr_);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 99);

    DBRequest req;
    uint64_t last_time = getTimestamp(), cur_time = 0;
    std::vector<DB::DB_Operation> wops, wops_txn;
    while (true) {
      cur_time = getTimestamp();
      if (cur_time - last_time > interval) {
        invalid_pub.push({0, dis(gen), wops, wops_txn});
        wops.clear();
        wops_txn.clear();
        last_time = cur_time;
      }

      if (!db_queue.dequeue(req)) {
        continue;
      }

      MemcacheResponse& resp = req.resp;
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
      SendResponse(resp, req.server_addr, responses); 
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
