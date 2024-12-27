cmake_minimum_required(VERSION 3.29)
project(OpenMPtoMPI)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(MPI REQUIRED)
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Found Clang ${CLANG_VERSION}")
message(STATUS "Using ClangConfig.cmake in: ${CLANG_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(CLANG_LIBS
        clangAST
        clangASTMatchers
        clangFormat
        clangBasic
        clangDriver
        clangEdit
        clangFrontend
        clangFrontendTool
        clangLex
        clangParse
        clangSema
        clangSerialization
        clangTooling
        clangToolingCore
)

add_executable(OpenMPtoMPI
        main.cpp
#        tests/test1.cpp
        actions/PreprocessorAndASTAction.cpp
        actions/PreprocessorAndASTActionFactory.cpp
        finders/MainFunctionFinder.cpp
        finders/OpenMPPragmaFinder.cpp
        pragmas/OpenMPDirectiveBaseHandler.h
        pragmas/OpenMPParallelForHandler.cpp
        pragmas/OpenMPParallelHandler.cpp
        pragmas/OpenMPSingleHandler.cpp
        pragmas/OpenMPMasterHandler.cpp
        pragmas/OpenMPBarrierHandler.cpp
        preprocessor/PreprocessorHeaderCallback.cpp
        utils/FileUtils.cpp
        utils/StmtUtils.cpp
        utils/VariableAnalyzer.cpp
        utils/VariablesHandler.cpp

)

target_link_libraries(OpenMPtoMPI
        PRIVATE
        ${CLANG_LIBS}
        ${LLVM_LIBS}
        MPI::MPI_CXX
)