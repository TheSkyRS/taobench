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
    std::string host = props->GetProperty("host", "127.0.0.1");
    std::string self_addr = props->GetProperty("self_addr", "127.0.0.1");
    double send_rate = std::stod(props->GetProperty("send_rate", "1.0"));
    return new DBWrapper(nullptr, measurements, host, tid, self_addr, send_rate);
  }
  DB *db = CreateRawDB(props);
  if (db != nullptr) {
    return new DBWrapper(db, measurements);
  }
  return nullptr;
}

MemcacheServer *DBFactory::memcache_ = nullptr;
MemcacheServer *DBFactory::GetMemcacheServer(utils::Properties *props) {
  if (memcache_ == nullptr) {
    std::string self_addr = props->GetProperty("self_addr", "127.0.0.1");
    std::string db_addr = props->GetProperty("db_addr", "127.0.0.1");
    std::string server_type = props->GetProperty("server_type", "mix");
    memcache_ = new MemcacheServer(self_addr, db_addr);

    if (server_type == "mix" || server_type == "db") {
      std::vector<DB*> dbr, dbw;
      for (int i = 0; i < zmq_dbr_ports.size(); i++) {
        dbr.push_back(DBFactory::CreateRawDB(props));
      }
      for (int i = 0; i < zmq_dbw_ports.size(); i++) {
        dbw.push_back(DBFactory::CreateRawDB(props));
      }
      memcache_->StartDB(dbr, dbw);
    }
    
    if (server_type == "mix" || server_type == "cache") {
      memcache_->StartMemcache();
    }
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
