add_executable(compress_server
    main.cpp
    compress_service_impl.cpp  # 票 08 骨架
    heartbeat_reporter.cpp     # 票 08 骨架
    worker_pool.cpp            # 票 08 骨架
)
target_link_libraries(compress_server PRIVATE compress_common)
