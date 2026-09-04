plasma_add_applet(org.kyaulabs.codexbar
    QML_SOURCES
        CompactMeter.qml
        ProviderCard.qml
        UsageBar.qml
        main.qml
    CPP_SOURCES
        ../include/codexbar/dashboard_controller.hpp
        ../include/codexbar/dashboard_snapshot.hpp
        ../src/dashboard_controller.cpp
        ../src/dashboard_snapshot.cpp
    GENERATE_APPLET_CLASS
)

target_include_directories(org.kyaulabs.codexbar PRIVATE ../include ../include/codexbar)
target_link_libraries(org.kyaulabs.codexbar PRIVATE Qt6::Core Qt6::Qml Plasma::Plasma)
target_compile_features(org.kyaulabs.codexbar PRIVATE cxx_std_20)
