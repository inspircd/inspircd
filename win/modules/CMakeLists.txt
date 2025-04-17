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

# The libraries that modules depend on.
set(INSPIRCD_DEPS)

if(DEFINED CMAKE_TOOLCHAIN_FILE)
	function(enable_extra NAME PACKAGE TARGET)
		find_package(${PACKAGE})
		if(${PACKAGE}_FOUND)
			message("Enabling the ${NAME} module")
			list(APPEND INSPIRCD_MODULES "${INSPIRCD_BASE}/src/modules/extra/m_${NAME}.cpp")
			set(INSPIRCD_MODULES ${INSPIRCD_MODULES} PARENT_SCOPE)
			list(APPEND INSPIRCD_DEPS ${TARGET})
			set(INSPIRCD_DEPS ${INSPIRCD_DEPS} PARENT_SCOPE)
		else()
			message("Unable to enable the ${NAME} module (missing library)")
		endif()
	endfunction()

	enable_extra("argon2" "argon2" "argon2::argon2")
	enable_extra("geo_maxmind" "maxminddb" "maxminddb::maxminddb")
	enable_extra("log_json" "yyjson" "yyjson::yyjson")
	enable_extra("mysql" "libmysqlclient" "libmysqlclient::libmysqlclient")
	enable_extra("pgsql" "PostgreSQL" "PostgreSQL::PostgreSQL")
	enable_extra("regex_pcre2" "PCRE2" "pcre2::pcre2")
	enable_extra("regex_posix" "PCRE2" "pcre2::pcre2")
	enable_extra("regex_re2" "re2" "re2::re2")
	enable_extra("ssl_openssl" "OpenSSL" "openssl::openssl")
	enable_extra("sqlite3" "SQLite3" "SQLite::SQLite3")

	# Optional dependency of cloak_sha256
	find_package("libpsl")
	if (libpsl_FOUND)
		list(APPEND INSPIRCD_DEPS "libpsl::libpsl")
	endif()

	file(GLOB EXTRA_DLLS "${CMAKE_BINARY_DIR}/extradll/*.dll")
	install(FILES ${EXTRA_DLLS} DESTINATION .)
else()
	message("Unable to build extras: CMAKE_TOOLCHAIN_FILE is not set to the Conan toolchain!")
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

	# Link against any Conan dependencies
	target_link_libraries(${BASE_NAME} ${INSPIRCD_DEPS})

	set_target_properties(${BASE_NAME} PROPERTIES
		PREFIX ""
		FOLDER "Modules"
		COMPILE_DEFINITIONS "FMT_SHARED;MODNAME=\"${BASE_NAME}\""
	)

	# Set the module to be installed to the module directory
	install(TARGETS ${BASE_NAME} LIBRARY DESTINATION ${MODULE_DIR})
endforeach()
