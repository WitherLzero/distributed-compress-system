add_executable(compress_master
    main.cpp
    server_registry.cpp
    death_scanner.cpp
    master_service_impl.cpp  # CP0：BRPC service 骨架
)
target_link_libraries(compress_master PRIVATE compress_common compress_proto BRPC::brpc)
