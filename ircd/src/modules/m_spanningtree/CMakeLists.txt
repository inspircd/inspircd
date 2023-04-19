add_library(m_spanningtree MODULE
  addline.cpp
  away.cpp
  capab.cpp
  compat.cpp
  delline.cpp
  encap.cpp
  fjoin.cpp
  fmode.cpp
  ftopic.cpp
  hmac.cpp
  idle.cpp
  ijoin.cpp
  main.cpp
  metadata.cpp
  misccommands.cpp
  netburst.cpp
  nick.cpp
  nickcollide.cpp
  num.cpp
  opertype.cpp
  override_map.cpp
  override_squit.cpp
  override_stats.cpp
  override_whois.cpp
  ping.cpp
  pingtimer.cpp
  pong.cpp
  postcommand.cpp
  precommand.cpp
  protocolinterface.cpp
  rconnect.cpp
  remoteuser.cpp
  resolvers.cpp
  rsquit.cpp
  save.cpp
  server.cpp
  servercommand.cpp
  sinfo.cpp
  svsjoin.cpp
  svsnick.cpp
  svspart.cpp
  tags.cpp
  translate.cpp
  treeserver.cpp
  treesocket1.cpp
  treesocket2.cpp
  uid.cpp
  utils.cpp)
set_target_properties(m_spanningtree PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_spanningtree\"")
target_link_libraries(m_spanningtree clandestine)
target_include_directories(m_spanningtree PRIVATE
  ../../../include)
install(TARGETS m_spanningtree DESTINATION lib/clandestine/ircd/modules/)
