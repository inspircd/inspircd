#!/bin/bash
set -v
if [ "$TRAVIS_OS_NAME" = "linux" ]
then
	sudo apt-get update --assume-yes
	sudo apt-get install --assume-yes libgeoip-dev libgnutls-dev libldap2-dev libmysqlclient-dev libpcre3-dev libpq-dev libsqlite3-dev libssl-dev libtre-dev
elif [ "$TRAVIS_OS_NAME" = "osx" ]
then
	brew update
	brew install geoip gnutls mysql-connector-c openssl pcre postgresql sqlite3 tre
	brew link sqlite3 --force
else
	>&2 echo "'$TRAVIS_OS_NAME' is an unknown Travis CI environment!"
	exit 1
fi
set -e
export TEST_BUILD_MODULES="m_geoip.cpp,m_ldap.cpp,m_mysql.cpp,m_pgsql.cpp,m_regex_pcre.cpp,m_regex_posix.cpp,m_regex_tre.cpp,m_sqlite3.cpp,m_ssl_gnutls.cpp,m_ssl_openssl.cpp"
./tools/test-build $CXX
