add_library(irc2sql MODULE irc2sql.cpp)
set_target_properties(irc2sql PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"irc2sql\"")
target_link_libraries(irc2sql services)
install(TARGETS irc2sql DESTINATION lib/clandestine/servicesd/modules)

add_library(tables MODULE tables.cpp)
set_target_properties(tables PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"tables\"")
target_link_libraries(tables services)
install(TARGETS tables DESTINATION lib/clandestine/servicesd/modules)

add_library(utils MODULE utils.cpp)
set_target_properties(utils PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"utils\"")
target_link_libraries(utils services)
install(TARGETS utils DESTINATION lib/clandestine/servicesd/modules)

