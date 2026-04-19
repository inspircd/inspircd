#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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

cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project("InspIRCd" LANGUAGES "C" "CXX")

include("CheckFunctionExists")

# Include our custom CMake modules.
list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/src/cmake")
include("inspircd")
include("version")

# Require C++20 for all targets.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This is the C version equivalent to the C++ version we use.
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable -fPIC for all targets.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Enable -fvisibility for all targets.
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

if(NOT CMAKE_BUILD_TYPE)
	message(STATUS "CMAKE_BUILD_TYPE has not been specified; defaulting to Release")
	update_cache(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

if(WIN32)
	add_compile_definitions(
		"_WIN32_WINNT=0x0A00"      # Set the minimum OS to Windows 10.
		"FD_SETSIZE=24000"         # Increase the default socket limit to something reasonable.
		"NOMINMAX"                 # Avoid the Windows API breaking std::{min,max}
		"NTDDI_VERSION=0x0A000005" # Set the minimum OS to Windows 10 1803 "Redstone 4"
		"WIN32_LEAN_AND_MEAN"      # Trim down the size of the included headers.
	)
else()
	# On non-Windows we find a lot of our dependencies with pkg-config.
	find_package("PkgConfig")
	if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
		# On Haiku we need _DEFAULT_SOURCE for common C extensions and libnetwork for creating
		# sockets.
		add_compile_definitions("_DEFAULT_SOURCE")
		link_libraries("network")
	elseif(CMAKE_SYSTEM_NAME MATCHES "SunOS")
		# On Solaris and derivatives we need libsocket for creating sockets.
		link_libraries("socket")
	endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "(^GNU|Clang)$")
	add_compile_options(
		"-Wall" "-Wextra" "-Wfatal-errors" "-Wpedantic"
		"-Wno-unused-parameter"
		"$<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>"
		"$<$<CONFIG:Debug>:-g3;-Werror>"
		"$<$<CONFIG:Release,RelWithDebInfo>:-g1>"
		"$<$<NOT:$<CONFIG:Debug,None>>:-fno-rtti>"
	)
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options("-Wshadow")
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang$")
		add_compile_options("-Wshadow-all" "-Wshorten-64-to-32")
	endif()
elseif(MSVC)
	# On MSVC we need to set the character set to UTF-8 so {fmt} unicode support works.
	add_compile_options("/utf-8")
endif()

if(NOT WIN32)
	option(PORTABLE "Whether to install portably" OFF)
	option(SYSTEM "Whether to install system-wide" OFF)
	if(PORTABLE AND SYSTEM)
		message(FATAL_ERROR "PORTABLE and SYSTEM can not be used together!")
	endif()
	if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
		if(PORTABLE)
			# Portable installs use relative paths for relocatability.
			update_cache(CMAKE_INSTALL_PREFIX "run")
		elseif(SYSTEM)
			# System installs all have paths relative to the root.
			update_cache(CMAKE_INSTALL_PREFIX "/")
		else()
			# If nothing else is specified we install into the source directory.
			update_cache(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/run")
		endif()
	endif()

	option(DISABLE_OWNERSHIP "Whether to install without ownership" OFF)
	if(NOT DISABLE_OWNERSHIP)
		set(GROUP "" CACHE STRING "The name or id of the group to run as")
		if(GROUP)
			find_id("${GROUP}" "g" GROUP_ID GROUP_NAME)
			message(STATUS "InspIRCd will run as group ${GROUP_NAME} (${GROUP_ID})")
		endif()

		set(USER "" CACHE STRING "The name or id of the user to run as")
		if(USER)
			find_id("${USER}" "u" USER_ID USER_NAME)
			message(STATUS "InspIRCd will run as user ${USER_NAME} (${USER_ID})")
		endif()
	endif()
endif()

configure_path(
	NAME         "BINARY_DIR"
	SOURCE_DEF   "bin"
	PORTABLE_DEF "bin"
	SYSTEM_DEF   "usr/bin"
	DESCRIPTION  "The directory to install binary files to"
)
configure_path(
	NAME         "CONFIG_DIR"
	SOURCE_DEF   "conf"
	PORTABLE_DEF "conf"
	SYSTEM_DEF   "etc/inspircd"
	DESCRIPTION  "The directory to read config files from"
)
configure_path(
	NAME         "DATA_DIR"
	SOURCE_DEF   "data"
	PORTABLE_DEF "data"
	SYSTEM_DEF   "var/lib/inspircd"
	DESCRIPTION  "The directory to read and write data files to"
)
configure_path(
	NAME         "EXAMPLE_DIR"
	SOURCE_DEF   "${CONFIG_DIR}/examples"
	PORTABLE_DEF "${CONFIG_DIR}/examples"
	SYSTEM_DEF   "usr/share/doc/inspircd/examples"
	DESCRIPTION  "The directory to install example config files to"
)
configure_path(
	NAME LOG_DIR
	SOURCE_DEF   "logs"
	PORTABLE_DEF "logs"
	SYSTEM_DEF   "var/log/inspircd"
	DESCRIPTION  "The directory to write log files to"
)
configure_path(
	NAME         "MANUAL_DIR"
	SOURCE_DEF   "manuals"
	PORTABLE_DEF "manuals"
	SYSTEM_DEF   "usr/share/man/man1"
	DESCRIPTION  "The directory to install manual files to"
)
configure_path(
	NAME          "MODULE_DIR"
	SOURCE_DEF    "modules"
	PORTABLE_DEF  "modules"
	SYSTEM_DEF    "usr/lib/inspircd"
	DESCRIPTION   "The directory to install modules files to"
)
configure_path(
	NAME          "RUNTIME_DIR"
	SOURCE_DEF    "${DATA_DIR}"
	PORTABLE_DEF  "${DATA_DIR}"
	SYSTEM_DEF    "$<$<NOT:$<PLATFORM_ID:Linux>>:var/>run/inspircd"
	DESCRIPTION   "The directory to write runtime files to"
)
configure_path(
	NAME         "SCRIPT_DIR"
	SOURCE_DEF   "."
	PORTABLE_DEF "."
	SYSTEM_DEF   "/usr/share/inspircd"
	DESCRIPTION  "The directory to install script files to"
)

set(VERSION_FULL "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
if(DEFINED DISTRIBUTION_LABEL)
	# The builder specified an explicit distribution label.
	set(VERSION_LABEL ${DISTRIBUTION_LABEL})
elseif(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/.git")
	# The builder is building from Git, try to get the Git hash.
	find_program(GIT_BINARY "git")
	if(DEFINED GIT_BINARY)
		execute_process(
			COMMAND ${GIT_BINARY} "-C" "${PROJECT_SOURCE_DIR}" "describe" "--tags"
				ERROR_QUIET
				OUTPUT_VARIABLE GIT_DESCRIPTION
				OUTPUT_STRIP_TRAILING_WHITESPACE
				RESULT_VARIABLE GIT_RESULT
		)
		if(NOT GIT_RESULT AND GIT_DESCRIPTION MATCHES "^v[0-9]+\\.[0-9]+\\.[0-9]+([a-zA-Z0-9]*)-[0-9]+-g([a-f0-9]+)$")
			set(VERSION_LABEL ${CMAKE_MATCH_2})
		endif()
	endif()
endif()
if(DEFINED VERSION_LABEL)
	string(APPEND VERSION_FULL "-${VERSION_LABEL}")
endif()

include_directories("include" "${PROJECT_BINARY_DIR}/include")
include_directories(SYSTEM "vendor")
if(WIN32)
	include_directories("src/windows")
endif()

set_property(GLOBAL PROPERTY
	"USE_FOLDERS" ON # Organise targets into folders.
)

# Vendored libraries used by the core and modules.
add_subdirectory("vendor")

# The main inspircd binary and the win32compat library.
add_subdirectory("include")
add_subdirectory("src")

# Modules that can be loaded by the core.
add_subdirectory("modules")

# Miscellaneous data files.
add_subdirectory("docs")
add_subdirectory("tools")
