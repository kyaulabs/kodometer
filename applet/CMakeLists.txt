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
        ../include/kodometer/dashboard_controller.hpp
        ../include/kodometer/dashboard_snapshot.hpp
        ../include/kodometer/presentation_formatter.hpp
        ../include/kodometer/provider_selection_model.hpp
        ../src/dashboard_controller.cpp
        ../src/dashboard_snapshot.cpp
        ../src/presentation_formatter.cpp
        ../src/provider_selection_model.cpp
    GENERATE_APPLET_CLASS
)

target_include_directories(org.kyaulabs.kodometer PRIVATE ../include ../include/kodometer)
target_link_libraries(org.kyaulabs.kodometer PRIVATE Qt6::Core Qt6::Qml Plasma::Plasma)
target_compile_features(org.kyaulabs.kodometer PRIVATE cxx_std_20)
kodometer_enable_warnings(org.kyaulabs.kodometer)
