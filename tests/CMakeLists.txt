find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(proto_smoke_test proto_smoke_test.cc)
target_link_libraries(proto_smoke_test PRIVATE compress_proto GTest::gtest GTest::gtest_main)
add_test(NAME proto_smoke COMMAND proto_smoke_test)

add_executable(codec_test codec_test.cc)
target_link_libraries(codec_test PRIVATE compress_codec GTest::gtest GTest::gtest_main)
add_test(NAME codec COMMAND codec_test)

add_executable(header_test header_test.cc)
target_link_libraries(header_test PRIVATE compress_meta GTest::gtest GTest::gtest_main)
add_test(NAME header COMMAND header_test)

add_executable(task_queue_test task_queue_test.cc)
target_link_libraries(task_queue_test PRIVATE compress_api GTest::gtest GTest::gtest_main Threads::Threads)
add_test(NAME task_queue COMMAND task_queue_test)

add_executable(atomic_replacer_test atomic_replacer_test.cc)
target_link_libraries(atomic_replacer_test PRIVATE compress_meta GTest::gtest GTest::gtest_main)
add_test(NAME atomic_replacer COMMAND atomic_replacer_test)

# CP1：ServerRegistry 纯逻辑（四态流转 + 并发不变量）。
# 注：本 target 经 bvar 链接 BRPC，TSAN 对链接 BRPC 的目标无判别力（票 05 欠账节），
# TSAN 验证仅适用于不链 BRPC 的纯 std 构建。
add_executable(server_registry_test server_registry_test.cc
    ../src/master/server_registry.cpp)
target_include_directories(server_registry_test PRIVATE ../src/master)
target_link_libraries(server_registry_test PRIVATE BRPC::brpc GTest::gtest GTest::gtest_main Threads::Threads)
add_test(NAME server_registry COMMAND server_registry_test)

add_executable(death_scanner_test death_scanner_test.cc
    ../src/master/server_registry.cpp ../src/master/death_scanner.cpp)
target_include_directories(death_scanner_test PRIVATE ../src/master)
target_link_libraries(death_scanner_test PRIVATE BRPC::brpc GTest::gtest GTest::gtest_main Threads::Threads)
add_test(NAME death_scanner COMMAND death_scanner_test)

# CP3：BRPC 服务端填肉——端到端列表流转（注册→出现→刷新→TTL 消失）
add_executable(master_smoke_test master_smoke_test.cc)
target_link_libraries(master_smoke_test PRIVATE master_core GTest::gtest GTest::gtest_main)
add_test(NAME master_smoke COMMAND master_smoke_test)

# CP4：Compress 端点接线（延迟 done + WorkerPool；同进程回环）
add_executable(server_compress_test server_compress_test.cc)
target_include_directories(server_compress_test PRIVATE ../src/server)
target_link_libraries(server_compress_test PRIVATE server_core GTest::gtest GTest::gtest_main)
add_test(NAME server_compress COMMAND server_compress_test)

# CP5：心跳客户端（真 master 服务端 + reporter 被测件）
add_executable(heartbeat_reporter_test heartbeat_reporter_test.cc)
target_include_directories(heartbeat_reporter_test PRIVATE ../src/server ../src/master)
target_link_libraries(heartbeat_reporter_test PRIVATE server_core master_core GTest::gtest GTest::gtest_main)
add_test(NAME heartbeat_reporter COMMAND heartbeat_reporter_test)

# CP6：ServerRuntime 组合根端到端（真 master + 真 server 同进程）
add_executable(server_runtime_test server_runtime_test.cc)
target_include_directories(server_runtime_test PRIVATE ../src/server ../src/master)
target_link_libraries(server_runtime_test PRIVATE server_core master_core GTest::gtest GTest::gtest_main)
add_test(NAME server_runtime COMMAND server_runtime_test)

# CP6：心跳负载取样（CPU 增量 / MEM 余量，reader 注入可测）
add_executable(load_sampler_test load_sampler_test.cc)
target_include_directories(load_sampler_test PRIVATE ../src/server)
target_link_libraries(load_sampler_test PRIVATE server_core GTest::gtest GTest::gtest_main)
add_test(NAME load_sampler COMMAND load_sampler_test)
