cmake_minimum_required(VERSION 3.9.4)

project(pps-comparer VERSION 0.2)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions("-D_FILE_OFFSET_BITS=64")

set(CMAKE_BUILD_TYPE RelWithDebInfo)

add_compile_options(-Wall -pedantic)

add_executable(pps-comparer
	pc.cpp
	)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(pps-comparer Threads::Threads)

include(FindPkgConfig)

configure_file(config.h.in config.h)
target_include_directories(pps-comparer PUBLIC "${PROJECT_BINARY_DIR}")

pkg_check_modules(GPS libgps)
target_link_libraries(pps-comparer ${GPS_LIBRARIES})
target_include_directories(pps-comparer PUBLIC ${GPS_INCLUDE_DIRS})
target_compile_options(pps-comparer PUBLIC ${GPS_CFLAGS_OTHER})
