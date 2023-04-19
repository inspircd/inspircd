add_definitions(-DINSPIRCD_VERSION="${PROJECT_VERSION}")

add_subdirectory(extra/)

add_subdirectory(m_spanningtree/)

add_library(m_abbreviation MODULE m_abbreviation.cpp)
set_target_properties(m_abbreviation PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_abbreviation\"")
target_link_libraries(m_abbreviation clandestine)
target_include_directories(m_abbreviation PRIVATE
  ../../include)
install(TARGETS m_abbreviation DESTINATION lib/clandestine/ircd/modules)

add_library(m_alias MODULE m_alias.cpp)
set_target_properties(m_alias PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_alias\"")
target_link_libraries(m_alias clandestine)
target_include_directories(m_alias PRIVATE
  ../../include)
install(TARGETS m_alias DESTINATION lib/clandestine/ircd/modules)

add_library(m_allowinvite MODULE m_allowinvite.cpp)
set_target_properties(m_allowinvite PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_allowinvite\"")
target_link_libraries(m_allowinvite clandestine)
target_include_directories(m_allowinvite PRIVATE
  ../../include)
install(TARGETS m_allowinvite DESTINATION lib/clandestine/ircd/modules)

add_library(m_alltime MODULE m_alltime.cpp)
set_target_properties(m_alltime PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_alltime\"")
target_link_libraries(m_alltime clandestine)
target_include_directories(m_alltime PRIVATE
  ../../include)
install(TARGETS m_alltime DESTINATION lib/clandestine/ircd/modules)

add_library(m_anticaps MODULE m_anticaps.cpp)
set_target_properties(m_anticaps PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_anticaps\"")
target_link_libraries(m_anticaps clandestine)
target_include_directories(m_anticaps PRIVATE
  ../../include)
install(TARGETS m_anticaps DESTINATION lib/clandestine/ircd/modules)

add_library(m_auditorium MODULE m_auditorium.cpp)
set_target_properties(m_auditorium PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_auditorium\"")
target_link_libraries(m_auditorium clandestine)
target_include_directories(m_auditorium PRIVATE
  ../../include)
install(TARGETS m_auditorium DESTINATION lib/clandestine/ircd/modules)

add_library(m_autoop MODULE m_autoop.cpp)
set_target_properties(m_autoop PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_autoop\"")
target_link_libraries(m_autoop clandestine)
target_include_directories(m_autoop PRIVATE
  ../../include)
install(TARGETS m_autoop DESTINATION lib/clandestine/ircd/modules)

add_library(m_banexception MODULE m_banexception.cpp)
set_target_properties(m_banexception PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_banexception\"")
target_link_libraries(m_banexception clandestine)
target_include_directories(m_banexception PRIVATE
  ../../include)
install(TARGETS m_banexception DESTINATION lib/clandestine/ircd/modules)

add_library(m_banredirect MODULE m_banredirect.cpp)
set_target_properties(m_banredirect PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_banredirect\"")
target_link_libraries(m_banredirect clandestine)
target_include_directories(m_banredirect PRIVATE
  ../../include)
install(TARGETS m_banredirect DESTINATION lib/clandestine/ircd/modules)

add_library(m_bcrypt MODULE m_bcrypt.cpp)
set_target_properties(m_bcrypt PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_bcrypt\"")
target_link_libraries(m_bcrypt clandestine ${libxcrypt_LIBRARIES})
target_include_directories(m_bcrypt PRIVATE
  ${libxcrypt_INCLUDE_DIR}
  ../../include)
install(TARGETS m_bcrypt DESTINATION lib/clandestine/ircd/modules)

add_library(m_blockamsg MODULE m_blockamsg.cpp)
set_target_properties(m_blockamsg PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_blockamsg\"")
target_link_libraries(m_blockamsg clandestine)
target_include_directories(m_blockamsg PRIVATE
  ../../include)
install(TARGETS m_blockamsg DESTINATION lib/clandestine/ircd/modules)

add_library(m_blockcaps MODULE m_blockcaps.cpp)
set_target_properties(m_blockcaps PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_blockcaps\"")
target_link_libraries(m_blockcaps clandestine)
target_include_directories(m_blockcaps PRIVATE
  ../../include)
install(TARGETS m_blockcaps DESTINATION lib/clandestine/ircd/modules)

add_library(m_blockcolor MODULE m_blockcolor.cpp)
set_target_properties(m_blockcolor PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_blockcolor\"")
target_link_libraries(m_blockcolor clandestine)
target_include_directories(m_blockcolor PRIVATE
  ../../include)
install(TARGETS m_blockcolor DESTINATION lib/clandestine/ircd/modules)

add_library(m_botmode MODULE m_botmode.cpp)
set_target_properties(m_botmode PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_botmode\"")
target_link_libraries(m_botmode clandestine)
target_include_directories(m_botmode PRIVATE
  ../../include)
install(TARGETS m_botmode DESTINATION lib/clandestine/ircd/modules)

add_library(m_callerid MODULE m_callerid.cpp)
set_target_properties(m_callerid PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_callerid\"")
target_link_libraries(m_callerid clandestine)
target_include_directories(m_callerid PRIVATE
  ../../include)
install(TARGETS m_callerid DESTINATION lib/clandestine/ircd/modules)

add_library(m_cap MODULE m_cap.cpp)
set_target_properties(m_cap PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_cap\"")
target_link_libraries(m_cap clandestine)
target_include_directories(m_cap PRIVATE
  ../../include)
install(TARGETS m_cap DESTINATION lib/clandestine/ircd/modules)

add_library(m_cban MODULE m_cban.cpp)
set_target_properties(m_cban PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_cban\"")
target_link_libraries(m_cban clandestine)
target_include_directories(m_cban PRIVATE
  ../../include)
install(TARGETS m_cban DESTINATION lib/clandestine/ircd/modules)

add_library(m_censor MODULE m_censor.cpp)
set_target_properties(m_censor PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_censor\"")
target_link_libraries(m_censor clandestine)
target_include_directories(m_censor PRIVATE
  ../../include)
install(TARGETS m_censor DESTINATION lib/clandestine/ircd/modules)

add_library(m_cgiirc MODULE m_cgiirc.cpp)
set_target_properties(m_cgiirc PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_cgiirc\"")
target_link_libraries(m_cgiirc clandestine)
target_include_directories(m_cgiirc PRIVATE
  ../../include)
install(TARGETS m_cgiirc DESTINATION lib/clandestine/ircd/modules)

add_library(m_chancreate MODULE m_chancreate.cpp)
set_target_properties(m_chancreate PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_chancreate\"")
target_link_libraries(m_chancreate clandestine)
target_include_directories(m_chancreate PRIVATE
  ../../include)
install(TARGETS m_chancreate DESTINATION lib/clandestine/ircd/modules)

add_library(m_chanfilter MODULE m_chanfilter.cpp)
set_target_properties(m_chanfilter PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_chanfilter\"")
target_link_libraries(m_chanfilter clandestine)
target_include_directories(m_chanfilter PRIVATE
  ../../include)
install(TARGETS m_chanfilter DESTINATION lib/clandestine/ircd/modules)

add_library(m_chanhistory MODULE m_chanhistory.cpp)
set_target_properties(m_chanhistory PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_chanhistory\"")
target_link_libraries(m_chanhistory clandestine)
target_include_directories(m_chanhistory PRIVATE
  ../../include)
install(TARGETS m_chanhistory DESTINATION lib/clandestine/ircd/modules)

add_library(m_chanlog MODULE m_chanlog.cpp)
set_target_properties(m_chanlog PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_chanlog\"")
target_link_libraries(m_chanlog clandestine)
target_include_directories(m_chanlog PRIVATE
  ../../include)
install(TARGETS m_chanlog DESTINATION lib/clandestine/ircd/modules)

add_library(m_channames MODULE m_channames.cpp)
set_target_properties(m_channames PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_channames\"")
target_link_libraries(m_channames clandestine)
target_include_directories(m_channames PRIVATE
  ../../include)
install(TARGETS m_channames DESTINATION lib/clandestine/ircd/modules)

add_library(m_channelban MODULE m_channelban.cpp)
set_target_properties(m_channelban PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_channelban\"")
target_link_libraries(m_channelban clandestine)
target_include_directories(m_channelban PRIVATE
  ../../include)
install(TARGETS m_channelban DESTINATION lib/clandestine/ircd/modules)

add_library(m_check MODULE m_check.cpp)
set_target_properties(m_check PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_check\"")
target_link_libraries(m_check clandestine)
target_include_directories(m_check PRIVATE
  ../../include)
install(TARGETS m_check DESTINATION lib/clandestine/ircd/modules)

add_library(m_chghost MODULE m_chghost.cpp)
set_target_properties(m_chghost PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_chghost\"")
target_link_libraries(m_chghost clandestine)
target_include_directories(m_chghost PRIVATE
  ../../include)
install(TARGETS m_chghost DESTINATION lib/clandestine/ircd/modules)

add_library(m_chgident MODULE m_chgident.cpp)
set_target_properties(m_chgident PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_chgident\"")
target_link_libraries(m_chgident clandestine)
target_include_directories(m_chgident PRIVATE
  ../../include)
install(TARGETS m_chgident DESTINATION lib/clandestine/ircd/modules)

add_library(m_chgname MODULE m_chgname.cpp)
set_target_properties(m_chgname PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_chgname\"")
target_link_libraries(m_chgname clandestine)
target_include_directories(m_chgname PRIVATE
  ../../include)
install(TARGETS m_chgname DESTINATION lib/clandestine/ircd/modules)

add_library(m_classban MODULE m_classban.cpp)
set_target_properties(m_classban PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_classban\"")
target_link_libraries(m_classban clandestine)
target_include_directories(m_classban PRIVATE
  ../../include)
install(TARGETS m_classban DESTINATION lib/clandestine/ircd/modules)

add_library(m_clearchan MODULE m_clearchan.cpp)
set_target_properties(m_clearchan PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_clearchan\"")
target_link_libraries(m_clearchan clandestine)
target_include_directories(m_clearchan PRIVATE
  ../../include)
install(TARGETS m_clearchan DESTINATION lib/clandestine/ircd/modules)

add_library(m_cloaking MODULE m_cloaking.cpp)
set_target_properties(m_cloaking PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_cloaking\"")
target_link_libraries(m_cloaking clandestine)
target_include_directories(m_cloaking PRIVATE
  ../../include)
install(TARGETS m_cloaking DESTINATION lib/clandestine/ircd/modules)

add_library(m_clones MODULE m_clones.cpp)
set_target_properties(m_clones PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_clones\"")
target_link_libraries(m_clones clandestine)
target_include_directories(m_clones PRIVATE
  ../../include)
install(TARGETS m_clones DESTINATION lib/clandestine/ircd/modules)

add_library(m_codepage MODULE m_codepage.cpp)
set_target_properties(m_codepage PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_codepage\"")
target_link_libraries(m_codepage clandestine)
target_include_directories(m_codepage PRIVATE
  ../../include)
install(TARGETS m_codepage DESTINATION lib/clandestine/ircd/modules)

add_library(m_commonchans MODULE m_commonchans.cpp)
set_target_properties(m_commonchans PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_commonchans\"")
target_link_libraries(m_commonchans clandestine)
target_include_directories(m_commonchans PRIVATE
  ../../include)
install(TARGETS m_commonchans DESTINATION lib/clandestine/ircd/modules)

add_library(m_conn_join MODULE m_conn_join.cpp)
set_target_properties(m_conn_join PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_conn_join\"")
target_link_libraries(m_conn_join clandestine)
target_include_directories(m_conn_join PRIVATE
  ../../include)
install(TARGETS m_conn_join DESTINATION lib/clandestine/ircd/modules)

add_library(m_conn_umodes MODULE m_conn_umodes.cpp)
set_target_properties(m_conn_umodes PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_conn_umodes\"")
target_link_libraries(m_conn_umodes clandestine)
target_include_directories(m_conn_umodes PRIVATE
  ../../include)
install(TARGETS m_conn_umodes DESTINATION lib/clandestine/ircd/modules)

add_library(m_conn_waitpong MODULE m_conn_waitpong.cpp)
set_target_properties(m_conn_waitpong PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_conn_waitpong\"")
target_link_libraries(m_conn_waitpong clandestine)
target_include_directories(m_conn_waitpong PRIVATE
  ../../include)
install(TARGETS m_conn_umodes DESTINATION lib/clandestine/ircd/modules)

add_library(m_connectban MODULE m_connectban.cpp)
set_target_properties(m_connectban PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_connectban\"")
target_link_libraries(m_connectban clandestine)
target_include_directories(m_connectban PRIVATE
  ../../include)
install(TARGETS m_connectban DESTINATION lib/clandestine/ircd/modules)

add_library(m_connflood MODULE m_connflood.cpp)
set_target_properties(m_connflood PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_connflood\"")
target_link_libraries(m_connflood clandestine)
target_include_directories(m_connflood PRIVATE
  ../../include)
install(TARGETS m_connflood DESTINATION lib/clandestine/ircd/modules)

add_library(m_customprefix MODULE m_customprefix.cpp)
set_target_properties(m_customprefix PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_customprefix\"")
target_link_libraries(m_customprefix clandestine)
target_include_directories(m_customprefix PRIVATE
  ../../include)
install(TARGETS m_customprefix DESTINATION lib/clandestine/ircd/modules)

add_library(m_customtitle MODULE m_customtitle.cpp)
set_target_properties(m_customtitle PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_customtitle\"")
target_link_libraries(m_customtitle clandestine)
target_include_directories(m_customtitle PRIVATE
  ../../include)
install(TARGETS m_customtitle DESTINATION lib/clandestine/ircd/modules)

add_library(m_cycle MODULE m_cycle.cpp)
set_target_properties(m_cycle PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_cycle\"")
target_link_libraries(m_cycle clandestine)
target_include_directories(m_cycle PRIVATE
  ../../include)
install(TARGETS m_cycle DESTINATION lib/clandestine/ircd/modules)

add_library(m_dccallow MODULE m_dccallow.cpp)
set_target_properties(m_dccallow PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_dccallow\"")
target_link_libraries(m_dccallow clandestine)
target_include_directories(m_dccallow PRIVATE
  ../../include)
install(TARGETS m_dccallow DESTINATION lib/clandestine/ircd/modules)

add_library(m_deaf MODULE m_deaf.cpp)
set_target_properties(m_deaf PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_deaf\"")
target_link_libraries(m_deaf clandestine)
target_include_directories(m_deaf PRIVATE
  ../../include)
install(TARGETS m_deaf DESTINATION lib/clandestine/ircd/modules)

add_library(m_delayjoin MODULE m_delayjoin.cpp)
set_target_properties(m_delayjoin PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_delayjoin\"")
target_link_libraries(m_delayjoin clandestine)
target_include_directories(m_delayjoin PRIVATE
  ../../include)
install(TARGETS m_delayjoin DESTINATION lib/clandestine/ircd/modules)

add_library(m_delaymsg MODULE m_delaymsg.cpp)
set_target_properties(m_delaymsg PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_delaymsg\"")
target_link_libraries(m_delaymsg clandestine)
target_include_directories(m_delaymsg PRIVATE
  ../../include)
install(TARGETS m_delaymsg DESTINATION lib/clandestine/ircd/modules)

add_library(m_denychans MODULE m_denychans.cpp)
set_target_properties(m_denychans PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_denychans\"")
target_link_libraries(m_denychans clandestine)
target_include_directories(m_denychans PRIVATE
  ../../include)
install(TARGETS m_denychans DESTINATION lib/clandestine/ircd/modules)

add_library(m_disable MODULE m_disable.cpp)
set_target_properties(m_disable PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_disable\"")
target_link_libraries(m_disable clandestine)
target_include_directories(m_disable PRIVATE
  ../../include)
install(TARGETS m_disable DESTINATION lib/clandestine/ircd/modules)

add_library(m_dnsbl MODULE m_dnsbl.cpp)
set_target_properties(m_dnsbl PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_dnsbl\"")
target_link_libraries(m_dnsbl clandestine)
target_include_directories(m_dnsbl PRIVATE
  ../../include)
install(TARGETS m_dnsbl DESTINATION lib/clandestine/ircd/modules)

add_library(m_exemptchanops MODULE m_exemptchanops.cpp)
set_target_properties(m_exemptchanops PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_exemptchanops\"")
target_link_libraries(m_exemptchanops clandestine)
target_include_directories(m_exemptchanops PRIVATE
  ../../include)
install(TARGETS m_exemptchanops DESTINATION lib/clandestine/ircd/modules)

add_library(m_filter MODULE m_filter.cpp)
set_target_properties(m_filter PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_filter\"")
target_link_libraries(m_filter clandestine)
target_include_directories(m_filter PRIVATE
  ../../include)
install(TARGETS m_filter DESTINATION lib/clandestine/ircd/modules)

add_library(m_flashpolicyd MODULE m_flashpolicyd.cpp)
set_target_properties(m_flashpolicyd PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_flashpolicyd\"")
target_link_libraries(m_flashpolicyd clandestine)
target_include_directories(m_flashpolicyd PRIVATE
  ../../include)
install(TARGETS m_flashpolicyd DESTINATION lib/clandestine/ircd/modules)

add_library(m_gecosban MODULE m_gecosban.cpp)
set_target_properties(m_gecosban PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_gecosban\"")
target_link_libraries(m_gecosban clandestine)
target_include_directories(m_gecosban PRIVATE
  ../../include)
install(TARGETS m_gecosban DESTINATION lib/clandestine/ircd/modules)

add_library(m_geoban MODULE m_geoban.cpp)
set_target_properties(m_geoban PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_geoban\"")
target_link_libraries(m_geoban clandestine)
target_include_directories(m_geoban PRIVATE
  ../../include)
install(TARGETS m_geoban DESTINATION lib/clandestine/ircd/modules)

add_library(m_geoclass MODULE m_geoclass.cpp)
set_target_properties(m_geoclass PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_geoclass\"")
target_link_libraries(m_geoclass clandestine)
target_include_directories(m_geoclass PRIVATE
  ../../include)
install(TARGETS m_geoclass DESTINATION lib/clandestine/ircd/modules)

add_library(m_globalload MODULE m_globalload.cpp)
set_target_properties(m_globalload PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_globalload\"")
target_link_libraries(m_globalload clandestine)
target_include_directories(m_globalload PRIVATE
  ../../include)
install(TARGETS m_globalload DESTINATION lib/clandestine/ircd/modules)

add_library(m_globops MODULE m_globops.cpp)
set_target_properties(m_globops PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_globops\"")
target_link_libraries(m_globops clandestine)
target_include_directories(m_globops PRIVATE
  ../../include)
install(TARGETS m_globops DESTINATION lib/clandestine/ircd/modules)

add_library(m_haproxy MODULE m_haproxy.cpp)
set_target_properties(m_haproxy PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_haproxy\"")
target_link_libraries(m_haproxy clandestine)
target_include_directories(m_haproxy PRIVATE
  ../../include)
install(TARGETS m_haproxy DESTINATION lib/clandestine/ircd/modules)

add_library(m_helpop MODULE m_helpop.cpp)
set_target_properties(m_helpop PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_helpop\"")
target_link_libraries(m_helpop clandestine)
target_include_directories(m_helpop PRIVATE
  ../../include)
install(TARGETS m_helpop DESTINATION lib/clandestine/ircd/modules)

add_library(m_hidechans MODULE m_hidechans.cpp)
set_target_properties(m_hidechans PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_hidechans\"")
target_link_libraries(m_hidechans clandestine)
target_include_directories(m_hidechans PRIVATE
  ../../include)
install(TARGETS m_hidechans DESTINATION lib/clandestine/ircd/modules)

add_library(m_hidelist MODULE m_hidelist.cpp)
set_target_properties(m_hidelist PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_hidelist\"")
target_link_libraries(m_hidelist clandestine)
target_include_directories(m_hidelist PRIVATE
  ../../include)
install(TARGETS m_hidelist DESTINATION lib/clandestine/ircd/modules)

add_library(m_hidemode MODULE m_hidemode.cpp)
set_target_properties(m_hidemode PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_hidemode\"")
target_link_libraries(m_hidemode clandestine)
target_include_directories(m_hidemode PRIVATE
  ../../include)
install(TARGETS m_hidemode DESTINATION lib/clandestine/ircd/modules)

add_library(m_hideoper MODULE m_hideoper.cpp)
set_target_properties(m_hideoper PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_hideoper\"")
target_link_libraries(m_hideoper clandestine)
target_include_directories(m_hideoper PRIVATE
  ../../include)
install(TARGETS m_hideoper DESTINATION lib/clandestine/ircd/modules)

add_library(m_hostchange MODULE m_hostchange.cpp)
set_target_properties(m_hostchange PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_hostchange\"")
target_link_libraries(m_hostchange clandestine)
target_include_directories(m_hostchange PRIVATE
  ../../include)
install(TARGETS m_hostchange DESTINATION lib/clandestine/ircd/modules)

add_library(m_hostcycle MODULE m_hostcycle.cpp)
set_target_properties(m_hostcycle PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_hostcycle\"")
target_link_libraries(m_hostcycle clandestine)
target_include_directories(m_hostcycle PRIVATE
  ../../include)
install(TARGETS m_hostcycle DESTINATION lib/clandestine/ircd/modules)

add_library(m_httpd MODULE m_httpd.cpp)
set_target_properties(m_httpd PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_httpd\"")
target_link_libraries(m_httpd clandestine ${http_parser_LIBRARIES})
target_include_directories(m_httpd PRIVATE
  ${http_parser_INCLUDE_DIR}
  ../../include)
install(TARGETS m_httpd DESTINATION lib/clandestine/ircd/modules)

add_library(m_httpd_acl MODULE m_httpd_acl.cpp)
set_target_properties(m_httpd_acl PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_httpd_acl\"")
target_link_libraries(m_httpd_acl clandestine)
target_include_directories(m_httpd_acl PRIVATE
  ../../include)
install(TARGETS m_httpd_acl DESTINATION lib/clandestine/ircd/modules)

add_library(m_httpd_config MODULE m_httpd_config.cpp)
set_target_properties(m_httpd_config PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_httpd_config\"")
target_link_libraries(m_httpd_config clandestine)
target_include_directories(m_httpd_config PRIVATE
  ../../include)
install(TARGETS m_httpd_config DESTINATION lib/clandestine/ircd/modules)

add_library(m_httpd_stats MODULE m_httpd_stats.cpp)
set_target_properties(m_httpd_stats PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_httpd_stats\"")
target_link_libraries(m_httpd_stats clandestine)
target_include_directories(m_httpd_stats PRIVATE
  ../../include)
install(TARGETS m_httpd_stats DESTINATION lib/clandestine/ircd/modules)

add_library(m_ident MODULE m_ident.cpp)
set_target_properties(m_ident PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ident\"")
target_link_libraries(m_ident clandestine)
target_include_directories(m_ident PRIVATE
  ../../include)
install(TARGETS m_ident DESTINATION lib/clandestine/ircd/modules)

add_library(m_inviteexception MODULE m_inviteexception.cpp)
set_target_properties(m_inviteexception PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_inviteexception\"")
target_link_libraries(m_inviteexception clandestine)
target_include_directories(m_inviteexception PRIVATE
  ../../include)
install(TARGETS m_inviteexception DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3 MODULE m_ircv3.cpp)
set_target_properties(m_ircv3 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3\"")
target_link_libraries(m_ircv3 clandestine)
target_include_directories(m_ircv3 PRIVATE
  ../../include)
install(TARGETS m_ircv3 DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3_accounttag MODULE m_ircv3_accounttag.cpp)
set_target_properties(m_ircv3_accounttag PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3_accounttag\"")
target_link_libraries(m_ircv3_accounttag clandestine)
target_include_directories(m_ircv3_accounttag PRIVATE
  ../../include)
install(TARGETS m_ircv3_accounttag DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3_batch MODULE m_ircv3_batch.cpp)
set_target_properties(m_ircv3_batch PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3_batch\"")
target_link_libraries(m_ircv3_batch clandestine)
target_include_directories(m_ircv3_batch PRIVATE
  ../../include)
install(TARGETS m_ircv3_batch DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3_capnotify MODULE m_ircv3_capnotify.cpp)
set_target_properties(m_ircv3_capnotify PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3_capnotify\"")
target_link_libraries(m_ircv3_capnotify clandestine)
target_include_directories(m_ircv3_capnotify PRIVATE
  ../../include)
install(TARGETS m_ircv3_capnotify DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3_chghost MODULE m_ircv3_chghost.cpp)
set_target_properties(m_ircv3_chghost PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3_chghost\"")
target_link_libraries(m_ircv3_chghost clandestine)
target_include_directories(m_ircv3_chghost PRIVATE
  ../../include)
install(TARGETS m_ircv3_chghost DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3_ctctags MODULE m_ircv3_ctctags.cpp)
set_target_properties(m_ircv3_ctctags PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3_ctctags\"")
target_link_libraries(m_ircv3_ctctags clandestine)
target_include_directories(m_ircv3_ctctags PRIVATE
  ../../include)
install(TARGETS m_ircv3_ctctags DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3_echomessage MODULE m_ircv3_echomessage.cpp)
set_target_properties(m_ircv3_echomessage PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3_echomessage\"")
target_link_libraries(m_ircv3_echomessage clandestine)
target_include_directories(m_ircv3_echomessage PRIVATE
  ../../include)
install(TARGETS m_ircv3_echomessage DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3_invitenotify MODULE m_ircv3_invitenotify.cpp)
set_target_properties(m_ircv3_invitenotify PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3_invitenotify\"")
target_link_libraries(m_ircv3_invitenotify clandestine)
target_include_directories(m_ircv3_invitenotify PRIVATE
  ../../include)
install(TARGETS m_ircv3_invitenotify DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3_labeledresponse MODULE m_ircv3_labeledresponse.cpp)
set_target_properties(m_ircv3_labeledresponse PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3_labeledresponse\"")
target_link_libraries(m_ircv3_labeledresponse clandestine)
target_include_directories(m_ircv3_labeledresponse PRIVATE
  ../../include)
install(TARGETS m_ircv3_labeledresponse DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3_msgid MODULE m_ircv3_msgid.cpp)
set_target_properties(m_ircv3_msgid PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3_msgid\"")
target_link_libraries(m_ircv3_msgid clandestine)
target_include_directories(m_ircv3_msgid PRIVATE
  ../../include)
install(TARGETS m_ircv3_msgid DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3_servertime MODULE m_ircv3_servertime.cpp)
set_target_properties(m_ircv3_servertime PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3_servertime\"")
target_link_libraries(m_ircv3_servertime clandestine)
target_include_directories(m_ircv3_servertime PRIVATE
  ../../include)
install(TARGETS m_ircv3_servertime DESTINATION lib/clandestine/ircd/modules)

add_library(m_ircv3_sts MODULE m_ircv3_sts.cpp)
set_target_properties(m_ircv3_sts PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ircv3_sts\"")
target_link_libraries(m_ircv3_sts clandestine)
target_include_directories(m_ircv3_sts PRIVATE
  ../../include)
install(TARGETS m_ircv3_sts DESTINATION lib/clandestine/ircd/modules)

add_library(m_joinflood MODULE m_joinflood.cpp)
set_target_properties(m_joinflood PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_joinflood\"")
target_link_libraries(m_joinflood clandestine)
target_include_directories(m_joinflood PRIVATE
  ../../include)
install(TARGETS m_joinflood DESTINATION lib/clandestine/ircd/modules)

add_library(m_kicknorejoin MODULE m_kicknorejoin.cpp)
set_target_properties(m_kicknorejoin PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_kicknorejoin\"")
target_link_libraries(m_kicknorejoin clandestine)
target_include_directories(m_kicknorejoin PRIVATE
  ../../include)
install(TARGETS m_kicknorejoin DESTINATION lib/clandestine/ircd/modules)

add_library(m_knock MODULE m_knock.cpp)
set_target_properties(m_knock PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_knock\"")
target_link_libraries(m_knock clandestine)
target_include_directories(m_knock PRIVATE
  ../../include)
install(TARGETS m_knock DESTINATION lib/clandestine/ircd/modules)

add_library(m_lockserv MODULE m_lockserv.cpp)
set_target_properties(m_lockserv PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_lockserv\"")
target_link_libraries(m_lockserv clandestine)
target_include_directories(m_lockserv PRIVATE
  ../../include)
install(TARGETS m_lockserv DESTINATION lib/clandestine/ircd/modules)

add_library(m_maphide MODULE m_maphide.cpp)
set_target_properties(m_maphide PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_maphide\"")
target_link_libraries(m_maphide clandestine)
target_include_directories(m_maphide PRIVATE
  ../../include)
install(TARGETS m_maphide DESTINATION lib/clandestine/ircd/modules)

add_library(m_md5 MODULE m_md5.cpp)
set_target_properties(m_md5 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_md5\"")
target_link_libraries(m_md5 clandestine)
target_include_directories(m_md5 PRIVATE
  ../../include)
install(TARGETS m_md5 DESTINATION lib/clandestine/ircd/modules)

add_library(m_messageflood MODULE m_messageflood.cpp)
set_target_properties(m_messageflood PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_messageflood\"")
target_link_libraries(m_messageflood clandestine)
target_include_directories(m_messageflood PRIVATE
  ../../include)
install(TARGETS m_messageflood DESTINATION lib/clandestine/ircd/modules)

add_library(m_mlock MODULE m_mlock.cpp)
set_target_properties(m_mlock PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_mlock\"")
target_link_libraries(m_mlock clandestine)
target_include_directories(m_mlock PRIVATE
  ../../include)
install(TARGETS m_mlock DESTINATION lib/clandestine/ircd/modules)

add_library(m_modenotice MODULE m_modenotice.cpp)
set_target_properties(m_modenotice PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_modenotice\"")
target_link_libraries(m_modenotice clandestine)
target_include_directories(m_modenotice PRIVATE
  ../../include)
install(TARGETS m_modenotice DESTINATION lib/clandestine/ircd/modules)

add_library(m_monitor MODULE m_monitor.cpp)
set_target_properties(m_monitor PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_monitor\"")
target_link_libraries(m_monitor clandestine)
target_include_directories(m_monitor PRIVATE
  ../../include)
install(TARGETS m_monitor DESTINATION lib/clandestine/ircd/modules)

add_library(m_muteban MODULE m_muteban.cpp)
set_target_properties(m_muteban PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_muteban\"")
target_link_libraries(m_muteban clandestine)
target_include_directories(m_muteban PRIVATE
  ../../include)
install(TARGETS m_muteban DESTINATION lib/clandestine/ircd/modules)

add_library(m_namedmodes MODULE m_namedmodes.cpp)
set_target_properties(m_namedmodes PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_namedmodes\"")
target_link_libraries(m_namedmodes clandestine)
target_include_directories(m_namedmodes PRIVATE
  ../../include)
install(TARGETS m_namedmodes DESTINATION lib/clandestine/ircd/modules)

add_library(m_namesx MODULE m_namesx.cpp)
set_target_properties(m_namesx PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_namesx\"")
target_link_libraries(m_namesx clandestine)
target_include_directories(m_namesx PRIVATE
  ../../include)
install(TARGETS m_namesx DESTINATION lib/clandestine/ircd/modules)

add_library(m_nationalchars MODULE m_nationalchars.cpp)
set_target_properties(m_nationalchars PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_nationalchars\"")
target_link_libraries(m_nationalchars clandestine)
target_include_directories(m_nationalchars PRIVATE
  ../../include)
install(TARGETS m_nationalchars DESTINATION lib/clandestine/ircd/modules)

add_library(m_nickflood MODULE m_nickflood.cpp)
set_target_properties(m_nickflood PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_nickflood\"")
target_link_libraries(m_nickflood clandestine)
target_include_directories(m_nickflood PRIVATE
  ../../include)
install(TARGETS m_nickflood DESTINATION lib/clandestine/ircd/modules)

add_library(m_nicklock MODULE m_nicklock.cpp)
set_target_properties(m_nicklock PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_nicklock\"")
target_link_libraries(m_nicklock clandestine)
target_include_directories(m_nicklock PRIVATE
  ../../include)
install(TARGETS m_nicklock DESTINATION lib/clandestine/ircd/modules)

add_library(m_noctcp MODULE m_noctcp.cpp)
set_target_properties(m_noctcp PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_noctcp\"")
target_link_libraries(m_noctcp clandestine)
target_include_directories(m_noctcp PRIVATE
  ../../include)
install(TARGETS m_noctcp DESTINATION lib/clandestine/ircd/modules)

add_library(m_nokicks MODULE m_nokicks.cpp)
set_target_properties(m_nokicks PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_nokicks\"")
target_link_libraries(m_nokicks clandestine)
target_include_directories(m_nokicks PRIVATE
  ../../include)
install(TARGETS m_nokicks DESTINATION lib/clandestine/ircd/modules)

add_library(m_nonicks MODULE m_nonicks.cpp)
set_target_properties(m_nonicks PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_nonicks\"")
target_link_libraries(m_nonicks clandestine)
target_include_directories(m_nonicks PRIVATE
  ../../include)
install(TARGETS m_nonicks DESTINATION lib/clandestine/ircd/modules)

add_library(m_nonotice MODULE m_nonotice.cpp)
set_target_properties(m_nonotice PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_nonotice\"")
target_link_libraries(m_nonotice clandestine)
target_include_directories(m_nonotice PRIVATE
  ../../include)
install(TARGETS m_nonotice DESTINATION lib/clandestine/ircd/modules)

add_library(m_nopartmsg MODULE m_nopartmsg.cpp)
set_target_properties(m_nopartmsg PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_nopartmsg\"")
target_link_libraries(m_nopartmsg clandestine)
target_include_directories(m_nopartmsg PRIVATE
  ../../include)
install(TARGETS m_nopartmsg DESTINATION lib/clandestine/ircd/modules)

add_library(m_ojoin MODULE m_ojoin.cpp)
set_target_properties(m_ojoin PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ojoin\"")
target_link_libraries(m_ojoin clandestine)
target_include_directories(m_ojoin PRIVATE
  ../../include)
install(TARGETS m_ojoin DESTINATION lib/clandestine/ircd/modules)

add_library(m_operchans MODULE m_operchans.cpp)
set_target_properties(m_operchans PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_operchans\"")
target_link_libraries(m_operchans clandestine)
target_include_directories(m_operchans PRIVATE
  ../../include)
install(TARGETS m_operchans DESTINATION lib/clandestine/ircd/modules)

add_library(m_operjoin MODULE m_operjoin.cpp)
set_target_properties(m_operjoin PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_operjoin\"")
target_link_libraries(m_operjoin clandestine)
target_include_directories(m_operjoin PRIVATE
  ../../include)
install(TARGETS m_operjoin DESTINATION lib/clandestine/ircd/modules)

add_library(m_operlevels MODULE m_operlevels.cpp)
set_target_properties(m_operlevels PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_operlevels\"")
target_link_libraries(m_operlevels clandestine)
target_include_directories(m_operlevels PRIVATE
  ../../include)
install(TARGETS m_operlevels DESTINATION lib/clandestine/ircd/modules)

add_library(m_operlog MODULE m_operlog.cpp)
set_target_properties(m_operlog PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_operlog\"")
target_link_libraries(m_operlog clandestine)
target_include_directories(m_operlog PRIVATE
  ../../include)
install(TARGETS m_operlog DESTINATION lib/clandestine/ircd/modules)

add_library(m_opermodes MODULE m_opermodes.cpp)
set_target_properties(m_opermodes PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_opermodes\"")
target_link_libraries(m_opermodes clandestine)
target_include_directories(m_opermodes PRIVATE
  ../../include)
install(TARGETS m_opermodes DESTINATION lib/clandestine/ircd/modules)

add_library(m_opermotd MODULE m_opermotd.cpp)
set_target_properties(m_opermotd PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_opermotd\"")
target_link_libraries(m_opermotd clandestine)
target_include_directories(m_opermotd PRIVATE
  ../../include)
install(TARGETS m_opermotd DESTINATION lib/clandestine/ircd/modules)

add_library(m_operprefix MODULE m_operprefix.cpp)
set_target_properties(m_operprefix PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_operprefix\"")
target_link_libraries(m_operprefix clandestine)
target_include_directories(m_operprefix PRIVATE
  ../../include)
install(TARGETS m_operprefix DESTINATION lib/clandestine/ircd/modules)

add_library(m_override MODULE m_override.cpp)
set_target_properties(m_override PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_override\"")
target_link_libraries(m_override clandestine)
target_include_directories(m_override PRIVATE
  ../../include)
install(TARGETS m_override DESTINATION lib/clandestine/ircd/modules)

add_library(m_passforward MODULE m_passforward.cpp)
set_target_properties(m_passforward PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_passforward\"")
target_link_libraries(m_passforward clandestine)
target_include_directories(m_passforward PRIVATE
  ../../include)
install(TARGETS m_passforward DESTINATION lib/clandestine/ircd/modules)

add_library(m_password_hash MODULE m_password_hash.cpp)
set_target_properties(m_password_hash PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_password_hash\"")
target_link_libraries(m_password_hash clandestine)
target_include_directories(m_password_hash PRIVATE
  ../../include)
install(TARGETS m_password_hash DESTINATION lib/clandestine/ircd/modules)

add_library(m_pbkdf2 MODULE m_pbkdf2.cpp)
set_target_properties(m_pbkdf2 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_pbkdf2\"")
target_link_libraries(m_pbkdf2 clandestine)
target_include_directories(m_pbkdf2 PRIVATE
  ../../include)
install(TARGETS m_pbkdf2 DESTINATION lib/clandestine/ircd/modules)

add_library(m_permchannels MODULE m_permchannels.cpp)
set_target_properties(m_permchannels PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_permchannels\"")
target_link_libraries(m_permchannels clandestine)
target_include_directories(m_permchannels PRIVATE
  ../../include)
install(TARGETS m_permchannels DESTINATION lib/clandestine/ircd/modules)

add_library(m_randquote MODULE m_randquote.cpp)
set_target_properties(m_randquote PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_randquote\"")
target_link_libraries(m_randquote clandestine)
target_include_directories(m_randquote PRIVATE
  ../../include)
install(TARGETS m_randquote DESTINATION lib/clandestine/ircd/modules)

add_library(m_redirect MODULE m_redirect.cpp)
set_target_properties(m_redirect PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_redirect\"")
target_link_libraries(m_redirect clandestine)
target_include_directories(m_redirect PRIVATE
  ../../include)
install(TARGETS m_redirect DESTINATION lib/clandestine/ircd/modules)

add_library(m_regex_glob MODULE m_regex_glob.cpp)
set_target_properties(m_regex_glob PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_regex_glob\"")
target_link_libraries(m_regex_glob clandestine)
target_include_directories(m_regex_glob PRIVATE
  ../../include)
install(TARGETS m_regex_glob DESTINATION lib/clandestine/ircd/modules)

add_library(m_remove MODULE m_remove.cpp)
set_target_properties(m_remove PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_remove\"")
target_link_libraries(m_remove clandestine)
target_include_directories(m_remove PRIVATE
  ../../include)
install(TARGETS m_remove DESTINATION lib/clandestine/ircd/modules)

add_library(m_repeat MODULE m_repeat.cpp)
set_target_properties(m_repeat PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_repeat\"")
target_link_libraries(m_repeat clandestine)
target_include_directories(m_repeat PRIVATE
  ../../include)
install(TARGETS m_repeat DESTINATION lib/clandestine/ircd/modules)

add_library(m_restrictchans MODULE m_restrictchans.cpp)
set_target_properties(m_restrictchans PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_restrictchans\"")
target_link_libraries(m_restrictchans clandestine)
target_include_directories(m_restrictchans PRIVATE
  ../../include)
install(TARGETS m_restrictchans DESTINATION lib/clandestine/ircd/modules)

add_library(m_restrictmsg MODULE m_restrictmsg.cpp)
set_target_properties(m_restrictmsg PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_restrictmsg\"")
target_link_libraries(m_restrictmsg clandestine)
target_include_directories(m_restrictmsg PRIVATE
  ../../include)
install(TARGETS m_restrictmsg DESTINATION lib/clandestine/ircd/modules)

add_library(m_rline MODULE m_rline.cpp)
set_target_properties(m_rline PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_rline\"")
target_link_libraries(m_rline clandestine)
target_include_directories(m_rline PRIVATE
  ../../include)
install(TARGETS m_rline DESTINATION lib/clandestine/ircd/modules)

add_library(m_rmode MODULE m_rmode.cpp)
set_target_properties(m_rmode PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_rmode\"")
target_link_libraries(m_rmode clandestine)
target_include_directories(m_rmode PRIVATE
  ../../include)
install(TARGETS m_rmode DESTINATION lib/clandestine/ircd/modules)

add_library(m_sajoin MODULE m_sajoin.cpp)
set_target_properties(m_sajoin PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sajoin\"")
target_link_libraries(m_sajoin clandestine)
target_include_directories(m_sajoin PRIVATE
  ../../include)
install(TARGETS m_sajoin DESTINATION lib/clandestine/ircd/modules)

add_library(m_sakick MODULE m_sakick.cpp)
set_target_properties(m_sakick PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sakick\"")
target_link_libraries(m_sakick clandestine)
target_include_directories(m_sakick PRIVATE
  ../../include)
install(TARGETS m_sakick DESTINATION lib/clandestine/ircd/modules)

add_library(m_samode MODULE m_samode.cpp)
set_target_properties(m_samode PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_samode\"")
target_link_libraries(m_samode clandestine)
target_include_directories(m_samode PRIVATE
  ../../include)
install(TARGETS m_samode DESTINATION lib/clandestine/ircd/modules)

add_library(m_sanick MODULE m_sanick.cpp)
set_target_properties(m_sanick PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sanick\"")
target_link_libraries(m_sanick clandestine)
target_include_directories(m_sanick PRIVATE
  ../../include)
install(TARGETS m_sanick DESTINATION lib/clandestine/ircd/modules)

add_library(m_sapart MODULE m_sapart.cpp)
set_target_properties(m_sapart PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sapart\"")
target_link_libraries(m_sapart clandestine)
target_include_directories(m_sapart PRIVATE
  ../../include)
install(TARGETS m_sapart DESTINATION lib/clandestine/ircd/modules)

add_library(m_saquit MODULE m_saquit.cpp)
set_target_properties(m_saquit PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_saquit\"")
target_link_libraries(m_saquit clandestine)
target_include_directories(m_saquit PRIVATE
  ../../include)
install(TARGETS m_saquit DESTINATION lib/clandestine/ircd/modules)

add_library(m_sasl MODULE m_sasl.cpp)
set_target_properties(m_sasl PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sasl\"")
target_link_libraries(m_sasl clandestine)
target_include_directories(m_sasl PRIVATE
  ../../include)
install(TARGETS m_sasl DESTINATION lib/clandestine/ircd/modules)

add_library(m_satopic MODULE m_satopic.cpp)
set_target_properties(m_satopic PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_satopic\"")
target_link_libraries(m_satopic clandestine)
target_include_directories(m_satopic PRIVATE
  ../../include)
install(TARGETS m_satopic DESTINATION lib/clandestine/ircd/modules)

add_library(m_securelist MODULE m_securelist.cpp)
set_target_properties(m_securelist PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_securelist\"")
target_link_libraries(m_securelist clandestine)
target_include_directories(m_securelist PRIVATE
  ../../include)
install(TARGETS m_securelist DESTINATION lib/clandestine/ircd/modules)

add_library(m_seenicks MODULE m_seenicks.cpp)
set_target_properties(m_seenicks PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_seenicks\"")
target_link_libraries(m_seenicks clandestine)
target_include_directories(m_seenicks PRIVATE
  ../../include)
install(TARGETS m_seenicks DESTINATION lib/clandestine/ircd/modules)

add_library(m_serverban MODULE m_serverban.cpp)
set_target_properties(m_serverban PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_serverban\"")
target_link_libraries(m_serverban clandestine)
target_include_directories(m_serverban PRIVATE
  ../../include)
install(TARGETS m_serverban DESTINATION lib/clandestine/ircd/modules)

add_library(m_services_account MODULE m_services_account.cpp)
set_target_properties(m_services_account PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_services_account\"")
target_link_libraries(m_services_account clandestine)
target_include_directories(m_services_account PRIVATE
  ../../include)
install(TARGETS m_services_account DESTINATION lib/clandestine/ircd/modules)

add_library(m_servprotect MODULE m_servprotect.cpp)
set_target_properties(m_servprotect PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_servprotect\"")
target_link_libraries(m_servprotect clandestine)
target_include_directories(m_servprotect PRIVATE
  ../../include)
install(TARGETS m_servprotect DESTINATION lib/clandestine/ircd/modules)

add_library(m_sethost MODULE m_sethost.cpp)
set_target_properties(m_sethost PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sethost\"")
target_link_libraries(m_sethost clandestine)
target_include_directories(m_sethost PRIVATE
  ../../include)
install(TARGETS m_sethost DESTINATION lib/clandestine/ircd/modules)

add_library(m_setident MODULE m_setident.cpp)
set_target_properties(m_setident PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_setident\"")
target_link_libraries(m_setident clandestine)
target_include_directories(m_setident PRIVATE
  ../../include)
install(TARGETS m_setident DESTINATION lib/clandestine/ircd/modules)

add_library(m_setidle MODULE m_setidle.cpp)
set_target_properties(m_setidle PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_setidle\"")
target_link_libraries(m_setidle clandestine)
target_include_directories(m_setidle PRIVATE
  ../../include)
install(TARGETS m_setidle DESTINATION lib/clandestine/ircd/modules)

add_library(m_setname MODULE m_setname.cpp)
set_target_properties(m_setname PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_setname\"")
target_link_libraries(m_setname clandestine)
target_include_directories(m_setname PRIVATE
  ../../include)
install(TARGETS m_setname DESTINATION lib/clandestine/ircd/modules)

add_library(m_sha1 MODULE m_sha1.cpp)
set_target_properties(m_sha1 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sha1\"")
target_link_libraries(m_sha1 clandestine)
target_include_directories(m_sha1 PRIVATE
  ../../include)
install(TARGETS m_sha1 DESTINATION lib/clandestine/ircd/modules)

add_library(m_sha256 MODULE m_sha256.cpp)
set_target_properties(m_sha256 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sha256\"")
target_link_libraries(m_sha256 clandestine ${hash-library_LIBRARIES})
target_include_directories(m_sha256 PRIVATE
  ${hash-library_INCLUDE_DIR}
  ../../include)
install(TARGETS m_sha256 DESTINATION lib/clandestine/ircd/modules)

add_library(m_showfile MODULE m_showfile.cpp)
set_target_properties(m_showfile PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_showfile\"")
target_link_libraries(m_showfile clandestine)
target_include_directories(m_showfile PRIVATE
  ../../include)
install(TARGETS m_showfile DESTINATION lib/clandestine/ircd/modules)

add_library(m_showwhois MODULE m_showwhois.cpp)
set_target_properties(m_showwhois PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_showwhois\"")
target_link_libraries(m_showwhois clandestine)
target_include_directories(m_showwhois PRIVATE
  ../../include)
install(TARGETS m_showwhois DESTINATION lib/clandestine/ircd/modules)

add_library(m_shun MODULE m_shun.cpp)
set_target_properties(m_shun PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_shun\"")
target_link_libraries(m_shun clandestine)
target_include_directories(m_shun PRIVATE
  ../../include)
install(TARGETS m_shun DESTINATION lib/clandestine/ircd/modules)

add_library(m_silence MODULE m_silence.cpp)
set_target_properties(m_silence PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_silence\"")
target_link_libraries(m_silence clandestine)
target_include_directories(m_silence PRIVATE
  ../../include)
install(TARGETS m_silence DESTINATION lib/clandestine/ircd/modules)

add_library(m_sqlauth MODULE m_sqlauth.cpp)
set_target_properties(m_sqlauth PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sqlauth\"")
target_link_libraries(m_sqlauth clandestine)
target_include_directories(m_sqlauth PRIVATE
  ../../include)
install(TARGETS m_sqlauth DESTINATION lib/clandestine/ircd/modules)

add_library(m_sqloper MODULE m_sqloper.cpp)
set_target_properties(m_sqloper PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sqloper\"")
target_link_libraries(m_sqloper clandestine)
target_include_directories(m_sqloper PRIVATE
  ../../include)
install(TARGETS m_sqloper DESTINATION lib/clandestine/ircd/modules)

add_library(m_sslinfo MODULE m_sslinfo.cpp)
set_target_properties(m_sslinfo PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sslinfo\"")
target_link_libraries(m_sslinfo clandestine)
target_include_directories(m_sslinfo PRIVATE
  ../../include)
install(TARGETS m_sslinfo DESTINATION lib/clandestine/ircd/modules)

add_library(m_sslmodes MODULE m_sslmodes.cpp)
set_target_properties(m_sslmodes PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sslmodes\"")
target_link_libraries(m_sslmodes clandestine)
target_include_directories(m_sslmodes PRIVATE
  ../../include)
install(TARGETS m_sslmodes DESTINATION lib/clandestine/ircd/modules)

add_library(m_starttls MODULE m_starttls.cpp)
set_target_properties(m_starttls PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_starttls\"")
target_link_libraries(m_starttls clandestine)
target_include_directories(m_starttls PRIVATE
  ../../include)
install(TARGETS m_starttls DESTINATION lib/clandestine/ircd/modules)

add_library(m_stripcolor MODULE m_stripcolor.cpp)
set_target_properties(m_stripcolor PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_stripcolor\"")
target_link_libraries(m_stripcolor clandestine)
target_include_directories(m_stripcolor PRIVATE
  ../../include)
install(TARGETS m_stripcolor DESTINATION lib/clandestine/ircd/modules)

add_library(m_svshold MODULE m_svshold.cpp)
set_target_properties(m_svshold PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_svshold\"")
target_link_libraries(m_svshold clandestine)
target_include_directories(m_svshold PRIVATE
  ../../include)
install(TARGETS m_svshold DESTINATION lib/clandestine/ircd/modules)

add_library(m_swhois MODULE m_swhois.cpp)
set_target_properties(m_swhois PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_swhois\"")
target_link_libraries(m_swhois clandestine)
target_include_directories(m_swhois PRIVATE
  ../../include)
install(TARGETS m_swhois DESTINATION lib/clandestine/ircd/modules)

add_library(m_timedbans MODULE m_timedbans.cpp)
set_target_properties(m_timedbans PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_timedbans\"")
target_link_libraries(m_timedbans clandestine)
target_include_directories(m_timedbans PRIVATE
  ../../include)
install(TARGETS m_timedbans DESTINATION lib/clandestine/ircd/modules)

add_library(m_tline MODULE m_tline.cpp)
set_target_properties(m_tline PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_tline\"")
target_link_libraries(m_tline clandestine)
target_include_directories(m_tline PRIVATE
  ../../include)
install(TARGETS m_tline DESTINATION lib/clandestine/ircd/modules)

add_library(m_topiclock MODULE m_topiclock.cpp)
set_target_properties(m_topiclock PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_topiclock\"")
target_link_libraries(m_topiclock clandestine)
target_include_directories(m_topiclock PRIVATE
  ../../include)
install(TARGETS m_topiclock DESTINATION lib/clandestine/ircd/modules)

add_library(m_uhnames MODULE m_uhnames.cpp)
set_target_properties(m_uhnames PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_uhnames\"")
target_link_libraries(m_uhnames clandestine)
target_include_directories(m_uhnames PRIVATE
  ../../include)
install(TARGETS m_uhnames DESTINATION lib/clandestine/ircd/modules)

add_library(m_uninvite MODULE m_uninvite.cpp)
set_target_properties(m_uninvite PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_uninvite\"")
target_link_libraries(m_uninvite clandestine)
target_include_directories(m_uninvite PRIVATE
  ../../include)
install(TARGETS m_uninvite DESTINATION lib/clandestine/ircd/modules)

add_library(m_userip MODULE m_userip.cpp)
set_target_properties(m_userip PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_userip\"")
target_link_libraries(m_userip clandestine)
target_include_directories(m_userip PRIVATE
  ../../include)
install(TARGETS m_userip DESTINATION lib/clandestine/ircd/modules)

add_library(m_vhost MODULE m_vhost.cpp)
set_target_properties(m_vhost PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_vhost\"")
target_link_libraries(m_vhost clandestine)
target_include_directories(m_vhost PRIVATE
  ../../include)
install(TARGETS m_vhost DESTINATION lib/clandestine/ircd/modules)

add_library(m_watch MODULE m_watch.cpp)
set_target_properties(m_watch PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_watch\"")
target_link_libraries(m_watch clandestine)
target_include_directories(m_watch PRIVATE
  ../../include)
install(TARGETS m_watch DESTINATION lib/clandestine/ircd/modules)

add_library(m_websocket MODULE m_websocket.cpp)
set_target_properties(m_websocket PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_websocket\"")
target_link_libraries(m_websocket clandestine)
target_include_directories(m_websocket PRIVATE
  ${utf8cpp_INCLUDE_DIR}
  ../../include)
install(TARGETS m_websocket DESTINATION lib/clandestine/ircd/modules)

add_library(m_xline_db MODULE m_xline_db.cpp)
set_target_properties(m_xline_db PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_xline_db\"")
target_link_libraries(m_xline_db clandestine)
target_include_directories(m_xline_db PRIVATE
  ../../include)
install(TARGETS m_xline_db DESTINATION lib/clandestine/ircd/modules)
