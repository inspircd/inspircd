#!/bin/bash
set -ev
sudo apt-get update --assume-yes
sudo apt-get install --assume-yes --no-install-recommends libgnutls-dev libldap2-dev libmaxminddb-dev libmbedtls-dev libmysqlclient-dev libpcre3-dev libpq-dev libre2-dev libsqlite3-dev libssl-dev libtre-dev
TEST_BUILD_MODULES="m_geo_maxmind.cpp,m_ldap.cpp,m_mysql.cpp,m_pgsql.cpp,m_regex_pcre.cpp,m_regex_posix.cpp,m_regex_re2.cpp,m_regex_stdlib.cpp,m_regex_tre.cpp,m_sqlite3.cpp,m_ssl_gnutls.cpp,m_ssl_mbedtls.cpp,m_ssl_openssl.cpp,m_sslrehashsignal.cpp"
./tools/test-build $CXX
