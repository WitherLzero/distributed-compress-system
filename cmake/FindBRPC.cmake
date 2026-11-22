# FindBRPC.cmake —— Apache BRPC 不提供 CMake config 包（只装 brpc.pc），
# find_package(BRPC) 依赖本文件。W3 工程脚手架原样复用。
#
# 安装布局（cmake --install 到 /usr/local）：
#   include/brpc/*.h
#   lib/libbrpc.a          （默认静态；shared 库名为 libbrpc-shared.so，本项目用静态）

find_path(BRPC_INCLUDE_DIR
    NAMES brpc/server.h
    HINTS /usr/local/include /usr/include
)

find_library(BRPC_LIBRARY
    NAMES brpc
    HINTS /usr/local/lib /usr/lib
)

# 静态链接 libbrpc.a 时的传递依赖（brpc.pc Libs.private 全量：
# -lprotoc -lleveldb -lgflags -lprotobuf -lssl -lcrypto -lz -ldl -lrt）
# 顺序要求：gflags 必须在 brpc 之后（链接器从左到右解析符号）。
find_library(BRPC_LEVELDB_LIBRARY NAMES leveldb)
find_library(BRPC_SSL_LIBRARY NAMES ssl)
find_library(BRPC_CRYPTO_LIBRARY NAMES crypto)
find_library(BRPC_Z_LIBRARY NAMES z)
find_library(BRPC_PROTOC_LIBRARY NAMES protoc)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BRPC
    REQUIRED_VARS BRPC_LIBRARY BRPC_INCLUDE_DIR
                  BRPC_LEVELDB_LIBRARY BRPC_SSL_LIBRARY
                  BRPC_CRYPTO_LIBRARY BRPC_Z_LIBRARY
)

if(BRPC_FOUND AND NOT TARGET BRPC::brpc)
    add_library(BRPC::brpc UNKNOWN IMPORTED)
    set_target_properties(BRPC::brpc PROPERTIES
        IMPORTED_LOCATION "${BRPC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${BRPC_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES
            "${BRPC_LEVELDB_LIBRARY};${BRPC_SSL_LIBRARY};${BRPC_CRYPTO_LIBRARY};${BRPC_Z_LIBRARY};${BRPC_PROTOC_LIBRARY};gflags;protobuf;dl;rt"
    )
endif()

mark_as_advanced(BRPC_INCLUDE_DIR BRPC_LIBRARY
    BRPC_LEVELDB_LIBRARY BRPC_SSL_LIBRARY BRPC_CRYPTO_LIBRARY BRPC_Z_LIBRARY)
