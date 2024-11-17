cmake_minimum_required(VERSION 3.20)
project(ComputationManager)

set(CMAKE_CXX_STANDARD 17)

add_executable(ComputationManager main.cpp)

target_link_libraries(ComputationManager pthread)
