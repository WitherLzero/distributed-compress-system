cmake_minimum_required(VERSION 3.16)   # 下限沿用 hello-brpc 已验证值
project(distributed_compress_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # compile_commands.json：clangd / VS Code 补全

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Threads REQUIRED)
find_package(gflags REQUIRED)          # 与 BRPC 同源，配置项解析依赖
find_package(BRPC REQUIRED)            # BRPC 无 CMake config 包，依赖 cmake/FindBRPC.cmake

add_subdirectory(src)

include(CTest)                         # BUILD_TESTING 开关 + ctest 收集入口
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
