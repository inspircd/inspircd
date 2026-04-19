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

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	message(FATAL_ERROR "You must run CMake using the CMakeLists.txt in the root directory!")
endif()

function(configure_script FILE DIR TYPE)
	file(STRINGS "${FILE}.in" LINES)
	foreach(FILE_LINE IN LISTS LINES)
		if(FILE_LINE MATCHES "^([\t ]*)%([a-z]+) +([^ ]+) *(.*)\r?$")
			if(CMAKE_MATCH_2 STREQUAL "ifdef" AND ${${CMAKE_MATCH_3}})
				string(APPEND PROCESSED_LINES "${CMAKE_MATCH_1}${CMAKE_MATCH_4}\n")
			elseif(CMAKE_MATCH_2 STREQUAL "ifndef" AND NOT ${${CMAKE_MATCH_3}})
				string(APPEND PROCESSED_LINES "${CMAKE_MATCH_1}${CMAKE_MATCH_4}\n")
			endif()
		else()
			string(APPEND PROCESSED_LINES "${FILE_LINE}\n")
		endif()
	endforeach()

	file(CONFIGURE
		CONTENT "${PROCESSED_LINES}"
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FILE}"
		@ONLY
	)
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
