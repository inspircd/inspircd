add_library(inspircd3 MODULE inspircd3.cpp)
set_target_properties(inspircd3 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"inspircd3\"")
target_link_libraries(inspircd3 clandestine_services)
install(TARGETS inspircd3 DESTINATION lib/clandestine/servicesd/modules/protocol)

