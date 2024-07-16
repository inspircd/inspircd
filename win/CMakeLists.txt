#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2019 Robby <robby@chatbelgie.be>
#   Copyright (C) 2018-2019, 2021-2024 Sadie Powell <sadie@witchery.services>
#   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
#   Copyright (C) 2013-2014 Adam <Adam@anope.org>
#
# This file is part of InspIRCd.  InspIRCd is free software: you can
# redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, version 2.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project("InspIRCd" CXX)

if(NOT WIN32)
	message(FATAL_ERROR "CMake can only be used on Windows!")
endif()

set(CONFIG_DIR "conf" CACHE PATH "Configuration path")
set(MODULE_DIR "modules" CACHE PATH "Module path")
set(DATA_DIR "data" CACHE PATH "Data path")
set(LOG_DIR "logs" CACHE PATH "Log path")

# Find the root of the source directory
cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH INSPIRCD_BASE)

# Build with multiple processes
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

# Use our own NSIS template
set(CMAKE_MODULE_PATH "${INSPIRCD_BASE}/win")

# Require C++17
set(CMAKE-CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD "20")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Put modules in their own folder
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_compile_definitions(
	"_WIN32_WINNT=0x0A00"      # Set the minimum OS to Windows 10
	"FD_SETSIZE=24000"         # Increase the default socket limit to something reasonable
	"NOMINMAX"                 # Avoid the Windows API breaking std::{min,max}
	"NTDDI_VERSION=0x0A000005" # Set the minimum OS to Windows 10 1803 "Redstone 4"
	"WIN32_LEAN_AND_MEAN"      # Trim down the size of the included headers.
)

add_compile_options("/utf-8")

include_directories("${INSPIRCD_BASE}/win" "${INSPIRCD_BASE}/include")
include_directories(SYSTEM "${INSPIRCD_BASE}/vendor")

# win32memory is statically linked into every target later
add_library("win32memory" STATIC "${INSPIRCD_BASE}/win/win32memory.cpp")

# Grab version info from version.sh
file(STRINGS "${INSPIRCD_BASE}/src/version.sh" VERSIONSH)
string(REGEX REPLACE ".*InspIRCd-([0-9]*).*" "\\1" VERSION_MAJOR "${VERSIONSH}")
string(REGEX REPLACE ".*InspIRCd-[0-9]*\\.([0-9]*).*" "\\1" VERSION_MINOR "${VERSIONSH}")
string(REGEX REPLACE ".*InspIRCd-[0-9]*\\.[0-9]*\\.([0-9]*).*" "\\1" VERSION_PATCH "${VERSIONSH}")
string(REGEX REPLACE ".*InspIRCd-([^\"]+).*" "\\1" VERSION_FULL "${VERSIONSH}")

# Preconfigure files that the InspIRCd build needs.
configure_file("${INSPIRCD_BASE}/make/template/config.h" "${INSPIRCD_BASE}/include/config.h")
configure_file("${INSPIRCD_BASE}/win/inspircd.rc.cmake" "${INSPIRCD_BASE}/win/inspircd.rc")

file(GLOB INSPIRCD_HEADERS
	"${INSPIRCD_BASE}/include/*.h"
	"${INSPIRCD_BASE}/include/modules/*.h"
	"${INSPIRCD_BASE}/include/utility/*.h"
)
list(SORT INSPIRCD_HEADERS)

file(GLOB INSPIRCD_SOURCES
	"${INSPIRCD_BASE}/src/*.cpp"
	"${INSPIRCD_BASE}/src/socketengines/select.cpp"
	"${INSPIRCD_BASE}/win/win32wrapper.cpp"
	"${INSPIRCD_BASE}/win/win32service.cpp"
)
list(SORT INSPIRCD_SOURCES)

# The main server executable
add_executable("inspircd" ${INSPIRCD_HEADERS} ${INSPIRCD_SOURCES} "${INSPIRCD_BASE}/win/inspircd.rc")
add_dependencies("inspircd" "win32memory")
target_compile_definitions("inspircd" PRIVATE "FMT_LIB_EXPORT" "INSPIRCD_CORE")
set_target_properties("inspircd" PROPERTIES "ENABLE_EXPORTS" ON)
target_link_libraries("inspircd" "win32memory")
install(TARGETS "inspircd" RUNTIME DESTINATION ".")

# Also build modules
add_subdirectory("modules")

# Package any DLLs from Conan
file(GLOB EXTRA_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/*.dll")
install(FILES ${EXTRA_DLLS} DESTINATION ".")

# Install example configs
install(DIRECTORY "${INSPIRCD_BASE}/docs/conf/" DESTINATION "${CONFIG_DIR}/examples")
install(DIRECTORY "${INSPIRCD_BASE}/docs/sql/" DESTINATION "${CONFIG_DIR}/sql")

# Create an empty data and logs directory and install them
file(MAKE_DIRECTORY ${DATA_DIR})
install(DIRECTORY ${DATA_DIR} DESTINATION .)
file(MAKE_DIRECTORY ${LOG_DIR})
install(DIRECTORY ${LOG_DIR} DESTINATION .)

if(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
	# Place runtime libraries next to the InspIRCd binary
	set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
	include("InstallRequiredSystemLibraries")

	set(CPACK_PACKAGE_NAME "InspIRCd -- Internet Relay Chat Daemon")
	set(CPACK_PACKAGE_VENDOR "InspIRCd Team")
	set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
	set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
	set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
	set(CPACK_PACKAGE_FILE_NAME "InspIRCd-${VERSION_FULL}")
	set(CPACK_RESOURCE_FILE_LICENSE "${INSPIRCD_BASE}/docs/LICENSE.txt")

	set(CPACK_GENERATOR "NSIS")
	set(CPACK_PACKAGE_INSTALL_DIRECTORY "InspIRCd")
	# NSIS has a bug with full nix paths, so this must contain at least one backslash
	set(CPACK_PACKAGE_ICON "${INSPIRCD_BASE}/win\\\\inspircd.ico")
	set(CPACK_NSIS_MUI_ICON "${INSPIRCD_BASE}/win\\\\inspircd.ico")
	set(CPACK_NSIS_MUI_UNIICON "${INSPIRCD_BASE}/win\\\\inspircd.ico")
	set(CPACK_NSIS_INSTALLED_ICON_NAME "inspircd.exe")
	set(CPACK_NSIS_URL_INFO_ABOUT "https://www.inspircd.org")
	set(CPACK_NSIS_COMPRESSOR "/SOLID zlib")

	include("CPack")
endif()
