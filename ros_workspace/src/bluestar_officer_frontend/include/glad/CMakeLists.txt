cmake_minimum_required(VERSION 3.15)

project(glad C)

add_library(glad STATIC
  src/glad.c
)

add_library(glad::glad ALIAS glad)

target_include_directories(glad PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(glad PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)