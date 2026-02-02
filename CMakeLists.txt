cmake_minimum_required(VERSION 3.10)

project(MicroMeowDB)




set(CMAKE_CXX_COMPILER "clang++")

set(CMAKE_C_COMPILER "clang")

set(CMAKE_BUILD_TYPE "Release")




add_executable(MicroMeowDB src/main.c)
