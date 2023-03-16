cmake_minimum_required(VERSION 3.18.4)

project(clandestine)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 11)
set(PROJECT_VERSION "clandestine-IRCd-3.15.0")
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(utf8cpp REQUIRED)
find_package(openldap)
find_package(http_parser)
find_package(argon2)
find_package(libxcrypt)
find_package(pcre)
find_package(hash-library)
find_package(maxminddb)
find_package(gnutls)
find_package(mariadb-connector-c)
find_package(libqrencode)
find_package(re2)
find_package(pcre2)
find_package(postgresql)
find_package(mbedtls)
find_package(openssl)
find_package(sqlite3)

add_definitions(-DHAVE_LIBUTF8_H=1)
add_definitions(-DHAVE_REG_ERRCODE_T=1)
add_definitions(-DHAVE_SYS_TYPES_H=1)
add_definitions(-DHAVE_WCHAR_H=1)
add_definitions(-DTRE_APPROX=1)
add_definitions(-DTRE_MULTIBYTE=1)
add_definitions(-DTRE_SYSTEM_REGEX_H_PATH="regex.h")
add_definitions(-DTRE_USE_SYSTEM_REGEX_H=1)
add_definitions(-DTRE_WCHAR=1)
add_definitions(-DTRE_VERSION=3)
add_definitions(-DTRE_VERSION_1=1)
add_definitions(-DTRE_VERSION_2=2)
add_definitions(-DTRE_VERSION_3=3)

remove_definitions(-DHAVE_LIBUTF8_H=1)
remove_definitions(-DTRE_USE_SYSTEM_REGEX_H=1)
remove_definitions(-DTRE_MULTIBYTE=1)

add_definitions(-DINSPIRCD_VERSION="${PROJECT_VERSION}")

add_subdirectory(lib/tre)

include_directories(
  include/
  ${argon2_INCLUDE_DIR}
  ${http_parser_INCLUDE_DIR}
  ${libxcrypt_INCLUDE_DIR}
  ${pcre_INCLUDE_DIR}
  ${utf8cpp_INCLUDE_DIR}
  ${hash-library_INCLUDE_DIR}
  ${maxminddb_INCLUDE_DIR}
  ${GnuTLS_INCLUDE_DIR}
  ${mariadb-connector-c_INCLUDE_DIR}
  ${libqrencode_INCLUDE_DIR}
  ${re2_INCLUDE_DIR}
  ${PCRE2_INCLUDE_DIR}
  ${PostgreSQL_INCLUDE_DIR}
  ${MbedTLS_INCLUDE_DIR}
  ${OpenSSL_INCLUDE_DIR}
  ${SQLite3_INCLUDE_DIR}
  lib/tre)

add_subdirectory(src/)
