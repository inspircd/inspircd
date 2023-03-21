add_definitions(-DVERSION_MAJOR=${VERSION_MAJOR})
add_definitions(-DVERSION_MINOR=${VERSION_MINOR})
add_definitions(-DVERSION_PATCH=${VERSION_PATCH})
add_definitions(-DVERSION_EXTRA="${VERSION_EXTRA}")
add_definitions(-DBUILD=${BUILD})

configure_file(../include/sysconf.h.cmake generated/include/sysconf.h)

add_subdirectory(socketengines/)

add_library(clandestine_services 
  main.cpp
  access.cpp
  account.cpp
  base.cpp
  base64.cpp
  bots.cpp
  channels.cpp
  command.cpp
  config.cpp
  extensible.cpp
  hashcomp.cpp
  init.cpp
  language.cpp
  logger.cpp
  mail.cpp
  memos.cpp
  messages.cpp
  misc.cpp
  modes.cpp
  module.cpp
  modulemanager.cpp
  nickalias.cpp
  nickcore.cpp
  opertype.cpp
  pipeengine.cpp
  process.cpp
  protocol.cpp
  regchannel.cpp
  serialize.cpp
  servers.cpp
  siphash.cpp
  socket_clients.cpp
  socket_transport.cpp
  sockets.cpp
  threadengine.cpp
  timers.cpp
  uplink.cpp
  users.cpp
  xline.cpp)

target_include_directories(clandestine_services PUBLIC
  /usr/include
  ../include/
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../lib/tre/)

target_link_libraries(clandestine_services
  clandestine_services_socketengines)

install(TARGETS clandestine_services DESTINATION lib)

add_executable(servicesd main.cpp)
target_link_libraries(servicesd
  clandestine_services
  -ldl
  -pthread)

install(TARGETS servicesd DESTINATION sbin)
