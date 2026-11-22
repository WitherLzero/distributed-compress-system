# 公共叶子：错误码 / Options 为头文件（include/compress/），本目标承载基础设施装配胶水。
add_library(compress_common STATIC infra.cpp infra_glog.cpp)
target_include_directories(compress_common PUBLIC ${CMAKE_SOURCE_DIR}/include)
# glog 不在此链接：InitInfra 的调用方自行链接 glog——brpc WITH_GLOG=OFF 与系统 glog
# 双 -v 注册冲突，同一进程只能二选一（见票 05 欠账）；master 走 butil 日志不碰 glog。
target_link_libraries(compress_common PUBLIC gflags)
