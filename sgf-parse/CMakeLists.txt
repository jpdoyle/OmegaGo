cmake_minimum_required(VERSION 2.6)
project(sgfparse)

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    add_definitions( -DCNN_USE_OMP)
endif()

add_definitions(-DCNN_USE_SSE)
#add_definitions(-DCNN_USE_AVX)
SET(EXTRA_C_FLAGS  " ${EXTRA_C_FLAGS} -msse3 ")
#SET(EXTRA_C_FLAGS  "${EXTRA_C_FLAGS} -mavx ")

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Werror ${CMAKE_CXX_FLAGS} ${EXTRA_C_FLAGS}")
find_package (Threads)

# Enable debug symbols by default
if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE Debug)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -DNDEBUG")
endif()
# (you can also set it on the command line: -D CMAKE_BUILD_TYPE=Release)

# Set version information in a config.h file
# Define sources and executable

include_directories(
    src/ ../tiny-cnn/ ../base-n/include/
        libsocket/headers)
    #add_subdirectory(libsocket)
    #link_libraries(sgfparse socket++)

set(SRC_FILES
    src/main.cpp
    src/Sgf.cpp
    src/Board.cpp
    src/Bot.cpp
    src/GameServer.cpp
    src/MonteCarloTree.cpp
    src/Features.cpp
    src/AsyncPolicy.cpp
)


set(EXECUTABLE_NAME "sgfparse")
add_executable(${EXECUTABLE_NAME}
    ${SRC_FILES}
)
target_link_libraries (${EXECUTABLE_NAME} ${CMAKE_THREAD_LIBS_INIT})

