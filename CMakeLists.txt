cmake_minimum_required(VERSION 2.6)
project(yuv2jpeg)

add_executable(yuv2jpeg main.cpp)

install(TARGETS yuv2jpeg RUNTIME DESTINATION bin)
