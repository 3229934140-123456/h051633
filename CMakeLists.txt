cmake_minimum_required(VERSION 3.10)

# Skip compiler test when cross-compiling or using TCC
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_C_ABI_COMPILED 1)

project(moonjit C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR}/src)

set(LIB_SOURCES
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

set(MAIN_SOURCES
    src/main.c
    ${LIB_SOURCES}
)

add_executable(moonjit ${MAIN_SOURCES})

# Host integration demo
set(DEMO_SOURCES
    examples/host_demo.c
    ${LIB_SOURCES}
)

add_executable(host_demo ${DEMO_SOURCES})

# Print summary
add_custom_command(TARGET moonjit POST_BUILD
    COMMAND echo "Built target: moonjit.exe"
)
add_custom_command(TARGET host_demo POST_BUILD
    COMMAND echo "Built target: host_demo.exe"
)


