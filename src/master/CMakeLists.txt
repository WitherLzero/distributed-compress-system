# master 模块核心库：registry / scanner / service / 装配线。
# 独立成库让测试与 compress_master 可执行文件共享同一条装配路径（测的 = 产的）。
add_library(master_core STATIC
    server_registry.cpp
    death_scanner.cpp
    master_service_impl.cpp
    master_runtime.cpp
)
target_include_directories(master_core PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(master_core PUBLIC compress_proto BRPC::brpc Threads::Threads)

add_executable(compress_master
    main.cpp
)
target_link_libraries(compress_master PRIVATE compress_common master_core)
