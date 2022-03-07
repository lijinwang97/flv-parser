cmake_minimum_required(VERSION 3.21)
project(cppDemo)

set(CMAKE_CXX_STANDARD 14)

add_executable(cppDemo main.cpp FlvParser.cpp
        FlvParser.h
        vadbg.cpp
        vadbg.h
        Videojj.cpp
        Videojj.h)
