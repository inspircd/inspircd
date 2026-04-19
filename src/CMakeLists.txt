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

# Even though we are using <thread> we still need to link against pthreads.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package("Threads" REQUIRED)

# Find our source files.
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB CORE_HEADERS CONFIGURE_DEPENDS
	"${PROJECT_BINARY_DIR}/include/*.h"
	"${PROJECT_SOURCE_DIR}/include/*.h"
)

# Look for all possible socket engines.
check_function_exists("epoll_wait" HAS_EPOLL)
check_function_exists("kqueue"     HAS_KQUEUE)
check_function_exists("poll"       HAS_POLL)
check_function_exists("select"     HAS_SELECT)

set(SOCKET_ENGINE "" CACHE STRING "The socket engine to build with")
if(SOCKET_ENGINE)
	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/socketengines/${SOCKET_ENGINE}.cpp")
		message(FATAL_ERROR "The socket engine you requested does not exist: ${SOCKET_ENGINE}")
	endif()
	list(APPEND CORE_SOURCES "socketengines/${SOCKET_ENGINE}.cpp")
elseif(HAS_EPOLL)
	list(APPEND CORE_SOURCES "socketengines/epoll.cpp")
elseif(HAS_KQUEUE)
	list(APPEND CORE_SOURCES "socketengines/kqueue.cpp")
elseif(HAS_POLL)
	list(APPEND CORE_SOURCES "socketengines/poll.cpp")
elseif(HAS_SELECT OR WIN32)
	list(APPEND CORE_SOURCES "socketengines/select.cpp")
else()
	message(FATAL_ERROR "Your system does not support any socket engines!")
endif()

add_executable("inspircd" ${CORE_SOURCES})
target_compile_definitions("inspircd" PRIVATE
	"FMT_LIB_EXPORT"
	"INSPIRCD_CORE"
)
target_link_libraries("inspircd" PUBLIC
	"vendored_fmt"
	"$<$<BOOL:${WIN32}>:win32compat>"
	${CMAKE_DL_LIBS}
	Threads::Threads
)
target_precompile_headers("inspircd" PRIVATE
	"${PROJECT_SOURCE_DIR}/include/inspircd.h"
	${CORE_HEADERS}
)
set_target_properties("inspircd" PROPERTIES
	ENABLE_EXPORTS ON
)
install_owned(
	TARGETS "inspircd"
	DESTINATION "${BINARY_DIR}"
)

add_subdirectory("scripts")
if(WIN32)
	add_subdirectory("windows")
endif()
