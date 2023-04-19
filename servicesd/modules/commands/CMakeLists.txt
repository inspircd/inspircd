configure_file(../../include/sysconf.h.cmake generated/include/sysconf.h)

add_library(bs_assign MODULE bs_assign.cpp)
set_target_properties(bs_assign PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"bs_assign\"")
target_link_libraries(bs_assign clandestine_services)
target_include_directories(bs_assign PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS bs_assign DESTINATION lib/clandestine/servicesd/modules)

add_library(bs_badwords MODULE bs_badwords.cpp)
set_target_properties(bs_badwords PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"bs_badwords\"")
target_link_libraries(bs_badwords clandestine_services)
target_include_directories(bs_badwords PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS bs_badwords DESTINATION lib/clandestine/servicesd/modules)

add_library(bs_bot MODULE bs_bot.cpp)
set_target_properties(bs_bot PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"bs_bot\"")
target_link_libraries(bs_bot clandestine_services)
target_include_directories(bs_bot PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS bs_bot DESTINATION lib/clandestine/servicesd/modules)

add_library(bs_botlist MODULE bs_botlist.cpp)
set_target_properties(bs_botlist PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"bs_botlist\"")
target_link_libraries(bs_botlist clandestine_services)
target_include_directories(bs_botlist PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS bs_botlist DESTINATION lib/clandestine/servicesd/modules)

add_library(bs_control MODULE bs_control.cpp)
set_target_properties(bs_control PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"bs_control\"")
target_link_libraries(bs_control clandestine_services)
target_include_directories(bs_control PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS bs_control DESTINATION lib/clandestine/servicesd/modules)

add_library(bs_info MODULE bs_info.cpp)
set_target_properties(bs_info PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"bs_info\"")
target_link_libraries(bs_info clandestine_services)
target_include_directories(bs_info PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS bs_info DESTINATION lib/clandestine/servicesd/modules)

add_library(bs_kick MODULE bs_kick.cpp)
set_target_properties(bs_kick PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"bs_kick\"")
target_link_libraries(bs_kick clandestine_services)
target_include_directories(bs_kick PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS bs_kick DESTINATION lib/clandestine/servicesd/modules)

add_library(bs_set MODULE bs_set.cpp)
set_target_properties(bs_set PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"bs_set\"")
target_link_libraries(bs_set clandestine_services)
target_include_directories(bs_set PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS bs_set DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_access MODULE cs_access.cpp)
set_target_properties(cs_access PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_access\"")
target_link_libraries(cs_access clandestine_services)
target_include_directories(cs_access PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_access DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_akick MODULE cs_akick.cpp)
set_target_properties(cs_akick PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_akick\"")
target_link_libraries(cs_akick clandestine_services)
target_include_directories(cs_akick PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_akick DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_ban MODULE cs_ban.cpp)
set_target_properties(cs_ban PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_ban\"")
target_link_libraries(cs_ban clandestine_services)
target_include_directories(cs_ban PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_ban DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_clone MODULE cs_clone.cpp)
set_target_properties(cs_clone PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_clone\"")
target_link_libraries(cs_clone clandestine_services)
target_include_directories(cs_clone PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_clone DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_drop MODULE cs_drop.cpp)
set_target_properties(cs_drop PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_drop\"")
target_link_libraries(cs_drop clandestine_services)
target_include_directories(cs_drop PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_drop DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_enforce MODULE cs_enforce.cpp)
set_target_properties(cs_enforce PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_enforce\"")
target_link_libraries(cs_enforce clandestine_services)
target_include_directories(cs_enforce PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_enforce DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_entrymsg MODULE cs_entrymsg.cpp)
set_target_properties(cs_entrymsg PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_entrymsg\"")
target_link_libraries(cs_entrymsg clandestine_services)
target_include_directories(cs_entrymsg PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_entrymsg DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_flags MODULE cs_flags.cpp)
set_target_properties(cs_flags PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_flags\"")
target_link_libraries(cs_flags clandestine_services)
target_include_directories(cs_flags PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_flags DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_getkey MODULE cs_getkey.cpp)
set_target_properties(cs_getkey PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_getkey\"")
target_link_libraries(cs_getkey clandestine_services)
target_include_directories(cs_getkey PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_getkey DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_info MODULE cs_info.cpp)
set_target_properties(cs_info PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_info\"")
target_link_libraries(cs_info clandestine_services)
target_include_directories(cs_info PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_info DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_invite MODULE cs_invite.cpp)
set_target_properties(cs_invite PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_invite\"")
target_link_libraries(cs_invite clandestine_services)
target_include_directories(cs_invite PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_invite DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_kick MODULE cs_kick.cpp)
set_target_properties(cs_kick PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_kick\"")
target_link_libraries(cs_kick clandestine_services)
target_include_directories(cs_kick PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_kick DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_list MODULE cs_list.cpp)
set_target_properties(cs_list PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_list\"")
target_link_libraries(cs_list clandestine_services)
target_include_directories(cs_list PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_list DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_log MODULE cs_log.cpp)
set_target_properties(cs_log PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_log\"")
target_link_libraries(cs_log clandestine_services)
target_include_directories(cs_log PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_log DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_mode MODULE cs_mode.cpp)
set_target_properties(cs_mode PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_mode\"")
target_link_libraries(cs_mode clandestine_services)
target_include_directories(cs_mode PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_mode DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_register MODULE cs_register.cpp)
set_target_properties(cs_register PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_register\"")
target_link_libraries(cs_register clandestine_services)
target_include_directories(cs_register PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_register DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_seen MODULE cs_seen.cpp)
set_target_properties(cs_seen PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_seen\"")
target_link_libraries(cs_seen clandestine_services)
target_include_directories(cs_seen PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_seen DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_set MODULE cs_set.cpp)
set_target_properties(cs_set PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_set\"")
target_link_libraries(cs_set clandestine_services)
target_include_directories(cs_set PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_set DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_set_misc MODULE cs_set_misc.cpp)
set_target_properties(cs_set_misc PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_set_misc\"")
target_link_libraries(cs_set_misc clandestine_services)
target_include_directories(cs_set_misc PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_set_misc DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_status MODULE cs_status.cpp)
set_target_properties(cs_status PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_status\"")
target_link_libraries(cs_status clandestine_services)
target_include_directories(cs_status PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_status DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_suspend MODULE cs_suspend.cpp)
set_target_properties(cs_suspend PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_suspend\"")
target_link_libraries(cs_suspend clandestine_services)
target_include_directories(cs_suspend PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_suspend DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_sync MODULE cs_sync.cpp)
set_target_properties(cs_sync PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_sync\"")
target_link_libraries(cs_sync clandestine_services)
target_include_directories(cs_sync PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_sync DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_topic MODULE cs_topic.cpp)
set_target_properties(cs_topic PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_topic\"")
target_link_libraries(cs_topic clandestine_services)
target_include_directories(cs_topic PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_topic DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_unban MODULE cs_unban.cpp)
set_target_properties(cs_unban PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_unban\"")
target_link_libraries(cs_unban clandestine_services)
target_include_directories(cs_unban PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_unban DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_updown MODULE cs_updown.cpp)
set_target_properties(cs_updown PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_updown\"")
target_link_libraries(cs_updown clandestine_services)
target_include_directories(cs_updown PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_updown DESTINATION lib/clandestine/servicesd/modules)

add_library(cs_xop MODULE cs_xop.cpp)
set_target_properties(cs_xop PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"cs_xop\"")
target_link_libraries(cs_xop clandestine_services)
target_include_directories(cs_xop PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS cs_xop DESTINATION lib/clandestine/servicesd/modules)

add_library(gl_global MODULE gl_global.cpp)
set_target_properties(gl_global PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"gl_global\"")
target_link_libraries(gl_global clandestine_services)
target_include_directories(gl_global PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS gl_global DESTINATION lib/clandestine/servicesd/modules)

add_library(greet MODULE greet.cpp)
set_target_properties(greet PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"greet\"")
target_link_libraries(greet clandestine_services)
target_include_directories(greet PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS greet DESTINATION lib/clandestine/servicesd/modules)

add_library(_help MODULE help.cpp)
set_target_properties(_help PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"help\"")
target_link_libraries(_help clandestine_services)
target_include_directories(_help PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS _help DESTINATION lib/clandestine/servicesd/modules)

add_library(hs_del MODULE hs_del.cpp)
set_target_properties(hs_del PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"hs_del\"")
target_link_libraries(hs_del clandestine_services)
target_include_directories(hs_del PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS hs_del DESTINATION lib/clandestine/servicesd/modules)

add_library(hs_group MODULE hs_group.cpp)
set_target_properties(hs_group PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"hs_group\"")
target_link_libraries(hs_group clandestine_services)
target_include_directories(hs_group PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS hs_group DESTINATION lib/clandestine/servicesd/modules)

add_library(hs_list MODULE hs_list.cpp)
set_target_properties(hs_list PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"hs_list\"")
target_link_libraries(hs_list clandestine_services)
target_include_directories(hs_list PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS hs_list DESTINATION lib/clandestine/servicesd/modules)

add_library(hs_off MODULE hs_off.cpp)
set_target_properties(hs_off PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"hs_off\"")
target_link_libraries(hs_off clandestine_services)
target_include_directories(hs_off PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS hs_off DESTINATION lib/clandestine/servicesd/modules)

add_library(hs_on MODULE hs_on.cpp)
set_target_properties(hs_on PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"hs_on\"")
target_link_libraries(hs_on clandestine_services)
target_include_directories(hs_on PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS hs_on DESTINATION lib/clandestine/servicesd/modules)

add_library(hs_request MODULE hs_request.cpp)
set_target_properties(hs_request PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"hs_request\"")
target_link_libraries(hs_request clandestine_services)
target_include_directories(hs_request PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS hs_request DESTINATION lib/clandestine/servicesd/modules)

add_library(hs_set MODULE hs_set.cpp)
set_target_properties(hs_set PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"hs_set\"")
target_link_libraries(hs_set clandestine_services)
target_include_directories(hs_set PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS hs_set DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_cancel MODULE ms_cancel.cpp)
set_target_properties(ms_cancel PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_cancel\"")
target_link_libraries(ms_cancel clandestine_services)
target_include_directories(ms_cancel PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_cancel DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_check MODULE ms_check.cpp)
set_target_properties(ms_check PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_check\"")
target_link_libraries(ms_check clandestine_services)
target_include_directories(ms_check PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_check DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_del MODULE ms_del.cpp)
set_target_properties(ms_del PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_del\"")
target_link_libraries(ms_del clandestine_services)
target_include_directories(ms_del PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_del DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_ignore MODULE ms_ignore.cpp)
set_target_properties(ms_ignore PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_ignore\"")
target_link_libraries(ms_ignore clandestine_services)
target_include_directories(ms_ignore PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_ignore DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_info MODULE ms_info.cpp)
set_target_properties(ms_info PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_info\"")
target_link_libraries(ms_info clandestine_services)
target_include_directories(ms_info PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_info DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_list MODULE ms_list.cpp)
set_target_properties(ms_list PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_list\"")
target_link_libraries(ms_list clandestine_services)
target_include_directories(ms_list PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_list DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_read MODULE ms_read.cpp)
set_target_properties(ms_read PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_read\"")
target_link_libraries(ms_read clandestine_services)
target_include_directories(ms_read PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_read DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_rsend MODULE ms_rsend.cpp)
set_target_properties(ms_rsend PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_rsend\"")
target_link_libraries(ms_rsend clandestine_services)
target_include_directories(ms_rsend PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_rsend DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_send MODULE ms_send.cpp)
set_target_properties(ms_send PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_send\"")
target_link_libraries(ms_send clandestine_services)
target_include_directories(ms_send PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_send DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_sendall MODULE ms_sendall.cpp)
set_target_properties(ms_sendall PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_sendall\"")
target_link_libraries(ms_sendall clandestine_services)
target_include_directories(ms_sendall PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_sendall DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_set MODULE ms_set.cpp)
set_target_properties(ms_set PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_set\"")
target_link_libraries(ms_set clandestine_services)
target_include_directories(ms_set PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_set DESTINATION lib/clandestine/servicesd/modules)

add_library(ms_staff MODULE ms_staff.cpp)
set_target_properties(ms_staff PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ms_staff\"")
target_link_libraries(ms_staff clandestine_services)
target_include_directories(ms_staff PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ms_staff DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_access MODULE ns_access.cpp)
set_target_properties(ns_access PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_access\"")
target_link_libraries(ns_access clandestine_services)
target_include_directories(ns_access PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_access DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_ajoin MODULE ns_ajoin.cpp)
set_target_properties(ns_ajoin PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_ajoin\"")
target_link_libraries(ns_ajoin clandestine_services)
target_include_directories(ns_ajoin PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_ajoin DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_alist MODULE ns_alist.cpp)
set_target_properties(ns_alist PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_alist\"")
target_link_libraries(ns_alist clandestine_services)
target_include_directories(ns_alist PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_alist DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_cert MODULE ns_cert.cpp)
set_target_properties(ns_cert PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_cert\"")
target_link_libraries(ns_cert clandestine_services)
target_include_directories(ns_cert PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_cert DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_drop MODULE ns_drop.cpp)
set_target_properties(ns_drop PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_drop\"")
target_link_libraries(ns_drop clandestine_services)
target_include_directories(ns_drop PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_drop DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_getemail MODULE ns_getemail.cpp)
set_target_properties(ns_getemail PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_getemail\"")
target_link_libraries(ns_getemail clandestine_services)
target_include_directories(ns_getemail PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_getemail DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_getpass MODULE ns_getpass.cpp)
set_target_properties(ns_getpass PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_getpass\"")
target_link_libraries(ns_getpass clandestine_services)
target_include_directories(ns_getpass PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_getpass DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_group MODULE ns_group.cpp)
set_target_properties(ns_group PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_group\"")
target_link_libraries(ns_group clandestine_services)
target_include_directories(ns_group PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_group DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_identify MODULE ns_identify.cpp)
set_target_properties(ns_identify PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_identify\"")
target_link_libraries(ns_identify clandestine_services)
target_include_directories(ns_identify PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_identify DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_info MODULE ns_info.cpp)
set_target_properties(ns_info PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_info\"")
target_link_libraries(ns_info clandestine_services)
target_include_directories(ns_info PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_info DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_list MODULE ns_list.cpp)
set_target_properties(ns_list PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_list\"")
target_link_libraries(ns_list clandestine_services)
target_include_directories(ns_list PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_list DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_logout MODULE ns_logout.cpp)
set_target_properties(ns_logout PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_logout\"")
target_link_libraries(ns_logout clandestine_services)
target_include_directories(ns_logout PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_logout DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_recover MODULE ns_recover.cpp)
set_target_properties(ns_recover PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_recover\"")
target_link_libraries(ns_recover clandestine_services)
target_include_directories(ns_recover PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_recover DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_register MODULE ns_register.cpp)
set_target_properties(ns_register PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_register\"")
target_link_libraries(ns_register clandestine_services)
target_include_directories(ns_register PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_register DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_resetpass MODULE ns_resetpass.cpp)
set_target_properties(ns_resetpass PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_resetpass\"")
target_link_libraries(ns_resetpass clandestine_services)
target_include_directories(ns_resetpass PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_resetpass DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_set MODULE ns_set.cpp)
set_target_properties(ns_set PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_set\"")
target_link_libraries(ns_set clandestine_services)
target_include_directories(ns_set PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_set DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_set_misc MODULE ns_set_misc.cpp)
set_target_properties(ns_set_misc PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_set_misc\"")
target_link_libraries(ns_set_misc clandestine_services)
target_include_directories(ns_set_misc PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_set_misc DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_status MODULE ns_status.cpp)
set_target_properties(ns_status PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_status\"")
target_link_libraries(ns_status clandestine_services)
target_include_directories(ns_status PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_status DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_suspend MODULE ns_suspend.cpp)
set_target_properties(ns_suspend PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_suspend\"")
target_link_libraries(ns_suspend clandestine_services)
target_include_directories(ns_suspend PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_suspend DESTINATION lib/clandestine/servicesd/modules)

add_library(ns_update MODULE ns_update.cpp)
set_target_properties(ns_update PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"ns_update\"")
target_link_libraries(ns_update clandestine_services)
target_include_directories(ns_update PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS ns_update DESTINATION lib/clandestine/servicesd/modules)

add_library(os_akill MODULE os_akill.cpp)
set_target_properties(os_akill PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_akill\"")
target_link_libraries(os_akill clandestine_services)
target_include_directories(os_akill PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_akill DESTINATION lib/clandestine/servicesd/modules)

add_library(os_chankill MODULE os_chankill.cpp)
set_target_properties(os_chankill PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_chankill\"")
target_link_libraries(os_chankill clandestine_services)
target_include_directories(os_chankill PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_chankill DESTINATION lib/clandestine/servicesd/modules)

add_library(os_config MODULE os_config.cpp)
set_target_properties(os_config PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_config\"")
target_link_libraries(os_config clandestine_services)
target_include_directories(os_config PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_config DESTINATION lib/clandestine/servicesd/modules)

add_library(os_defcon MODULE os_defcon.cpp)
set_target_properties(os_defcon PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_defcon\"")
target_link_libraries(os_defcon clandestine_services)
target_include_directories(os_defcon PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_defcon DESTINATION lib/clandestine/servicesd/modules)

add_library(os_dns MODULE os_dns.cpp)
set_target_properties(os_dns PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_dns\"")
target_link_libraries(os_dns clandestine_services)
target_include_directories(os_dns PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_dns DESTINATION lib/clandestine/servicesd/modules)

add_library(os_forbid MODULE os_forbid.cpp)
set_target_properties(os_forbid PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_forbid\"")
target_link_libraries(os_forbid clandestine_services)
target_include_directories(os_forbid PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_forbid DESTINATION lib/clandestine/servicesd/modules)

add_library(os_ignore MODULE os_ignore.cpp)
set_target_properties(os_ignore PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_ignore\"")
target_link_libraries(os_ignore clandestine_services)
target_include_directories(os_ignore PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_ignore DESTINATION lib/clandestine/servicesd/modules)

add_library(os_info MODULE os_info.cpp)
set_target_properties(os_info PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_info\"")
target_link_libraries(os_info clandestine_services)
target_include_directories(os_info PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_info DESTINATION lib/clandestine/servicesd/modules)

add_library(os_jupe MODULE os_jupe.cpp)
set_target_properties(os_jupe PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_jupe\"")
target_link_libraries(os_jupe clandestine_services)
target_include_directories(os_jupe PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_jupe DESTINATION lib/clandestine/servicesd/modules)

add_library(os_kick MODULE os_kick.cpp)
set_target_properties(os_kick PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_kick\"")
target_link_libraries(os_kick clandestine_services)
target_include_directories(os_kick PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_kick DESTINATION lib/clandestine/servicesd/modules)

add_library(os_kill MODULE os_kill.cpp)
set_target_properties(os_kill PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_kill\"")
target_link_libraries(os_kill clandestine_services)
target_include_directories(os_kill PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_kill DESTINATION lib/clandestine/servicesd/modules)

add_library(os_list MODULE os_list.cpp)
set_target_properties(os_list PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_list\"")
target_link_libraries(os_list clandestine_services)
target_include_directories(os_list PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_list DESTINATION lib/clandestine/servicesd/modules)

add_library(os_login MODULE os_login.cpp)
set_target_properties(os_login PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_login\"")
target_link_libraries(os_login clandestine_services)
target_include_directories(os_login PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_login DESTINATION lib/clandestine/servicesd/modules)

add_library(os_logsearch MODULE os_logsearch.cpp)
set_target_properties(os_logsearch PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_logsearch\"")
target_link_libraries(os_logsearch clandestine_services)
target_include_directories(os_logsearch PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_logsearch DESTINATION lib/clandestine/servicesd/modules)

add_library(os_mode MODULE os_mode.cpp)
set_target_properties(os_mode PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_mode\"")
target_link_libraries(os_mode clandestine_services)
target_include_directories(os_mode PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_mode DESTINATION lib/clandestine/servicesd/modules)

add_library(os_modinfo MODULE os_modinfo.cpp)
set_target_properties(os_modinfo PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_modinfo\"")
target_link_libraries(os_modinfo clandestine_services)
target_include_directories(os_modinfo PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_modinfo DESTINATION lib/clandestine/servicesd/modules)

add_library(os_module MODULE os_module.cpp)
set_target_properties(os_module PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_module\"")
target_link_libraries(os_module clandestine_services)
target_include_directories(os_module PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_module DESTINATION lib/clandestine/servicesd/modules)

add_library(os_news MODULE os_news.cpp)
set_target_properties(os_news PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_news\"")
target_link_libraries(os_news clandestine_services)
target_include_directories(os_news PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_news DESTINATION lib/clandestine/servicesd/modules)

add_library(os_noop MODULE os_noop.cpp)
set_target_properties(os_noop PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_noop\"")
target_link_libraries(os_noop clandestine_services)
target_include_directories(os_noop PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_noop DESTINATION lib/clandestine/servicesd/modules)

add_library(os_oline MODULE os_oline.cpp)
set_target_properties(os_oline PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_oline\"")
target_link_libraries(os_oline clandestine_services)
target_include_directories(os_oline PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_oline DESTINATION lib/clandestine/servicesd/modules)

add_library(os_oper MODULE os_oper.cpp)
set_target_properties(os_oper PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_oper\"")
target_link_libraries(os_oper clandestine_services)
target_include_directories(os_oper PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_oper DESTINATION lib/clandestine/servicesd/modules)

add_library(os_reload MODULE os_reload.cpp)
set_target_properties(os_reload PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_reload\"")
target_link_libraries(os_reload clandestine_services)
target_include_directories(os_reload PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_reload DESTINATION lib/clandestine/servicesd/modules)

add_library(os_session MODULE os_session.cpp)
set_target_properties(os_session PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_session\"")
target_link_libraries(os_session clandestine_services)
target_include_directories(os_session PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_session DESTINATION lib/clandestine/servicesd/modules)

add_library(os_set MODULE os_set.cpp)
set_target_properties(os_set PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_set\"")
target_link_libraries(os_set clandestine_services)
target_include_directories(os_set PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_set DESTINATION lib/clandestine/servicesd/modules)

add_library(os_shutdown MODULE os_shutdown.cpp)
set_target_properties(os_shutdown PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_shutdown\"")
target_link_libraries(os_shutdown clandestine_services)
target_include_directories(os_shutdown PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_shutdown DESTINATION lib/clandestine/servicesd/modules)

add_library(os_stats MODULE os_stats.cpp)
set_target_properties(os_stats PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_stats\"")
target_link_libraries(os_stats clandestine_services)
target_include_directories(os_stats PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_stats DESTINATION lib/clandestine/servicesd/modules)

add_library(os_svs MODULE os_svs.cpp)
set_target_properties(os_svs PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_svs\"")
target_link_libraries(os_svs clandestine_services)
target_include_directories(os_svs PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_svs DESTINATION lib/clandestine/servicesd/modules)

add_library(os_sxline MODULE os_sxline.cpp)
set_target_properties(os_sxline PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_sxline\"")
target_link_libraries(os_sxline clandestine_services)
target_include_directories(os_sxline PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_sxline DESTINATION lib/clandestine/servicesd/modules)

add_library(os_update MODULE os_update.cpp)
set_target_properties(os_update PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"os_update\"")
target_link_libraries(os_update clandestine_services)
target_include_directories(os_update PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/
  ../../include/)
install(TARGETS os_update DESTINATION lib/clandestine/servicesd/modules)
