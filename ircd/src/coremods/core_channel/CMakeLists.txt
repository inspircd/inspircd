add_library(core_channel MODULE
  cmd_invite.cpp
  cmd_join.cpp
  cmd_kick.cpp
  cmd_names.cpp
  cmd_topic.cpp
  cmode_k.cpp
  cmode_l.cpp
  core_channel.cpp
  invite.cpp)

set_target_properties(core_channel PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"core_channel\"")

target_link_libraries(core_channel clandestine)

install(TARGETS core_channel DESTINATION lib/clandestine/ircd/modules)
