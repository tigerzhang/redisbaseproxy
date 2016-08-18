cmake_minimum_required(VERSION 3.6)
project(redisbaseproxy)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -L${CMAKE_SOURCE_DIR}/redox/build")

set(SOURCE_FILES
    main.cc redisproxy.cc redisproxy.h)

find_package(libev REQUIRED)
include_directories(${LIBEV_INCLUDE_DIRS})
set(LIBS ${LIBS} ${LIBEV_LIBRARIES}
        redox)

include_directories("resp" "redox/include")

add_executable(redisbaseproxy ${SOURCE_FILES})
target_link_libraries(redisbaseproxy ${LIBS})
