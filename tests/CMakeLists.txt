# Feather PDF — tests.
# Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).

qt_add_executable(test_pdfeditor
    test_pdfeditor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/PdfEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/PdfEditor.h
)

target_include_directories(test_pdfeditor PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Linking Qt6::Gui (not Widgets) makes QTEST_MAIN use QGuiApplication, which runs
# headless under the offscreen platform plugin (set in the test environment).
target_link_libraries(test_pdfeditor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
)

add_test(NAME pdfeditor COMMAND test_pdfeditor)
set_tests_properties(pdfeditor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_formeditor
    test_formeditor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/FormEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/FormEditor.h
)
target_include_directories(test_formeditor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_formeditor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
)
add_test(NAME formeditor COMMAND test_formeditor)
set_tests_properties(formeditor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_formdetector
    test_formdetector.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/FormDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/FormDetector.h
)
target_include_directories(test_formdetector PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_formdetector PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::POPPLER_QT6
)
add_test(NAME formdetector COMMAND test_formdetector)
set_tests_properties(formdetector PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_readaloud
    test_readaloud.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ReadAloud.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ReadAloud.h
)
target_include_directories(test_readaloud PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_readaloud PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::POPPLER_QT6
)
add_test(NAME readaloud COMMAND test_readaloud)
set_tests_properties(readaloud PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_scanner
    test_scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Scanner.h
)
target_include_directories(test_scanner PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_scanner PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME scanner COMMAND test_scanner)
set_tests_properties(scanner PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_annotator
    test_annotator.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Annotator.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Annotator.h
)
target_include_directories(test_annotator PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_annotator PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
)
add_test(NAME annotator COMMAND test_annotator)
set_tests_properties(annotator PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_formroundtrip
    test_formroundtrip.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/FormEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/FormEditor.h
    ${CMAKE_SOURCE_DIR}/src/backends/FormFiller.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/FormFiller.h
)
target_include_directories(test_formroundtrip PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_formroundtrip PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
    PkgConfig::POPPLER_QT6
)
add_test(NAME formroundtrip COMMAND test_formroundtrip)
set_tests_properties(formroundtrip PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_texteditor
    test_texteditor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/TextEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/TextEditor.h
    ${CMAKE_SOURCE_DIR}/src/backends/Annotator.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Annotator.h
)
target_include_directories(test_texteditor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_texteditor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
)
add_test(NAME texteditor COMMAND test_texteditor)
set_tests_properties(texteditor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_xfdf
    test_xfdf.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Xfdf.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Xfdf.h
    ${CMAKE_SOURCE_DIR}/src/backends/FormFiller.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/FormFiller.h
    ${CMAKE_SOURCE_DIR}/src/backends/FormEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/FormEditor.h
)
target_include_directories(test_xfdf PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_xfdf PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
    PkgConfig::POPPLER_QT6
)
add_test(NAME xfdf COMMAND test_xfdf)
set_tests_properties(xfdf PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_document
    test_document.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FeatherDocument.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FeatherDocument.h
)
target_include_directories(test_document PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_document PRIVATE Qt6::Core Qt6::Gui Qt6::Pdf Qt6::Test)
add_test(NAME document COMMAND test_document)
set_tests_properties(document PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_converter
    test_converter.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Converter.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Converter.h
)
target_include_directories(test_converter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_converter PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::POPPLER_QT6
)
add_test(NAME converter COMMAND test_converter)
set_tests_properties(converter PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_imageexporter
    test_imageexporter.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ImageExporter.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ImageExporter.h
)
target_include_directories(test_imageexporter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_imageexporter PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Pdf
    Qt6::Test
)
add_test(NAME imageexporter COMMAND test_imageexporter)
set_tests_properties(imageexporter PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_snapshotexporter
    test_snapshotexporter.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/SnapshotExporter.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/SnapshotExporter.h
)
target_include_directories(test_snapshotexporter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_snapshotexporter PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Pdf
    Qt6::Test
)
add_test(NAME snapshotexporter COMMAND test_snapshotexporter)
set_tests_properties(snapshotexporter PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_patternredactor
    test_patternredactor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/PatternRedactor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/PatternRedactor.h
)
target_include_directories(test_patternredactor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_patternredactor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::POPPLER_QT6
)
add_test(NAME patternredactor COMMAND test_patternredactor)
set_tests_properties(patternredactor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_sanitizer
    test_sanitizer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Sanitizer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Sanitizer.h
)
target_include_directories(test_sanitizer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_sanitizer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
)
add_test(NAME sanitizer COMMAND test_sanitizer)
set_tests_properties(sanitizer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_pdfa
    test_pdfa.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/PdfA.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/PdfA.h
)
target_include_directories(test_pdfa PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_pdfa PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
)
add_test(NAME pdfa COMMAND test_pdfa)
set_tests_properties(pdfa PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_optimizer
    test_optimizer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Optimizer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Optimizer.h
)
target_include_directories(test_optimizer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_optimizer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
)
add_test(NAME optimizer COMMAND test_optimizer)
set_tests_properties(optimizer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_linkeditor
    test_linkeditor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/LinkEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/LinkEditor.h
)
target_include_directories(test_linkeditor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_linkeditor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
)
add_test(NAME linkeditor COMMAND test_linkeditor)
set_tests_properties(linkeditor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_headerfooter
    test_headerfooter.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Watermarker.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Watermarker.h
)
target_include_directories(test_headerfooter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_headerfooter PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
    PkgConfig::POPPLER_QT6
)
add_test(NAME headerfooter COMMAND test_headerfooter)
set_tests_properties(headerfooter PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_textcomparer
    test_textcomparer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/TextComparer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/TextComparer.h
)
target_include_directories(test_textcomparer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_textcomparer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::POPPLER_QT6
)
add_test(NAME textcomparer COMMAND test_textcomparer)
set_tests_properties(textcomparer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_measurer
    test_measurer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Measurer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Measurer.h
)
target_include_directories(test_measurer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_measurer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)
add_test(NAME measurer COMMAND test_measurer)
set_tests_properties(measurer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_stamplibrary
    test_stamplibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/StampLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/StampLibrary.h
)
target_include_directories(test_stamplibrary PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_stamplibrary PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::QPDF
)
add_test(NAME stamplibrary COMMAND test_stamplibrary)
set_tests_properties(stamplibrary PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_ocrpreprocess
    test_ocrpreprocess.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/OcrPreprocess.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/OcrPreprocess.h
)
target_include_directories(test_ocrpreprocess PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_ocrpreprocess PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)
add_test(NAME ocrpreprocess COMMAND test_ocrpreprocess)
set_tests_properties(ocrpreprocess PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_signer
    test_signer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Signer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Signer.h
    ${CMAKE_SOURCE_DIR}/src/backends/LtvSigner.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/LtvSigner.h
)
target_include_directories(test_signer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_signer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
    PkgConfig::POPPLER_QT6
    PkgConfig::QPDF
)
add_test(NAME signer COMMAND test_signer)
set_tests_properties(signer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_batchrunner
    test_batchrunner.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/BatchRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/BatchRunner.h
    ${CMAKE_SOURCE_DIR}/src/backends/PdfEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Optimizer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Sanitizer.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Watermarker.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Flattener.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/PdfA.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/Ocr.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/OcrPreprocess.cpp
)
target_include_directories(test_batchrunner PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_batchrunner PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Pdf
    Qt6::Test
    PkgConfig::QPDF
    PkgConfig::POPPLER_QT6
)
add_test(NAME batchrunner COMMAND test_batchrunner)
set_tests_properties(batchrunner PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_cli test_cli.cpp)
target_include_directories(test_cli PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_cli PRIVATE FEATHERPDF_BIN="$<TARGET_FILE:feather-pdf>"
    FEATHERPDF_ACTION="${CMAKE_SOURCE_DIR}/packaging/feather-pdf-action"
    FEATHERPDF_CUPS_BACKEND="${CMAKE_SOURCE_DIR}/packaging/cups/feather-pdf-print")
target_link_libraries(test_cli PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_dependencies(test_cli feather-pdf)
add_test(NAME cli COMMAND test_cli)
set_tests_properties(cli PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(test_translations test_translations.cpp)
target_include_directories(test_translations PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_translations PRIVATE
    TRANSLATIONS_TS="${CMAKE_SOURCE_DIR}/translations/feather_id.ts")
target_link_libraries(test_translations PRIVATE Qt6::Core Qt6::Test)
add_test(NAME translations COMMAND test_translations)
