cmake_minimum_required(VERSION 3.10)
project(moonjit C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

include_directories(${PROJECT_SOURCE_DIR}/src)

set(SOURCES
    src/main.c
    src/memory.c
    src/value.c
    src/object.c
    src/chunk.c
    src/table.c
    src/lexer.c
    src/compiler.c
    src/vm.c
    src/gc.c
    src/host.c
)

add_executable(moonjit ${SOURCES})

if(NOT MSVC)
    target_link_libraries(moonjit m)
endif()
