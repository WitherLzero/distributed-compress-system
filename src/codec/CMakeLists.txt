# 纯计算叶子：统一编解码（LZ4 浅压 / LZMA 深压同一调用面，票 06）。
find_package(LibLZMA REQUIRED) # CMake 自带 FindLibLZMA
set_target_properties(LibLZMA::LibLZMA PROPERTIES IMPORTED_GLOBAL TRUE)  # 提升为全局可见：tests/ 经 compress_codec 的 PUBLIC 需求引用它
find_library(LZ4_LIBRARY NAMES lz4 REQUIRED)   # LZ4 无官方 config 包，定位系统库

add_library(compress_codec STATIC codec.cc)
target_include_directories(compress_codec PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(compress_codec PUBLIC LibLZMA::LibLZMA ${LZ4_LIBRARY})
