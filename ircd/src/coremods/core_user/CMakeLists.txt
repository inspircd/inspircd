add_library(core_user MODULE
cmd_away.cpp
cmd_ison.cpp
cmd_nick.cpp
cmd_part.cpp
cmd_quit.cpp
cmd_user.cpp
cmd_userhost.cpp
core_user.cpp
umode_o.cpp
umode_s.cpp)

set_target_properties(core_user PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"core_user\"")

target_link_libraries(core_user clandestine)

target_include_directories(core_user PRIVATE
  ../../../include)

install(TARGETS core_user DESTINATION lib/clandestine/ircd/modules)
