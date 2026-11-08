# 公共叶子：错误码 / Options 为头文件（include/compress/），本目标承载基础设施装配胶水。
add_library(compress_common STATIC infra.cpp)
target_include_directories(compress_common PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(compress_common PUBLIC gflags glog)
