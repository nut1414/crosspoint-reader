add_executable(WifiSelectionPolicyTest
  WifiSelectionPolicyTest.cpp
  ${REPO_ROOT}/src/activities/network/WifiSelectionPolicy.cpp
)

target_include_directories(WifiSelectionPolicyTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src
  ${REPO_ROOT}/src/activities/network
  ${REPO_ROOT}/lib/KOReaderSync
  ${REPO_ROOT}/lib/Serialization
)

target_link_libraries(WifiSelectionPolicyTest PRIVATE
  crosspoint_test_common
  ArduinoJson
  GTest::gtest_main
)

gtest_discover_tests(WifiSelectionPolicyTest)
