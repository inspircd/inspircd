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


################################################################################
# build_module: Builds a single module.
#
# MODULE (STRING): The name of the module. This will be used as the CMake target
#                  name and (with CMAKE_SHARED_LIBRARY_SUFFIX appended) as the
#                  module's filename.
#
# MODULE_SOURCE(LIST): One or more source files for the module.
function(build_module MODULE MODULE_SOURCE)
	if(NOT ${MODULE} MATCHES "^core_")
		set(MODULE "m_${MODULE}")
	endif()

	add_library(${MODULE} MODULE ${MODULE_SOURCE})
	target_compile_definitions(${MODULE} PRIVATE
		"MODNAME=\"${MODULE}\""
	)
	target_link_libraries(${MODULE} PRIVATE
		"inspircd"
	)
	set_target_properties(${MODULE} PROPERTIES
		APPLE_RESOLVE_SYMBOLS_DYNAMICALLY ON
		FOLDER "Modules"
		PREFIX ""
	)
	list(LENGTH MODULE_SOURCE MODULE_SOURCE_COUNT)
	if(MODULE_SOURCE_COUNT LESS_EQUAL 1)
		set_target_properties(${MODULE} PROPERTIES
			UNITY_BUILD OFF
		)
	endif()
	inline_cmake(${MODULE} ${MODULE_SOURCE})
	install_owned(
		TARGETS ${MODULE}
		DESTINATION "${MODULE_DIR}"
	)
endfunction()

################################################################################
# build_modules: Builds all modules in a directory and any subdirectories.
#
# If a subdirectory contains CMakeLists.txt, add_subdirectory will be called
# on it. If it contains a file called main.cpp or a .cpp file with the same
# name as its directory, build_module will be called on it. Otherwise,
# build_modules (i.e. this function) will be called on it.
#
# MODULE_SOURCE_DIR (STRING): The directory to build modules in.
function(build_modules MODULE_SOURCE_DIR)
	file(GLOB MODULES CONFIGURE_DEPENDS "${MODULE_SOURCE_DIR}/*")
	foreach(MODULE IN LISTS MODULES)
		cmake_path(GET MODULE STEM MODULE_NAME)
		if(IS_DIRECTORY ${MODULE} AND NOT MODULE_NAME IN_LIST ARGN)
			if(EXISTS "${MODULE}/CMakeLists.txt")
				add_subdirectory(${MODULE})
			elseif(EXISTS "${MODULE}/${MODULE_NAME}.cpp" OR EXISTS "${MODULE}/main.cpp")
				file(GLOB_RECURSE MODULE_SOURCE CONFIGURE_DEPENDS
					"${MODULE}/*.cpp"
					"${MODULE}/*.h"
				)
				build_module(${MODULE_NAME} "${MODULE_SOURCE}")
			else()
				build_modules(${MODULE})
			endif()
		elseif(MODULE MATCHES "\\.cpp$")
			build_module(${MODULE_NAME} ${MODULE})
		endif()
	endforeach()
endfunction()

################################################################################
# configure_path: Configures a path based on the path layout options passed to
# CMake.
#
# NAME (STRING): The name of the variable to store the path in. An absolute
#                version of the path is also stored in ABSOLUTE_${NAME}.
#
# SOURCE_DEF (PATH): The default path used on UNIX when neither PORTABLE nor
#                    SYSTEM are set.
#
# PORTABLE_DEF (PATH): The default path used when the PORTABLE flag is set or
#                      when building on Windows.
#
# SYSTEM_DEF (PATH): The default path used when the SYSTEM flag is set.
#
# DESCRIPTION (STRING): A description of the path for the cache entry.
function(configure_path)
	cmake_parse_arguments(PARSE_ARGV 0 "OPT" "" "NAME;SOURCE_DEF;PORTABLE_DEF;SYSTEM_DEF;DESCRIPTION" "")
	if(NOT DEFINED ${OPT_NAME})
		if(PORTABLE OR WIN32)
			set(${OPT_NAME} ${OPT_PORTABLE_DEF} CACHE PATH ${OPT_DESCRIPTION})
		elseif(SYSTEM)
			set(${OPT_NAME} ${OPT_SYSTEM_DEF} CACHE PATH ${OPT_DESCRIPTION})
		else()
			set(${OPT_NAME} ${OPT_SOURCE_DEF} CACHE PATH ${OPT_DESCRIPTION})
		endif()

		if(PORTABLE OR WIN32)
			set(ABSOLUTE_${OPT_NAME} ${${OPT_NAME}})
		else()
			cmake_path(
				APPEND CMAKE_INSTALL_PREFIX "${${OPT_NAME}}"
				OUTPUT_VARIABLE ABSOLUTE_${OPT_NAME}
			)
			cmake_path(NORMAL_PATH ABSOLUTE_${OPT_NAME})
		endif()
	endif()
	install_owned(
		DIRECTORY
		DESTINATION ${${OPT_NAME}}
	)
	return(PROPAGATE ABSOLUTE_${OPT_NAME} ${OPT_NAME})
endfunction()

################################################################################
# find_id: Retrieves the id and name for a system group or user.
#
# ID (STRING): The name or numeric id to look up. This MUST already exist on the
#              system.
#
# FLAG (STRING): The type of entry to look up; either "g" for a group or "u" for
#                a user.
#
# ID_VAR (STRING): The variable to store the retrieved id in.
#
# NAME_VAR (STRING): The variable to store the retrieved name in.
function(find_id ID FLAG ID_VAR NAME_VAR)
	# `id` has existed since System V so this should always be available.
	find_program(ID_BINARY "id" REQUIRED)
	if(SYSTEM AND NOT ID)
		set(ID 0) # On system-wide installs we default to root ownership.
	endif()
	execute_process(
		COMMAND ${ID_BINARY} "-${FLAG}" ${ID}
		COMMAND_ERROR_IS_FATAL ANY
		OUTPUT_STRIP_TRAILING_WHITESPACE
		OUTPUT_VARIABLE ${ID_VAR}
	)
	execute_process(
		COMMAND ${ID_BINARY} "-${FLAG}n" ${ID}
		COMMAND_ERROR_IS_FATAL ANY
		OUTPUT_STRIP_TRAILING_WHITESPACE
		OUTPUT_VARIABLE ${NAME_VAR}
	)
	return(PROPAGATE ${ID_VAR} ${NAME_VAR})
endfunction()

################################################################################
# inline_cmake: Runs inline CMake code from within a source file.
#
# The inline CMake code is extracted from a triple-commented block beginning
# with "/// BEGIN CMAKE" and ending with "/// END CMAKE". This allows modules
# to have custom build configuration without needing a separate CMakeLists.txt.
#
# TARGET (STRING): The name of the CMake target. This is available to the
#                  target's CMake code, allowing it to call functions like
#                  target_link_libraries on itself.
#
# FILE (STRING): The path to the source file from which to parse inline CMake
#                code.
function(inline_cmake TARGET FILE)
	file(STRINGS ${FILE} SRC)
	set(CODE "")
	set(IN_CODE OFF)
	foreach(LINE IN LISTS SRC)
		if(IN_CODE)
			string(REGEX REPLACE "^/// " "" CLEAN_LINE ${LINE})
			if(CLEAN_LINE MATCHES "^END CMAKE$")
				cmake_language(EVAL CODE "${CODE}")
				set(CODE "")
				set(IN_CODE OFF)
			else()
				set(CODE "${CODE}\n${CLEAN_LINE}")
			endif()
		elseif(LINE MATCHES "^/// BEGIN CMAKE$")
			set(IN_CODE ON)
		endif()
	endforeach()
endfunction()

################################################################################
# install_owned: Installs a target with the destination owned by the group and
# user specified in GID and UID.
#
# DESTINATION (STRING): The location that the targt will be installed to.
function(install_owned)
	install(${ARGN})

	if(DEFINED GROUP_ID OR DEFINED USER_ID)
		cmake_parse_arguments(PARSE_ARGV 0 "OPT" "" "DESTINATION" "")

		# `chown` has existed since Unix V1 so this should always be available.
		find_program(CHOWN_BINARY "chown" REQUIRED)

		# Build the owner for the installed file.
		set(OWNER "${USER_NAME}")
		if(GROUP_NAME)
			string(APPEND OWNER ":${GROUP_NAME}")
		endif()

		# Build the destination of the installed file. We need to make sure to
		# respect DESTDIR so changing the owner still works when packaging.
		cmake_path(
			ABSOLUTE_PATH OPT_DESTINATION
			BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
			OUTPUT_VARIABLE OPT_DESTINATION
		)
		string(PREPEND OPT_DESTINATION "\${DESTDIR}")

		install(CODE "
			execute_process(
				COMMAND ${CHOWN_BINARY} -R \"${OWNER}\" \"${OPT_DESTINATION}\"
				ERROR_STRIP_TRAILING_WHITESPACE
				ERROR_VARIABLE CHOWN_ERROR
				RESULT_VARIABLE CHOWN_RESULT
			)
			if(CHOWN_RESULT)
				message(FATAL_ERROR \"Unable to set owner of ${OPT_DESTINATION} to ${OWNER} - \${CHOWN_ERROR}\")
			endif()
		")
	endif()
endfunction()

################################################################################
# update_cache: Updates the value of a cache variable whilst keeping the same
# type and description.
#
# VAR_NAME (STRING): The the cache variable to update.
#
# VAR_VALUE (STRING): The new value for the cache variable.
function(update_cache VAR_NAME VAR_VALUE)
	get_property(OLD_DESC CACHE ${VAR_NAME} PROPERTY HELPSTRING)
	get_property(OLD_TYPE CACHE ${VAR_NAME} PROPERTY TYPE)
	if(NOT OLD_TYPE)
		set(OLD_TYPE "STRING") # Fall back to string
	endif()
	set(${VAR_NAME} "${VAR_VALUE}" CACHE ${OLD_TYPE} "${OLD_DESC}" FORCE)
endfunction()

################################################################################
# target_report_error: Enables a CMake target to report an error that is treated
# as non-fatal during a dry-run, but becomes fatal whilst trying to build the
# target.
#
# TARGET (STRING): The name of the CMake target to which the error refers. This
#                  is used for diagnostic reporting and does not need to exist
#                  yet.
#
# MESSAGE (STRING): The specific error message to report.
function(target_report_error TARGET MESSAGE)
	if(FIND_DEPENDENCY_DRY_RUN)
		update_cache(FIND_DEPENDENCY_FOUND NO)
		set(MESSAGE_TYPE "STATUS")
	else()
		set(MESSAGE_TYPE "FATAL_ERROR")
	endif()
	message(${MESSAGE_TYPE} "${TARGET}: ${MESSAGE}")
endfunction()

################################################################################
# target_require_package: Finds a package that a target depends on and links
# to it.
#
# On Windows, this uses find_package to try and find the package in the Conan
# cache. On other platforms, it uses pkg-config to try and find the package
# from the system package manager.
#
# PKGCONF_NAMES (STRING): A space-delimited list of pkg-config package names for
# use on non-Windows systems. The first available one is used.
#
# CMAKE_NAME (STRING): The CMake package name for use on Windows systems.
#
# LINK_TARGET (STRING): The name of the CMake import target to link against the
# module if the package is available. For pkg-config, this is an alias target to
# the pkg-config target; on Windows, this is the target provided by Conan.
function(target_require_package TARGET PKGCONF_NAMES CMAKE_NAME LINK_TARGET)
	if(NOT TARGET ${LINK_TARGET})
		if(WIN32)
			find_package(${CMAKE_NAME})
		elseif(PkgConfig_FOUND)
			string(REPLACE " " ";" PKGCONF_NAMES "${PKGCONF_NAMES}")
			foreach(PKGCONF_NAME IN LISTS PKGCONF_NAMES)
				pkg_check_modules(${PKGCONF_NAME} IMPORTED_TARGET ${PKGCONF_NAME})
				if(${PKGCONF_NAME}_FOUND)
					add_library(${LINK_TARGET} ALIAS PkgConfig::${PKGCONF_NAME})
					break()
				endif()
			endforeach()
		endif()
	endif()

	if(TARGET ${LINK_TARGET})
		update_cache(FIND_DEPENDENCY_FOUND YES)
		if(NOT FIND_DEPENDENCY_DRY_RUN)
			target_link_libraries(${TARGET} PRIVATE ${LINK_TARGET})
		endif()
	else()
		update_cache(FIND_DEPENDENCY_FOUND NO)
		if(WIN32)
			target_report_error(${TARGET} "Unable to find ${CMAKE_NAME} with find_package!")
		else()
			target_report_error(${TARGET} "Unable to find ${PKGCONF_NAMES} with pkg-config!")
		endif()
	endif()
endfunction()
