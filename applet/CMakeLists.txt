plasma_add_applet(org.kyaulabs.kodometer
    QML_SOURCES
        CompactMeter.qml
        OverviewPage.qml
        ProviderDetails.qml
        ProviderSummary.qml
        ProviderTabs.qml
        UsageBar.qml
        WindowRow.qml
        main.qml
    CPP_SOURCES
        ../include/kodometer/codex_credentials.hpp
        ../include/kodometer/codex_provider_adapter.hpp
        ../include/kodometer/codex_usage_parser.hpp
        ../include/kodometer/claude_credentials.hpp
        ../include/kodometer/claude_provider_adapter.hpp
        ../include/kodometer/claude_usage_parser.hpp
        ../include/kodometer/presentation_formatter.hpp
        ../include/kodometer/provider_adapter.hpp
        ../include/kodometer/provider_selection_model.hpp
        ../include/kodometer/usage_controller.hpp
        ../src/codex_credentials.cpp
        ../src/codex_provider_adapter.cpp
        ../src/codex_usage_parser.cpp
        ../src/claude_credentials.cpp
        ../src/claude_provider_adapter.cpp
        ../src/claude_usage_parser.cpp
        ../src/presentation_formatter.cpp
        ../src/provider_selection_model.cpp
        ../src/usage_controller.cpp
    GENERATE_APPLET_CLASS
)

target_include_directories(org.kyaulabs.kodometer PRIVATE ../include ../include/kodometer)
target_link_libraries(
    org.kyaulabs.kodometer PRIVATE Qt6::Core Qt6::Network Qt6::Qml Plasma::Plasma
)
target_compile_features(org.kyaulabs.kodometer PRIVATE cxx_std_20)
kodometer_enable_warnings(org.kyaulabs.kodometer)
