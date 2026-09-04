add_executable(fake-codexbar fake_codexbar.cpp)
target_link_libraries(fake-codexbar PRIVATE Qt6::Core)
kodometer_enable_warnings(fake-codexbar)
target_compile_definitions(
    fake-codexbar
    PRIVATE FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dashboard-v1.json"
)

add_executable(dashboard-controller-test tst_dashboard_controller.cpp)
target_link_libraries(dashboard-controller-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(dashboard-controller-test)
target_compile_definitions(
    dashboard-controller-test
    PRIVATE FAKE_CODEXBAR_PATH="$<TARGET_FILE:fake-codexbar>"
)
add_dependencies(dashboard-controller-test fake-codexbar)
add_test(NAME dashboard-controller COMMAND dashboard-controller-test)

add_executable(codex-credentials-test tst_codex_credentials.cpp)
target_link_libraries(codex-credentials-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(codex-credentials-test)
add_test(NAME codex-credentials COMMAND codex-credentials-test)

add_executable(codex-provider-adapter-test tst_codex_provider_adapter.cpp)
target_link_libraries(codex-provider-adapter-test PRIVATE kodometer-core Qt6::Network Qt6::Test)
kodometer_enable_warnings(codex-provider-adapter-test)
add_test(NAME codex-provider-adapter COMMAND codex-provider-adapter-test)

add_executable(codex-usage-parser-test tst_codex_usage_parser.cpp)
target_link_libraries(codex-usage-parser-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(codex-usage-parser-test)
add_test(NAME codex-usage-parser COMMAND codex-usage-parser-test)

add_executable(presentation-formatter-test tst_presentation_formatter.cpp)
target_link_libraries(presentation-formatter-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(presentation-formatter-test)
add_test(NAME presentation-formatter COMMAND presentation-formatter-test)

add_executable(provider-selection-model-test tst_provider_selection_model.cpp)
target_link_libraries(provider-selection-model-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(provider-selection-model-test)
add_test(NAME provider-selection-model COMMAND provider-selection-model-test)

add_executable(usage-controller-test tst_usage_controller.cpp)
target_link_libraries(usage-controller-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(usage-controller-test)
add_test(NAME usage-controller COMMAND usage-controller-test)

add_executable(dashboard-snapshot-test tst_dashboard_snapshot.cpp)
target_link_libraries(dashboard-snapshot-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(dashboard-snapshot-test)
target_compile_definitions(
    dashboard-snapshot-test
    PRIVATE FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dashboard-v1.json"
)

add_test(NAME dashboard-snapshot COMMAND dashboard-snapshot-test)

find_program(QMLTESTRUNNER_EXECUTABLE qmltestrunner HINTS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
if(QMLTESTRUNNER_EXECUTABLE)
    add_test(
        NAME visual-primitives
        COMMAND "${QMLTESTRUNNER_EXECUTABLE}" -input "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    )
    set_tests_properties(
        visual-primitives
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    )
endif()
