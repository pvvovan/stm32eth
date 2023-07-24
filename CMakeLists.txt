cmake_minimum_required(VERSION 3.0)
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
project(lwIP C)

set(LOCAL_SRCS "")
set(LOCAL_HDRS "")

set(LWIP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lwip)
set (LWIP_INCLUDE_DIRS
    "${LWIP_DIR}/src/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Inc/arch")

include(${LWIP_DIR}/src/Filelists.cmake)

add_library(${PROJECT_NAME} empty.c)
target_link_libraries(${PROJECT_NAME} lwipcore)
