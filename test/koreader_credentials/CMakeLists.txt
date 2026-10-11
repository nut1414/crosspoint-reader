if(NOT TARGET ArduinoJson)
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
endif()

add_executable(KOReaderCredentialStoreTest
  KOReaderCredentialStoreTest.cpp
  ${REPO_ROOT}/lib/KOReaderSync/KOReaderCredentialStore.cpp
  ${REPO_ROOT}/lib/Serialization/PersistableStore.cpp
)

target_include_directories(KOReaderCredentialStoreTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/test/webdav_store/stubs
  ${REPO_ROOT}/lib/KOReaderSync
  ${REPO_ROOT}/lib/Serialization
)

target_link_libraries(KOReaderCredentialStoreTest PRIVATE
  crosspoint_test_common
  ArduinoJson
  GTest::gtest_main
)

gtest_discover_tests(KOReaderCredentialStoreTest)
