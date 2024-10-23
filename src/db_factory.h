#ifndef DB_FACTORY_H_
#define DB_FACTORY_H_

#include "db.h"
#include "measurements.h"
#include "properties.h"
#include "memcache_wrapper.h"

#include <string>
#include <map>

namespace benchmark {

class DBFactory {
 public:
  using DBCreator = DB *(*)();
  static bool RegisterDB(std::string db_name, DBCreator db_creator);
  static DB *CreateDB(utils::Properties *props, Measurements *measurements);
  static MemcacheWrapper *CreateMemcache(utils::Properties *props, Measurements *measurements);
 private:
  static DB *CreateRawDB(utils::Properties *props);
  static std::map<std::string, DBCreator> &Registry();
};

} // benchmark

#endif // DB_FACTORY_H_
