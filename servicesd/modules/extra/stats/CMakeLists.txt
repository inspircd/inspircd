add_library(cs_fantasy_stats MODULE cs_fantasy_stats.cpp)
set_target_properties(cs_fantasy_stats PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_fantasy_stats\"")
target_link_libraries(cs_fantasy_stats clandestine_services)
install(TARGETS cs_fantasy_stats DESTINATION lib/clandestine/servicesd/modules/extra/stats)

add_library(cs_fantasy_top MODULE cs_fantasy_top.cpp)
set_target_properties(cs_fantasy_top PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_fantasy_top\"")
target_link_libraries(cs_fantasy_top clandestine_services)
install(TARGETS cs_fantasy_top DESTINATION lib/clandestine/servicesd/modules/extra/stats)

add_library(m_chanstats MODULE m_chanstats.cpp)
set_target_properties(m_chanstats PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_chanstats\"")
target_link_libraries(m_chanstats clandestine_services)
install(TARGETS m_chanstats DESTINATION lib/clandestine/servicesd/modules/extra/stats)

