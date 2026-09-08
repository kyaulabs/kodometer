plasma_add_applet(org.kyaulabs.kodometer
    QML_SOURCES
        AccountSwitching.qml
        AccountSwitchDialog.qml
        AccountSelector.qml
        UsageNavigation.qml
        BrandPalette.qml
        CompactMeter.qml
        QuotaRing.qml
        ProviderIcon.qml
        CostHistory.qml
        DetailSection.qml
        OverviewPage.qml
        ProviderActionRow.qml
        ProviderDetails.qml
        ProviderSummary.qml
        ProviderTabs.qml
        UsageBar.qml
        WindowRow.qml
        config/ConfigCredentials.qml
        config/ConfigGeneral.qml
        config/ConfigProfiles.qml
        config/ConfigWalletAccounts.qml
        config/WalletAccountEditor.qml
        config/ProfileEditor.qml
        config.qml
        main.qml
    RESOURCES
        main.xml
    CPP_SOURCES
        ../include/kodometer/codex_credentials.hpp
        ../include/kodometer/codex_provider_adapter.hpp
        ../include/kodometer/codex_usage_parser.hpp
        ../include/kodometer/credential_store.hpp
        ../include/kodometer/wallet_accounts.hpp
        ../include/kodometer/account_switch_model.hpp
        ../include/kodometer/kwallet_credential_store.hpp
        ../include/kodometer/claude_credentials.hpp
        ../include/kodometer/claude_provider_adapter.hpp
        ../include/kodometer/claude_usage_parser.hpp
        ../include/kodometer/deepseek_credentials.hpp
        ../include/kodometer/deepseek_provider_adapter.hpp
        ../include/kodometer/deepseek_usage_parser.hpp
        ../include/kodometer/gemini_credentials.hpp
        ../include/kodometer/gemini_provider_adapter.hpp
        ../include/kodometer/gemini_usage_parser.hpp
        ../include/kodometer/kimi_credentials.hpp
        ../include/kodometer/kimi_provider_adapter.hpp
        ../include/kodometer/kimi_usage_parser.hpp
        ../include/kodometer/openrouter_credentials.hpp
        ../include/kodometer/openrouter_provider_adapter.hpp
        ../include/kodometer/openrouter_usage_parser.hpp
        ../include/kodometer/xai_credentials.hpp
        ../include/kodometer/xai_provider_adapter.hpp
        ../include/kodometer/xai_usage_parser.hpp
        ../include/kodometer/zai_credentials.hpp
        ../include/kodometer/zai_provider_adapter.hpp
        ../include/kodometer/zai_usage_parser.hpp
        ../include/kodometer/oauth_profiles.hpp
        ../include/kodometer/cost_history_model.hpp
        ../include/kodometer/presentation_formatter.hpp
        ../include/kodometer/provider_actions.hpp
        ../include/kodometer/provider_adapter.hpp
        ../include/kodometer/provider_selection_model.hpp
        ../include/kodometer/usage_controller.hpp
        ../include/kodometer/quota_alert_policy.hpp
        ../include/kodometer/quota_notifier.hpp
        ../src/codex_credentials.cpp
        ../src/codex_provider_adapter.cpp
        ../src/codex_usage_parser.cpp
        ../src/credential_store.cpp
        ../src/wallet_accounts.cpp
        ../src/account_switch_model.cpp
        ../src/kwallet_credential_store.cpp
        ../src/claude_credentials.cpp
        ../src/claude_provider_adapter.cpp
        ../src/claude_usage_parser.cpp
        ../src/deepseek_credentials.cpp
        ../src/deepseek_provider_adapter.cpp
        ../src/deepseek_usage_parser.cpp
        ../src/gemini_credentials.cpp
        ../src/gemini_provider_adapter.cpp
        ../src/gemini_usage_parser.cpp
        ../src/kimi_credentials.cpp
        ../src/kimi_provider_adapter.cpp
        ../src/kimi_usage_parser.cpp
        ../src/openrouter_credentials.cpp
        ../src/openrouter_provider_adapter.cpp
        ../src/openrouter_usage_parser.cpp
        ../src/xai_credentials.cpp
        ../src/xai_provider_adapter.cpp
        ../src/xai_usage_parser.cpp
        ../src/zai_credentials.cpp
        ../src/zai_provider_adapter.cpp
        ../src/zai_usage_parser.cpp
        ../src/oauth_profiles.cpp
        ../src/cost_history_model.cpp
        ../src/presentation_formatter.cpp
        ../src/provider_actions.cpp
        ../src/provider_adapter.cpp
        ../src/provider_selection_model.cpp
        ../src/usage_controller.cpp
        ../src/quota_alert_policy.cpp
        ../src/quota_notifier.cpp
    GENERATE_APPLET_CLASS
)

# ECM otherwise flattens resource names. Keep source and compiled asset URLs identical.
ecm_target_qml_sources(org.kyaulabs.kodometer PATH assets SOURCES
    assets/kodometer-symbolic.svg
    assets/kodometer-iris-on-dark.svg
    assets/kodometer-deep-iris-on-light.svg
)
ecm_target_qml_sources(org.kyaulabs.kodometer PATH assets/providers SOURCES
    assets/providers/codex.svg
    assets/providers/claude.svg
    assets/providers/deepseek.svg
    assets/providers/gemini.svg
    assets/providers/kimi.svg
    assets/providers/openrouter.svg
    assets/providers/xai.svg
    assets/providers/zai.svg
)

# A metadata-only edit must recompile the generated factory in the same build,
# not only refresh its moc file and leave the previous metadata in the plugin.
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/org.kyaulabs.kodometer.cpp"
    PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json")

target_include_directories(org.kyaulabs.kodometer PRIVATE ../include ../include/kodometer)
target_link_libraries(
    org.kyaulabs.kodometer PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml KF6::Notifications KF6::Wallet Plasma::Plasma
)
target_compile_definitions(org.kyaulabs.kodometer PRIVATE KODOMETER_VERSION="${PROJECT_VERSION}")
target_compile_features(org.kyaulabs.kodometer PRIVATE cxx_std_20)
kodometer_enable_warnings(org.kyaulabs.kodometer)
install(FILES kodometer.notifyrc DESTINATION "${KDE_INSTALL_KNOTIFYRCDIR}")
install(FILES ../branding/kodometer-brand-pack/icons/app-tile-dark.svg
    DESTINATION "${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps" RENAME kodometer.svg)
