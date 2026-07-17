add_executable(BluetoothPageTurnerInputTest
  BluetoothPageTurnerHidTest.cpp
  BluetoothPageTurnerInputTest.cpp
  BluetoothPageTurnerPowerTest.cpp
  ${REPO_ROOT}/src/MappedInputManager.cpp
)

target_include_directories(BluetoothPageTurnerInputTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src
)

target_link_libraries(BluetoothPageTurnerInputTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BluetoothPageTurnerInputTest)
