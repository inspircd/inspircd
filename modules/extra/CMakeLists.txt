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

option(AUTO_ENABLE_EXTRAS "Whether to automatically enable extra modules" ON)
if(AUTO_ENABLE_EXTRAS)
	set(FIND_DEPENDENCY_DRY_RUN YES CACHE BOOL "Whether prospective check for dependencies is currently being performed" FORCE)
	set(FIND_DEPENDENCY_FOUND   NO  CACHE BOOL "Whether the dependencies were found during the last check" FORCE)

	file(GLOB EXTRAS CONFIGURE_DEPENDS "*.cpp")
	foreach(EXTRA IN LISTS EXTRAS)
		cmake_path(GET EXTRA FILENAME EXTRA_FILE)
		set(MODULE_FILE "${PROJECT_SOURCE_DIR}/modules/${EXTRA_FILE}")
		cmake_path(GET EXTRA STEM EXTRA_NAME)
		inline_cmake("m_${EXTRA_NAME}" ${EXTRA})
		if(FIND_DEPENDENCY_FOUND)
			message(STATUS "Automatically enabled the ${EXTRA_NAME} extra module")
			file(CREATE_LINK ${EXTRA} ${MODULE_FILE} COPY_ON_ERROR SYMBOLIC)
		elseif(IS_SYMLINK ${EXTRA_FILE})
			file(REMOVE ${EXTRA_FILE})
		endif()
	endforeach()

	unset(FIND_DEPENDENCY_DRY_RUN CACHE)
	unset(FIND_DEPENDENCY_FOUND   CACHE)
endif()
