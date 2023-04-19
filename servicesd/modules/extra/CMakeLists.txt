configure_file(../../include/sysconf.h.cmake generated/include/sysconf.h)

add_library(m_svc_mysql MODULE m_mysql.cpp)
set_target_properties(m_svc_mysql PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_svc_mysql\"")
target_link_libraries(m_svc_mysql clandestine_services ${mariadb-connector-c_LIBRARIES})
target_include_directories(m_svc_mysql PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ${mariadb-connector-c_INCLUDE_DIR}
  ../../include/)
install(TARGETS m_svc_mysql DESTINATION lib/clandestine/servicesd/modules/extra)

add_library(m_regex_svc_pcre MODULE m_regex_pcre.cpp)
set_target_properties(m_regex_svc_pcre PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_regex_svc_pcre\"")
target_link_libraries(m_regex_svc_pcre clandestine_services ${pcre_LIBRARIES})
target_include_directories(m_regex_svc_pcre PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ${pcre_INCLUDE_DIR}
  ../../include/)
install(TARGETS m_regex_svc_pcre DESTINATION lib/clandestine/servicesd/modules/extra)

add_library(m_regex_svc_pcre2 MODULE m_regex_pcre2.cpp)
set_target_properties(m_regex_svc_pcre2 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_regex_svc_pcre2\"")
target_link_libraries(m_regex_svc_pcre2 clandestine_services ${PCRE2_LIBRARIES})
target_include_directories(m_regex_svc_pcre2 PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ${PCRE2_INCLUDE_DIR}
  ../../include/)
install(TARGETS m_regex_svc_pcre2 DESTINATION lib/clandestine/servicesd/modules/extra)

add_library(m_regex_svc_posix MODULE m_regex_posix.cpp)
set_target_properties(m_regex_svc_posix PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_regex_svc_posix\"")
target_link_libraries(m_regex_svc_posix clandestine_services clandestine_tre)
target_include_directories(m_regex_svc_posix PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../../lib/tre
  ../../include/)
install(TARGETS m_regex_svc_posix DESTINATION lib/clandestine/servicesd/modules/extra)

add_library(m_regex_svc_tre MODULE m_regex_tre.cpp)
set_target_properties(m_regex_svc_tre PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_regex_svc_tre\"")
target_link_libraries(m_regex_svc_tre clandestine_services clandestine_tre)
target_include_directories(m_regex_svc_tre PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../../lib/tre
  ../../include/)
install(TARGETS m_regex_svc_tre DESTINATION lib/clandestine/servicesd/modules/extra)

add_library(m_sql_authentication MODULE m_sql_authentication.cpp)
set_target_properties(m_sql_authentication PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sql_authentication\"")
target_link_libraries(m_sql_authentication clandestine_services)
target_include_directories(m_sql_authentication PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS m_sql_authentication DESTINATION lib/clandestine/servicesd/modules/extra)

add_library(m_sql_log MODULE m_sql_log.cpp)
set_target_properties(m_sql_log PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sql_log\"")
target_link_libraries(m_sql_log clandestine_services)
target_include_directories(m_sql_log PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS m_sql_log DESTINATION lib/clandestine/servicesd/modules/extra)

add_library(m_sql_oper MODULE m_sql_oper.cpp)
set_target_properties(m_sql_oper PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sql_oper\"")
target_link_libraries(m_sql_oper clandestine_services)
target_include_directories(m_sql_oper PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS m_sql_oper DESTINATION lib/clandestine/servicesd/modules/extra)

add_library(m_sqlite MODULE m_sqlite.cpp)
set_target_properties(m_sqlite PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sqlite\"")
target_link_libraries(m_sqlite clandestine_services ${SQLite3_LIBRARIES})
target_include_directories(m_sqlite PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ${SQLite3_INCLUDE_DIR}
  ../../include/)
install(TARGETS m_sqlite DESTINATION lib/clandestine/servicesd/modules/extra)

add_library(m_ssl_svc_gnutls MODULE m_ssl_gnutls.cpp)
set_target_properties(m_ssl_svc_gnutls PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ssl_svc_gnutls\"")
target_link_libraries(m_ssl_svc_gnutls clandestine_services ${GnuTLS_LIBRARIES})
target_include_directories(m_ssl_svc_gnutls PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ${GnuTLS_INCLUDE_DIR}
  ../../include/)
install(TARGETS m_ssl_svc_gnutls DESTINATION lib/clandestine/servicesd/modules/extra)

add_library(m_ssl_svc_openssl MODULE m_ssl_openssl.cpp)
set_target_properties(m_ssl_svc_openssl PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ssl_svc_openssl\"")
target_link_libraries(m_ssl_svc_openssl clandestine_services ${OpenSSL_LIBRARIES})
target_include_directories(m_ssl_svc_openssl PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ${OpenSSL_INCLUDE_DIR}
  ../../include/)
install(TARGETS m_ssl_svc_openssl DESTINATION lib/clandestine/servicesd/modules/extra)
