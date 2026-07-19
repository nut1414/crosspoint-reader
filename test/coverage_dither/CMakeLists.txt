add_executable(CoverageDitherTest
  CoverageDitherTest.cpp
)

target_link_libraries(CoverageDitherTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(CoverageDitherTest)
