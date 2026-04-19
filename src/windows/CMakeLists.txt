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

# Generate the resource file for the main binary.
configure_file("inspircd.rc.cmake" "inspircd.rc")
target_sources("inspircd" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/inspircd.rc")

# Shim the differences between Windows and UNIX platforms.
file(GLOB COMPAT_SOURCES CONFIGURE_DEPENDS
	"*.cpp"
	"*.h"
)
add_library("win32compat" STATIC ${COMPAT_SOURCES})

# Package any DLLs from Conan.
file(GLOB EXTRA_DLLS CONFIGURE_DEPENDS "${PROJECT_BINARY_DIR}/extradll/*.dll")
install_owned(
	FILES ${EXTRA_DLLS}
	DESTINATION "${BINARY_DIR}"
)

# Set package metadata
set(CPACK_PACKAGE_NAME              "InspIRCd - Internet Relay Chat Daemon")
set(CPACK_PACKAGE_VENDOR            "InspIRCd Contributors")
set(CPACK_PACKAGE_VERSION_MAJOR     ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR     ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH     ${VERSION_PATCH})
set(CPACK_PACKAGE_FILE_NAME         "InspIRCd-${VERSION_FULL}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "InspIRCd")
set(CPACK_RESOURCE_FILE_LICENSE     "${PROJECT_SOURCE_DIR}/docs/LICENSE.txt")

# Configure NSIS-specific settings.
set(CPACK_GENERATOR       "NSIS")
set(CPACK_NSIS_COMPRESSOR "/SOLID zlib")

set(CPACK_NSIS_INSTALLED_ICON_NAME "${BINARY_DIR}\\\\inspircd.exe")
set(CPACK_NSIS_MUI_ICON            "${CMAKE_CURRENT_SOURCE_DIR}/inspircd.ico")
set(CPACK_NSIS_MUI_UNIICON         "${CMAKE_CURRENT_SOURCE_DIR}/inspircd.ico")

set(CPACK_NSIS_CONTACT        "ircs://irc.teranova.net/inspircd")
set(CPACK_NSIS_HELP_LINK      "https://docs.inspircd.org/support")
set(CPACK_NSIS_URL_INFO_ABOUT "https://www.inspircd.org")

# Install the C runtime alongside InspIRCd.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${BINARY_DIR}")
include("InstallRequiredSystemLibraries")

# CPack has to be included last.
set(CPACK_COMMAND_LINE_OPTIONS "-V")
include("CPack")
