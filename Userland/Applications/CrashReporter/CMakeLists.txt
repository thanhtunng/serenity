serenity_component(
    CrashReporter
    REQUIRED
    TARGETS CrashReporter
)

compile_gml(CrashReporterWindow.gml CrashReporterWindowGML.cpp)


set(SOURCES
    CrashReporterWindowGML.cpp
    main.cpp
)

serenity_app(CrashReporter ICON app-crash-reporter)
target_link_libraries(CrashReporter PRIVATE LibCore LibCoredump LibDebug LibDesktop LibFileSystem LibFileSystemAccessClient LibGfx LibGUI LibMain LibThreading)
