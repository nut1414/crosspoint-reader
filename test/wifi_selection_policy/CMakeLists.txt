add_executable(WifiSelectionPolicyTest
  WifiSelectionPolicyTest.cpp
  ${REPO_ROOT}/src/activities/network/WifiSelectionPolicy.cpp
)

target_include_directories(WifiSelectionPolicyTest PRIVATE
  ${REPO_ROOT}/src
  ${REPO_ROOT}/src/activities/network
  ${REPO_ROOT}/lib/KOReaderSync
)

target_link_libraries(WifiSelectionPolicyTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WifiSelectionPolicyTest)
