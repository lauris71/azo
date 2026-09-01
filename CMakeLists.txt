cmake_minimum_required(VERSION 3.30)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)

PROJECT(azo)

#if you don't want the full compiler output, remove the following line
SET(CMAKE_VERBOSE_MAKEFILE ON)
SET(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)

# arikkei is a drop-in static library, expected either nested (az/arikkei) or as a
# sibling (toplevel_project_dir/arikkei). If the parent project already added it,
# that target is used instead. Linking the arikkei target resolves the library
# location, name (libarikkei/libarikkeid) and per-config subdirectory automatically.
if(NOT TARGET arikkei)
	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/arikkei/CMakeLists.txt)
		add_subdirectory(arikkei)
	elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../arikkei/CMakeLists.txt)
		# Outside the source tree - needs an explicit binary directory
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../arikkei ${CMAKE_BINARY_DIR}/arikkei)
	else()
		message(FATAL_ERROR "arikkei not found - expected at ${CMAKE_CURRENT_SOURCE_DIR}/arikkei or ${CMAKE_CURRENT_SOURCE_DIR}/../arikkei")
	endif()
endif()
# az is a drop-in static library, expected either nested (az/az) or as a
# sibling (toplevel_project_dir/az). If the parent project already added it,
# that target is used instead. Linking the az target resolves the library
# location, name (libaz/libazd) and per-config subdirectory automatically.
if(NOT TARGET az)
	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/az/CMakeLists.txt)
		add_subdirectory(az)
	elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../az/CMakeLists.txt)
		# Outside the source tree - needs an explicit binary directory
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../az ${CMAKE_BINARY_DIR}/az)
	else()
		message(FATAL_ERROR "az not found - expected at ${CMAKE_CURRENT_SOURCE_DIR}/az or ${CMAKE_CURRENT_SOURCE_DIR}/../az")
endif()
endif()

add_subdirectory(azo)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(NOT PROJECT_IS_TOP_LEVEL)
  set(HAS_AZ true PARENT_SCOPE)
endif()
