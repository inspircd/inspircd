# m_regex_stdlib is supported by every version of Visual Studio we support,
# so copy the file out of extra/
file(COPY "${INSPIRCD_BASE}/src/modules/extra/m_regex_stdlib.cpp" DESTINATION "${INSPIRCD_BASE}/src/modules/")

if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
	file(COPY "${INSPIRCD_BASE}/src/modules/extra/m_argon2.cpp" DESTINATION "${INSPIRCD_BASE}/src/modules/")
	file(COPY "${INSPIRCD_BASE}/src/modules/extra/m_pgsql.cpp" DESTINATION "${INSPIRCD_BASE}/src/modules/")
	file(COPY "${INSPIRCD_BASE}/src/modules/extra/m_mysql.cpp" DESTINATION "${INSPIRCD_BASE}/src/modules/")
	file(COPY "${INSPIRCD_BASE}/src/modules/extra/m_regex_pcre.cpp" DESTINATION "${INSPIRCD_BASE}/src/modules/")
	file(COPY "${INSPIRCD_BASE}/src/modules/extra/m_regex_re2.cpp" DESTINATION "${INSPIRCD_BASE}/src/modules/")
	file(COPY "${INSPIRCD_BASE}/src/modules/extra/m_sqlite3.cpp" DESTINATION "${INSPIRCD_BASE}/src/modules/")

	include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
	link_directories("${CMAKE_BINARY_DIR}/extradll" "${CMAKE_BINARY_DIR}/extralib")

	file(GLOB EXTRA_DLLS "${CMAKE_BINARY_DIR}/extradll/*.dll")
	install(FILES ${EXTRA_DLLS} DESTINATION .)

	conan_basic_setup(TARGETS)
else()
	message("Unable to build extras: conanbuildinfo.cmake does not exist in the build directory!")
endif()


file(GLOB INSPIRCD_MODULES "${INSPIRCD_BASE}/src/coremods/core_*" "${INSPIRCD_BASE}/src/modules/m_*")
list(SORT INSPIRCD_MODULES)

add_definitions("-DDLL_BUILD")

foreach(MODULE_NAME ${INSPIRCD_MODULES})
	if(IS_DIRECTORY "${MODULE_NAME}")
		string(REGEX REPLACE "^.*[/\\](.*)$" "\\1" BASE_NAME ${MODULE_NAME})
	else(IS_DIRECTORY "${MODULE_NAME}")
		string(REGEX REPLACE "^.*[/\\](.*).cpp$" "\\1" BASE_NAME ${MODULE_NAME})
	endif(IS_DIRECTORY "${MODULE_NAME}")
	set(SO_NAME "${BASE_NAME}.so")

	if(IS_DIRECTORY "${MODULE_NAME}")
		file(GLOB MODULES_SUBDIR_SRCS "${MODULE_NAME}/*.cpp")
		list(SORT MODULES_SUBDIR_SRCS)
		add_library(${SO_NAME} MODULE ${MODULES_SUBDIR_SRCS})
	else(IS_DIRECTORY "${MODULE_NAME}")
		add_library(${SO_NAME} MODULE ${MODULE_NAME})
	endif(IS_DIRECTORY "${MODULE_NAME}")

	# Generate the module and set its linker flags, also set it to depend on the main executable to be built beforehand
	target_link_libraries(${SO_NAME} inspircd)
	add_dependencies(${SO_NAME} inspircd)
	if(MSVC)
		target_link_libraries(${SO_NAME} win32_memory)
		add_dependencies(${SO_NAME} win32_memory)
	endif(MSVC)

	if(CONAN_CXX_FLAGS)
		conan_target_link_libraries("${SO_NAME}")
	endif()

	set_target_properties(${SO_NAME} PROPERTIES
		PREFIX ""
		SUFFIX ""
		COMPILE_DEFINITIONS "MODNAME=\"${BASE_NAME}\""
	)

	# Set the module to be installed to the module directory
	install(TARGETS ${SO_NAME} DESTINATION ${MODULE_DIR})
endforeach(MODULE_NAME ${INSPIRCD_MODULES})
