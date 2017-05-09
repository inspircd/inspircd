cmake_minimum_required(VERSION 2.8)

project(InspIRCd CXX)

set(CONF_PATH "conf" CACHE PATH "Configuration file path")
set(MODULE_PATH "modules" CACHE PATH "Module path")
set(DATA_PATH "data" CACHE PATH "Data path")
set(LOG_PATH "logs" CACHE PATH "Log file path")

set(EXTRA_INCLUDES "" CACHE PATH "Extra include paths")
set(EXTRA_LIBS "" CACHE PATH "Extra library paths")

set(INSPIRCD_BASE "${CMAKE_CURRENT_SOURCE_DIR}/../")

# Use our own NSIS template
set(CMAKE_MODULE_PATH "${INSPIRCD_BASE}/win")

# Grab version info from version.sh
file(STRINGS "${INSPIRCD_BASE}/src/version.sh" VERSIONSH)
string(REGEX REPLACE ".*InspIRCd-([0-9]*).*" "\\1" MAJOR_VERSION "${VERSIONSH}")
string(REGEX REPLACE ".*InspIRCd-[0-9]*\\.([0-9]*).*" "\\1" MINOR_VERSION "${VERSIONSH}")
string(REGEX REPLACE ".*InspIRCd-[0-9]*\\.[0-9]*\\.([0-9]*).*" "\\1" PATCH_VERSION "${VERSIONSH}")
set(FULL_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")

if(MSVC)
	# Without /SAFESEH:NO old libraries compiled with VS 2010 or older won't link correctly to VS2012 (eg, extra module libs)
	set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /SAFESEH:NO")
	set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} /SAFESEH:NO")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /SAFESEH:NO")
	set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /SAFESEH:NO")
	set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /SAFESEH:NO")
	set(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL "${CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL} /SAFESEH:NO")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /SAFESEH:NO")
	set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /SAFESEH:NO")
	set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} /SAFESEH:NO")
	set(CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL "${CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL} /SAFESEH:NO")
	set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /SAFESEH:NO")
	set(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO} /SAFESEH:NO")
endif(MSVC)

file(GLOB INSPIRCD_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
"${INSPIRCD_BASE}/win/inspircd_win32wrapper.cpp"
"${INSPIRCD_BASE}/win/win32service.cpp" "${INSPIRCD_BASE}/src/*.cpp"
"${INSPIRCD_BASE}/src/modes/*.cpp"
"${INSPIRCD_BASE}/src/socketengines/socketengine_select.cpp"
"${INSPIRCD_BASE}/src/threadengines/threadengine_win32.cpp")
list(SORT INSPIRCD_SOURCES)

include_directories("${INSPIRCD_BASE}/win" "${INSPIRCD_BASE}/include")

include_directories(${EXTRA_INCLUDES})
link_directories(${EXTRA_LIBS})

if(MSVC)
	add_library(win32_memory STATIC "${INSPIRCD_BASE}/win/inspircd_memory_functions.cpp")
endif(MSVC)

configure_file("${INSPIRCD_BASE}/win/inspircd.rc.cmake" "${INSPIRCD_BASE}/win/inspircd.rc")
configure_file("${INSPIRCD_BASE}/win/inspircd_version.h.cmake" "${INSPIRCD_BASE}/win/inspircd_version.h")
configure_file("${INSPIRCD_BASE}/win/inspircd_config.h.cmake" "${INSPIRCD_BASE}/win/inspircd_config.h")

add_executable(inspircd ${INSPIRCD_SOURCES} "${INSPIRCD_BASE}/win/inspircd.rc")
target_link_libraries(inspircd win32_memory)
set_target_properties(inspircd PROPERTIES ENABLE_EXPORTS ON)
install(TARGETS inspircd DESTINATION .)

add_subdirectory(modules)

# Package any DLLs in win/
file(GLOB EXTRA_DLLS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${INSPIRCD_BASE}/win/*.dll")
install(FILES ${EXTRA_DLLS} DESTINATION .)

# Install example configs
file(GLOB_RECURSE EXAMPLE_CONFIGS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${INSPIRCD_BASE}/docs/conf/*.example")
install(FILES ${EXAMPLE_CONFIGS} DESTINATION conf)

# Install nationalchars files
file(GLOB_RECURSE EXAMPLE_LOCALES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${INSPIRCD_BASE}/locales/*")
install(FILES ${EXAMPLE_LOCALES} DESTINATION locales)

# Create an empty data and logs directory and install them
file(MAKE_DIRECTORY ${DATA_PATH})
install(DIRECTORY ${DATA_PATH} DESTINATION .)
file(MAKE_DIRECTORY ${LOG_PATH})
install(DIRECTORY ${LOG_PATH} DESTINATION .)

if(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
	set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".") # place runtime libraries next to InspIRCd binary
	include(InstallRequiredSystemLibraries)

	set(CPACK_PACKAGE_NAME "InspIRCd IRC Daemon")
	set(CPACK_PACKAGE_VENDOR "InspIRCd Development Team")
	set(CPACK_PACKAGE_VERSION_MAJOR ${MAJOR_VERSION})
	set(CPACK_PACKAGE_VERSION_MINOR ${MINOR_VERSION})
	set(CPACK_PACKAGE_VERSION_PATCH ${PATCH_VERSION})
	set(CPACK_PACKAGE_FILE_NAME "InspIRCd-${FULL_VERSION}")
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../docs/COPYING")

	set(CPACK_GENERATOR "NSIS")
	set(CPACK_PACKAGE_INSTALL_DIRECTORY "InspIRCd")
	# NSIS has a bug with full nix paths, so this must contain at least one backslash
	set(CPACK_PACKAGE_ICON "${INSPIRCD_BASE}/win\\\\inspircd.ico")
	set(CPACK_NSIS_MUI_ICON "${INSPIRCD_BASE}/win\\\\inspircd.ico")
	set(CPACK_NSIS_MUI_UNIICON "${INSPIRCD_BASE}/win\\\\inspircd.ico")
	set(CPACK_NSIS_INSTALLED_ICON_NAME "inspircd.exe")
	set(CPACK_NSIS_URL_INFO_ABOUT "http://www.inspircd.org/")
	set(CPACK_NSIS_COMPRESSOR "/SOLID zlib")

	include(CPack)
endif(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
