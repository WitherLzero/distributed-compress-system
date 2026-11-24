# server 模块核心库：Compress 服务 + WorkerPool。测试与 compress_server 共享同一条装配路径。
add_library(server_core STATIC
    compress_service_impl.cpp
    worker_pool.cpp
    heartbeat_reporter.cpp
)
target_include_directories(server_core PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(server_core PUBLIC compress_proto compress_codec BRPC::brpc Threads::Threads)

add_executable(compress_server
    main.cpp

)
target_link_libraries(compress_server PRIVATE compress_common server_core glog)
