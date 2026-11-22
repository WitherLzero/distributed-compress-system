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

# CP1：ServerRegistry 纯逻辑（四态流转 + 并发不变量；TSAN 下验证无竞态）
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
