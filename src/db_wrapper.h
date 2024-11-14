#ifndef DB_WRAPPER_H_
#define DB_WRAPPER_H_

#include <string>
#include <vector>
#include <iostream>
#include <cassert>
#include <future>
#include <chrono>

#include "db.h"
#include "measurements.h"
#include "timer.h"
#include "utils.h"
#include "db_factory.h"

namespace benchmark {

// Wrapper Class around DB; times and logs each Execute and ExecuteTransaction operation.
class DBWrapper : public DB {
 public:
  DBWrapper(DB *db, Measurements *measurements, std::string host="127.0.0.1", int tid=0):
    db_(db), ans_port(std::to_string(7000+tid)), measurements_(measurements) 
  {
    if (db == nullptr) {
      memcache_read = new WebQueuePush<MemcacheRequest>(new zmq::context_t(1));
      memcache_read_txn = new WebQueuePush<MemcacheRequest>(new zmq::context_t(1));
      memcache_write = new WebQueuePush<MemcacheRequest>(new zmq::context_t(1));
      memcache_ans = new WebQueuePull<MemcacheResponse>(new zmq::context_t(1), ans_port);
      
      memcache_read->connect(zmq_read_ports[tid % zmq_read_ports.size()], host);
      memcache_read_txn->connect(zmq_read_txn_ports[tid % zmq_read_txn_ports.size()], host);
      memcache_write->connect(zmq_write_ports[tid % zmq_write_ports.size()], host);
      thread_pool_.push_back(std::async(std::launch::async, PullResp, this));
    }
  }
  ~DBWrapper() {
    if(db_) delete db_;
    if(memcache_read) delete memcache_read;
    if(memcache_read_txn) delete memcache_read_txn;
    if(memcache_write) delete memcache_write;
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
    SendCommand(operations, txn_op, read_only);
    return Status::kOK;
  }

  Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
                            std::vector<TimestampValue> &read_buffer,
                            bool read_only = false) 
  {
    SendCommand(operations, true, read_only);
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

  static void PullResp(DBWrapper* clz) {
    MemcacheResponse resp;
    while (true) {
      if (!clz->memcache_ans->dequeue(resp)) {
        continue;
      }
      uint64_t elapsed = getTimestamp() - resp.timestamp;
      if (resp.s == Status::kOK) {
        clz->measurements_->Report(resp.operation, elapsed);
        clz->measurements_->ReportRead(resp.hit_count, resp.read_count);
      }
    }
  }

  void SendCommand(const std::vector<DB_Operation> &operations, bool txn_op, bool read_only) {
    MemcacheRequest req{getTimestamp(), operations, ans_port, read_only, txn_op};
    if (read_only) {
      if (txn_op) {
        memcache_read_txn->enqueue(req);
      } else {
        memcache_read->enqueue(req);
      }
    } else {
      memcache_write->enqueue(req);
    }
  }

  DB *db_;
  std::string ans_port;
  Measurements *measurements_;

  WebQueuePush<MemcacheRequest>* memcache_read = nullptr;
  WebQueuePush<MemcacheRequest>* memcache_read_txn = nullptr;
  WebQueuePush<MemcacheRequest>* memcache_write = nullptr;
  WebQueuePull<MemcacheResponse>* memcache_ans = nullptr;
  std::vector<std::future<void>> thread_pool_;
};

} // benchmark

#endif // DB_WRAPPER_H_
