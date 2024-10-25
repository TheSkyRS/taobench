#ifndef DB_WRAPPER_H_
#define DB_WRAPPER_H_

#include <string>
#include <vector>
#include <iostream>
#include <cassert>

#include "db.h"
#include "measurements.h"
#include "timer.h"
#include "utils.h"
#include "memcache.h"

namespace benchmark {

// Wrapper Class around DB; times and logs each Execute and ExecuteTransaction operation.
class DBWrapper : public DB {
 public:
  DBWrapper(DB *db, Measurements *measurements) :
    db_(db) , measurements_(measurements) {
      std::vector<std::pair<std::string, in_port_t>> servers = {
        {"127.0.0.1", 11211},
        {"127.0.0.1", 11212}
    };
      memcache_ = new MemcachedClient(servers);
    }
  ~DBWrapper() {
    delete db_;
  }
  void Init() {
    db_->Init();
  }
  void Cleanup() {
    db_->Cleanup();
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
                 bool txn_op = false) {
    timer_.Start();
    Status s;
    if (operation.operation == Operation::READ) {
      if (memcache_->get(operation, read_buffer)) {
        measurements_->ReportRead(true);
        s = Status::kOK;
      } else {
        measurements_->ReportRead(false);
        s = db_->Execute(operation, read_buffer, txn_op);
        if (s == Status::kOK) {
          memcache_->put(operation, read_buffer);
        }
      }
    } else {
      s = db_->Execute(operation, read_buffer, txn_op);
      memcache_->invalidate(operation);
    }
    uint64_t elapsed = timer_.End();
    if (s == Status::kOK) {
      measurements_->Report(operation.operation, elapsed);
    }
    return s;
  }

  Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
                            std::vector<TimestampValue> &read_buffer,
                            bool read_only = false)
  {
    timer_.Start();
    Status s;
    if (read_only) {
      std::vector<DB_Operation> miss_ops;
      std::vector<TimestampValue> rsl_cache;
      std::vector<TimestampValue> rsl_db;
      // TODO: set global write lock to memcache.
      for (size_t i = 0; i < operations.size(); i++) {
        if (!memcache_->get(operations[i], rsl_cache)) {
          // TODO: set "key" write lock to memcache.
          measurements_->ReportRead(false);
          rsl_cache.emplace_back(-1, "");
          miss_ops.push_back(operations[i]);
        } else {
          measurements_->ReportRead(true);
        }
      }
      // TODO: unset global write lock to memcache.
      assert(rsl_cache.size() == operations.size()); // TODO: remove
      s = db_->ExecuteTransaction(miss_ops, rsl_db, read_only);
      if (s == Status::kOK) {
        size_t db_pos = 0;
        for (size_t i = 0; i < operations.size(); i++) {
          if (rsl_cache[i].timestamp == -1){
            read_buffer.push_back(rsl_db[db_pos]);
            memcache_->put(operations[i], read_buffer);
            db_pos++;
          } else {
            read_buffer.push_back(rsl_cache[i]);
            // TODO: unset "key" write lock to memcache.
          }
        }
      }
    } else {
      s = db_->ExecuteTransaction(operations, read_buffer, read_only);
      for (const DB_Operation& op : operations) {
        memcache_->invalidate(op);
      }
    }
    uint64_t elapsed = timer_.End();
    assert(!operations.empty());
    if (s != Status::kOK) {
      return s;
    }
    if (read_only) {
      measurements_->Report(Operation::READTRANSACTION, elapsed);
    } else {
      measurements_->Report(Operation::WRITETRANSACTION, elapsed);
    }
    return s;
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
  DB *db_;
  Measurements *measurements_;
  utils::Timer<uint64_t, std::nano> timer_;

  MemcachedClient *memcache_;
};

} // benchmark

#endif // DB_WRAPPER_H_
