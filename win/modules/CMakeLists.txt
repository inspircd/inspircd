#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2021-2024 Sadie Powell <sadie@witchery.services>
#   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
#   Copyright (C) 2013 Adam <Adam@anope.org>
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


# These modules can always be built on all platforms
file(GLOB INSPIRCD_MODULES
	"${INSPIRCD_BASE}/src/coremods/core_*"
	"${INSPIRCD_BASE}/src/modules/m_*")

# These modules have their dependencies provided by Windows
list(APPEND INSPIRCD_MODULES
	"${INSPIRCD_BASE}/src/modules/extra/m_ldap.cpp")

if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
	include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")

	function(enable_extra NAME PACKAGE)
		if(DEFINED "CONAN_${PACKAGE}_ROOT")
			message("Enabling the ${NAME} module")
			list(APPEND INSPIRCD_MODULES "${INSPIRCD_BASE}/src/modules/extra/m_${NAME}.cpp")
			set(INSPIRCD_MODULES ${INSPIRCD_MODULES} PARENT_SCOPE)
		else()
			message("Unable to enable the ${NAME} module (missing library)")
		endif()
	endfunction()

	enable_extra("argon2" "ARGON2")
	enable_extra("geo_maxmind" "LIBMAXMINDDB")
	enable_extra("log_json" "RAPIDJSON")
	enable_extra("mysql" "LIBMYSQLCLIENT")
	enable_extra("pgsql" "LIBPQ")
	enable_extra("regex_pcre2" "PCRE2")
	enable_extra("regex_posix" "PCRE2")
	enable_extra("regex_re2" "RE2")
	enable_extra("ssl_openssl" "OPENSSL")
	enable_extra("sqlite3" "SQLITE3")

	link_directories("${CMAKE_BINARY_DIR}/extradll" "${CMAKE_BINARY_DIR}/extralib")
	file(GLOB EXTRA_DLLS "${CMAKE_BINARY_DIR}/extradll/*.dll")
	install(FILES ${EXTRA_DLLS} DESTINATION .)

	conan_basic_setup(TARGETS)
else()
	message("Unable to build extras: conanbuildinfo.cmake does not exist in the build directory!")
endif()

list(SORT INSPIRCD_MODULES)

foreach(MODULE_NAME ${INSPIRCD_MODULES})
	if(IS_DIRECTORY "${MODULE_NAME}")
		string(REGEX REPLACE "^.*[/\\](.*)$" "\\1" BASE_NAME ${MODULE_NAME})
	else()
		string(REGEX REPLACE "^.*[/\\](.*).cpp$" "\\1" BASE_NAME ${MODULE_NAME})
	endif()

	if(IS_DIRECTORY "${MODULE_NAME}")
		file(GLOB MODULES_SUBDIR_SRCS "${MODULE_NAME}/*.cpp")
		list(SORT MODULES_SUBDIR_SRCS)
		add_library(${BASE_NAME} MODULE ${MODULES_SUBDIR_SRCS})
	else()
		add_library(${BASE_NAME} MODULE ${MODULE_NAME})
	endif()

	# Link against the core and memory library
	add_dependencies(${BASE_NAME} "inspircd" "win32memory")
	target_link_libraries(${BASE_NAME} "inspircd" "win32memory")

	# Link against any dependencies if available
	if(CONAN_CXX_FLAGS)
		conan_target_link_libraries(${BASE_NAME})
	endif()

	set_target_properties(${BASE_NAME} PROPERTIES
		PREFIX ""
		FOLDER "Modules"
		COMPILE_DEFINITIONS "FMT_SHARED;MODNAME=\"${BASE_NAME}\""
	)

	# Set the module to be installed to the module directory
	install(TARGETS ${BASE_NAME} LIBRARY DESTINATION ${MODULE_DIR})
endforeach()
