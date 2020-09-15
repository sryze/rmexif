cmake_minimum_required(VERSION 3.0)

project(rmexif VERSION 0.1)

add_compile_options("$<$<CONFIG:DEBUG>:-DDEBUG>")

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_executable(rmexif rmexif.c)

install(TARGETS rmexif)

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
if(WIN32)
  set(CPACK_GENERATOR ZIP)
endif()

include(CPack)