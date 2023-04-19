add_definitions(-DINSPIRCD_VERSION="${PROJECT_VERSION}")

add_subdirectory(socketengines/)

add_subdirectory(threadengines/)

add_library(clandestine
  inspircd.cpp
  bancache.cpp
  base.cpp
  channels.cpp
  cidr.cpp
  clientprotocol.cpp
  command_parse.cpp
  commands.cpp
  configparser.cpp
  configreader.cpp
  cull_list.cpp
  dynamic.cpp
  filelogger.cpp
  fileutils.cpp
  hashcomp.cpp
  helperfuncs.cpp
  inspsocket.cpp
  inspstring.cpp
  listensocket.cpp
  listmode.cpp
  logger.cpp
  mode.cpp
  modulemanager.cpp
  modules.cpp
  serializable.cpp
  server.cpp
  snomasks.cpp
  socket.cpp
  socketengine.cpp
  threadengine.cpp
  timer.cpp
  usermanager.cpp
  users.cpp
  wildcard.cpp
  xline.cpp)

target_include_directories(clandestine PRIVATE
  ../include)

target_link_libraries(clandestine
  clandestine_ircd_socketengines
  clandestine_ircd_threadengines)

add_executable(ircd main.cpp)

target_include_directories(ircd PRIVATE
  ../include)

target_link_libraries(ircd
  clandestine
  -ldl
  -pthread)

install(TARGETS ircd DESTINATION sbin/)
