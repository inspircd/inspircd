cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project(InspIRCd CXX)

set(CONFIG_DIR "conf" CACHE PATH "Configuration file path")
set(MODULE_DIR "modules" CACHE PATH "Module path")
set(DATA_DIR "data" CACHE PATH "Data path")
set(LOG_DIR "logs" CACHE PATH "Log file path")

set(INSPIRCD_BASE "${CMAKE_CURRENT_SOURCE_DIR}/../")

# Build with multiple processes
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

# Use our own NSIS template
set(CMAKE_MODULE_PATH "${INSPIRCD_BASE}/win")

# Require C++17
set(CMAKE-CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD "17")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Put modules in their own folder
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_compile_definitions(
	"_WIN32_WINNT=0x0A00"      # Set the minimum OS to Windows 10
	"FD_SETSIZE=24000"         # Increase the default socket limit to something reasonable
	"NOMINMAX"                 # Avoid the Windows API breaking std::{min,max}
	"NTDDI_VERSION=0x0A000005" # Set the minimum OS to Windows 10 1803 "Redstone 4"
	"WIN32_LEAN_AND_MEAN"      # Trim down the size of the included headers.
)

# Grab version info from version.sh
file(STRINGS "${INSPIRCD_BASE}/src/version.sh" VERSIONSH)
string(REGEX REPLACE ".*InspIRCd-([0-9]*).*" "\\1" VERSION_MAJOR "${VERSIONSH}")
string(REGEX REPLACE ".*InspIRCd-[0-9]*\\.([0-9]*).*" "\\1" VERSION_MINOR "${VERSIONSH}")
string(REGEX REPLACE ".*InspIRCd-[0-9]*\\.[0-9]*\\.([0-9]*).*" "\\1" VERSION_PATCH "${VERSIONSH}")
string(REGEX REPLACE ".*InspIRCd-([^\"]+).*" "\\1" VERSION_FULL "${VERSIONSH}")

file(GLOB INSPIRCD_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	"${INSPIRCD_BASE}/src/*.cpp"
	"${INSPIRCD_BASE}/src/socketengines/select.cpp"
	"${INSPIRCD_BASE}/win/inspircd_win32wrapper.cpp"
	"${INSPIRCD_BASE}/win/win32service.cpp"
)
list(SORT INSPIRCD_SOURCES)

include_directories("${INSPIRCD_BASE}/win" "${INSPIRCD_BASE}/include" "${INSPIRCD_BASE}/vendor")

if(MSVC)
	add_library(win32_memory STATIC "${INSPIRCD_BASE}/win/inspircd_memory_functions.cpp")
endif()

configure_file("${INSPIRCD_BASE}/win/inspircd.rc.cmake" "${INSPIRCD_BASE}/win/inspircd.rc")
configure_file("${INSPIRCD_BASE}/make/template/config.h" "${INSPIRCD_BASE}/include/config.h")

add_executable(inspircd ${INSPIRCD_SOURCES} "${INSPIRCD_BASE}/win/inspircd.rc")
target_link_libraries(inspircd win32_memory)
set_target_properties(inspircd PROPERTIES COMPILE_DEFINITIONS "FMT_EXPORT" ENABLE_EXPORTS ON)
install(TARGETS inspircd RUNTIME DESTINATION .)

add_subdirectory(modules)

# Package any DLLs in win/
file(GLOB EXTRA_DLLS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${INSPIRCD_BASE}/win/*.dll")
install(FILES ${EXTRA_DLLS} DESTINATION .)

# Install example configs
install(DIRECTORY "${INSPIRCD_BASE}/docs/conf/" DESTINATION "${CONFIG_DIR}/examples")
install(DIRECTORY "${INSPIRCD_BASE}/docs/sql/" DESTINATION "${CONFIG_DIR}/sql")

# Create an empty data and logs directory and install them
file(MAKE_DIRECTORY ${DATA_DIR})
install(DIRECTORY ${DATA_DIR} DESTINATION .)
file(MAKE_DIRECTORY ${LOG_DIR})
install(DIRECTORY ${LOG_DIR} DESTINATION .)

if(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
	set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".") # place runtime libraries next to InspIRCd binary
	include(InstallRequiredSystemLibraries)

	set(CPACK_PACKAGE_NAME "InspIRCd IRC Daemon")
	set(CPACK_PACKAGE_VENDOR "InspIRCd Development Team")
	set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
	set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
	set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
	set(CPACK_PACKAGE_FILE_NAME "InspIRCd-${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../docs/LICENSE.txt")

	set(CPACK_GENERATOR "NSIS")
	set(CPACK_PACKAGE_INSTALL_DIRECTORY "InspIRCd")
	# NSIS has a bug with full nix paths, so this must contain at least one backslash
	set(CPACK_PACKAGE_ICON "${INSPIRCD_BASE}/win\\\\inspircd.ico")
	set(CPACK_NSIS_MUI_ICON "${INSPIRCD_BASE}/win\\\\inspircd.ico")
	set(CPACK_NSIS_MUI_UNIICON "${INSPIRCD_BASE}/win\\\\inspircd.ico")
	set(CPACK_NSIS_INSTALLED_ICON_NAME "inspircd.exe")
	set(CPACK_NSIS_URL_INFO_ABOUT "https://www.inspircd.org")
	set(CPACK_NSIS_COMPRESSOR "/SOLID zlib")

	include(CPack)
endif()
