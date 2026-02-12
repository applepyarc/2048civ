file(GLOB_RECURSE ALL_SRCS "*.c")

find_package(PkgConfig REQUIRED)

add_executable(${CMAKE_PROJECT_NAME} ${ALL_SRCS})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE m)
if(SDL2_CFLAGS_OTHER)
	target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE ${SDL2_CFLAGS_OTHER})
endif()
if(SDL2_TTF_CFLAGS_OTHER)
	target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE ${SDL2_TTF_CFLAGS_OTHER})
endif()
