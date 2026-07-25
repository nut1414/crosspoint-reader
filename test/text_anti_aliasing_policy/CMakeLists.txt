add_executable(TextAntiAliasingPolicyTest
  TextAntiAliasingPolicyTest.cpp
)

target_include_directories(TextAntiAliasingPolicyTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src
)

target_link_libraries(TextAntiAliasingPolicyTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(TextAntiAliasingPolicyTest)
