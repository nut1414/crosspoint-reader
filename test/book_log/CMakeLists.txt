set(LOCAL_ARDUINOJSON "${REPO_ROOT}/.pio/libdeps/default/ArduinoJson/src")
if(EXISTS "${LOCAL_ARDUINOJSON}/ArduinoJson.h")
  add_library(ArduinoJson INTERFACE)
  target_include_directories(ArduinoJson INTERFACE "${LOCAL_ARDUINOJSON}")
else()
  FetchContent_Declare(
    arduinojson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG v7.4.2
  )
  FetchContent_MakeAvailable(arduinojson)
endif()

add_executable(BookLogStoreTest
  BookLogStoreTest.cpp
  ${REPO_ROOT}/src/BookLogStore.cpp
)

target_include_directories(BookLogStoreTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src
)

target_link_libraries(BookLogStoreTest PRIVATE
  crosspoint_test_common
  ArduinoJson
  GTest::gtest_main
)

gtest_discover_tests(BookLogStoreTest)
