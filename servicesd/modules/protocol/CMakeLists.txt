configure_file(../../include/sysconf.h.cmake generated/include/sysconf.h)

add_library(inspircd3 MODULE inspircd3.cpp)
set_target_properties(inspircd3 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"inspircd3\"")
target_link_libraries(inspircd3 clandestine_services)
target_include_directories(inspircd3 PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS inspircd3 DESTINATION lib/clandestine/servicesd/modules/protocol)
