cmake_minimum_required(VERSION 3.16)   # 下限沿用 hello-brpc 已验证值
project(distributed_compress_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # compile_commands.json：clangd / VS Code 补全

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# header-only 公共头（如 compress_task_queue.h）的归属目标：
# 不产库文件，只传染 include 路径，消费者一律 target_link_libraries(... compress_api)
add_library(compress_api INTERFACE)
target_include_directories(compress_api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

find_package(Threads REQUIRED)
find_package(gflags REQUIRED)          # 与 BRPC 同源，配置项解析依赖
find_package(glog REQUIRED)            # 进程日志：仅带 main() 的二进制使用，库不强制
find_package(BRPC REQUIRED)            # BRPC 无 CMake config 包，依赖 cmake/FindBRPC.cmake

add_subdirectory(proto)
add_subdirectory(src)
add_subdirectory(mock)                 # 联调替身进程：mock_datanode（替换 hello-brpc 冒烟位）

include(CTest)                         # BUILD_TESTING 开关 + ctest 收集入口
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
