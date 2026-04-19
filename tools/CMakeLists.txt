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

set(TOOLS
	"convert-unreal-channel" # Converts an UnrealIRCd channel database to InspIRCd permchannels config.
	"convert-unreal-tkl" # Converts an UnrealIRCd TKL database to an InspIRCd xline_db database.
	"test-tls" # Tests whether TLS is correctly configured on an endpoint.
)

foreach(TOOL IN LISTS TOOLS)
	install_owned(
		PROGRAMS "${PROJECT_SOURCE_DIR}/tools/${TOOL}"
		DESTINATION ${BINARY_DIR}
		RENAME "inspircd-${TOOL}"
	)
endforeach()
