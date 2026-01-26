cmake_minimum_required(VERSION 3.30)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)

PROJECT(azo)

#if you don't want the full compiler output, remove the following line
SET(CMAKE_VERBOSE_MAKEFILE ON)
SET(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)

if(NOT HAS_ARIKKEI)
add_subdirectory(arikkei)
endif()
if(NOT HAS_AZ)
add_subdirectory(az)
endif()

add_subdirectory(azo)

#if(BUILD_TESTING)
#  add_subdirectory(tests)
#endif()

if(NOT PROJECT_IS_TOP_LEVEL)
  set(HAS_AZ true PARENT_SCOPE)
endif()
