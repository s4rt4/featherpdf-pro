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
    ui/SecurityDevicesDialog.cpp
    ui/SecurityDevicesDialog.h
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

# Bundle the Qt (and other) runtime DLLs next to the exe on install/cpack, so
# the ZIP is portable. windeployqt also copies the platform/imageformat plugins.
if(WIN32)
    qt_generate_deploy_app_script(
        TARGET feather-pdf
        OUTPUT_SCRIPT deploy_script
        NO_TRANSLATIONS
    )
    install(SCRIPT ${deploy_script})
endif()

# Windows shell integration (file association, "Open with Feather PDF Pro",
# Explorer right-click actions) is written to the registry by the installer —
# see packaging/windows — not by the build.
