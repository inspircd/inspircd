configure_file(../include/sysconf.h.cmake generated/include/sysconf.h)

add_definitions(-DVERSION_MAJOR=${VERSION_MAJOR})
add_definitions(-DVERSION_MINOR=${VERSION_MINOR})
add_definitions(-DVERSION_PATCH=${VERSION_PATCH})
add_definitions(-DVERSION_EXTRA="${VERSION_EXTRA}")
add_definitions(-DBUILD=${BUILD})

add_library(bs_autoassign MODULE bs_autoassign.cpp)
set_target_properties(bs_autoassign PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"bs_autoassign\"")
target_link_libraries(bs_autoassign clandestine_services)
target_include_directories(bs_autoassign PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS bs_autoassign DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_statusupdate MODULE cs_statusupdate.cpp)
set_target_properties(cs_statusupdate PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_statusupdate\"")
target_link_libraries(cs_statusupdate clandestine_services)
target_include_directories(cs_statusupdate PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS cs_statusupdate DESTINATION lib/clandestine/servicesd/modules)

add_library(fantasy MODULE fantasy.cpp)
set_target_properties(fantasy PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"fantasy\"")
target_link_libraries(fantasy clandestine_services)
target_include_directories(fantasy PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS fantasy DESTINATION lib/clandestine/servicesd/modules)

add_library(m_dns MODULE m_dns.cpp)
set_target_properties(m_dns PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_dns\"")
target_link_libraries(m_dns clandestine_services)
target_include_directories(m_dns PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS m_dns DESTINATION lib/clandestine/servicesd/modules)

add_library(m_svc_dnsbl MODULE m_dnsbl.cpp)
set_target_properties(m_svc_dnsbl PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_svc_dnsbl\"")
target_link_libraries(m_svc_dnsbl clandestine_services)
target_include_directories(m_svc_dnsbl PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS m_svc_dnsbl DESTINATION lib/clandestine/servicesd/modules)

add_library(m_helpchan MODULE m_helpchan.cpp)
set_target_properties(m_helpchan PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_helpchan\"")
target_link_libraries(m_helpchan clandestine_services)
target_include_directories(m_helpchan PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS m_helpchan DESTINATION lib/clandestine/servicesd/modules)

add_library(m_svc_httpd MODULE m_httpd.cpp)
set_target_properties(m_svc_httpd PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_svc_httpd\"")
target_link_libraries(m_svc_httpd clandestine_services)
target_include_directories(m_svc_httpd PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS m_svc_httpd DESTINATION lib/clandestine/servicesd/modules)

add_library(m_proxyscan MODULE m_proxyscan.cpp)
set_target_properties(m_proxyscan PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_proxyscan\"")
target_link_libraries(m_proxyscan clandestine_services)
target_include_directories(m_proxyscan PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS m_proxyscan DESTINATION lib/clandestine/servicesd/modules)

add_library(m_redis MODULE m_redis.cpp)
set_target_properties(m_redis PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_redis\"")
target_link_libraries(m_redis clandestine_services)
target_include_directories(m_redis PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS m_redis DESTINATION lib/clandestine/servicesd/modules)

add_library(m_rewrite MODULE m_rewrite.cpp)
set_target_properties(m_rewrite PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_rewrite\"")
target_link_libraries(m_rewrite clandestine_services)
target_include_directories(m_rewrite PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS m_rewrite DESTINATION lib/clandestine/servicesd/modules)

add_library(m_svc_sasl MODULE m_sasl.cpp)
set_target_properties(m_svc_sasl PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_svc_sasl\"")
target_link_libraries(m_svc_sasl clandestine_services)
target_include_directories(m_svc_sasl PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS m_svc_sasl DESTINATION lib/clandestine/servicesd/modules)

add_library(m_xmlrpc MODULE m_xmlrpc.cpp)
set_target_properties(m_xmlrpc PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_xmlrpc\"")
target_link_libraries(m_xmlrpc clandestine_services)
target_include_directories(m_xmlrpc PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS m_xmlrpc DESTINATION lib/clandestine/servicesd/modules)

add_library(m_xmlrpc_main MODULE m_xmlrpc_main.cpp)
set_target_properties(m_xmlrpc_main PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_xmlrpc_main\"")
target_link_libraries(m_xmlrpc_main clandestine_services)
target_include_directories(m_xmlrpc_main PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS m_xmlrpc_main DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_maxemail MODULE ns_maxemail.cpp)
set_target_properties(ns_maxemail PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_maxemail\"")
target_link_libraries(ns_maxemail clandestine_services)
target_include_directories(ns_maxemail PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../include/)
install(TARGETS ns_maxemail DESTINATION lib/clandestine/servicesd/modules)

add_subdirectory(./encryption)
add_subdirectory(./database)
add_subdirectory(./pseudoclients)
add_subdirectory(./protocol)
add_subdirectory(./extra)
add_subdirectory(./commands)
