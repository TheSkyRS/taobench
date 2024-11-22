cmake_minimum_required(VERSION 3.7)

project(taobench)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER "g++-11")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -O3")
# set(CMAKE_BUILD_TYPE Debug)

option(WITH_MEMCACHE ON)
option(WITH_CRDB OFF)
option(WITH_MYSQL OFF)
option(WITH_SPANNER OFF)
option(WITH_YUGABYTE OFF)

include_directories(src)
file(GLOB SOURCES src/*.h src/*.cc)
add_executable(taobench ${SOURCES})
target_link_libraries(taobench -lzmq -lmsgpackc)
target_compile_definitions(taobench PRIVATE CACHE_ID=${CACHE_ID})

if(WITH_MEMCACHE)
  include_directories(memcache)
  target_sources(taobench PRIVATE
    memcache/memcache.h
    memcache/memcache.cc
    memcache/zmq_utils.h
    memcache/memcache_wrapper.h)
  target_link_libraries(taobench -lmemcached)
endif()

if(WITH_CRDB)
  include_directories(crdb)
  target_sources(taobench PRIVATE
    crdb/crdb_db.h
    crdb/crdb_db.cc)
  target_link_libraries(taobench -lpqxx)
  target_link_libraries(taobench -lpq)
endif()

if(WITH_MYSQL)
  include_directories(mysqldb)
  target_sources(taobench PRIVATE
    mysqldb/mysql_db.h
    mysqldb/mysql_db.cc)
  target_link_libraries(taobench -lmysqlclient)
endif()

if(WITH_SPANNER)
  include_directories(spanner_db)
  target_sources(taobench PRIVATE
    spanner_db/spanner_db.h
    spanner_db/spanner_db.cc)
  target_link_libraries(taobench google-cloud-cpp::spanner)
endif()

if(WITH_YUGABYTE)
  include_directories(yugabytedb)
  target_sources(taobench PRIVATE
    yugabytedb/yugabytedb.h
    yugabytedb/yugabytedb.cc)
  target_link_libraries(taobench -lpqxx)
  target_link_libraries(taobench -lpq)
endif()
