# Feather PDF Pro - application target.
# Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).

qt_add_executable(feather-pdf
    app/main.cpp
    cli/Cli.cpp
    cli/Cli.h
    app/MainWindow.cpp
    app/MainWindow.h
    core/FeatherDocument.cpp
    core/FeatherDocument.h
    core/PageCommands.h
    backends/PdfEditor.cpp
    backends/PdfEditor.h
    backends/Annotator.cpp
    backends/Annotator.h
    backends/FormFiller.cpp
    backends/FormFiller.h
    backends/FormEditor.cpp
    backends/FormEditor.h
    backends/FormDetector.cpp
    backends/FormDetector.h
    backends/Xfdf.cpp
    backends/Xfdf.h
    backends/TextEditor.cpp
    backends/TextEditor.h
    backends/Signer.cpp
    backends/Signer.h
    backends/LtvSigner.cpp
    backends/LtvSigner.h
    backends/PdfIncrement.cpp
    backends/PdfIncrement.h
    backends/Converter.cpp
    backends/Converter.h
    backends/Ocr.cpp
    backends/Ocr.h
    backends/OcrPreprocess.cpp
    backends/OcrPreprocess.h
    backends/Comparer.cpp
    backends/Comparer.h
    backends/Watermarker.cpp
    backends/Watermarker.h
    backends/BatchRunner.cpp
    backends/WatchFolder.cpp
    backends/BatchRunner.h
    backends/Flattener.cpp
    backends/Flattener.h
    backends/Splitter.cpp
    backends/Splitter.h
    backends/CmykConverter.cpp
    backends/CmykConverter.h
    backends/ImageExporter.cpp
    backends/ImageExporter.h
    backends/SnapshotExporter.cpp
    backends/SnapshotExporter.h
    backends/PatternRedactor.cpp
    backends/PatternRedactor.h
    backends/Sanitizer.cpp
    backends/Sanitizer.h
    backends/PdfA.cpp
    backends/PdfA.h
    backends/Optimizer.cpp
    backends/Optimizer.h
    backends/LinkEditor.cpp
    backends/LinkEditor.h
    backends/TextComparer.cpp
    backends/TextComparer.h
    backends/Measurer.cpp
    backends/Measurer.h
    backends/StampLibrary.cpp
    backends/StampLibrary.h
    backends/ToolLocator.cpp
    backends/ToolLocator.h
    backends/ReadAloud.cpp
    backends/ReadAloud.h
    backends/Scanner.cpp
    backends/Scanner.h
    ui/Theme.cpp
    ui/Theme.h
    ui/Viewport.cpp
    ui/Viewport.h
    ui/PageView.cpp
    ui/PageView.h
    ui/HomeView.cpp
    ui/HomeView.h
    ui/ThumbnailPanel.cpp
    ui/ThumbnailPanel.h
    ui/OutlinePanel.cpp
    ui/OutlinePanel.h
    ui/AnnotationsPanel.cpp
    ui/AnnotationsPanel.h
    ui/AttachmentsPanel.cpp
    ui/AttachmentsPanel.h
    ui/LayersPanel.cpp
    ui/LayersPanel.h
    ui/FormPanel.cpp
    ui/FormPanel.h
    ui/SignDialog.cpp
    ui/SignDialog.h
    ui/SignaturesDialog.cpp
    ui/SignaturesDialog.h
    ui/OcrDialog.cpp
    ui/OcrDialog.h
    ui/DocsView.cpp
    ui/DocsView.h
    ui/WatermarkDialog.cpp
    ui/WatermarkDialog.h
    ui/BatesDialog.cpp
    ui/BatesDialog.h
    ui/FlattenDialog.cpp
    ui/FlattenDialog.h
    ui/SplitDialog.cpp
    ui/SplitDialog.h
    ui/ExtractDialog.cpp
    ui/ExtractDialog.h
    ui/ExportImageDialog.cpp
    ui/ExportImageDialog.h
    ui/SnapshotDialog.cpp
    ui/SnapshotDialog.h
    ui/FindRedactDialog.cpp
    ui/FindRedactDialog.h
    ui/HeaderFooterDialog.cpp
    ui/HeaderFooterDialog.h
    ui/CompareReportDialog.cpp
    ui/CompareReportDialog.h
    ui/SanitizeDialog.cpp
    ui/SanitizeDialog.h
    ui/BatchDialog.cpp
    ui/BatchDialog.h
    ui/StepConfigDialog.cpp
    ui/StepConfigDialog.h
    ui/PdfADialog.cpp
    ui/PdfADialog.h
    ui/OptimizeDialog.cpp
    ui/OptimizeDialog.h
    ui/LinkUrlDialog.cpp
    ui/LinkUrlDialog.h
    ui/StampDialog.cpp
    ui/StampDialog.h
    ui/LinksDialog.cpp
    ui/LinksDialog.h
    ui/InsertDialog.cpp
    ui/InsertDialog.h
    ui/CropDialog.cpp
    ui/CropDialog.h
    ui/FormFieldDialog.cpp
    ui/FormFieldDialog.h
    ui/PrepareFormDialog.cpp
    ui/PrepareFormDialog.h
    ui/TextEditDialog.cpp
    ui/TextEditDialog.h
    ui/CustomizeToolsDialog.cpp
    ui/CustomizeToolsDialog.h
    ui/PreferencesDialog.cpp
    ui/PreferencesDialog.h
    ui/TabStrip.cpp
    ui/TabStrip.h
    ui/CommandBar.cpp
    ui/CommandBar.h
    ui/FindBar.cpp
    ui/FindBar.h
    ui/RedactionBar.cpp
    ui/RedactionBar.h
    ui/AnnotationBar.cpp
    ui/AnnotationBar.h
    ui/MeasureBar.cpp
    ui/MeasureBar.h
    ui/ReadAloudBar.cpp
    ui/ReadAloudBar.h
    ui/ScanDialog.cpp
    ui/ScanDialog.h
    ui/NoteDialog.cpp
    ui/NoteDialog.h
    ui/PrintDialog.cpp
    ui/PrintDialog.h
    ui/CombineDialog.cpp
    ui/CombineDialog.h
    ui/ProtectDialog.cpp
    ui/ProtectDialog.h
    ui/PasswordDialog.cpp
    ui/PasswordDialog.h
    ui/GoToPageDialog.cpp
    ui/GoToPageDialog.h
    ui/NavigationRail.cpp
    ui/NavigationRail.h
    ui/ToolsPane.cpp
    ui/ToolsPane.h
    ui/FloatingPill.cpp
    ui/FloatingPill.h
    ui/Toast.cpp
    ui/Toast.h
    # Embedded symbolic icons (lucide SVGs → ":/icons/<name>.svg"); AUTORCC compiles it.
    ${CMAKE_SOURCE_DIR}/resources/feather.qrc
)

# Allow includes like "core/FeatherDocument.h" from anywhere in the tree.
target_include_directories(feather-pdf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(feather-pdf PRIVATE
    FEATHERPDF_VERSION="${PROJECT_VERSION}"
    FEATHERPDF_APP_ID="io.github.s4rt4.FeatherPDF"
)

target_link_libraries(feather-pdf PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Pdf
    Qt6::PdfWidgets
    Qt6::Svg
    Qt6::PrintSupport
    Qt6::Concurrent
    Qt6::TextToSpeech
    feather::qpdf
    feather::poppler-qt6
    # WIA scanning (Scanner backend)
    wiaguid
    shlwapi
    ole32
    oleaut32
    # CNG/CryptoAPI signing (Signer backend)
    crypt32
)

# Localization. lrelease compiles the .ts into .qm and qt_add_translations embeds
# them as resources under :/i18n; the `update_translations` target re-runs lupdate.
# English is the source language, so feather_en.ts is an identity (empty) catalog.
qt_add_translations(feather-pdf
    TS_FILES
        ${CMAKE_SOURCE_DIR}/translations/feather_id.ts
        ${CMAKE_SOURCE_DIR}/translations/feather_en.ts
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete
)

install(TARGETS feather-pdf RUNTIME DESTINATION bin)

# Explorer thumbnail provider. A COM DLL Explorer loads into its DllHost
# surrogate, so it must not depend on the app: page one is rendered with the
# Windows.Data.Pdf classes that ship with Windows 10+, never Qt/Poppler.
# The installer registers it; see packaging/windows/feather-pdf-pro.iss.
if(WIN32)
    add_library(feather-thumb SHARED
        shellext/ThumbnailProvider.cpp
        shellext/feather-thumb.def
    )
    target_link_libraries(feather-thumb PRIVATE
        WindowsApp   # C++/WinRT activation
        shcore       # IStream <-> IRandomAccessStream interop
        shlwapi      # QISearch, SHCreateMemStream
        d3d11
        gdi32
        ole32
        advapi32
    )
    install(TARGETS feather-thumb RUNTIME DESTINATION bin)
endif()

# Bundle the Qt (and other) runtime DLLs next to the exe on install/cpack, so
# the ZIP is portable. windeployqt also copies the platform/imageformat plugins.
if(WIN32)
    qt_generate_deploy_app_script(
        TARGET feather-pdf
        OUTPUT_SCRIPT deploy_script
        NO_TRANSLATIONS
    )
    install(SCRIPT ${deploy_script})

    # windeployqt deploys the plugins a GUI app needs, which does not include
    # the off-screen platform — but that is exactly the one the headless CLI
    # switches to (see src/app/main.cpp), and the installer wires the Explorer
    # right-click actions to that CLI. Ship it explicitly or every one of them
    # dies with "no Qt platform plugin could be initialized".
    #
    # QT_DEPLOY_PLUGINS_DIR only exists inside the generated deploy script, so
    # mirror its default here — the same "plugins" directory bin/qt.conf points
    # at. An empty destination would land the DLL at the root of the drive.
    if(NOT QT_DEPLOY_PLUGINS_DIR)
        set(QT_DEPLOY_PLUGINS_DIR "plugins")
    endif()
    install(FILES "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}/platforms/qoffscreen.dll"
            DESTINATION "${QT_DEPLOY_PLUGINS_DIR}/platforms")

    # windeployqt only ever copies Qt's own libraries, but the app also loads
    # poppler-qt6, qpdf and the vcpkg-built image/crypto libraries underneath
    # them. Walk the real import tables instead of listing DLL names by hand:
    # every dependency resolves out of the <prefix>/bin directories the build
    # was configured against, and anything that resolves to System32 is a
    # Windows DLL we must not ship.
    #
    # Qt's own bin directory is deliberately left out: windeployqt has already
    # put those DLLs beside the exe, and searching both places makes
    # file(GET_RUNTIME_DEPENDENCIES) fail with a conflict for every Qt library.
    set(feather_dll_dirs "")
    foreach(prefix IN LISTS CMAKE_PREFIX_PATH)
        if(IS_DIRECTORY "${prefix}/bin" AND NOT EXISTS "${prefix}/bin/Qt6Core.dll")
            list(APPEND feather_dll_dirs "${prefix}/bin")
        endif()
    endforeach()

    install(CODE "
        set(dll_dirs \"${feather_dll_dirs}\")
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES \"\${CMAKE_INSTALL_PREFIX}/bin/feather-pdf.exe\"
            LIBRARIES \"\${CMAKE_INSTALL_PREFIX}/bin/feather-thumb.dll\"
            RESOLVED_DEPENDENCIES_VAR resolved
            UNRESOLVED_DEPENDENCIES_VAR unresolved
            DIRECTORIES \${dll_dirs}
            PRE_EXCLUDE_REGEXES \"^api-ms-\" \"^ext-ms-\"
            POST_EXCLUDE_REGEXES \"[Ss]ystem32\"
        )
        foreach(dep IN LISTS resolved)
            file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/bin\"
                 TYPE SHARED_LIBRARY FOLLOW_SYMLINK_CHAIN FILES \"\${dep}\")
        endforeach()
        # Qt libraries come out unresolved because they sit beside the exe
        # rather than in a searched directory; only warn about the rest.
        foreach(dep IN LISTS unresolved)
            if(NOT EXISTS \"\${CMAKE_INSTALL_PREFIX}/bin/\${dep}\")
                message(STATUS \"Not bundled (assumed system DLL): \${dep}\")
            endif()
        endforeach()
    ")
endif()

# Windows shell integration (file association, "Open with Feather PDF Pro",
# Explorer right-click actions) is written to the registry by the installer —
# see packaging/windows — not by the build.
