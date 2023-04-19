add_library(m_argon2 MODULE m_argon2.cpp)
set_target_properties(m_argon2 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_argon2\"")
target_link_libraries(m_argon2 clandestine ${argon2_LIBRARIES})
target_include_directories(m_argon2 PRIVATE
  ${argon2_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_argon2 DESTINATION lib/clandestine/ircd/modules/)

add_library(m_asciiswitch MODULE m_asciiswitch.cpp)
set_target_properties(m_asciiswitch PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_asciiswitch\"")
target_link_libraries(m_asciiswitch clandestine)
target_include_directories(m_asciiswitch PRIVATE
  ../../../include)
install(TARGETS m_asciiswitch DESTINATION lib/clandestine/ircd/modules/)

add_library(m_asn MODULE m_asn.cpp)
set_target_properties(m_asn PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_asn\"")
target_link_libraries(m_asn clandestine)
target_include_directories(m_asn PRIVATE
  ../../../include)
install(TARGETS m_asn DESTINATION lib/clandestine/ircd/modules/)

add_library(m_autoaway MODULE m_autoaway.cpp)
set_target_properties(m_autoaway PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_autoaway\"")
target_link_libraries(m_autoaway clandestine)
target_include_directories(m_autoaway PRIVATE
  ../../../include)
install(TARGETS m_autoaway DESTINATION lib/clandestine/ircd/modules/)

add_library(m_autodrop MODULE m_autodrop.cpp)
set_target_properties(m_autodrop PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_autodrop\"")
target_link_libraries(m_autodrop clandestine)
target_include_directories(m_autodrop PRIVATE
  ../../../include)
install(TARGETS m_autodrop DESTINATION lib/clandestine/ircd/modules/)

add_library(m_autokick MODULE m_autokick.cpp)
set_target_properties(m_autokick PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_autokick\"")
target_link_libraries(m_autokick clandestine)
target_include_directories(m_autokick PRIVATE
  ../../../include)
install(TARGETS m_autokick DESTINATION lib/clandestine/ircd/modules/)

add_library(m_bannegate MODULE m_bannegate.cpp)
set_target_properties(m_bannegate PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_bannegate\"")
target_link_libraries(m_bannegate clandestine)
target_include_directories(m_bannegate PRIVATE
  ../../../include)
install(TARGETS m_bannegate DESTINATION lib/clandestine/ircd/modules/)

add_library(m_blockhighlight MODULE m_blockhighlight.cpp)
set_target_properties(m_blockhighlight PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_blockhighlight\"")
target_link_libraries(m_blockhighlight clandestine)
target_include_directories(m_blockhighlight PRIVATE
  ../../../include)
install(TARGETS m_blockhighlight DESTINATION lib/clandestine/ircd/modules/)

add_library(m_blockinvite MODULE m_blockinvite.cpp)
set_target_properties(m_blockinvite PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_blockinvite\"")
target_link_libraries(m_blockinvite clandestine)
target_include_directories(m_blockinvite PRIVATE
  ../../../include)
install(TARGETS m_blockinvite DESTINATION lib/clandestine/ircd/modules/)

add_library(m_changecap MODULE m_changecap.cpp)
set_target_properties(m_changecap PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_changecap\"")
target_link_libraries(m_changecap clandestine)
target_include_directories(m_changecap PRIVATE
  ../../../include)
install(TARGETS m_changecap DESTINATION lib/clandestine/ircd/modules/)

add_library(m_checkbans MODULE m_checkbans.cpp)
set_target_properties(m_checkbans PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_checkbans\"")
target_link_libraries(m_checkbans clandestine)
target_include_directories(m_checkbans PRIVATE
  ../../../include)
install(TARGETS m_checkbans DESTINATION lib/clandestine/ircd/modules/)

add_library(m_clientcheck MODULE m_clientcheck.cpp)
set_target_properties(m_clientcheck PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_clientcheck\"")
target_link_libraries(m_clientcheck clandestine)
target_include_directories(m_clientcheck PRIVATE
  ../../../include)
install(TARGETS m_clientcheck DESTINATION lib/clandestine/ircd/modules/)

add_library(m_close MODULE m_close.cpp)
set_target_properties(m_close PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_close\"")
target_link_libraries(m_close clandestine)
target_include_directories(m_close PRIVATE
  ../../../include)
install(TARGETS m_close DESTINATION lib/clandestine/ircd/modules/)

add_library(m_complete MODULE m_complete.cpp)
set_target_properties(m_complete PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_complete\"")
target_link_libraries(m_complete clandestine)
target_include_directories(m_complete PRIVATE
  ../../../include)
install(TARGETS m_complete DESTINATION lib/clandestine/ircd/modules/)

add_library(m_conn_accounts MODULE m_conn_accounts.cpp)
set_target_properties(m_conn_accounts PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_conn_accounts\"")
target_link_libraries(m_conn_accounts clandestine)
target_include_directories(m_conn_accounts PRIVATE
  ../../../include)
install(TARGETS m_conn_accounts DESTINATION lib/clandestine/ircd/modules/)

add_library(m_conn_banner MODULE m_conn_banner.cpp)
set_target_properties(m_conn_banner PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_conn_banner\"")
target_link_libraries(m_conn_banner clandestine)
target_include_directories(m_conn_banner PRIVATE
  ../../../include)
install(TARGETS m_conn_banner DESTINATION lib/clandestine/ircd/modules/)

add_library(m_conn_join_geoip MODULE m_conn_join_geoip.cpp)
set_target_properties(m_conn_join_geoip PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_conn_join_geoip\"")
target_link_libraries(m_conn_join_geoip clandestine)
target_include_directories(m_conn_join_geoip PRIVATE
  ../../../include)
install(TARGETS m_conn_join_geoip DESTINATION lib/clandestine/ircd/modules/)

add_library(m_conn_join_ident MODULE m_conn_join_ident.cpp)
set_target_properties(m_conn_join_ident PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_conn_join_ident\"")
target_link_libraries(m_conn_join_ident clandestine)
target_include_directories(m_conn_join_ident PRIVATE
  ../../../include)
install(TARGETS m_conn_join_ident DESTINATION lib/clandestine/ircd/modules/)

add_library(m_conn_matchident MODULE m_conn_matchident.cpp)
set_target_properties(m_conn_matchident PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_conn_matchident\"")
target_link_libraries(m_conn_matchident clandestine)
target_include_directories(m_conn_matchident PRIVATE
  ../../../include)
install(TARGETS m_conn_matchident DESTINATION lib/clandestine/ircd/modules/)

add_library(m_conn_require MODULE m_conn_require.cpp)
set_target_properties(m_conn_require PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_conn_require\"")
target_link_libraries(m_conn_require clandestine)
target_include_directories(m_conn_require PRIVATE
  ../../../include)
install(TARGETS m_conn_require DESTINATION lib/clandestine/ircd/modules/)

add_library(m_conn_strictsasl MODULE m_conn_strictsasl.cpp)
set_target_properties(m_conn_strictsasl PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_conn_strictsasl\"")
target_link_libraries(m_conn_strictsasl clandestine)
target_include_directories(m_conn_strictsasl PRIVATE
  ../../../include)
install(TARGETS m_conn_strictsasl DESTINATION lib/clandestine/ircd/modules/)

add_library(m_conn_vhost MODULE m_conn_vhost.cpp)
set_target_properties(m_conn_vhost PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_conn_vhost\"")
target_link_libraries(m_conn_vhost clandestine)
target_include_directories(m_conn_vhost PRIVATE
  ../../../include)
install(TARGETS m_conn_vhost DESTINATION lib/clandestine/ircd/modules/)

add_library(m_custompenalty MODULE m_custompenalty.cpp)
set_target_properties(m_custompenalty PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_custompenalty\"")
target_link_libraries(m_custompenalty clandestine)
target_include_directories(m_custompenalty PRIVATE
  ../../../include)
install(TARGETS m_custompenalty DESTINATION lib/clandestine/ircd/modules/)

add_library(m_customtags MODULE m_customtags.cpp)
set_target_properties(m_customtags PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_customtags\"")
target_link_libraries(m_customtags clandestine)
target_include_directories(m_customtags PRIVATE
  ../../../include)
install(TARGETS m_customtags DESTINATION lib/clandestine/ircd/modules/)

add_library(m_dccblock MODULE m_dccblock.cpp)
set_target_properties(m_dccblock PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_dccblock\"")
target_link_libraries(m_dccblock clandestine)
target_include_directories(m_dccblock PRIVATE
  ../../../include)
install(TARGETS m_dccblock DESTINATION lib/clandestine/ircd/modules/)

add_library(m_defaulttopic MODULE m_defaulttopic.cpp)
set_target_properties(m_defaulttopic PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_defaulttopic\"")
target_link_libraries(m_defaulttopic clandestine)
target_include_directories(m_defaulttopic PRIVATE
  ../../../include)
install(TARGETS m_defaulttopic DESTINATION lib/clandestine/ircd/modules/)

add_library(m_discordnick MODULE m_discordnick.cpp)
set_target_properties(m_discordnick PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_discordnick\"")
target_link_libraries(m_discordnick clandestine)
target_include_directories(m_discordnick PRIVATE
  ../../../include)
install(TARGETS m_discordnick DESTINATION lib/clandestine/ircd/modules/)

add_library(m_eventexec MODULE m_eventexec.cpp)
set_target_properties(m_eventexec PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_eventexec\"")
target_link_libraries(m_eventexec clandestine)
target_include_directories(m_eventexec PRIVATE
  ../../../include)
install(TARGETS m_eventexec DESTINATION lib/clandestine/ircd/modules/)

add_library(m_exmode MODULE m_exmode.cpp)
set_target_properties(m_exmode PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_exmode\"")
target_link_libraries(m_exmode clandestine)
target_include_directories(m_exmode PRIVATE
  ../../../include)
install(TARGETS m_exmode DESTINATION lib/clandestine/ircd/modules/)

add_library(m_extbanbanlist MODULE m_extbanbanlist.cpp)
set_target_properties(m_extbanbanlist PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_extbanbanlist\"")
target_link_libraries(m_extbanbanlist clandestine)
target_include_directories(m_extbanbanlist PRIVATE
  ../../../include)
install(TARGETS m_extbanbanlist DESTINATION lib/clandestine/ircd/modules/)

add_library(m_extbanredirect MODULE m_extbanredirect.cpp)
set_target_properties(m_extbanredirect PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_extbanredirect\"")
target_link_libraries(m_extbanredirect clandestine)
target_include_directories(m_extbanredirect PRIVATE
  ../../../include)
install(TARGETS m_extbanredirect DESTINATION lib/clandestine/ircd/modules/)

add_library(m_extbanregex MODULE m_extbanregex.cpp)
set_target_properties(m_extbanregex PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_extbanregex\"")
target_link_libraries(m_extbanregex clandestine)
target_include_directories(m_extbanregex PRIVATE
  ../../../include)
install(TARGETS m_extbanregex DESTINATION lib/clandestine/ircd/modules/)

add_library(m_fakelist MODULE m_fakelist.cpp)
set_target_properties(m_fakelist PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_fakelist\"")
target_link_libraries(m_fakelist clandestine)
target_include_directories(m_fakelist PRIVATE
  ../../../include)
install(TARGETS m_fakelist DESTINATION lib/clandestine/ircd/modules/)

add_library(m_forceident MODULE m_forceident.cpp)
set_target_properties(m_forceident PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_forceident\"")
target_link_libraries(m_forceident clandestine)
target_include_directories(m_forceident PRIVATE
  ../../../include)
install(TARGETS m_forceident DESTINATION lib/clandestine/ircd/modules/)

add_library(m_geo_maxmind MODULE m_geo_maxmind.cpp)
set_target_properties(m_geo_maxmind PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_geo_maxmind\"")
target_link_libraries(m_geo_maxmind clandestine ${maxminddb_LIBRARIES})
target_include_directories(m_geo_maxmind PRIVATE
  ${maxminddb_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_geo_maxmind DESTINATION lib/clandestine/ircd/modules/)

add_library(m_geocmd MODULE m_geocmd.cpp)
set_target_properties(m_geocmd PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_geocmd\"")
target_link_libraries(m_geocmd clandestine)
target_include_directories(m_geocmd PRIVATE
  ../../../include)
install(TARGETS m_geocmd DESTINATION lib/clandestine/ircd/modules/)

add_library(m_globalmessageflood MODULE m_globalmessageflood.cpp)
set_target_properties(m_globalmessageflood PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_globalmessageflood\"")
target_link_libraries(m_globalmessageflood clandestine)
target_include_directories(m_globalmessageflood PRIVATE
  ../../../include)
install(TARGETS m_globalmessageflood DESTINATION lib/clandestine/ircd/modules/)

add_library(m_hash_gnutls MODULE m_hash_gnutls.cpp)
set_target_properties(m_hash_gnutls PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_hash_gnutls\"")
target_link_libraries(m_hash_gnutls clandestine ${GnuTLS_LIBRARIES})
target_include_directories(m_hash_gnutls PRIVATE
  ${GnuTLS_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_hash_gnutls DESTINATION lib/clandestine/ircd/modules/)

add_library(m_hideidle MODULE m_hideidle.cpp)
set_target_properties(m_hideidle PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_hideidle\"")
target_link_libraries(m_hideidle clandestine)
target_include_directories(m_hideidle PRIVATE
  ../../../include)
install(TARGETS m_hideidle DESTINATION lib/clandestine/ircd/modules/)

add_library(m_identmeta MODULE m_identmeta.cpp)
set_target_properties(m_identmeta PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_identmeta\"")
target_link_libraries(m_identmeta clandestine)
target_include_directories(m_identmeta PRIVATE
  ../../../include)
install(TARGETS m_identmeta DESTINATION lib/clandestine/ircd/modules/)

add_library(m_join0 MODULE m_join0.cpp)
set_target_properties(m_join0 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_join0\"")
target_link_libraries(m_join0 clandestine)
target_include_directories(m_join0 PRIVATE
  ../../../include)
install(TARGETS m_join0 DESTINATION lib/clandestine/ircd/modules/)

add_library(m_joinpartsno MODULE m_joinpartsno.cpp)
set_target_properties(m_joinpartsno PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_joinpartsno\"")
target_link_libraries(m_joinpartsno clandestine)
target_include_directories(m_joinpartsno PRIVATE
  ../../../include)
install(TARGETS m_joinpartsno DESTINATION lib/clandestine/ircd/modules/)

add_library(m_joinpartspam MODULE m_joinpartspam.cpp)
set_target_properties(m_joinpartspam PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_joinpartspam\"")
target_link_libraries(m_joinpartspam clandestine)
target_include_directories(m_joinpartspam PRIVATE
  ../../../include)
install(TARGETS m_joinpartspam DESTINATION lib/clandestine/ircd/modules/)

add_library(m_jumpserver MODULE m_jumpserver.cpp)
set_target_properties(m_jumpserver PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_jumpserver\"")
target_link_libraries(m_jumpserver clandestine)
target_include_directories(m_jumpserver PRIVATE
  ../../../include)
install(TARGETS m_jumpserver DESTINATION lib/clandestine/ircd/modules/)

add_library(m_kill_idle MODULE m_kill_idle.cpp)
set_target_properties(m_kill_idle PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_kill_idle\"")
target_link_libraries(m_kill_idle clandestine)
target_include_directories(m_kill_idle PRIVATE
  ../../../include)
install(TARGETS m_kill_idle DESTINATION lib/clandestine/ircd/modules/)

add_library(m_messagelength MODULE m_messagelength.cpp)
set_target_properties(m_messagelength PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_messagelength\"")
target_link_libraries(m_messagelength clandestine)
target_include_directories(m_messagelength PRIVATE
  ../../../include)
install(TARGETS m_messagelength DESTINATION lib/clandestine/ircd/modules/)

add_library(m_mysql MODULE m_mysql.cpp)
set_target_properties(m_mysql PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_mysql\"")
target_link_libraries(m_mysql clandestine ${mariadb-connector-c_LIBRARIES})
target_include_directories(m_mysql PRIVATE
  ${mariadb-connector-c_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_mysql DESTINATION lib/clandestine/ircd/modules/)

add_library(m_namedstats MODULE m_namedstats.cpp)
set_target_properties(m_namedstats PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_namedstats\"")
target_link_libraries(m_namedstats clandestine)
target_include_directories(m_namedstats PRIVATE
  ../../../include)
install(TARGETS m_namedstats DESTINATION lib/clandestine/ircd/modules/)

add_library(m_nickdelay MODULE m_nickdelay.cpp)
set_target_properties(m_nickdelay PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_nickdelay\"")
target_link_libraries(m_nickdelay clandestine)
target_include_directories(m_nickdelay PRIVATE
  ../../../include)
install(TARGETS m_nickdelay DESTINATION lib/clandestine/ircd/modules/)

add_library(m_nocreate MODULE m_nocreate.cpp)
set_target_properties(m_nocreate PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_nocreate\"")
target_link_libraries(m_nocreate clandestine)
target_include_directories(m_nocreate PRIVATE
  ../../../include)
install(TARGETS m_nocreate DESTINATION lib/clandestine/ircd/modules/)

add_library(m_noidletyping MODULE m_noidletyping.cpp)
set_target_properties(m_noidletyping PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_noidletyping\"")
target_link_libraries(m_noidletyping clandestine)
target_include_directories(m_noidletyping PRIVATE
  ../../../include)
install(TARGETS m_noidletyping DESTINATION lib/clandestine/ircd/modules/)

add_library(m_noprivatemode MODULE m_noprivatemode.cpp)
set_target_properties(m_noprivatemode PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_noprivatemode\"")
target_link_libraries(m_noprivatemode clandestine)
target_include_directories(m_noprivatemode PRIVATE
  ../../../include)
install(TARGETS m_noprivatemode DESTINATION lib/clandestine/ircd/modules/)

add_library(m_nouidnick MODULE m_nouidnick.cpp)
set_target_properties(m_nouidnick PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_nouidnick\"")
target_link_libraries(m_nouidnick clandestine)
target_include_directories(m_nouidnick PRIVATE
  ../../../include)
install(TARGETS m_nouidnick DESTINATION lib/clandestine/ircd/modules/)

add_library(m_opmoderated MODULE m_opmoderated.cpp)
set_target_properties(m_opmoderated PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_opmoderated\"")
target_link_libraries(m_opmoderated clandestine)
target_include_directories(m_opmoderated PRIVATE
  ../../../include)
install(TARGETS m_opmoderated DESTINATION lib/clandestine/ircd/modules/)

add_library(m_owoifier MODULE m_owoifier.cpp)
set_target_properties(m_owoifier PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_owoifier\"")
target_link_libraries(m_owoifier clandestine)
target_include_directories(m_owoifier PRIVATE
  ../../../include)
install(TARGETS m_owoifier DESTINATION lib/clandestine/ircd/modules/)

add_library(m_pgsql MODULE m_pgsql.cpp)
set_target_properties(m_pgsql PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_pgsql\"")
target_link_libraries(m_pgsql clandestine ${PostgreSQL_LIBRARIES})
target_include_directories(m_pgsql PRIVATE
  ${PostgreSQL_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_pgsql DESTINATION lib/clandestine/ircd/modules/)

add_library(m_qrcode MODULE m_qrcode.cpp)
set_target_properties(m_qrcode PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_qrcode\"")
target_link_libraries(m_qrcode clandestine ${libqrencode_LIBRARIES})
target_include_directories(m_qrcode PRIVATE
  ${libqrencode_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_qrcode DESTINATION lib/clandestine/ircd/modules/)

add_library(m_randomnotice MODULE m_randomnotice.cpp)
set_target_properties(m_randomnotice PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_randomnotice\"")
target_link_libraries(m_randomnotice clandestine)
target_include_directories(m_randomnotice PRIVATE
  ../../../include)
install(TARGETS m_randomnotice DESTINATION lib/clandestine/ircd/modules/)

add_library(m_regex_pcre MODULE m_regex_pcre.cpp)
set_target_properties(m_regex_pcre PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_regex_pcre\"")
target_link_libraries(m_regex_pcre clandestine ${pcre_LIBRARIES})
target_include_directories(m_regex_pcre PRIVATE
  ${pcre_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_regex_pcre DESTINATION lib/clandestine/ircd/modules/)

add_library(m_regex_pcre2 MODULE m_regex_pcre2.cpp)
set_target_properties(m_regex_pcre2 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_regex_pcre2\"")
target_link_libraries(m_regex_pcre2 clandestine ${PCRE2_LIBRARIES})
target_include_directories(m_regex_pcre2 PRIVATE
  ${PCRE2_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_regex_pcre2 DESTINATION lib/clandestine/ircd/modules/)

add_library(m_regex_posix MODULE m_regex_posix.cpp)
set_target_properties(m_regex_posix PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_regex_posix\"")
target_link_libraries(m_regex_posix clandestine clandestine_tre)
target_include_directories(m_regex_posix PRIVATE
  ../../../../lib/tre
  ../../../include)
install(TARGETS m_regex_posix DESTINATION lib/clandestine/ircd/modules/)

add_library(m_regex_re2 MODULE m_regex_re2.cpp)
set_target_properties(m_regex_re2 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_regex_re2\"")
target_link_libraries(m_regex_re2 clandestine ${re2_LIBRARIES})
target_include_directories(m_regex_re2 PRIVATE
  ${re2_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_regex_re2 DESTINATION lib/clandestine/ircd/modules/)

add_library(m_regex_stdlib MODULE m_regex_stdlib.cpp)
set_target_properties(m_regex_stdlib PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_regex_stdlib\"")
target_link_libraries(m_regex_stdlib clandestine)
target_include_directories(m_regex_stdlib PRIVATE
  ../../../include)
install(TARGETS m_regex_stdlib DESTINATION lib/clandestine/ircd/modules/)

add_library(m_regex_tre MODULE m_regex_tre.cpp)
set_target_properties(m_regex_tre PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_regex_tre\"")
target_link_libraries(m_regex_tre clandestine clandestine_tre)
target_include_directories(m_regex_tre PRIVATE
  ../../../../lib/tre
  ../../../include)
install(TARGETS m_regex_tre DESTINATION lib/clandestine/ircd/modules/)

add_library(m_relaymsg MODULE m_relaymsg.cpp)
set_target_properties(m_relaymsg PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_relaymsg\"")
target_link_libraries(m_relaymsg clandestine)
target_include_directories(m_relaymsg PRIVATE
  ../../../include)
install(TARGETS m_relaymsg DESTINATION lib/clandestine/ircd/modules/)

add_library(m_require_auth MODULE m_require_auth.cpp)
set_target_properties(m_require_auth PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_require_auth\"")
target_link_libraries(m_require_auth clandestine)
target_include_directories(m_require_auth PRIVATE
  ../../../include)
install(TARGETS m_require_auth DESTINATION lib/clandestine/ircd/modules/)

add_library(m_restrictmsg_duration MODULE m_restrictmsg_duration.cpp)
set_target_properties(m_restrictmsg_duration PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_restrictmsg_duration\"")
target_link_libraries(m_restrictmsg_duration clandestine)
target_include_directories(m_restrictmsg_duration PRIVATE
  ../../../include)
install(TARGETS m_restrictmsg_duration DESTINATION lib/clandestine/ircd/modules/)

add_library(m_roleplay MODULE m_roleplay.cpp)
set_target_properties(m_roleplay PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_roleplay\"")
target_link_libraries(m_roleplay clandestine)
target_include_directories(m_roleplay PRIVATE
  ../../../include)
install(TARGETS m_roleplay DESTINATION lib/clandestine/ircd/modules/)

add_library(m_rotatelog MODULE m_rotatelog.cpp)
set_target_properties(m_rotatelog PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_rotatelog\"")
target_link_libraries(m_rotatelog clandestine)
target_include_directories(m_rotatelog PRIVATE
  ../../../include)
install(TARGETS m_rotatelog DESTINATION lib/clandestine/ircd/modules/)

add_library(m_samove MODULE m_samove.cpp)
set_target_properties(m_samove PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_samove\"")
target_link_libraries(m_samove clandestine)
target_include_directories(m_samove PRIVATE
  ../../../include)
install(TARGETS m_samove DESTINATION lib/clandestine/ircd/modules/)

add_library(m_sha3 MODULE m_sha3.cpp)
set_target_properties(m_sha3 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sha3\"")
target_link_libraries(m_sha3 clandestine ${hash-library_LIBRARIES})
target_include_directories(m_sha3 PRIVATE
  ${hash-library_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_sha3 DESTINATION lib/clandestine/ircd/modules/)

add_library(m_shed_users MODULE m_shed_users.cpp)
set_target_properties(m_shed_users PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_shed_users\"")
target_link_libraries(m_shed_users clandestine)
target_include_directories(m_shed_users PRIVATE
  ../../../include)
install(TARGETS m_shed_users DESTINATION lib/clandestine/ircd/modules/)

add_library(m_slowmode MODULE m_slowmode.cpp)
set_target_properties(m_slowmode PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_slowmode\"")
target_link_libraries(m_slowmode clandestine)
target_include_directories(m_slowmode PRIVATE
  ../../../include)
install(TARGETS m_slowmode DESTINATION lib/clandestine/ircd/modules/)

add_library(m_solvemsg MODULE m_solvemsg.cpp)
set_target_properties(m_solvemsg PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_solvemsg\"")
target_link_libraries(m_solvemsg clandestine)
target_include_directories(m_solvemsg PRIVATE
  ../../../include)
install(TARGETS m_solvemsg DESTINATION lib/clandestine/ircd/modules/)

add_library(m_sqlite3 MODULE m_sqlite3.cpp)
set_target_properties(m_sqlite3 PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sqlite3\"")
target_link_libraries(m_sqlite3 clandestine ${SQLite3_LIBRARIES})
target_include_directories(m_sqlite3 PRIVATE
  ${SQLite3_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_sqlite3 DESTINATION lib/clandestine/ircd/modules/)

add_library(m_ssl_gnutls MODULE m_ssl_gnutls.cpp)
set_target_properties(m_ssl_gnutls PROPERTIES PREFIX ""
 VERSION ${PROJECT_VERSION}
 COMPILE_DEFINITIONS "MODNAME=\"m_ssl_gnutls\"")
target_link_libraries(m_ssl_gnutls clandestine ${GnuTLS_LIBRARIES})
target_include_directories(m_ssl_gnutls PRIVATE
  ${GnuTLS_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_ssl_gnutls DESTINATION lib/clandestine/ircd/modules/)

add_library(m_ssl_mbedtls MODULE m_ssl_mbedtls.cpp)
set_target_properties(m_ssl_mbedtls PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ssl_mbedtls\"")
target_link_libraries(m_ssl_mbedtls ${MbedTLS_LIBRARIES})
target_include_directories(m_ssl_mbedtls PRIVATE
  ${MbedTLS_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_ssl_mbedtls DESTINATION lib/clandestine/ircd/modules/)

add_library(m_ssl_openssl MODULE m_ssl_openssl.cpp)
set_target_properties(m_ssl_openssl PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_ssl_openssl\"")
target_link_libraries(m_ssl_openssl clandestine ${OpenSSL_LIBRARIES})
target_include_directories(m_ssl_openssl PRIVATE
  ${OpenSSL_INCLUDE_DIR}
  ../../../include)
install(TARGETS m_ssl_openssl DESTINATION lib/clandestine/ircd/modules/)

add_library(m_sslrehashsignal MODULE m_sslrehashsignal.cpp)
set_target_properties(m_sslrehashsignal PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_sslrehashsignal\"")
target_link_libraries(m_sslrehashsignal clandestine)
target_include_directories(m_sslrehashsignal PRIVATE
  ../../../include)
install(TARGETS m_sslrehashsignal DESTINATION lib/clandestine/ircd/modules/)

add_library(m_stats_unlinked MODULE m_stats_unlinked.cpp)
set_target_properties(m_stats_unlinked PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_stats_unlinked\"")
target_link_libraries(m_stats_unlinked clandestine)
target_include_directories(m_stats_unlinked PRIVATE
  ../../../include)
install(TARGETS m_stats_unlinked DESTINATION lib/clandestine/ircd/modules/)

add_library(m_svsoper MODULE m_svsoper.cpp)
set_target_properties(m_svsoper PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_svsoper\"")
target_link_libraries(m_svsoper clandestine)
target_include_directories(m_svsoper PRIVATE
  ../../../include)
install(TARGETS m_svsoper DESTINATION lib/clandestine/ircd/modules/)

add_library(m_tag_iphost MODULE m_tag_iphost.cpp)
set_target_properties(m_tag_iphost PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_tag_iphost\"")
target_link_libraries(m_tag_iphost clandestine)
target_include_directories(m_tag_iphost PRIVATE
  ../../../include)
install(TARGETS m_tag_iphost DESTINATION lib/clandestine/ircd/modules/)

add_library(m_teams MODULE m_teams.cpp)
set_target_properties(m_teams PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_teams\"")
target_link_libraries(m_teams clandestine)
target_include_directories(m_teams PRIVATE
  ../../../include)
install(TARGETS m_teams DESTINATION lib/clandestine/ircd/modules/)

add_library(m_telegraf MODULE m_telegraf.cpp)
set_target_properties(m_telegraf PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_telegraf\"")
target_link_libraries(m_telegraf clandestine)
target_include_directories(m_telegraf PRIVATE
  ../../../include)
install(TARGETS m_telegraf DESTINATION lib/clandestine/ircd/modules/)

add_library(m_teststdrpl MODULE m_teststdrpl.cpp)
set_target_properties(m_teststdrpl PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_teststdrpl\"")
target_link_libraries(m_teststdrpl clandestine)
target_include_directories(m_teststdrpl PRIVATE
  ../../../include)
install(TARGETS m_teststdrpl DESTINATION lib/clandestine/ircd/modules/)

add_library(m_tgchange MODULE m_tgchange.cpp)
set_target_properties(m_tgchange PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_tgchange\"")
target_link_libraries(m_tgchange clandestine)
target_include_directories(m_tgchange PRIVATE
  ../../../include)
install(TARGETS m_tgchange DESTINATION lib/clandestine/ircd/modules/)

add_library(m_timedstaticquit MODULE m_timedstaticquit.cpp)
set_target_properties(m_timedstaticquit PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_timedstaticquit\"")
target_link_libraries(m_timedstaticquit clandestine)
target_include_directories(m_timedstaticquit PRIVATE
  ../../../include)
install(TARGETS m_timedstaticquit DESTINATION lib/clandestine/ircd/modules/)

add_library(m_totp MODULE m_totp.cpp)
set_target_properties(m_totp PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_totp\"")
target_link_libraries(m_totp clandestine)
target_include_directories(m_totp PRIVATE
  ../../../include)
install(TARGETS m_totp DESTINATION lib/clandestine/ircd/modules/)

add_library(m_xlinetools MODULE m_xlinetools.cpp)
set_target_properties(m_xlinetools PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_xlinetools\"")
target_link_libraries(m_xlinetools clandestine)
target_include_directories(m_xlinetools PRIVATE
  ../../../include)
install(TARGETS m_xlinetools DESTINATION lib/clandestine/ircd/modules/)

add_library(m_zombie MODULE m_zombie.cpp)
set_target_properties(m_zombie PROPERTIES PREFIX ""
  VERSION ${PROJECT_VERSION}
  COMPILE_DEFINITIONS "MODNAME=\"m_zombie\"")
target_link_libraries(m_zombie clandestine)
target_include_directories(m_zombie PRIVATE
  ../../../include)
install(TARGETS m_zombie DESTINATION lib/clandestine/ircd/modules/)
