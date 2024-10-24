#ifndef DB_FACTORY_H_
#define DB_FACTORY_H_

#include "db.h"
#include "measurements.h"
#include "properties.h"
#include "memcache_wrapper.h"

#include <string>
#include <map>
#include <cassert>

namespace benchmark {

class DBFactory {
 public:
  using DBCreator = DB *(*)();
  static bool RegisterDB(std::string db_name, DBCreator db_creator);
  static DB *CreateDB(utils::Properties *props, Measurements *measurements, 
                      bool memcache=false, int tid=0);
  static DB *CreateRawDB(utils::Properties *props);
  
 private:
  static MemcacheWrapper *GetMemcache(utils::Properties *props);
  static std::map<std::string, DBCreator> &Registry();
  static MemcacheWrapper *memcache_;
};

} // benchmark

#endif // DB_FACTORY_H_
