file(GLOB_RECURSE ALL_SRCS "*.c")

add_executable(${CMAKE_PROJECT_NAME} ${ALL_SRCS})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES} m)
if(SDL2_CFLAGS_OTHER)
	target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE ${SDL2_CFLAGS_OTHER})
endif()
