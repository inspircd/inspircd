add_library(core_xline MODULE
cmd_eline.cpp
cmd_gline.cpp
cmd_kline.cpp
cmd_qline.cpp
cmd_zline.cpp
core_xline.cpp)

set_target_properties(core_xline PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"core_xline\"")

target_link_libraries(core_xline clandestine)

install(TARGETS core_xline DESTINATION lib/clandestine/ircd/modules)
