#ifndef MEMCACHE_WRAPPER_H_
#define MEMCACHE_WRAPPER_H_

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

class MemcacheWrapper {
 public:
  MemcacheWrapper(DB *db, Measurements *measurements) :
    db_(db) , measurements_(measurements) {
      memcache_ = new MemcachedClient();
    }
  ~MemcacheWrapper() {
    delete db_;
  }
  void Init() {
    db_->Init();
  }
  void Cleanup() {
    db_->Cleanup();
  }

  Status Execute(const DB::DB_Operation &operation,
                 std::vector<DB::TimestampValue> &read_buffer,
                 bool txn_op, int& hit_count, int& read_count) {
    Status s;
    if (operation.operation == Operation::READ) {
        read_count += 1;
      if (memcache_->get(operation, read_buffer)) {
        hit_count += 1;
        s = Status::kOK;
      } else {
        s = db_->Execute(operation, read_buffer, txn_op);
        if (s == Status::kOK) {
          memcache_->put(operation, read_buffer);
        }
      }
    } else {
      s = db_->Execute(operation, read_buffer, txn_op);
      memcache_->invalidate(operation);
    }
    return s;
  }

  Status ExecuteTransaction(const std::vector<DB::DB_Operation> &operations,
                            std::vector<DB::TimestampValue> &read_buffer,
                            bool read_only, int& hit_count, int& read_count)
  {
    timer_.Start();
    Status s;
    if (read_only) {
      std::vector<DB::DB_Operation> miss_ops;
      std::vector<DB::TimestampValue> rsl_cache;
      std::vector<DB::TimestampValue> rsl_db;
      // TODO: set global write lock to memcache.
      read_count += operations.size();
      for (size_t i = 0; i < operations.size(); i++) {
        if (!memcache_->get(operations[i], rsl_cache)) {
          // TODO: set "key" write lock to memcache.
          rsl_cache.emplace_back(-1, "");
          miss_ops.push_back(operations[i]);
        } else {
          hit_count++;
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
      for (const DB::DB_Operation& op : operations) {
        memcache_->invalidate(op);
      }
    }
    assert(!operations.empty());
    return s;
  }

 private:
  DB *db_;
  Measurements *measurements_;
  utils::Timer<uint64_t, std::nano> timer_;
  MemcachedClient *memcache_;
};

} // benchmark

#endif // MEMCACHE_WRAPPER_H_
