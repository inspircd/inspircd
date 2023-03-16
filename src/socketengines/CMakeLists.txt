IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  add_library(socketengines
    socketengine_kqueue.cpp
    socketengine_poll.cpp
    socketengine_select.cpp)
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  add_library(socketengines
    socketengine_epoll.cpp
    socketengine_poll.cpp
    socketengine_select.cpp)
ELSE()
  add_library(socketengines
    socketengine_poll.cpp
    socketengine_select.cpp)
ENDIF()
