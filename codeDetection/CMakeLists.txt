cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 20)

project(arucoRec)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(arucoRec arucoRec.cpp)
target_link_libraries(arucoRec ${OpenCV_LIBS})