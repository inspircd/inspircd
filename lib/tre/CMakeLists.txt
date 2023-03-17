add_definitions(-DHAVE_LIBUTF8_H=1)
add_definitions(-DHAVE_REG_ERRCODE_T=1)
add_definitions(-DHAVE_SYS_TYPES_H=1)
add_definitions(-DHAVE_WCHAR_H=1)
add_definitions(-DTRE_APPROX=1)
add_definitions(-DTRE_MULTIBYTE=1)
add_definitions(-DTRE_SYSTEM_REGEX_H_PATH="regex.h")
add_definitions(-DTRE_USE_SYSTEM_REGEX_H=1)
add_definitions(-DTRE_WCHAR=1)
add_definitions(-DTRE_VERSION=3)
add_definitions(-DTRE_VERSION_1=1)
add_definitions(-DTRE_VERSION_2=2)
add_definitions(-DTRE_VERSION_3=3)

remove_definitions(-DHAVE_LIBUTF8_H=1)
remove_definitions(-DTRE_USE_SYSTEM_REGEX_H=1)
remove_definitions(-DTRE_MULTIBYTE=1)

add_library(clandestine_tre
  regcomp.c
  regerror.c
  regexec.c
  tre-ast.c
  tre-compile.c
  tre-filter.c
  tre-match-approx.c
  tre-match-backtrack.c
  tre-match-parallel.c
  tre-mem.c
  tre-parse.c
  tre-stack.c
  xmalloc.c)

install(TARGETS clandestine_tre DESTINATION lib/)
