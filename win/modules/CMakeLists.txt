# m_regex_stdlib is supported by every version of Visual Studio we support,
# so copy the file out of extra/
file(COPY "${INSPIRCD_BASE}/src/modules/extra/m_regex_stdlib.cpp" DESTINATION "${INSPIRCD_BASE}/src/modules/")

file(GLOB INSPIRCD_MODULES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${INSPIRCD_BASE}/src/commands/*.cpp" "${INSPIRCD_BASE}/src/modules/*.cpp")
list(SORT INSPIRCD_MODULES)

add_definitions("-DDLL_BUILD")

foreach(MODULE_NAME ${INSPIRCD_MODULES})
	string(REGEX REPLACE "^.*[/\\](.*).cpp$" "\\1.so" SO_NAME ${MODULE_NAME})
	add_library(${SO_NAME} MODULE ${MODULE_NAME})
	set_target_properties(${SO_NAME} PROPERTIES PREFIX "" SUFFIX "")
	target_link_libraries(${SO_NAME} inspircd)
	add_dependencies(${SO_NAME} inspircd)
	if(MSVC)
		target_link_libraries(${SO_NAME} win32_memory)
		add_dependencies(${SO_NAME} win32_memory)
	endif(MSVC)
	install(TARGETS ${SO_NAME} DESTINATION modules)
endforeach(MODULE_NAME ${INSPIRCD_MODULES})

file(GLOB INSPIRCD_MODULES_SPANNINGTREE RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${INSPIRCD_BASE}/src/modules/m_spanningtree/*.cpp")
list(SORT INSPIRCD_MODULES_SPANNINGTREE)

add_library(m_spanningtree.so MODULE ${INSPIRCD_MODULES_SPANNINGTREE})
set_target_properties(m_spanningtree.so PROPERTIES PREFIX "" SUFFIX "")
target_link_libraries(m_spanningtree.so inspircd)
add_dependencies(m_spanningtree.so inspircd)
if(MSVC)
	target_link_libraries(m_spanningtree.so win32_memory)
	add_dependencies(m_spanningtree.so win32_memory)
endif(MSVC)
install(TARGETS m_spanningtree.so DESTINATION modules)