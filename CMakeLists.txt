cmake_minimum_required(VERSION 3.8)
project(YunQuake)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -m64 -O3 -Wall -fcheck-new -DWINDOWS=1 -DUNICODE=1 -D_DEBUG=1" )
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m64" )

file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.h)
add_executable(YunQuake  ${SRCS} ${HDRS})
target_link_libraries (YunQuake kernel32 user32 gdi32 ole32 dinput8 dxguid)