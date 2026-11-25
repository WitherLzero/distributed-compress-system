# IDL -> 消息+service 代码：构建期 protoc 生成，产物落在 build 树（generated/），不入库。
# cc_generic_services=true 使 protoc 同时生成 Service/Stub 基类（BRPC 直接消费）。
find_package(Protobuf REQUIRED)   # apt 包无 CMake config，走 CMake 自带 FindProtobuf 模块

set(PROTO_FILE compress_service.proto)
set(GEN_DIR  ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(GEN_SRC  ${GEN_DIR}/compress_service.pb.cc)
set(GEN_HDR  ${GEN_DIR}/compress_service.pb.h)

add_custom_command(
    OUTPUT  ${GEN_SRC} ${GEN_HDR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN_DIR}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            --cpp_out=${GEN_DIR}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "protoc: ${PROTO_FILE} -> compress_service.pb.{cc,h}"
)

add_library(compress_proto STATIC ${GEN_SRC} ${GEN_HDR})
target_include_directories(compress_proto PUBLIC ${GEN_DIR})
target_link_libraries(compress_proto PUBLIC protobuf::libprotobuf)
