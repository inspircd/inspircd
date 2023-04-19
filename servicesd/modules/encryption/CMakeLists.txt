configure_file(../../include/sysconf.h.cmake generated/include/sysconf.h)

add_library(enc_bcrypt MODULE enc_bcrypt.cpp)
set_target_properties(enc_bcrypt PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"enc_bcrypt\"")
target_link_libraries(enc_bcrypt clandestine_services)
target_include_directories(enc_bcrypt PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS enc_bcrypt DESTINATION lib/clandestine/servicesd/modules/encryption)

add_library(enc_md5 MODULE enc_md5.cpp)
set_target_properties(enc_md5 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"enc_md5\"")
target_link_libraries(enc_md5 clandestine_services)
target_include_directories(enc_md5 PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS enc_md5 DESTINATION lib/clandestine/servicesd/modules/encryption)

add_library(enc_none MODULE enc_none.cpp)
set_target_properties(enc_none PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"enc_none\"")
target_link_libraries(enc_none clandestine_services)
target_include_directories(enc_none PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS enc_none DESTINATION lib/clandestine/servicesd/modules/encryption)

add_library(enc_old MODULE enc_old.cpp)
set_target_properties(enc_old PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"enc_old\"")
target_link_libraries(enc_old clandestine_services)
target_include_directories(enc_old PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS enc_old DESTINATION lib/clandestine/servicesd/modules/encryption)

add_library(enc_sha1 MODULE enc_sha1.cpp)
set_target_properties(enc_sha1 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"enc_sha1\"")
target_link_libraries(enc_sha1 clandestine_services)
target_include_directories(enc_sha1 PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS enc_sha1 DESTINATION lib/clandestine/servicesd/modules/encryption)

add_library(enc_sha256 MODULE enc_sha256.cpp)
set_target_properties(enc_sha256 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"enc_sha256\"")
target_link_libraries(enc_sha256 clandestine_services)
target_include_directories(enc_sha256 PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS enc_sha256 DESTINATION lib/clandestine/servicesd/modules/encryption)
