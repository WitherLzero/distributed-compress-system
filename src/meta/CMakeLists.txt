# 纯逻辑叶子：块级文件头读写（票 06）。零第三方依赖，只依赖错误码头文件。
add_library(compress_meta STATIC compressed_file_header.cc atomic_replacer.cpp)
target_include_directories(compress_meta PUBLIC ${CMAKE_SOURCE_DIR}/include)
