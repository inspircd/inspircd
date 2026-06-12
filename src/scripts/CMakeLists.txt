#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2026 Sadie Powell <sadie@sadiepowell.dev>
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

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	message(FATAL_ERROR "You must run CMake using the CMakeLists.txt in the root directory!")
endif()

function(configure_script FILE DIR TYPE)
	file(STRINGS "${FILE}.in" LINES)

	set(CURRENT_STATE TRUE)
	set(PROCESSED_LINES)
	set(STATE_STACK)
	foreach(FILE_LINE IN LISTS LINES)
		if(FILE_LINE MATCHES "^[ \t]*#[ \t]*cmakeifdef[ \t]+([A-Za-z0-9_]+)[\t ]?")
			list(APPEND STATE_STACK "${CURRENT_STATE}")
			if(CURRENT_STATE AND ${CMAKE_MATCH_1})
				set(CURRENT_STATE TRUE)
			else()
				set(CURRENT_STATE FALSE)
			endif()
			continue()
		elseif(FILE_LINE MATCHES "^[ \t]*#[ \t]*cmakeifndef[ \t]+([A-Za-z0-9_]+)[\t ]?")
			list(APPEND STATE_STACK "${CURRENT_STATE}")
			if(CURRENT_STATE AND NOT ${CMAKE_MATCH_1})
				set(CURRENT_STATE TRUE)
			else()
				set(CURRENT_STATE FALSE)
			endif()
			continue()
		elseif(FILE_LINE MATCHES "^[ \t]*#[ \t]*cmakeelse[\t ]?")
			list(GET STATE_STACK -1 PARENT_STATE)
			if(PARENT_STATE AND NOT CURRENT_STATE)
				set(CURRENT_STATE TRUE)
			else()
				set(CURRENT_STATE FALSE)
			endif()
			continue()
		elseif(FILE_LINE MATCHES "^[ \t]*#[ \t]*cmakeendif[\t ]?")
			if(STATE_STACK)
				list(POP_BACK STATE_STACK CURRENT_STATE)
			else()
				set(CURRENT_STATE TRUE)
			endif()
			continue()
		endif()
		if(CURRENT_STATE)
			string(CONFIGURE "${FILE_LINE}" PROCESSED_LINE @ONLY)
			string(APPEND PROCESSED_LINES "${PROCESSED_LINE}\n")
		endif()
	endforeach()

	file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${FILE}" "${PROCESSED_LINES}")

	install_owned(
		${TYPE} "${CMAKE_CURRENT_BINARY_DIR}/${FILE}"
		DESTINATION ${DIR}
		${ARGN}
	)
endfunction()

configure_script("help.txt" ${CONFIG_DIR} FILES)

if(NOT WIN32)
	configure_script("deploy-tls.sh"       ${SCRIPT_DIR} PROGRAMS)
	configure_script("inspircd"            ${SCRIPT_DIR} PROGRAMS)
	configure_script("inspircd-test-tls.1" ${MANUAL_DIR} FILES)
	configure_script("inspircd.1"          ${MANUAL_DIR} FILES)
	configure_script("logrotate"           ${SCRIPT_DIR} FILES)
	if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		configure_script("org.inspircd.plist" ${SCRIPT_DIR} FILES)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		configure_script("apparmor"         ${SCRIPT_DIR} FILES)
		configure_script("inspircd.openrc"  ${SCRIPT_DIR} PROGRAMS)
		configure_script("inspircd.service" ${SCRIPT_DIR} FILES)
	endif()
endif()
