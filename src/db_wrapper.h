#ifndef DB_WRAPPER_H_
#define DB_WRAPPER_H_

#include <string>
#include <vector>
#include <iostream>
#include <cassert>
#include <future>
#include <chrono>
#include <random>

#include "db.h"
#include "measurements.h"
#include "timer.h"
#include "utils.h"
#include "db_factory.h"

namespace benchmark {

// Wrapper Class around DB; times and logs each Execute and ExecuteTransaction operation.
class DBWrapper : public DB {
 public:
  DBWrapper(DB *db, Measurements *measurements, std::string host="127.0.0.1", int tid=0, 
    std::string self_addr="127.0.0.1", double send_rate = 1.0):
    db_(db), measurements_(measurements), ans_addr_(self_addr), ans_port_(std::to_string(7000+tid)), 
    tid_(tid), send_rate_(send_rate), gen(std::random_device{}()), dist(0.0, 1.0) 
  {
    if (db == nullptr) { // this means the DBWrapper is for memcache
      memcache_router = new WebQueuePush<MemcacheData>(new zmq::context_t(1));
      memcache_ans = new WebQueuePull<MemcacheData>(new zmq::context_t(1), ans_port_, ans_addr_);
      
      for(int i = 0; i < zmq_router_ports.size(); i ++) {
        memcache_router->connect(zmq_router_ports[i], host);
      }
      thread_pool_.push_back(std::async(std::launch::async, PullAns, this));
    }
  }
  ~DBWrapper() {
    if(db_) delete db_;
    if(memcache_router) delete memcache_router;
    if(memcache_ans) delete memcache_ans;
  }
  void Init() { // deprecated
    if(db_ != nullptr) {
      db_->Init();
    } 
  }
  void Cleanup() { // deprecated
    if(db_ != nullptr) {
      db_->Cleanup();
    } 
  }
  Status Read(DataTable table, const std::vector<Field> &key,
              std::vector<TimestampValue> &buffer) {
    throw std::invalid_argument("DBWrapper Read method should never be called.");
  }

  Status Scan(DataTable table, const std::vector<Field> &key, int n,
              std::vector<TimestampValue> &buffer) {
    throw std::invalid_argument("DBWrapper Scan method should never be called.");
  }

  Status Update(DataTable table, const std::vector<Field> &key, const TimestampValue &value) {
    throw std::invalid_argument("DBWrapper Update method should never be called.");
  }

  Status Insert(DataTable table, const std::vector<Field> &key, const TimestampValue &value) {
    throw std::invalid_argument("DBWrapper Insert method should never be called.");
  }

  Status Delete(DataTable table, const std::vector<Field> &key, const TimestampValue &value) {
    throw std::invalid_argument("DBWrapper Delete method should never be called.");
  }

  Status Execute(const DB_Operation &operation,
                 std::vector<TimestampValue> &read_buffer,
                 bool txn_op = false) 
  {
    bool read_only = operation.operation == Operation::READ;
    const std::vector<DB::DB_Operation> operations{operation};
    const std::vector<FullAddr> resp_addr{FullAddr{ans_addr_, ans_port_}};

    if (dist(gen) <= send_rate_) {
      memcache_router->enqueue(
        {getTimestamp(), resp_addr, operations, read_buffer, read_only, txn_op},
      tid_);
    }
    
    return Status::kOK;
  }

  Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
                            std::vector<TimestampValue> &read_buffer,
                            bool read_only = false) 
  {
    const std::vector<FullAddr> resp_addr{FullAddr{ans_addr_, ans_port_}};

    if (dist(gen) <= send_rate_) {
      memcache_router->enqueue(
        {getTimestamp(), resp_addr, operations, read_buffer, read_only, true},
      tid_);
    }
    return Status::kOK;
  }

  Status BatchInsert(DataTable table, 
                     const std::vector<std::vector<Field>> &keys, 
                     const std::vector<TimestampValue> &values) 
  {
    return db_->BatchInsert(table, keys, values);
  }

  Status BatchRead(DataTable table,
                   const std::vector<Field> & floor,
                   const std::vector<Field> & ceil,
                   int n,
                   std::vector<std::vector<Field>> &key_buffer)
  {
    return db_->BatchRead(table, floor, ceil, n, key_buffer);
  }

 private:
  static uint64_t getTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  }

  static Operation getOperation(const MemcacheData& data) {
    if (data.txn_op) {
      if (data.read_only) {
        return Operation::READTRANSACTION;
      }
      return Operation::WRITETRANSACTION;
    }
    return data.operations[0].operation;
  }

  static void PullAns(DBWrapper* clz) {
    MemcacheData resp;
    while (true) {
      if (!clz->memcache_ans->dequeue(resp)) {
        continue;
      }
      uint64_t elapsed = getTimestamp() - resp.timestamp;
      if (resp.s == Status::kOK) {
        clz->measurements_->Report(getOperation(resp), elapsed);
        clz->measurements_->ReportRead(resp.hit_count.hit, resp.hit_count.read);
      }
    }
  }

  DB *db_;
  Measurements *measurements_;
  const std::string ans_addr_, ans_port_;
  int tid_;

  double send_rate_;
  std::mt19937 gen;
  std::uniform_real_distribution<> dist;

  WebQueuePush<MemcacheData>* memcache_router = nullptr;
  WebQueuePull<MemcacheData>* memcache_ans = nullptr;
  std::vector<std::future<void>> thread_pool_;
};

} // benchmark

#endif // DB_WRAPPER_H_
