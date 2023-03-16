add_subdirectory(core_xline/)

add_subdirectory(core_user/)

add_subdirectory(core_oper/)

add_subdirectory(core_info/)

add_subdirectory(core_channel/)

add_library(core_dns MODULE core_dns.cpp)
set_target_properties(core_dns PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_dns\"")
target_link_libraries(core_dns clandestine)
install(TARGETS core_dns DESTINATION lib/clandestine/coremods)

add_library(core_hostname_lookup MODULE core_hostname_lookup.cpp)
set_target_properties(core_hostname_lookup PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_hostname_lookup\"")
target_link_libraries(core_hostname_lookup clandestine)
install(TARGETS core_hostname_lookup DESTINATION lib/clandestine/coremods)

add_library(core_list MODULE core_list.cpp)
set_target_properties(core_list PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_list\"")
target_link_libraries(core_list clandestine)
install(TARGETS core_list DESTINATION lib/clandestine/coremods)

add_library(core_loadmodule MODULE core_loadmodule.cpp)
set_target_properties(core_loadmodule PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_loadmodule\"")
target_link_libraries(core_loadmodule clandestine)
install(TARGETS core_loadmodule DESTINATION lib/clandestine/coremods)

add_library(core_lusers MODULE core_lusers.cpp)
set_target_properties(core_lusers PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_lusers\"")
target_link_libraries(core_lusers clandestine)
install(TARGETS core_lusers DESTINATION lib/clandestine/coremods)

add_library(core_message MODULE core_message.cpp)
set_target_properties(core_message PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_message\"")
target_link_libraries(core_message clandestine)
install(TARGETS core_message DESTINATION lib/clandestine/coremods)

add_library(core_mode MODULE core_mode.cpp)
set_target_properties(core_mode PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_mode\"")
target_link_libraries(core_mode clandestine)
install(TARGETS core_mode DESTINATION lib/clandestine/coremods)

add_library(core_reloadmodule MODULE core_reloadmodule.cpp)
set_target_properties(core_reloadmodule PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_reloadmodule\"")
target_link_libraries(core_reloadmodule clandestine)
install(TARGETS core_reloadmodule DESTINATION lib/clandestine/coremods)

add_library(core_serialize_rfc MODULE core_serialize_rfc.cpp)
set_target_properties(core_serialize_rfc PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_serialize_rfc\"")
target_link_libraries(core_serialize_rfc clandestine)
install(TARGETS core_serialize_rfc DESTINATION lib/clandestine/coremods)

add_library(core_stats MODULE core_stats.cpp)
set_target_properties(core_stats PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_stats\"")
target_link_libraries(core_stats clandestine)
install(TARGETS core_stats DESTINATION lib/clandestine/coremods)

add_library(core_stub MODULE core_stub.cpp)
set_target_properties(core_stub PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_stub\"")
target_link_libraries(core_stub clandestine)
install(TARGETS core_stub DESTINATION lib/clandestine/coremods)

add_library(core_wallops MODULE core_wallops.cpp)
set_target_properties(core_wallops PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_wallops\"")
target_link_libraries(core_wallops clandestine)
install(TARGETS core_wallops DESTINATION lib/clandestine/coremods)

add_library(core_who MODULE core_who.cpp)
set_target_properties(core_who PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_who\"")
target_link_libraries(core_who clandestine)
install(TARGETS core_who DESTINATION lib/clandestine/coremods)

add_library(core_whois MODULE core_whois.cpp)
set_target_properties(core_whois PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_whois\"")
target_link_libraries(core_whois clandestine)
install(TARGETS core_whois DESTINATION lib/clandestine/coremods)

add_library(core_whowas MODULE core_whowas.cpp)
set_target_properties(core_whowas PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
   COMPILE_DEFINITIONS "MODNAME=\"core_whowas\"")
target_link_libraries(core_whowas clandestine)
install(TARGETS core_whowas DESTINATION lib/clandestine/coremods)
