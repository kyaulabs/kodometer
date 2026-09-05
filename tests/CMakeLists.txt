add_executable(credential-store-test tst_credential_store.cpp)
target_link_libraries(credential-store-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(credential-store-test)
add_test(NAME credential-store COMMAND credential-store-test)

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

add_executable(deepseek-credentials-test tst_deepseek_credentials.cpp)
target_link_libraries(deepseek-credentials-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(deepseek-credentials-test)
add_test(NAME deepseek-credentials COMMAND deepseek-credentials-test)

add_executable(deepseek-provider-adapter-test tst_deepseek_provider_adapter.cpp)
target_link_libraries(deepseek-provider-adapter-test PRIVATE kodometer-core Qt6::Network Qt6::Test)
kodometer_enable_warnings(deepseek-provider-adapter-test)
add_test(NAME deepseek-provider-adapter COMMAND deepseek-provider-adapter-test)

add_executable(deepseek-usage-parser-test tst_deepseek_usage_parser.cpp)
target_link_libraries(deepseek-usage-parser-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(deepseek-usage-parser-test)
add_test(NAME deepseek-usage-parser COMMAND deepseek-usage-parser-test)

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

add_executable(kimi-credentials-test tst_kimi_credentials.cpp)
target_link_libraries(kimi-credentials-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(kimi-credentials-test)
add_test(NAME kimi-credentials COMMAND kimi-credentials-test)

add_executable(kimi-provider-adapter-test tst_kimi_provider_adapter.cpp)
target_link_libraries(kimi-provider-adapter-test PRIVATE kodometer-core Qt6::Network Qt6::Test)
kodometer_enable_warnings(kimi-provider-adapter-test)
add_test(NAME kimi-provider-adapter COMMAND kimi-provider-adapter-test)

add_executable(kimi-usage-parser-test tst_kimi_usage_parser.cpp)
target_link_libraries(kimi-usage-parser-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(kimi-usage-parser-test)
add_test(NAME kimi-usage-parser COMMAND kimi-usage-parser-test)

add_executable(xai-credentials-test tst_xai_credentials.cpp)
target_link_libraries(xai-credentials-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(xai-credentials-test)
add_test(NAME xai-credentials COMMAND xai-credentials-test)

add_executable(xai-provider-adapter-test tst_xai_provider_adapter.cpp)
target_link_libraries(xai-provider-adapter-test PRIVATE kodometer-core Qt6::Network Qt6::Test)
kodometer_enable_warnings(xai-provider-adapter-test)
add_test(NAME xai-provider-adapter COMMAND xai-provider-adapter-test)

add_executable(xai-usage-parser-test tst_xai_usage_parser.cpp)
target_link_libraries(xai-usage-parser-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(xai-usage-parser-test)
add_test(NAME xai-usage-parser COMMAND xai-usage-parser-test)

add_executable(zai-credentials-test tst_zai_credentials.cpp)
target_link_libraries(zai-credentials-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(zai-credentials-test)
add_test(NAME zai-credentials COMMAND zai-credentials-test)

add_executable(zai-provider-adapter-test tst_zai_provider_adapter.cpp)
target_link_libraries(zai-provider-adapter-test PRIVATE kodometer-core Qt6::Network Qt6::Test)
kodometer_enable_warnings(zai-provider-adapter-test)
add_test(NAME zai-provider-adapter COMMAND zai-provider-adapter-test)

add_executable(zai-usage-parser-test tst_zai_usage_parser.cpp)
target_link_libraries(zai-usage-parser-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(zai-usage-parser-test)
add_test(NAME zai-usage-parser COMMAND zai-usage-parser-test)

add_executable(openrouter-credentials-test tst_openrouter_credentials.cpp)
target_link_libraries(openrouter-credentials-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(openrouter-credentials-test)
add_test(NAME openrouter-credentials COMMAND openrouter-credentials-test)

add_executable(openrouter-provider-adapter-test tst_openrouter_provider_adapter.cpp)
target_link_libraries(openrouter-provider-adapter-test PRIVATE kodometer-core Qt6::Network Qt6::Test)
kodometer_enable_warnings(openrouter-provider-adapter-test)
add_test(NAME openrouter-provider-adapter COMMAND openrouter-provider-adapter-test)

add_executable(openrouter-usage-parser-test tst_openrouter_usage_parser.cpp)
target_link_libraries(openrouter-usage-parser-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(openrouter-usage-parser-test)
add_test(NAME openrouter-usage-parser COMMAND openrouter-usage-parser-test)

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
