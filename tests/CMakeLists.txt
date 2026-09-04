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

add_executable(claude-credentials-test tst_claude_credentials.cpp)
target_link_libraries(claude-credentials-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(claude-credentials-test)
add_test(NAME claude-credentials COMMAND claude-credentials-test)

add_executable(claude-provider-adapter-test tst_claude_provider_adapter.cpp)
target_link_libraries(claude-provider-adapter-test PRIVATE kodometer-core Qt6::Network Qt6::Test)
kodometer_enable_warnings(claude-provider-adapter-test)
add_test(NAME claude-provider-adapter COMMAND claude-provider-adapter-test)

add_executable(claude-usage-parser-test tst_claude_usage_parser.cpp)
target_link_libraries(claude-usage-parser-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(claude-usage-parser-test)
add_test(NAME claude-usage-parser COMMAND claude-usage-parser-test)

add_executable(gemini-credentials-test tst_gemini_credentials.cpp)
target_link_libraries(gemini-credentials-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(gemini-credentials-test)
add_test(NAME gemini-credentials COMMAND gemini-credentials-test)

add_executable(gemini-provider-adapter-test tst_gemini_provider_adapter.cpp)
target_link_libraries(gemini-provider-adapter-test PRIVATE kodometer-core Qt6::Network Qt6::Test)
kodometer_enable_warnings(gemini-provider-adapter-test)
add_test(NAME gemini-provider-adapter COMMAND gemini-provider-adapter-test)

add_executable(gemini-usage-parser-test tst_gemini_usage_parser.cpp)
target_link_libraries(gemini-usage-parser-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(gemini-usage-parser-test)
add_test(NAME gemini-usage-parser COMMAND gemini-usage-parser-test)

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
