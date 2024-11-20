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
#include "zmq_utils.h"
#include "db_factory.h"
#include "memcache_struct.h"

#define RTHREADS 2

namespace benchmark {

class MemcacheWrapper {
  typedef std::unordered_map<FullAddr, WebQueuePush<MemcacheData>*, FullAddrHash> ResponseMap;

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
        this, zmq_read_txn_ports[i], db_port, zmq_read_txn_rports[i], i
      ));
    }
    for (size_t i = 0; i < zmq_write_ports.size(); i++) {
      auto& db_port = zmq_dbw_ports[i % zmq_dbw_ports.size()];
      thread_pool_.push_back(std::async(std::launch::async, &MemcacheWrapper::PollWrite, 
        this, zmq_write_ports[i], db_port, zmq_write_rports[i], 3000
      ));
    }
    for (size_t i = 0; i < zmq_dbr_ports.size(); i++) {
      thread_pool_.push_back(std::async(std::launch::async, &MemcacheWrapper::DBRThread, 
        this, dbr_[i], zmq_dbr_ports[i]
      ));
    }
    for (size_t i = 0; i < zmq_dbw_ports.size(); i++) {
      thread_pool_.push_back(std::async(std::launch::async, &MemcacheWrapper::DBWThread, 
        this, dbw_[i], zmq_dbw_ports[i], 1000
      ));
    }
    std::cout << "starting MemcacheWrapper" << std::endl;
  }

  void Reset() {}

 private:
  inline uint64_t getTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  }

  inline void SendResponse(MemcacheData& resp, ResponseMap& responses) {
    FullAddr addr = resp.resp_addr.back();
    resp.resp_addr.pop_back();
    if (responses.find(addr) == responses.end()) {
      responses[addr] = new WebQueuePush<MemcacheData>(new zmq::context_t(1));
      responses[addr]->connect(addr.port, addr.addr);
    }
    responses[addr]->enqueue(resp);
  }

  inline void InvalidCache(WebSubscribe<InvalidCmd>& invalid_sub, MemcachedClient& memcache, int idx) {
    InvalidCmd req, full_req;
    while (invalid_sub.poll(req)) {
      if (req.hash_id % zmq_read_txn_ports.size() != idx) {
        continue;
      }
      full_req.wops.insert(full_req.wops.end(), req.wops.begin(), req.wops.end());
      full_req.wops_txn.insert(full_req.wops_txn.end(), req.wops_txn.begin(), req.wops_txn.end());
    }
    
    for (const auto& op: full_req.wops) {
      memcache.invalidate(op);
    }
    write_txn_flag.store(true);
    for (const auto& op: full_req.wops_txn) {
      memcache.invalidate(op);
    }
    write_txn_flag.store(false);
    std::cout << "write back " << full_req.wops.size() << " scala and " << full_req.wops_txn.size() 
              << " txn" << std::endl;
  }


  void PollRead(std::string port, std::string db_port, std::string ans_port) {
    WebQueuePull<MemcacheData> requests(new zmq::context_t(1), port, self_addr_);
    WebQueuePull<MemcacheData> answers(new zmq::context_t(1), ans_port, self_addr_);
    ResponseMap responses;
    
    WebQueuePush<MemcacheData> db_queue(new zmq::context_t(1));
    db_queue.connect(db_port, self_addr_);

    MemcacheData data;
    MemcachedClient memcache;
    const auto& operations = data.operations;
    auto& read_buffer = data.read_buffer;

    while (true) {
      if (answers.dequeue(data)) {
        if (data.s == Status::kOK) {
          memcache.put(operations[0], read_buffer[0]);
        }
        SendResponse(data, responses);
      }

      if (write_txn_flag.load() || !requests.dequeue(data)) {
        continue;
      }
      
      data.hit_count.read += 1;
      if (memcache.get(operations[0], read_buffer)) {
        data.s = Status::kOK;
        data.hit_count.hit += 1;
        SendResponse(data, responses);
      } else {
        data.resp_addr.emplace_back(self_addr_, ans_port);
        db_queue.enqueue(data);
      }
    }
  }

  void PollReadTxn(std::string port, std::string db_port, std::string ans_port, int idx) {
    WebQueuePull<MemcacheData> requests(new zmq::context_t(1), port, self_addr_);
    WebQueuePull<MemcacheData> answers(new zmq::context_t(1), ans_port, self_addr_);
    ResponseMap responses;

    WebSubscribe<InvalidCmd> invalid_sub(new zmq::context_t(1), zmq_invalid_port, self_addr_);
    WebQueuePush<MemcacheData> db_queue(new zmq::context_t(1));
    db_queue.connect(db_port, self_addr_);

    MemcacheData data;
    MemcachedClient memcache;
    const auto& operations = data.operations;
    auto& read_buffer = data.read_buffer;
    auto& miss_ops = data.miss_ops;

    while (true) {
      if (write_flag.load() > 0) {
        write_flag.fetch_sub(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        InvalidCache(invalid_sub, memcache, idx);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      if (answers.dequeue(data)) {
        if (data.s == Status::kOK) {
          for (size_t i: miss_ops) {
            memcache.put(operations[i], read_buffer[i]);
          } 
        }
        SendResponse(data, responses);
      }

      if (!requests.dequeue(data)) {
        continue;
      }
      
      read_buffer.reserve(operations.size());
      data.hit_count.read += operations.size();
      for (size_t i = 0; i < operations.size(); i++) {
        if (!memcache.get(operations[i], read_buffer)) {
          read_buffer.emplace_back(-1, "");
          miss_ops.push_back(i);
        }
      }

      if (miss_ops.empty()) {
        data.s = Status::kOK; 
        SendResponse(data, responses);
      } else {
        data.hit_count.hit += operations.size() - miss_ops.size();
        data.resp_addr.emplace_back(self_addr_, ans_port);
        db_queue.enqueue(data);
      }
    }
  }

  void PollWrite(std::string port, std::string db_port, std::string ans_port, int interval) {
    WebQueuePull<MemcacheData> requests(new zmq::context_t(1), port, self_addr_);
    WebQueuePull<MemcacheData> answers(new zmq::context_t(1), ans_port, self_addr_);
    ResponseMap responses;
    WebQueuePush<MemcacheData> db_queue(new zmq::context_t(1));
    db_queue.connect(db_port, self_addr_);

    MemcacheData data;
    const auto& operations = data.operations;

    uint64_t last_time = getTimestamp(), cur_time = 0;
    while (true) {
      cur_time = getTimestamp();
      if (cur_time - last_time > interval) {
        write_flag.store(zmq_read_txn_ports.size());
        last_time = cur_time;
      }

      if (answers.dequeue(data)) {
        SendResponse(data, responses);
      }

      if (requests.dequeue(data)) {
        data.resp_addr.emplace_back(self_addr_, ans_port);
        db_queue.enqueue(data);
      }
    }
  }

  void DBRThread(DB *db, std::string db_port) {
    ResponseMap responses;
    WebQueuePull<MemcacheData> db_queue(new zmq::context_t(1), db_port);
    
    MemcacheData data;
    MemcachedClient memcache;
    const auto& operations = data.operations;
    auto& read_buffer = data.read_buffer;
    auto& miss_ops = data.miss_ops;

    while (true) {
      if (!db_queue.dequeue(data)) {
        continue;
      }

      if (data.txn_op) {
        std::vector<DB::DB_Operation> db_ops;
        std::vector<DB::TimestampValue> db_rsl;
        for (size_t i:miss_ops) {
          db_ops.push_back(operations[i]);
        }
        data.s = db->ExecuteTransaction(db_ops, db_rsl, true);
        if (data.s == Status::kOK){
          for (size_t i = 0; i < miss_ops.size(); i++) {
            read_buffer[miss_ops[i]] = db_rsl[i];
          }
        } else {
          read_buffer.clear();
        }
      } else {
        data.s = db->Execute(operations[0], read_buffer);
      }
      SendResponse(data, responses);  
    }
  }

  void DBWThread(DB *db, std::string db_port, int interval) {
    ResponseMap responses;
    WebQueuePull<MemcacheData> db_queue(new zmq::context_t(1), db_port, self_addr_);
    WebPublish<InvalidCmd> invalid_pub(new zmq::context_t(1), zmq_invalid_port, self_addr_);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 99);

    MemcacheData data;
    const auto& operations = data.operations;
    auto& read_buffer = data.read_buffer;

    uint64_t last_time = getTimestamp(), cur_time = 0;
    std::vector<DB::DB_Operation> wops, wops_txn;
    while (true) {
      cur_time = getTimestamp();
      if (cur_time - last_time > interval) {
        invalid_pub.push({0, dis(gen), wops, wops_txn});
        std::cout << "broadcast invalid " << wops.size() << " scala and " << wops_txn.size() 
                  << " txn" << std::endl;
        wops.clear();
        wops_txn.clear();
        last_time = cur_time;
      }

      if (!db_queue.dequeue(data)) {
        continue;
      }

      if (data.txn_op) {
        data.s = db->ExecuteTransaction(operations, read_buffer, false);
        if (data.s == Status::kOK) {
          wops_txn.insert(wops_txn.end(), operations.begin(), operations.end());
        }
      } else {
        data.s = db->Execute(operations[0], read_buffer);
        if (data.s == Status::kOK) {
          wops.push_back(operations[0]);
        }
      }
      SendResponse(data, responses); 
    }
  }

  std::vector<DB*> dbr_, dbw_;
  std::vector<std::future<void>> thread_pool_;
  const std::string self_addr_;
  
  std::atomic<int> write_flag{0};
  std::atomic<bool> write_txn_flag{false};
};

} // benchmark

#endif // MEMCACHE_WRAPPER_H_
