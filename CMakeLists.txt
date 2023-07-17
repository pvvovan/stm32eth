cmake_minimum_required(VERSION 3.0)
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")
set(CROSS_COMPILE "arm-none-eabi-")
set(CMAKE_C_COMPILER "${CROSS_COMPILE}gcc")
set(CMAKE_CXX_COMPILER "${CROSS_COMPILE}g++")
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

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m4 -std=gnu11 -g3 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb")

add_library(${PROJECT_NAME} empty.c)
target_link_libraries(${PROJECT_NAME} lwipcore)
