#include "db_factory.h"
#include "db_wrapper.h"

namespace benchmark {


std::map<std::string, DBFactory::DBCreator> &DBFactory::Registry() {
  static std::map<std::string, DBCreator> registry;
  return registry;
}

bool DBFactory::RegisterDB(std::string db_name, DBCreator db_creator) {
  Registry()[db_name] = db_creator;
  return true;
}

DB *DBFactory::CreateDB(utils::Properties *props, Measurements *measurements, 
  bool memcache, int tid) {
  if (memcache) {
    GetMemcache(props);
    return new DBWrapper(nullptr, measurements, tid);
  }
  DB *db = CreateRawDB(props);
  if (db != nullptr) {
    return new DBWrapper(db, measurements, -1);
  }
  return nullptr;
}

MemcacheWrapper *DBFactory::memcache_ = nullptr;
MemcacheWrapper *DBFactory::GetMemcache(utils::Properties *props) {
  if (memcache_ == nullptr) {
    std::vector<DB*> dbr, dbw;
    for (int i = 0; i < zmq_db_ports.size(); i++) {
      dbr.push_back(CreateRawDB(props));
    }
    for (int i = 0; i < zmq_write_ports.size(); i++) {
      dbw.push_back(CreateRawDB(props));
    }
    memcache_ = new MemcacheWrapper(dbr, dbw);
    memcache_->Start();
  }
  return memcache_;
}

DB *DBFactory::CreateRawDB(utils::Properties *props) {
  std::string db_name = props->GetProperty("dbname", "test");
  DB *db = nullptr;
  std::map<std::string, DBCreator> &registry = Registry();
  if (registry.find(db_name) != registry.end()) {
    db = (*registry[db_name])();
    db->SetProps(props);
    db->Init();
  }
  return db;
}

} // benchmark
