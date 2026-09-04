add_executable(dashboard-snapshot-test tst_dashboard_snapshot.cpp)
target_link_libraries(dashboard-snapshot-test PRIVATE codexbar-core Qt6::Test)
target_compile_definitions(
    dashboard-snapshot-test
    PRIVATE FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dashboard-v1.json"
)

add_test(NAME dashboard-snapshot COMMAND dashboard-snapshot-test)
