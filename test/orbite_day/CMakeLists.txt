add_executable(OrbiteDayTest
  OrbiteDayTest.cpp
  ${REPO_ROOT}/src/orbite/OrbiteDay.cpp
)
target_include_directories(OrbiteDayTest PRIVATE ${REPO_ROOT}/src/orbite)
target_link_libraries(OrbiteDayTest PRIVATE crosspoint_test_common GTest::gtest_main ArduinoJson)
gtest_discover_tests(OrbiteDayTest)
