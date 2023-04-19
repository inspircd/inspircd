add_library(core_oper MODULE
cmd_die.cpp
cmd_kill.cpp
cmd_oper.cpp
cmd_rehash.cpp
cmd_restart.cpp
core_oper.cpp)

set_target_properties(core_oper PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"core_oper\"")

target_link_libraries(core_oper clandestine)

target_include_directories(core_oper PRIVATE
  ../../../include)

install(TARGETS core_oper DESTINATION lib/clandestine/ircd/modules)
