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

# CP0：BRPC 通信链路冒烟（同进程 server+client，验证 Heartbeat 收发）
add_executable(master_smoke_test master_smoke_test.cc)
target_link_libraries(master_smoke_test PRIVATE compress_proto BRPC::brpc GTest::gtest GTest::gtest_main)
add_test(NAME master_smoke COMMAND master_smoke_test)
