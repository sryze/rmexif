cmake_minimum_required(VERSION 3.0)

project(rmexif
  VERSION 0.0
  LANGUAGES C)

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_executable(rmexif rmexif.c)
