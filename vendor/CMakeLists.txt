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

file(GLOB LIBRARIES CONFIGURE_DEPENDS "*")
foreach(LIBRARY IN LISTS LIBRARIES)
	if(IS_DIRECTORY ${LIBRARY})
		file(GLOB_RECURSE LIBRARY_SOURCES CONFIGURE_DEPENDS
			"${LIBRARY}/*.c"
			"${LIBRARY}/*.cc"
			"${LIBRARY}/*.cpp"
		)
		if(LIBRARY_SOURCES)
			cmake_path(GET LIBRARY FILENAME LIBRARY_NAME)
			set(LIBRARY_TARGET "vendored_${LIBRARY_NAME}")
			add_library(${LIBRARY_TARGET} STATIC ${LIBRARY_SOURCES})
			target_compile_options(${LIBRARY_TARGET} PRIVATE "$<IF:$<BOOL:${MSVC}>,/W0,-w>")
			set_target_properties(${LIBRARY_TARGET} PROPERTIES
				FOLDER "VendoredLibraries"
				PREFIX ""
			)
		endif()
	endif()
endforeach()
