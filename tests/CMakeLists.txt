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

add_executable(presentation-formatter-test tst_presentation_formatter.cpp)
target_link_libraries(presentation-formatter-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(presentation-formatter-test)
add_test(NAME presentation-formatter COMMAND presentation-formatter-test)

add_executable(provider-selection-model-test tst_provider_selection_model.cpp)
target_link_libraries(provider-selection-model-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(provider-selection-model-test)
add_test(NAME provider-selection-model COMMAND provider-selection-model-test)

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
