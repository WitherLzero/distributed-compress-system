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

# 静态链接 libbrpc.a 时的传递依赖（BRPC master 将以下库无条件链入，
# brpc.pc 的 private_libs 里也是这些：-lssl -lcrypto -lleveldb ...）
find_library(BRPC_LEVELDB_LIBRARY NAMES leveldb)
find_library(BRPC_SSL_LIBRARY NAMES ssl)
find_library(BRPC_CRYPTO_LIBRARY NAMES crypto)
find_library(BRPC_Z_LIBRARY NAMES z)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BRPC
    REQUIRED_VARS BRPC_LIBRARY BRPC_INCLUDE_DIR
)

if(BRPC_FOUND AND NOT TARGET BRPC::brpc)
    add_library(BRPC::brpc UNKNOWN IMPORTED)
    set_target_properties(BRPC::brpc PROPERTIES
        IMPORTED_LOCATION "${BRPC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${BRPC_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES
            "${BRPC_LEVELDB_LIBRARY};${BRPC_SSL_LIBRARY};${BRPC_CRYPTO_LIBRARY};${BRPC_Z_LIBRARY}"
    )
endif()

mark_as_advanced(BRPC_INCLUDE_DIR BRPC_LIBRARY
    BRPC_LEVELDB_LIBRARY BRPC_SSL_LIBRARY BRPC_CRYPTO_LIBRARY BRPC_Z_LIBRARY)
