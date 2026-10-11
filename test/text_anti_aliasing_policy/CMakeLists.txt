add_executable(TextAntiAliasingPolicyTest
  TextAntiAliasingPolicyTest.cpp
)

target_include_directories(TextAntiAliasingPolicyTest PRIVATE
  ${REPO_ROOT}/test/settings_stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/Epub
)

target_link_libraries(TextAntiAliasingPolicyTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(TextAntiAliasingPolicyTest)
