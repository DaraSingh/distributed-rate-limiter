cmake_minimum_required(VERSION 3.15)
project(redis_demo CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(redis++ REQUIRED)
find_package(hiredis REQUIRED)

add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE redis++::redis++ hiredis::hiredis)