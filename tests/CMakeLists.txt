add_executable(account-switch-model-test tst_account_switch_model.cpp)
target_link_libraries(account-switch-model-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(account-switch-model-test)
add_test(NAME account-switch-model COMMAND account-switch-model-test)

add_executable(wallet-accounts-test tst_wallet_accounts.cpp)
target_link_libraries(wallet-accounts-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(wallet-accounts-test)
add_test(NAME wallet-accounts COMMAND wallet-accounts-test)

add_executable(oauth-profiles-test tst_oauth_profiles.cpp)
target_link_libraries(oauth-profiles-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(oauth-profiles-test)
add_test(NAME oauth-profiles COMMAND oauth-profiles-test)

add_executable(cost-history-model-test tst_cost_history_model.cpp)
target_link_libraries(cost-history-model-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(cost-history-model-test)
add_test(NAME cost-history-model COMMAND cost-history-model-test)

add_executable(quota-alert-policy-test tst_quota_alert_policy.cpp)
target_link_libraries(quota-alert-policy-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(quota-alert-policy-test)
add_test(NAME quota-alert-policy COMMAND quota-alert-policy-test)

add_executable(provider-actions-test tst_provider_actions.cpp)
target_link_libraries(provider-actions-test PRIVATE kodometer-core Qt6::Test)
kodometer_enable_warnings(provider-actions-test)
add_test(NAME provider-actions COMMAND provider-actions-test)
set_tests_properties(provider-actions PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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

if(KODOMETER_BUILD_APPLET)
    find_package(Qt6 6.4 REQUIRED COMPONENTS DBus)
    find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
    add_executable(quota-notifier-test tst_quota_notifier.cpp
        ../src/quota_notifier.cpp ../include/kodometer/quota_notifier.hpp)
    target_link_libraries(quota-notifier-test PRIVATE kodometer-core Qt6::Test Qt6::DBus KF6::Notifications)
    kodometer_enable_warnings(quota-notifier-test)
    configure_file(../applet/kodometer.notifyrc
        notification-data/knotifications6/kodometer.notifyrc COPYONLY)
    add_test(NAME quota-notifier COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- $<TARGET_FILE:quota-notifier-test>)
    set_tests_properties(quota-notifier PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;KODOMETER_TEST_PRIVATE_BUS=1;XDG_DATA_DIRS=${CMAKE_CURRENT_BINARY_DIR}/notification-data:/usr/share")

    add_test(NAME staged-install COMMAND "${CMAKE_COMMAND}"
        "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DTEST_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
        "-DTEST_ECM_DIR=${ECM_DIR}"
        "-DTEST_PLUGIN=$<TARGET_FILE:org.kyaulabs.kodometer>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/test_install.cmake")

    add_executable(applet-configuration-test tst_applet_configuration.cpp)
    target_link_libraries(applet-configuration-test PRIVATE Qt6::Test Qt6::Quick Qt6::Qml KF6::ConfigCore KF6::ConfigGui KF6::ConfigQml)
    target_compile_definitions(applet-configuration-test PRIVATE
        KODOMETER_APPLET_LIBRARY="$<TARGET_FILE:org.kyaulabs.kodometer>")
    add_dependencies(applet-configuration-test org.kyaulabs.kodometer)
    kodometer_enable_warnings(applet-configuration-test)
    add_test(NAME applet-configuration COMMAND applet-configuration-test)
    set_tests_properties(applet-configuration PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

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
