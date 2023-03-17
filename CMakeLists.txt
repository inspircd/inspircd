cmake_minimum_required(VERSION 3.18.4)

project(clandestine)

set(CMAKE_CXX_STANDARD 11)
set(PROJECT_VERSION "clandestine-IRCd-3.15.0")
set(CMAKE_BUILD_TYPE Release)

set(VERSION_MAJOR 3)
set(VERSION_MINOR 15)
set(VERSION_PATCH 0)
set(VERSION_EXTRA "none")
set(BUILD 0)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(lib/tre)
add_subdirectory(lib/hash)
add_subdirectory(ircd/src/)
add_subdirectory(ircd/src/coremods/)
add_subdirectory(ircd/src/modules/)
add_subdirectory(servicesd/src)
add_subdirectory(servicesd/modules)
