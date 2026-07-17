add_executable(WebDAVClientTest
  WebDAVClientTest.cpp
  ${REPO_ROOT}/src/network/WebDAVPropfindParser.cpp
  ${REPO_ROOT}/src/network/WebDAVUrl.cpp
  ${REPO_ROOT}/src/util/UrlUtils.cpp
  ${REPO_ROOT}/lib/expat/xmlparse.c
  ${REPO_ROOT}/lib/expat/xmlrole.c
  ${REPO_ROOT}/lib/expat/xmltok.c
)

target_include_directories(WebDAVClientTest PRIVATE
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/XmlParserUtils
  ${REPO_ROOT}/lib/expat
)

target_compile_definitions(WebDAVClientTest PRIVATE
  XML_STATIC
  XML_GE=0
  XML_CONTEXT_BYTES=1024
)

target_link_libraries(WebDAVClientTest PRIVATE
  crosspoint_test_common
)

add_test(NAME WebDAVClientTest COMMAND WebDAVClientTest)
