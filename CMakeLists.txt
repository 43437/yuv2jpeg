cmake_minimum_required(VERSION 2.6)
project(yuv2jpeg)

include_directories("/home/acmax/app/ffmpeg-dev/include")

link_directories("/home/acmax/app/ffmpeg-dev/lib")

add_executable(yuv2jpeg main.cpp)

target_link_libraries(yuv2jpeg "avcodec" "avformat" "swscale" "avutil")
