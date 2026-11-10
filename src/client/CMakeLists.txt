# 目标形态为共享库（宿主以动态库形式载入 client）。
# 票 08：§7.2 client 侧类声明骨架落源（声明冻结，方法定义随 W4/W5 填肉）。
add_library(compress_client SHARED
    block_meta_view.cpp
    client_core.cpp
    cold_scanner.cpp
    compress_executor.cpp
    compress_service_rpc_stub.cpp
    remote_file_accessor.cpp
    server_list_syncer.cpp
)
target_link_libraries(compress_client PUBLIC compress_api)   # header-only 公共头的 include 传染
