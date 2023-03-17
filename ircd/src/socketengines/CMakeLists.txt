IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  add_library(clandestine_ircd_socketengines
    socketengine_kqueue.cpp)
#    socketengine_poll.cpp
#    socketengine_select.cpp)
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  add_library(clandestine_ircd_socketengines
    socketengine_epoll.cpp)
#    socketengine_poll.cpp
#    socketengine_select.cpp)
ELSE()
  add_library(clandestine_ircd_socketengines
    socketengine_poll.cpp
    socketengine_select.cpp)
ENDIF()

target_include_directories(clandestine_ircd_socketengines PUBLIC
  ../../include)

install(TARGETS clandestine_ircd_socketengines DESTINATION lib/)
