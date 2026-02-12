cmake_minimum_required(VERSION 3.20)

project(2048civ)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)

set(CMAKE_C_FLAGS "-g")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

add_subdirectory(src bin)

