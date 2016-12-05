#!/bin/bash
set -v
if [ "$TRAVIS_OS_NAME" = "linux" ]
then
	sudo apt-get update --assume-yes
	sudo apt-get install --assume-yes libgeoip-dev libgnutls-dev libldap2-dev libmysqlclient-dev libpcre3-dev libpq-dev libsqlite3-dev libssl-dev libtre-dev
else
	>&2 echo "'$TRAVIS_OS_NAME' is an unknown Travis CI environment!"
	exit 1
fi
set -e
./configure --enable-extras=m_geoip.cpp,m_ldapauth.cpp,m_ldapoper.cpp,m_mysql.cpp,m_pgsql.cpp,m_regex_pcre.cpp,m_regex_posix.cpp,m_regex_tre.cpp,m_sqlite3.cpp,m_ssl_gnutls.cpp,m_ssl_openssl.cpp
./configure --with-cc=$CXX
make -j4 install
./run/bin/inspircd --version
