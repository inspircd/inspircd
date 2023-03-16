add_library(core_user
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
  SOVERSION 1
  COMPILE_DEFINITIONS "MODNAME=\"core_user\"")

target_link_libraries(core_user clandestine)

install(TARGETS core_user DESTINATION lib/clandestine/coremods)
