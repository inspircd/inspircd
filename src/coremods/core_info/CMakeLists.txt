add_library(core_info
  cmd_admin.cpp
  cmd_commands.cpp
  cmd_info.cpp
  cmd_modules.cpp
  cmd_motd.cpp
  cmd_servlist.cpp
  cmd_time.cpp
  cmd_version.cpp
  core_info.cpp)

set_target_properties(core_info PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
  COMPILE_DEFINITIONS "MODNAME=\"core_info\"")

target_link_libraries(core_info clandestine)

install(TARGETS core_info DESTINATION lib/clandestine/coremods)
