cmake_minimum_required(VERSION 3.4.3)
project(Kaleiscope)

find_package(LLVM REQUIRED CONFIG)

# Set gdb
SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
# Set your project compile flags.
# E.g. if using the C++ header files
# you will need to enable C++11 support
# for your compiler.

#include_directories("libs")
add_definitions(${LLVM_DEFINITIONS})

# Now build our tools
add_executable(toy toy.cpp ../include/KaleidoscopeJIT.h)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fno-rtti")

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader ScalarOpts
        ExecutionEngine OrcJIT native Analysis RuntimeDyld Object InstCombine mcjit)

# Link against LLVM libraries
target_link_libraries(toy ${llvm_libs})

include_directories(${LLVM_INCLUDE_DIRS})