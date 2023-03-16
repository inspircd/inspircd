add_subdirectory(coremods/)

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

target_link_libraries(clandestine
  socketengines
  threadengines)

add_executable(ircd inspircd.cpp)
target_link_libraries(ircd clandestine)
target_link_libraries(ircd -ldl -pthread)

add_subdirectory(modules/)

install(TARGETS ircd DESTINATION sbin)
