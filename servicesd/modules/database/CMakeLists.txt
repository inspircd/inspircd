add_library(db_flatfile MODULE db_flatfile.cpp)
set_target_properties(db_flatfile PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"db_flatfile\"")
target_link_libraries(db_flatfile clandestine_services)
install(TARGETS db_flatfile DESTINATION lib/clandestine/servicesd/modules/database)

add_library(db_old MODULE db_old.cpp)
set_target_properties(db_old PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"db_old\"")
target_link_libraries(db_old clandestine_services)
install(TARGETS db_old DESTINATION lib/clandestine/servicesd/modules/database)

add_library(db_redis MODULE db_redis.cpp)
set_target_properties(db_redis PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"db_redis\"")
target_link_libraries(db_redis clandestine_services)
install(TARGETS db_redis DESTINATION lib/clandestine/servicesd/modules/database)

add_library(db_sql MODULE db_sql.cpp)
set_target_properties(db_sql PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"db_sql\"")
target_link_libraries(db_sql clandestine_services)
install(TARGETS db_sql DESTINATION lib/clandestine/servicesd/modules/database)

add_library(db_sql_live MODULE db_sql_live.cpp)
set_target_properties(db_sql_live PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"db_sql_live\"")
target_link_libraries(db_sql_live clandestine_services)
install(TARGETS db_sql_live DESTINATION lib/clandestine/servicesd/modules/database)

