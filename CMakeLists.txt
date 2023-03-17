cmake_minimum_required(VERSION 3.20)

set(APP_NAME ph3dat)

project(${APP_NAME})

find_package(ZLIB REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(PH3DAT_SOURCES
	pch.h
	Archive.h
	Archive.cpp
	main.cpp
)

add_executable(${APP_NAME} ${PH3DAT_SOURCES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${ZLIB_INCLUDE_DIR})
target_link_libraries(${APP_NAME} PUBLIC ${ZLIB_LIBRARY})
