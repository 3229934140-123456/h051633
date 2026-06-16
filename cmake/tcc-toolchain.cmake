set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER tcc)
set(CMAKE_C_COMPILER_ID TCC)

# Skip compiler ABI check and test compilation
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_C_ABI_COMPILED TRUE)

# Tell CMake we are cross-compiling to avoid test programs
set(CMAKE_CROSSCOMPILING FALSE)

# TCC does not need separate linker
set(CMAKE_LINKER tcc)

# No need for math library on Windows with TCC
set(MATH_LIBRARY "")
