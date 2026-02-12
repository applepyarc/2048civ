file(GLOB_RECURSE ALL_SRCS "*.c")

add_executable(${CMAKE_PROJECT_NAME} ${ALL_SRCS})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE sdl2)
