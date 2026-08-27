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

# We need to manually add this first because otherwise the glob in build_modules()
# gets invalidated and forces CMake to re-run when building.
add_subdirectory("extra")

# We need to manually build this first so that contrib modules can override
# vendor modules with the same name.
build_modules("${CMAKE_CURRENT_SOURCE_DIR}/contrib")

build_modules(${CMAKE_CURRENT_SOURCE_DIR}
	SKIP "contrib" "extra"
)
