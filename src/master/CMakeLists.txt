add_executable(compress_master
    main.cpp
    server_registry.cpp   # 票 08 骨架
    death_scanner.cpp     # 票 08 骨架
)
target_link_libraries(compress_master PRIVATE compress_common)
