add_library(clandestine_ircd_threadengines
  threadengine_pthread.cpp
)

target_include_directories(clandestine_ircd_threadengines PUBLIC
  ../../include)

install(TARGETS clandestine_ircd_threadengines DESTINATION lib/)
