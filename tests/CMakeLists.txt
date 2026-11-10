# 单测收集入口。纯逻辑类单测按票 06/07 增补；此处挂票 05 的 proto 冒烟。
find_package(GTest REQUIRED)   # libgtest-dev 1.11：静态库 + GTestConfig

add_executable(proto_smoke_test proto_smoke_test.cc)
target_link_libraries(proto_smoke_test PRIVATE compress_proto GTest::gtest GTest::gtest_main)
add_test(NAME proto_smoke COMMAND proto_smoke_test)
# 票 06：纯逻辑叶子单测
add_executable(codec_test codec_test.cc)
target_link_libraries(codec_test PRIVATE compress_codec GTest::gtest GTest::gtest_main)
add_test(NAME codec COMMAND codec_test)

add_executable(header_test header_test.cc)
target_link_libraries(header_test PRIVATE compress_meta GTest::gtest GTest::gtest_main)
add_test(NAME header COMMAND header_test)

# 票 07：header-only 有界任务队列，无项目库依赖；std::thread 需显式链线程库
find_package(Threads REQUIRED)
add_executable(task_queue_test task_queue_test.cc)
target_link_libraries(task_queue_test PRIVATE compress_api GTest::gtest GTest::gtest_main Threads::Threads)
add_test(NAME task_queue COMMAND task_queue_test)

# 票 08：原子替换器（临时文件 + rename）单测
add_executable(atomic_replacer_test atomic_replacer_test.cc)
target_link_libraries(atomic_replacer_test PRIVATE compress_meta GTest::gtest GTest::gtest_main)
add_test(NAME atomic_replacer COMMAND atomic_replacer_test)
