// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "app/MainWindow.h"

#include "backends/PdfEditor.h"
#include "backends/Annotator.h"
#include "backends/CmykConverter.h"
#include "backends/Comparer.h"
#include "backends/Converter.h"
#include "backends/ImageExporter.h"
#include "backends/SnapshotExporter.h"
#include "backends/LinkEditor.h"
#include "backends/LtvSigner.h"
#include "backends/Optimizer.h"
#include "backends/PatternRedactor.h"
#include "backends/Sanitizer.h"
#include "backends/TextComparer.h"
#include "backends/Flattener.h"
#include "backends/FormDetector.h"
#include "backends/FormEditor.h"
#include "backends/FormFiller.h"
#include "backends/Splitter.h"
#include "backends/StampLibrary.h"
#include "backends/TextEditor.h"
#include "backends/Ocr.h"
#include "backends/Scanner.h"
#include "backends/Signer.h"
#include "backends/Watermarker.h"
#include "backends/Xfdf.h"
#include "core/FeatherDocument.h"
#include "core/PageCommands.h"
#include "ui/AnnotationBar.h"
#include "ui/AnnotationsPanel.h"
#include "ui/BatesDialog.h"
#include "ui/AttachmentsPanel.h"
#include "ui/CombineDialog.h"
#include "ui/CropDialog.h"
#include "ui/CommandBar.h"
#include "ui/DocsView.h"
#include "ui/ExportImageDialog.h"
#include "ui/SnapshotDialog.h"
#include "ui/ExtractDialog.h"
#include "ui/FindRedactDialog.h"
#include "ui/CompareReportDialog.h"
#include "ui/HeaderFooterDialog.h"
#include "ui/LinkUrlDialog.h"
#include "ui/LinksDialog.h"
#include "ui/OptimizeDialog.h"
#include "ui/BatchDialog.h"
#include "ui/SanitizeDialog.h"
#include "ui/PdfADialog.h"
#include "ui/StampDialog.h"
#include "ui/FlattenDialog.h"
#include "ui/FormFieldDialog.h"
#include "ui/PrepareFormDialog.h"
#include "ui/GoToPageDialog.h"
#include "ui/PreferencesDialog.h"
#include "ui/TextEditDialog.h"
#include "ui/InsertDialog.h"
#include "ui/FindBar.h"
#include "ui/FloatingPill.h"
#include "ui/ReadAloudBar.h"
#include "ui/FormPanel.h"
#include "ui/HomeView.h"
#include "ui/LayersPanel.h"
#include "ui/NavigationRail.h"
#include "ui/NoteDialog.h"
#include "ui/OcrDialog.h"
#include "ui/ScanDialog.h"
#include "ui/OutlinePanel.h"
#include "ui/PasswordDialog.h"
#include "ui/PrintDialog.h"
#include "ui/ProtectDialog.h"
#include "ui/MeasureBar.h"
#include "ui/RedactionBar.h"
#include "ui/SecurityDevicesDialog.h"
#include "ui/SignDialog.h"
#include "ui/SignaturesDialog.h"
#include "ui/SplitDialog.h"
#include "ui/WatermarkDialog.h"
#include "ui/TabStrip.h"
#include "ui/Theme.h"
#include "ui/ThumbnailPanel.h"
#include "ui/Toast.h"
#include "ui/ToolsPane.h"
#include "ui/Viewport.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDate>
#include <QEasingCurve>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QLocale>
#include <QBuffer>
#include <QFutureWatcher>
#include <QImage>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
#include <QPrinter>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUndoGroup>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {
constexpr auto kRecentFilesKey = "recentFiles";
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildActions();
    buildMenus();
    buildShell();
    wireSignals();

    resize(1180, 820);
    updateWindowTitle();
    updateChromeState();
    rebuildRecentMenu();
    showHome(); // land on the start screen until a document is opened
}

MainWindow::~MainWindow() = default;

void MainWindow::buildActions() {
    m_openAct = new QAction(tr("&Open…"), this);
    m_openAct->setShortcut(QKeySequence::Open);
    connect(m_openAct, &QAction::triggered, this, &MainWindow::openFileDialog);

    m_saveAct = new QAction(tr("&Save"), this);
    m_saveAct->setShortcut(QKeySequence::Save);
    connect(m_saveAct, &QAction::triggered, this, [this] { saveActive(); });

    m_saveAsAct = new QAction(tr("Save &As…"), this);
    m_saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAct, &QAction::triggered, this, [this] { saveActiveAs(); });

    m_protectAct = new QAction(tr("Pro&tect with Password…"), this);
    connect(m_protectAct, &QAction::triggered, this, &MainWindow::protectDocument);

    m_removeProtectionAct = new QAction(tr("&Remove Password…"), this);
    connect(m_removeProtectionAct, &QAction::triggered, this, &MainWindow::removeProtection);

    m_printAct = new QAction(tr("&Print…"), this);
    m_printAct->setShortcut(QKeySequence::Print);
    connect(m_printAct, &QAction::triggered, this, &MainWindow::printActive);

    // One undo history per document, tracked by a group so Undo/Redo always act
    // on the active tab (ui-guidelines §10).
    m_undoGroup = new QUndoGroup(this);
    m_undoAct = m_undoGroup->createUndoAction(this, tr("&Undo"));
    m_undoAct->setShortcut(QKeySequence::Undo);
    m_redoAct = m_undoGroup->createRedoAction(this, tr("&Redo"));
    m_redoAct->setShortcut(QKeySequence::Redo);

    m_rotateLeftAct = new QAction(tr("Rotate &Left"), this);
    m_rotateLeftAct->setShortcut(Qt::CTRL | Qt::Key_BracketLeft);
    connect(m_rotateLeftAct, &QAction::triggered, this, [this] { rotateActivePage(-90); });
    m_rotateRightAct = new QAction(tr("Rotate &Right"), this);
    m_rotateRightAct->setShortcut(Qt::CTRL | Qt::Key_BracketRight);
    connect(m_rotateRightAct, &QAction::triggered, this, [this] { rotateActivePage(90); });

    m_deletePageAct = new QAction(tr("&Delete Page"), this);
    connect(m_deletePageAct, &QAction::triggered, this, &MainWindow::deleteActivePage);

    m_extractPagesAct = new QAction(tr("&Extract Pages…"), this);
    connect(m_extractPagesAct, &QAction::triggered, this, &MainWindow::extractActivePages);

    m_insertPagesAct = new QAction(tr("&Insert Pages…"), this);
    connect(m_insertPagesAct, &QAction::triggered, this, &MainWindow::insertPagesIntoActive);

    m_cropPagesAct = new QAction(tr("&Crop Pages…"), this);
    connect(m_cropPagesAct, &QAction::triggered, this, &MainWindow::cropActivePages);

    m_addFieldAct = new QAction(tr("Add Form &Field…"), this);
    connect(m_addFieldAct, &QAction::triggered, this, &MainWindow::addFormField);

    m_closeAct = new QAction(tr("&Close"), this);
    m_closeAct->setShortcut(QKeySequence::Close);
    connect(m_closeAct, &QAction::triggered, this, &MainWindow::closeDocument);

    m_quitAct = new QAction(tr("&Quit"), this);
    m_quitAct->setShortcut(QKeySequence::Quit);
    // Route Quit through close() so it goes past the unsaved-changes guard.
    connect(m_quitAct, &QAction::triggered, this, &MainWindow::close);

    m_zoomInAct = new QAction(tr("Zoom &In"), this);
    m_zoomInAct->setShortcut(QKeySequence::ZoomIn);

    m_zoomOutAct = new QAction(tr("Zoom &Out"), this);
    m_zoomOutAct->setShortcut(QKeySequence::ZoomOut);

    m_zoomActualAct = new QAction(tr("&Actual Size"), this);
    m_zoomActualAct->setShortcut(Qt::CTRL | Qt::Key_0);

    m_fitWidthAct = new QAction(tr("Fit &Width"), this);
    m_fitWidthAct->setShortcut(Qt::CTRL | Qt::Key_1);

    m_fitPageAct = new QAction(tr("Fit &Page"), this);
    m_fitPageAct->setShortcut(Qt::CTRL | Qt::Key_2);

    // Page layout - a radio group (default Continuous).
    auto* layoutGroup = new QActionGroup(this);
    m_singlePageAct = new QAction(tr("&Single Page"), this);
    m_continuousAct = new QAction(tr("&Continuous"), this);
    m_twoUpAct = new QAction(tr("&Two Pages"), this);
    for (QAction* a : {m_singlePageAct, m_continuousAct, m_twoUpAct}) {
        a->setCheckable(true);
        layoutGroup->addAction(a);
    }
    m_continuousAct->setChecked(true);
    connect(m_singlePageAct, &QAction::triggered, this,
            [this] { m_viewport->setLayoutMode(PageView::LayoutMode::SinglePage); });
    connect(m_continuousAct, &QAction::triggered, this,
            [this] { m_viewport->setLayoutMode(PageView::LayoutMode::Continuous); });
    connect(m_twoUpAct, &QAction::triggered, this,
            [this] { m_viewport->setLayoutMode(PageView::LayoutMode::TwoUp); });

    m_toggleThemeAct = new QAction(tr("Toggle &Light / Dark"), this);
    connect(m_toggleThemeAct, &QAction::triggered, this, [] { Theme::instance().toggleMode(); });

    m_immersiveAct = new QAction(tr("&Immersive Reading"), this);
    m_immersiveAct->setCheckable(true);
    m_immersiveAct->setShortcut(Qt::Key_F11);
    connect(m_immersiveAct, &QAction::toggled, this, &MainWindow::setImmersive);
    // Esc leaves immersive reading. The action stays enabled so the shortcut works
    // even with the menu bar hidden.
    addAction(m_immersiveAct);
    auto* escape = new QAction(this);
    escape->setShortcut(Qt::Key_Escape);
    connect(escape, &QAction::triggered, this, [this] {
        if (m_immersive)
            m_immersiveAct->setChecked(false);
    });
    addAction(escape);

    m_aboutAct = new QAction(tr("&About Feather PDF"), this);
    connect(m_aboutAct, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::buildMenus() {
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_openAct);
    m_recentMenu = file->addMenu(tr("Open &Recent"));
    QAction* createAct = file->addAction(tr("Create PDF &from…"));
    connect(createAct, &QAction::triggered, this, &MainWindow::createPdf);
    m_exportAct = file->addAction(tr("&Export to…"));
    connect(m_exportAct, &QAction::triggered, this, &MainWindow::exportDocument);
    m_exportImagesAct = file->addAction(tr("Export Pages as &Images…"));
    connect(m_exportImagesAct, &QAction::triggered, this, &MainWindow::exportPagesAsImages);
    m_extractImagesAct = file->addAction(tr("Extract Embedded I&mages…"));
    connect(m_extractImagesAct, &QAction::triggered, this, &MainWindow::extractEmbeddedImages);
    file->addSeparator();
    file->addAction(m_saveAct);
    file->addAction(m_saveAsAct);
    file->addAction(m_protectAct);
    file->addAction(m_removeProtectionAct);
    file->addSeparator();
    QMenu* formData = file->addMenu(tr("Form &Data"));
    m_exportXfdfAct = formData->addAction(tr("&Export to XFDF…"));
    connect(m_exportXfdfAct, &QAction::triggered, this, &MainWindow::exportFormData);
    m_importXfdfAct = formData->addAction(tr("&Import from XFDF…"));
    connect(m_importXfdfAct, &QAction::triggered, this, &MainWindow::importFormData);
    file->addSeparator();
    file->addAction(m_printAct);
    file->addSeparator();
    file->addAction(m_closeAct);
    file->addSeparator();
    file->addAction(m_quitAct);

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_undoAct);
    edit->addAction(m_redoAct);
    edit->addSeparator();
    QAction* find = edit->addAction(tr("&Find…"));
    find->setShortcut(QKeySequence::Find);
    connect(find, &QAction::triggered, this, &MainWindow::openFind);
    edit->addSeparator();
    QAction* prefs = edit->addAction(tr("&Preferences…"));
    prefs->setShortcut(QKeySequence::Preferences);
    prefs->setMenuRole(QAction::PreferencesRole);
    connect(prefs, &QAction::triggered, this, &MainWindow::showPreferences);

    QMenu* view = menuBar()->addMenu(tr("&View"));
    view->addAction(m_zoomInAct);
    view->addAction(m_zoomOutAct);
    view->addAction(m_zoomActualAct);
    view->addSeparator();
    view->addAction(m_fitWidthAct);
    view->addAction(m_fitPageAct);
    view->addSeparator();
    QMenu* layoutMenu = view->addMenu(tr("Page &Layout"));
    layoutMenu->addAction(m_singlePageAct);
    layoutMenu->addAction(m_continuousAct);
    layoutMenu->addAction(m_twoUpAct);
    view->addSeparator();
    view->addAction(m_immersiveAct);
    view->addSeparator();
    view->addAction(m_toggleThemeAct);

    QMenu* document = menuBar()->addMenu(tr("&Document"));
    document->addAction(m_rotateLeftAct);
    document->addAction(m_rotateRightAct);
    document->addAction(m_deletePageAct);
    document->addAction(m_insertPagesAct);
    document->addAction(m_extractPagesAct);
    document->addAction(m_cropPagesAct);
    document->addSeparator();
    document->addAction(m_addFieldAct);
    QAction* editText = document->addAction(tr("Edit &Text…"));
    connect(editText, &QAction::triggered, this, &MainWindow::editTextBoxes);
    QAction* loDraw = document->addAction(tr("Open in &LibreOffice Draw…"));
    connect(loDraw, &QAction::triggered, this, &MainWindow::editInLibreOffice);
    QAction* addLinkAct = document->addAction(tr("Add &Link…"));
    connect(addLinkAct, &QAction::triggered, this, &MainWindow::addLink);
    QAction* editLinksAct = document->addAction(tr("Edit Lin&ks…"));
    connect(editLinksAct, &QAction::triggered, this, &MainWindow::editLinks);
    QAction* addStampAct = document->addAction(tr("Add S&tamp…"));
    connect(addStampAct, &QAction::triggered, this, &MainWindow::addStamp);
    document->addSeparator();
    QAction* props = document->addAction(tr("Properties…"));
    connect(props, &QAction::triggered, this, &MainWindow::showProperties);
    document->addSeparator();
    QAction* sign = document->addAction(tr("&Sign…"));
    connect(sign, &QAction::triggered, this, &MainWindow::signDocument);
    QAction* sigs = document->addAction(tr("Si&gnatures…"));
    connect(sigs, &QAction::triggered, this, &MainWindow::viewSignatures);
    document->addSeparator();
    QAction* ocr = document->addAction(tr("&Recognize Text (OCR)…"));
    connect(ocr, &QAction::triggered, this, &MainWindow::recognizeText);
    QAction* optimize = document->addAction(tr("&Optimize…"));
    connect(optimize, &QAction::triggered, this, &MainWindow::optimizeDocument);
    QAction* compare = document->addAction(tr("Co&mpare with…"));
    connect(compare, &QAction::triggered, this, &MainWindow::compareDocuments);
    QAction* compareText = document->addAction(tr("Compare &Text with…"));
    connect(compareText, &QAction::triggered, this, &MainWindow::compareText);
    QAction* headerFooter = document->addAction(tr("Header && &Footer…"));
    connect(headerFooter, &QAction::triggered, this, &MainWindow::addHeaderFooter);
    document->addSeparator();
    QAction* findRedact = document->addAction(tr("&Find && Redact…"));
    connect(findRedact, &QAction::triggered, this, &MainWindow::findAndRedact);
    QAction* sanitize = document->addAction(tr("Remove &Hidden Information…"));
    connect(sanitize, &QAction::triggered, this, &MainWindow::sanitizeDocument);
    QAction* pdfa = document->addAction(tr("PDF/A && &Preflight…"));
    connect(pdfa, &QAction::triggered, this, &MainWindow::convertToPdfA);

    QMenu* tools = menuBar()->addMenu(tr("&Tools"));
    // The Tools list grew long, so it is grouped into a handful of submenus by
    // task. (Batch / Action stays top-level — it composes the rest.)
    struct ToolItem {
        const char* id;
        QString label;
    };
    struct ToolGroup {
        QString title;
        QList<ToolItem> items;
    };
    const ToolGroup toolGroups[] = {
        {tr("&Create && Export"),
         {{"create", tr("Create PDF…")}, {"scan", tr("Scan…")}, {"export", tr("Export…")}}},
        {tr("&Text && Forms"),
         {{"ocr", tr("Recognize Text (OCR)…")},
          {"edit", tr("Edit Text…")},
          {"read-aloud", tr("Read Aloud…")},
          {"forms", tr("Forms")},
          {"prepare-form", tr("Prepare Form…")}}},
        {tr("&Pages"),
         {{"combine", tr("Combine…")}, {"split", tr("Split…")}, {"organize", tr("Organize")}}},
        {tr("&Review && Markup"),
         {{"comment", tr("Comment")},
          {"redact", tr("Redact")},
          {"snapshot", tr("Snapshot")},
          {"measure", tr("Measure")},
          {"compare", tr("Compare…")},
          {"stamp", tr("Stamp…")},
          {"watermark", tr("Watermark…")},
          {"bates", tr("Bates Numbering…")}}},
        {tr("&Optimize && Convert"),
         {{"optimize", tr("Optimize…")},
          {"cmyk", tr("RGB to CMYK…")},
          {"flatten", tr("Flatten…")},
          {"pdfa", tr("PDF/A && Preflight…")}}},
        {tr("&Security"),
         {{"protect", tr("Protect…")},
          {"sign", tr("Sign…")},
          {"ltv", tr("Long-Term Validation…")}}},
    };
    for (const ToolGroup& g : toolGroups) {
        QMenu* sub = tools->addMenu(g.title);
        for (const ToolItem& it : g.items) {
            const QString id = QString::fromLatin1(it.id);
            QAction* a = sub->addAction(it.label);
            connect(a, &QAction::triggered, this, [this, id] { activateTool(id); });
        }
    }
    tools->addSeparator();
    QAction* batchAct = tools->addAction(tr("&Batch / Action…"));
    connect(batchAct, &QAction::triggered, this, [this] { activateTool(QStringLiteral("batch")); });

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    QAction* docs = help->addAction(tr("&Documentation"));
    connect(docs, &QAction::triggered, this, &MainWindow::openDocs);
    help->addAction(m_aboutAct);
}

void MainWindow::buildShell() {
    auto* shell = new QWidget(this);
    shell->setObjectName("Shell");
    setCentralWidget(shell);

    auto* outer = new QVBoxLayout(shell);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_tabStrip = new TabStrip(shell);
    m_commandBar = new CommandBar(shell);
    m_findBar = new FindBar(shell);
    m_findBar->hide();
    m_redactionBar = new RedactionBar(shell);
    m_redactionBar->hide();
    m_annotationBar = new AnnotationBar(shell);
    m_annotationBar->hide();
    m_measureBar = new MeasureBar(shell);
    m_measureBar->hide();
    m_readAloudBar = new ReadAloudBar(shell);
    m_readAloudBar->hide();
    outer->addWidget(m_tabStrip);
    outer->addWidget(m_commandBar);
    outer->addWidget(m_findBar);
    outer->addWidget(m_redactionBar);
    outer->addWidget(m_annotationBar);
    outer->addWidget(m_measureBar);
    outer->addWidget(m_readAloudBar);

    auto* body = new QWidget(shell);
    auto* bodyRow = new QHBoxLayout(body);
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(0);

    m_rail = new NavigationRail(body);
    m_viewport = new Viewport(body);
    m_home = new HomeView(body);
    m_toolsPane = new ToolsPane(body);

    // The rail's expandable panel (Thumbnails real; others a placeholder for now).
    m_panelHost = new QWidget(body);
    m_panelHost->setObjectName("RailPanel");
    m_panelHost->setFixedWidth(200);
    auto* panelCol = new QVBoxLayout(m_panelHost);
    panelCol->setContentsMargins(0, 0, 0, 0);
    panelCol->setSpacing(0);
    m_panelHead = new QLabel(m_panelHost);
    m_panelHead->setObjectName("RailPanelHead");
    m_panelHead->setContentsMargins(16, 14, 16, 8);
    panelCol->addWidget(m_panelHead);
    m_panelStack = new QStackedWidget(m_panelHost);
    m_thumbnails = new ThumbnailPanel(m_panelHost);
    m_outline = new OutlinePanel(m_panelHost);
    m_annotations = new AnnotationsPanel(m_panelHost);
    m_attachments = new AttachmentsPanel(m_panelHost);
    m_layers = new LayersPanel(m_panelHost);
    m_forms = new FormPanel(m_panelHost);
    m_panelStack->addWidget(m_thumbnails);  // index 0
    m_panelStack->addWidget(m_outline);     // index 1
    m_panelStack->addWidget(m_annotations); // index 2
    m_panelStack->addWidget(m_attachments); // index 3
    m_panelStack->addWidget(m_layers);      // index 4
    m_panelStack->addWidget(m_forms);       // index 5
    panelCol->addWidget(m_panelStack, 1);
    m_panelHost->setVisible(false);

    // The center swaps between the Home start screen and the document viewport.
    m_docsView = new DocsView(body);
    m_centerStack = new QStackedWidget(body);
    m_centerStack->addWidget(m_home);     // index 0
    m_centerStack->addWidget(m_viewport); // index 1
    m_centerStack->addWidget(m_docsView); // index 2

    bodyRow->addWidget(m_rail);
    bodyRow->addWidget(m_panelHost);
    bodyRow->addWidget(m_centerStack, 1);
    bodyRow->addWidget(m_toolsPane);
    outer->addWidget(body, 1);

    // Overlays: the floating pill rides the viewport; the toast rides the shell.
    m_pill = new FloatingPill(m_viewport);
    m_toast = new Toast(shell);
}

void MainWindow::showHome() {
    if (m_viewport->redactionMode())
        setRedactionMode(false);
    if (m_viewport->highlightMode())
        setHighlightMode(false);
    if (m_viewport->measureMode())
        setMeasureMode(false);
    if (m_immersive)
        m_immersiveAct->setChecked(false); // leave immersive before showing Home
    m_home->refresh();
    m_findBar->hide();
    m_centerStack->setCurrentWidget(m_home);
    m_rail->setVisible(false);
    m_panelHost->setVisible(false);
    m_toolsPane->setVisible(false);
    m_commandBar->setVisible(false);
    m_pill->setVisible(false);
    m_tabStrip->setActiveHome();
}

void MainWindow::showDocument() {
    m_centerStack->setCurrentWidget(m_viewport);
    m_rail->setVisible(true);
    // Restore the rail panel if one was open before we left for Home.
    m_panelHost->setVisible(m_rail->current() != NavigationRail::Panel::None);
    m_toolsPane->setVisible(true);
    m_commandBar->setVisible(true);
    m_pill->setVisible(true);
    // activateSession() owns which document tab is active.
}

void MainWindow::openDocs() {
    m_tabStrip->showDocsTab();
    showDocs();
}

void MainWindow::showDocs() {
    if (m_immersive)
        m_immersiveAct->setChecked(false);
    m_findBar->hide();
    m_centerStack->setCurrentWidget(m_docsView);
    // Documentation is a full-page view, like Home - chrome recedes.
    m_rail->setVisible(false);
    m_panelHost->setVisible(false);
    m_toolsPane->setVisible(false);
    m_commandBar->setVisible(false);
    m_pill->setVisible(false);
    m_tabStrip->setActiveDocs();
}

void MainWindow::closeDocs() {
    m_tabStrip->closeDocsTab();
    // Return to the active document if one is open, else Home.
    if (hasActiveDoc() && session(m_activeId))
        activateSession(m_activeId);
    else
        showHome();
}

void MainWindow::setImmersive(bool on) {
    if (on && !hasActiveDoc()) {
        m_immersiveAct->setChecked(false);
        return;
    }
    if (on == m_immersive)
        return;
    m_immersive = on;
    m_pill->setReadingMode(on); // flip the pill's reading-mode glyph

    if (on) {
        // Everything recedes; only the page and its floating pill remain.
        m_panelWasVisible = m_panelHost->isVisible();
        m_findBar->hide();
        m_toast->show(tr("Press Esc or F11 to leave immersive reading"));
    }
    animateChrome(/*collapse=*/on);
}

QVector<QPair<QWidget*, bool>> MainWindow::chromeItems() const {
    // {widget, vertical} - vertical collapses height, horizontal collapses width.
    QVector<QPair<QWidget*, bool>> items{
        {menuBar(), true}, {m_tabStrip, true}, {m_commandBar, true},
        {m_rail, false}, {m_toolsPane, false}};
    if (m_panelWasVisible)
        items.push_back({m_panelHost, false});
    return items;
}

void MainWindow::animateChrome(bool collapse) {
    if (m_chromeAnim) {
        m_chromeAnim->stop();
        m_chromeAnim->deleteLater();
        m_chromeAnim = nullptr;
    }

    const auto items = chromeItems();
    auto* group = new QParallelAnimationGroup(this);
    for (const auto& [w, vertical] : items) {
        int extent;
        if (collapse) {
            extent = vertical ? w->height() : w->width();
            m_chromeExtent[w] = extent;
        } else {
            extent = m_chromeExtent.value(
                w, vertical ? w->sizeHint().height() : w->sizeHint().width());
        }
        // Relax the minimum so the fixed-size bars/panes can actually shrink.
        if (vertical)
            w->setMinimumHeight(0);
        else
            w->setMinimumWidth(0);
        if (!collapse) {
            if (vertical)
                w->setMaximumHeight(0);
            else
                w->setMaximumWidth(0);
            w->show();
        }
        auto* anim = new QPropertyAnimation(w, vertical ? "maximumHeight" : "maximumWidth", group);
        anim->setDuration(220);
        anim->setEasingCurve(QEasingCurve::InOutCubic);
        anim->setStartValue(collapse ? extent : 0);
        anim->setEndValue(collapse ? 0 : extent);
        group->addAnimation(anim);
    }

    connect(group, &QParallelAnimationGroup::finished, this, [this, collapse] {
        for (const auto& [w, vertical] : chromeItems()) {
            if (collapse) {
                w->hide();
            } else {
                const int ext = m_chromeExtent.value(w, 0);
                if (w == menuBar()) {
                    w->setMinimumHeight(0);
                    w->setMaximumHeight(QWIDGETSIZE_MAX);
                } else if (vertical) {
                    w->setFixedHeight(ext);
                } else {
                    w->setFixedWidth(ext);
                }
            }
        }
        m_chromeAnim = nullptr;
    });

    m_chromeAnim = group;
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::wireSignals() {
    // Home start screen → open a file or a recent.
    connect(m_home, &HomeView::openRequested, this, &MainWindow::openFileDialog);
    connect(m_home, &HomeView::openPathRequested, this, [this](const QString& path) { openPath(path); });

    // Viewport → indicators (command bar + pill).
    connect(m_viewport, &Viewport::currentPageChanged, this, &MainWindow::updatePageIndicator);
    connect(m_viewport, &Viewport::pageCountChanged, this, &MainWindow::updatePageIndicator);
    connect(m_viewport, &Viewport::zoomChanged, this, &MainWindow::updateZoomIndicator);

    // Command bar.
    connect(m_commandBar, &CommandBar::zoomInRequested, m_viewport, &Viewport::zoomIn);
    connect(m_commandBar, &CommandBar::zoomOutRequested, m_viewport, &Viewport::zoomOut);
    connect(m_commandBar, &CommandBar::nextPageRequested, this, &MainWindow::nextPage);
    connect(m_commandBar, &CommandBar::prevPageRequested, this, &MainWindow::previousPage);
    connect(m_commandBar, &CommandBar::saveRequested, this, [this] { saveActive(); });
    connect(m_commandBar, &CommandBar::exportRequested, this, [this] { saveActiveAs(); });
    connect(m_commandBar, &CommandBar::printRequested, this, &MainWindow::printActive);
    connect(m_commandBar, &CommandBar::emailRequested, this, [this] {
        if (!hasActiveDoc())
            return;
        // Hand the saved file to the system email client as an attachment.
        if (QStandardPaths::findExecutable(QStringLiteral("xdg-email")).isEmpty() ||
            !QProcess::startDetached(QStringLiteral("xdg-email"),
                                     {QStringLiteral("--attach"), m_doc->filePath()})) {
            QMessageBox::information(
                this, tr("Email"),
                tr("Couldn't reach an email client. Install xdg-utils, or attach the "
                   "file manually from your mail app."));
        }
    });
    connect(m_commandBar, &CommandBar::moreRequested, this, [this] {
        QMenu menu(this);
        menu.addAction(m_rotateLeftAct);
        menu.addAction(m_rotateRightAct);
        menu.addAction(m_deletePageAct);
        menu.addAction(m_insertPagesAct);
        menu.addAction(m_extractPagesAct);
        menu.addAction(m_cropPagesAct);
        menu.addSeparator();
        menu.addAction(m_singlePageAct);
        menu.addAction(m_continuousAct);
        menu.addAction(m_twoUpAct);
        menu.addSeparator();
        menu.addAction(m_zoomActualAct);
        menu.addAction(m_fitWidthAct);
        menu.addAction(m_fitPageAct);
        menu.addSeparator();
        for (int pct : {50, 75, 125, 150, 200}) {
            QAction* a = menu.addAction(tr("Zoom to %1%").arg(pct));
            connect(a, &QAction::triggered, this,
                    [this, pct] { m_viewport->setZoomFactor(pct / 100.0); });
        }
        menu.exec(QCursor::pos());
    });

    // Keep the layout radio in sync if the mode changes by any path.
    connect(m_viewport, &Viewport::layoutModeChanged, this, [this](PageView::LayoutMode m) {
        if (m == PageView::LayoutMode::SinglePage)
            m_singlePageAct->setChecked(true);
        else if (m == PageView::LayoutMode::TwoUp)
            m_twoUpAct->setChecked(true);
        else
            m_continuousAct->setChecked(true);
    });
    const auto promptGoToPage = [this] {
        if (!hasActiveDoc())
            return;
        GoToPageDialog dialog(m_doc->pageCount(), m_viewport->currentPage() + 1, this);
        if (dialog.exec() == QDialog::Accepted)
            m_viewport->goToPage(dialog.selectedPage() - 1);
    };
    connect(m_commandBar, &CommandBar::counterClicked, this, promptGoToPage);

    // Floating pill.
    connect(m_pill, &FloatingPill::zoomInRequested, m_viewport, &Viewport::zoomIn);
    connect(m_pill, &FloatingPill::zoomOutRequested, m_viewport, &Viewport::zoomOut);
    connect(m_pill, &FloatingPill::nextPageRequested, this, &MainWindow::nextPage);
    connect(m_pill, &FloatingPill::prevPageRequested, this, &MainWindow::previousPage);
    connect(m_pill, &FloatingPill::goToPageRequested, this, promptGoToPage);
    connect(m_pill, &FloatingPill::readingModeToggleRequested, this,
            [this] { m_immersiveAct->toggle(); });

    // Read-aloud bar: follow the spoken sentence, tidy up on close.
    connect(m_readAloudBar, &ReadAloudBar::pageReached, this, [this](int page) {
        if (m_viewport->currentPage() != page)
            m_viewport->goToPage(page);
    });
    connect(m_readAloudBar, &ReadAloudBar::closed, this, [this] { m_readAloudBar->hide(); });

    // View actions → viewport.
    connect(m_zoomInAct, &QAction::triggered, m_viewport, &Viewport::zoomIn);
    connect(m_zoomOutAct, &QAction::triggered, m_viewport, &Viewport::zoomOut);
    connect(m_zoomActualAct, &QAction::triggered, m_viewport, &Viewport::zoomActualSize);
    connect(m_fitWidthAct, &QAction::triggered, m_viewport, &Viewport::fitToWidth);
    connect(m_fitPageAct, &QAction::triggered, m_viewport, &Viewport::fitWholePage);

    // Navigation rail → expandable panel. Each panel owns its own content; the
    // semantic panels (annotations/attachments/layers) load lazily when shown.
    connect(m_rail, &NavigationRail::docsRequested, this, &MainWindow::openDocs);
    connect(m_rail, &NavigationRail::panelChanged, this, [this](NavigationRail::Panel p) {
        switch (p) {
        case NavigationRail::Panel::None:
            m_panelHost->setVisible(false);
            return;
        case NavigationRail::Panel::Thumbnails:
            m_panelHead->setText(tr("THUMBNAILS"));
            m_panelStack->setCurrentWidget(m_thumbnails);
            break;
        case NavigationRail::Panel::Outline:
            m_panelHead->setText(tr("OUTLINE"));
            m_panelStack->setCurrentWidget(m_outline);
            break;
        case NavigationRail::Panel::Annotations:
            m_panelHead->setText(tr("ANNOTATIONS"));
            m_panelStack->setCurrentWidget(m_annotations);
            break;
        case NavigationRail::Panel::Attachments:
            m_panelHead->setText(tr("ATTACHMENTS"));
            m_panelStack->setCurrentWidget(m_attachments);
            break;
        case NavigationRail::Panel::Layers:
            m_panelHead->setText(tr("LAYERS"));
            m_panelStack->setCurrentWidget(m_layers);
            break;
        case NavigationRail::Panel::Forms:
            m_panelHead->setText(tr("FORMS"));
            m_panelStack->setCurrentWidget(m_forms);
            break;
        }
        m_panelHost->setVisible(true);
    });

    // Thumbnails ↔ viewport: click navigates; the current page stays highlighted.
    connect(m_thumbnails, &ThumbnailPanel::pageActivated, m_viewport, &Viewport::goToPage);
    connect(m_viewport, &Viewport::currentPageChanged, m_thumbnails, &ThumbnailPanel::setCurrentPage);
    connect(m_thumbnails, &ThumbnailPanel::pageMoved, this, [this](int from, int to) {
        Session* s = session(m_activeId);
        if (s && s->undo && hasActiveDoc())
            s->undo->push(new MovePageCommand(s->doc, from, to));
    });

    // Outline → viewport navigation; viewport → outline (where "Add" points).
    connect(m_outline, &OutlinePanel::pageActivated, m_viewport, &Viewport::goToPage);
    connect(m_viewport, &Viewport::currentPageChanged, m_outline, &OutlinePanel::setCurrentPage);
    connect(m_outline, &OutlinePanel::saveRequested, this, &MainWindow::saveOutline);

    // Form-field placement: a drawn rectangle becomes a new (or moved) field.
    connect(m_viewport, &Viewport::fieldRectDrawn, this, &MainWindow::placeFormField);
    connect(m_viewport, &Viewport::snapshotRegion, this, &MainWindow::snapshotRegion);
    connect(m_forms, &FormPanel::addFieldRequested, this, &MainWindow::addFormField);
    connect(m_forms, &FormPanel::prepareFormRequested, this, &MainWindow::prepareForm);
    connect(m_forms, &FormPanel::moveFieldRequested, this, &MainWindow::moveFormField);
    connect(m_forms, &FormPanel::deleteFieldRequested, this, &MainWindow::deleteFormField);

    // Redaction bar ↔ viewport.
    connect(m_viewport, &Viewport::redactionsChanged, this,
            [this](int count) { m_redactionBar->setCount(count); });
    connect(m_redactionBar, &RedactionBar::applyRequested, this, &MainWindow::applyRedactions);
    connect(m_redactionBar, &RedactionBar::clearRequested, m_viewport, &Viewport::clearRedactions);
    connect(m_redactionBar, &RedactionBar::doneRequested, this, [this] { setRedactionMode(false); });

    // Annotation bar ↔ viewport. The bar's count is every kind of mark.
    const auto refreshAnnotCount = [this] {
        m_annotationBar->setCount(m_viewport->highlightCount() + m_viewport->noteCount() +
                                  m_viewport->inkCount() + m_viewport->shapeCount());
    };
    connect(m_viewport, &Viewport::highlightsChanged, this, refreshAnnotCount);
    connect(m_viewport, &Viewport::notesChanged, this, refreshAnnotCount);
    connect(m_viewport, &Viewport::inksChanged, this, refreshAnnotCount);
    connect(m_viewport, &Viewport::shapesChanged, this, refreshAnnotCount);
    connect(m_annotationBar, &AnnotationBar::saveRequested, this, &MainWindow::applyAnnotations);
    connect(m_annotationBar, &AnnotationBar::clearRequested, m_viewport,
            &Viewport::clearAnnotations);
    connect(m_annotationBar, &AnnotationBar::doneRequested, this,
            [this] { setHighlightMode(false); });
    connect(m_annotationBar, &AnnotationBar::toolChanged, this, [this](int tool) {
        using T = PageView::AnnotTool;
        const T t = tool == 1   ? T::Note
                    : tool == 2 ? T::Ink
                    : tool == 3 ? T::Underline
                    : tool == 4 ? T::StrikeOut
                    : tool == 5 ? T::Rectangle
                    : tool == 6 ? T::Line
                    : tool == 7 ? T::Arrow
                    : tool == 8 ? T::TextBox
                                : T::Highlight;
        m_viewport->setAnnotationTool(t);
    });
    connect(m_annotationBar, &AnnotationBar::colorChanged, this,
            [this](const QColor& c) { m_viewport->setHighlightColor(c); });

    // Measure bar ↔ viewport: pick the mode/unit; the page view draws the overlay.
    connect(m_measureBar, &MeasureBar::modeChanged, this, [this](int mode) {
        using K = PageView::MeasureKind;
        m_viewport->setMeasureKind(mode == 1 ? K::Perimeter : mode == 2 ? K::Area : K::Distance);
    });
    connect(m_measureBar, &MeasureBar::unitChanged, this, [this](int unit) {
        using U = Measurer::Unit;
        m_viewport->setMeasureUnit(unit == 1 ? U::Centimeter : unit == 2 ? U::Inch : U::Millimeter);
    });
    connect(m_measureBar, &MeasureBar::clearRequested, m_viewport, &Viewport::clearMeasure);
    connect(m_measureBar, &MeasureBar::doneRequested, this, [this] { setMeasureMode(false); });
    // Note tool clicked → collect text, then add the note at that point.
    connect(m_viewport, &Viewport::noteRequested, this, [this](int slot, QPointF pos) {
        NoteDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted)
            m_viewport->addNote(slot, pos, dialog.text());
    });
    // Text box drawn → collect its text, then add it in that rectangle.
    connect(m_viewport, &Viewport::textBoxRequested, this, [this](int slot, QRectF rect) {
        NoteDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted && !dialog.text().isEmpty())
            m_viewport->addTextBox(slot, rect, dialog.text());
    });

    // Forms panel → fill and save via Poppler, then open the result.
    connect(m_forms, &FormPanel::saveRequested, this, [this](const QHash<int, QVariant>& values) {
        if (!hasActiveDoc() || values.isEmpty())
            return;
        const QFileInfo info(m_doc->filePath());
        const QString suggested =
            info.dir().filePath(info.completeBaseName() + QStringLiteral("-filled.pdf"));
        const QString out = QFileDialog::getSaveFileName(this, tr("Save filled form"), suggested,
                                                         tr("PDF documents (*.pdf)"));
        if (out.isEmpty())
            return;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString error;
        const bool ok = FormFiller::save(m_doc->filePath(), out, values, &error);
        QApplication::restoreOverrideCursor();
        if (!ok) {
            QMessageBox::warning(this, tr("Couldn't save form"), error);
            return;
        }
        m_toast->show(tr("Saved filled form to %1").arg(QFileInfo(out).fileName()));
        openPath(out);
    });

    // Annotations list → viewport. Poppler reports the original page index; map
    // it to the current display slot so edits (reorder/delete) are honoured.
    connect(m_annotations, &AnnotationsPanel::annotationActivated, this, [this](int originalPage) {
        if (!m_doc)
            return;
        const int slot = m_doc->pageOrder().indexOf(originalPage);
        if (slot >= 0)
            m_viewport->goToPage(slot);
    });

    // Tools pane. (Customize is handled inside the pane and persisted.)
    connect(m_toolsPane, &ToolsPane::toolActivated, this, &MainWindow::activateTool);

    // Tab strip.
    connect(m_tabStrip, &TabStrip::homeSelected, this, &MainWindow::showHome);
    connect(m_tabStrip, &TabStrip::documentSelected, this, &MainWindow::activateSession);
    connect(m_tabStrip, &TabStrip::docsSelected, this, &MainWindow::showDocs);
    connect(m_tabStrip, &TabStrip::docsCloseRequested, this, &MainWindow::closeDocs);
    connect(m_tabStrip, &TabStrip::newTabRequested, this, &MainWindow::openFileDialog);
    connect(m_tabStrip, &TabStrip::closeDocumentRequested, this, &MainWindow::closeTab);
    connect(m_tabStrip, &TabStrip::searchRequested, this, &MainWindow::openFind);

    // Find bar ↔ viewport search.
    connect(m_findBar, &FindBar::queryChanged, this, [this](const QString& q) {
        m_searchIndex = 0;
        const int count = m_viewport->search(q);
        if (count > 0)
            m_viewport->showSearchResult(0);
        updateFindCount();
    });
    connect(m_findBar, &FindBar::findNext, this, [this] { stepFind(1); });
    connect(m_findBar, &FindBar::findPrevious, this, [this] { stepFind(-1); });
    connect(m_findBar, &FindBar::closed, this, [this] {
        m_viewport->clearSearch();
        m_findBar->hide();
        m_viewport->setFocus();
    });
    connect(m_viewport, &Viewport::searchResultsChanged, this, &MainWindow::updateFindCount);
    connect(m_tabStrip, &TabStrip::menuRequested, this, [this] {
        QMenu menu(this);
        menu.addAction(m_openAct);
        menu.addAction(m_toggleThemeAct);
        menu.addSeparator();
        menu.addAction(m_aboutAct);
        menu.addAction(m_quitAct);
        menu.exec(QCursor::pos());
    });
}

void MainWindow::rotateActivePage(int deltaDegrees) {
    Session* s = session(m_activeId);
    if (!s || !hasActiveDoc() || !s->undo)
        return;
    s->undo->push(new RotatePageCommand(s->doc, m_viewport->currentPage(), deltaDegrees));
}

void MainWindow::deleteActivePage() {
    Session* s = session(m_activeId);
    if (!s || !hasActiveDoc() || !s->undo)
        return;
    if (m_doc->pageCount() <= 1) {
        m_toast->show(tr("A document must keep at least one page."));
        return;
    }
    s->undo->push(new DeletePageCommand(s->doc, m_viewport->currentPage()));
}

void MainWindow::extractActivePages() {
    if (!hasActiveDoc())
        return;

    ExtractDialog dialog(m_doc->pageCount(), m_viewport->currentPage() + 1, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QVector<int> pageSlots = dialog.selectedDisplayIndices();
    if (pageSlots.isEmpty()) {
        m_toast->show(tr("No pages in that range to extract."));
        return;
    }

    // Map the chosen display slots back to the on-disk page order + per-slot
    // rotation that PdfEditor::saveArrangement consumes, so the extract is
    // lossless and respects any in-session edits.
    QVector<int> order;
    QVector<int> rotations;
    order.reserve(pageSlots.size());
    rotations.reserve(pageSlots.size());
    for (int slot : pageSlots) {
        order.push_back(m_doc->originalPageAt(slot));
        rotations.push_back(m_doc->rotation(slot));
    }

    const QFileInfo info(m_doc->filePath());
    const QString suggested = info.dir().filePath(
        info.completeBaseName() +
        tr("-extracted", "suffix for an extracted-pages file") + QStringLiteral(".pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Extract Pages"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok =
        PdfEditor::saveArrangement(m_doc->filePath(), out, order, rotations, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't extract"), error);
        return;
    }
    m_toast->show(tr("Extracted %n page(s) to %1", "", pageSlots.size())
                      .arg(QFileInfo(out).fileName()));
}

void MainWindow::insertPagesIntoActive() {
    if (!hasActiveDoc())
        return;

    const QFileInfo info(m_doc->filePath());
    const QString src = QFileDialog::getOpenFileName(this, tr("Choose a PDF to insert"),
                                                     info.absolutePath(),
                                                     tr("PDF documents (*.pdf)"));
    if (src.isEmpty())
        return;

    // Read the source page count up front (and reject what we can't open, e.g. an
    // encrypted file — the QPDF copy path below opens it without a password too).
    QPdfDocument probe;
    if (probe.load(src) != QPdfDocument::Error::None || probe.pageCount() < 1) {
        m_toast->show(tr("That PDF couldn't be opened to insert from."));
        return;
    }

    InsertDialog dialog(QFileInfo(src).fileName(), probe.pageCount(),
                        m_viewport->currentPage() + 1, m_doc->pageCount(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QVector<int> pages = dialog.sourcePages();
    if (pages.isEmpty()) {
        m_toast->show(tr("No pages in that range to insert."));
        return;
    }

    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-merged.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save merged PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok =
        PdfEditor::insertPages(m_doc->filePath(), out, m_doc->pageOrder(), m_doc->rotations(), src,
                               pages, dialog.insertAtSlot(), &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't insert"), error);
        return;
    }
    m_toast->show(tr("Inserted %n page(s) into %1", "", pages.size())
                      .arg(QFileInfo(out).fileName()));
    openPath(out); // open the merged result
}

void MainWindow::cropActivePages() {
    if (!hasActiveDoc())
        return;

    CropDialog dialog(m_doc->pageCount(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (dialog.isEmptyCrop()) {
        m_toast->show(tr("Set a margin on at least one edge to crop."));
        return;
    }

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-cropped.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save cropped PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    const PdfEditor::CropMargins margins{dialog.leftPt(), dialog.rightPt(), dialog.topPt(),
                                         dialog.bottomPt()};

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = PdfEditor::cropPages(m_doc->filePath(), out, m_doc->pageOrder(),
                                         m_doc->rotations(), dialog.selectedSlots(), margins,
                                         &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't crop"), error);
        return;
    }
    m_toast->show(tr("Saved cropped copy to %1").arg(QFileInfo(out).fileName()));
    openPath(out); // open the cropped result
}

void MainWindow::saveOutline(const QVector<PdfEditor::OutlineItem>& items) {
    if (!hasActiveDoc())
        return;

    // Default to the current file so the common action is "overwrite in place";
    // choosing a new name writes a copy that opens in its own tab.
    const QString out = QFileDialog::getSaveFileName(this, tr("Save outline"), m_doc->filePath(),
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = PdfEditor::setOutline(m_doc->filePath(), out, items, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't save outline"), error);
        return;
    }
    m_toast->show(tr("Outline saved to %1").arg(QFileInfo(out).fileName()));
    openPath(out); // surface the saved result
}

void MainWindow::addFormField() {
    if (!hasActiveDoc())
        return;

    FormFieldDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (dialog.fieldName().isEmpty()) {
        m_toast->show(tr("Give the field a name."));
        return;
    }
    const bool isRadio = dialog.fieldType() == FormEditor::Type::Radio;
    if (isRadio && dialog.options().size() < 2) {
        m_toast->show(tr("A radio group needs at least two buttons."));
        return;
    }

    // Remember the field's properties, then let the user draw where it goes.
    m_pendingField = FormEditor::NewField{};
    m_pendingField.type = dialog.fieldType();
    m_pendingField.name = dialog.fieldName();
    m_pendingField.defaultValue = dialog.defaultValue();
    m_pendingField.checked = dialog.checked();
    m_pendingField.options = dialog.options();
    m_pendingField.format = dialog.format();
    m_placingField = true;
    m_movingField.clear(); // this is an add, not a move

    setRedactionMode(false); // the edit modes are mutually exclusive
    setHighlightMode(false);
    setMeasureMode(false);
    m_viewport->setFieldPlacementMode(true);
    m_toast->show(isRadio ? tr("Draw where the first button goes.")
                          : tr("Draw where the field goes."));
}

void MainWindow::prepareForm() {
    if (!hasActiveDoc())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const QList<FormEditor::NewField> detected = FormDetector::detect(m_doc->filePath(), &error);
    QApplication::restoreOverrideCursor();

    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Couldn't prepare form"), error);
        return;
    }
    if (detected.isEmpty()) {
        m_toast->show(tr("No fillable areas found."));
        return;
    }

    PrepareFormDialog dialog(detected, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QList<FormEditor::NewField> fields = dialog.selectedFields();
    if (fields.isEmpty()) {
        m_toast->show(tr("No fields selected."));
        return;
    }

    const QString out = QFileDialog::getSaveFileName(this, tr("Save form"), m_doc->filePath(),
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    int count = 0;
    const bool ok = FormEditor::addFields(m_doc->filePath(), out, fields, &count, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't prepare form"), error);
        return;
    }

    m_toast->show(tr("Added %n form field(s).", "", count));
    openPath(out);
    m_forms->reload();
}

void MainWindow::readAloud() {
    if (!hasActiveDoc())
        return;
    if (!ReadAloudBar::isAvailable()) {
        m_toast->show(tr("Read aloud needs speech-dispatcher (install the “speech-dispatcher” "
                         "package)."));
        return;
    }

    // Hide the sibling bars so only one strip shows at a time.
    m_findBar->hide();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const int count = m_readAloudBar->start(m_doc->filePath(), m_viewport->currentPage());
    QApplication::restoreOverrideCursor();
    if (count == 0) {
        m_readAloudBar->hide();
        m_toast->show(tr("There's no readable text on these pages."));
    }
}

void MainWindow::moveFormField(const QString& name) {
    if (name.isEmpty() || !hasActiveDoc())
        return;
    m_movingField = name;
    m_placingField = true;
    setRedactionMode(false);
    setHighlightMode(false);
    setMeasureMode(false);
    m_viewport->setFieldPlacementMode(true);
    m_toast->show(tr("Draw the new position for “%1”.").arg(name));
}

void MainWindow::deleteFormField(const QString& name) {
    if (name.isEmpty() || !hasActiveDoc())
        return;
    const QString out = QFileDialog::getSaveFileName(this, tr("Save form"), m_doc->filePath(),
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = FormEditor::deleteField(m_doc->filePath(), out, name, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't delete field"), error);
        return;
    }
    m_toast->show(tr("Deleted “%1”").arg(name));
    openPath(out);
    m_forms->reload(); // reflect the change even when overwriting in place
}

void MainWindow::exportFormData() {
    if (!hasActiveDoc())
        return;
    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral(".xfdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Export form data"), suggested,
                                                     tr("XFDF form data (*.xfdf)"));
    if (out.isEmpty())
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = Xfdf::exportFields(m_doc->filePath(), out, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't export form data"), error);
        return;
    }
    m_toast->show(tr("Exported form data to %1").arg(QFileInfo(out).fileName()));
}

void MainWindow::importFormData() {
    if (!hasActiveDoc())
        return;
    const QFileInfo info(m_doc->filePath());
    const QString xfdf = QFileDialog::getOpenFileName(this, tr("Import form data"),
                                                      info.absolutePath(),
                                                      tr("XFDF form data (*.xfdf)"));
    if (xfdf.isEmpty())
        return;
    const QString out =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-filled.pdf"));
    const QString dest = QFileDialog::getSaveFileName(this, tr("Save filled form"), out,
                                                      tr("PDF documents (*.pdf)"));
    if (dest.isEmpty())
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = Xfdf::importFields(m_doc->filePath(), xfdf, dest, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't import form data"), error);
        return;
    }
    m_toast->show(tr("Imported form data into %1").arg(QFileInfo(dest).fileName()));
    openPath(dest);
}

void MainWindow::editTextBoxes() {
    if (!hasActiveDoc())
        return;
    const QList<TextEditor::TextBox> boxes = TextEditor::read(m_doc->filePath());
    if (boxes.isEmpty()) {
        m_toast->show(tr("No editable text boxes yet, add one with the Text tool."));
        return;
    }

    TextEditDialog dialog(boxes, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const int idx = dialog.selectedIndex();
    if (idx < 0 || idx >= boxes.size())
        return;
    const TextEditor::TextBox& box = boxes[idx];

    const QString out = QFileDialog::getSaveFileName(this, tr("Save edited PDF"), m_doc->filePath(),
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    bool ok;
    QString message;
    if (dialog.action() == TextEditDialog::Action::Delete) {
        ok = TextEditor::remove(m_doc->filePath(), out, box.objId, box.objGen, &error);
        message = tr("Deleted the text box");
    } else {
        ok = TextEditor::setText(m_doc->filePath(), out, box.objId, box.objGen, dialog.text(),
                                 &error);
        message = tr("Updated the text");
    }
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't edit text"), error);
        return;
    }
    m_toast->show(message);
    openPath(out);
}

void MainWindow::editInLibreOffice() {
    if (!hasActiveDoc())
        return;
    QString soffice = QStandardPaths::findExecutable(QStringLiteral("soffice"));
    if (soffice.isEmpty())
        soffice = QStandardPaths::findExecutable(QStringLiteral("libreoffice"));
    if (soffice.isEmpty()) {
        QMessageBox::information(
            this, tr("LibreOffice not found"),
            tr("Install LibreOffice to edit heavy layout, body text, and images in Draw."));
        return;
    }
    // Hand the file to Draw for the edits Feather can't do natively yet (existing
    // body text with reflow, image move/resize). Detached — Feather keeps running.
    const bool ok = QProcess::startDetached(
        soffice, {QStringLiteral("--draw"), QDir::toNativeSeparators(m_doc->filePath())});
    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't open LibreOffice Draw"),
                             tr("LibreOffice is installed but couldn't be launched."));
        return;
    }
    m_toast->show(tr("Opening in LibreOffice Draw…"));
}

void MainWindow::showPreferences() {
    PreferencesDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // Appearance — apply immediately (and Theme persists the choice itself).
    switch (dialog.theme()) {
    case PreferencesDialog::Theme::System: Theme::instance().useSystem(); break;
    case PreferencesDialog::Theme::Light: Theme::instance().setMode(Theme::Mode::Light); break;
    case PreferencesDialog::Theme::Dark: Theme::instance().setMode(Theme::Mode::Dark); break;
    }

    // Default view for newly opened documents.
    QSettings settings;
    settings.setValue(QStringLiteral("view/defaultLayout"), dialog.layout());
    settings.setValue(QStringLiteral("view/defaultZoom"), dialog.zoom());
    if (hasActiveDoc())
        applyViewDefaults(); // reflect the new default on the current document too

    // Language is read at startup (tr() runs as widgets are built), so a change
    // needs a restart - persist it and let the user know.
    const QString oldLang =
        settings.value(QStringLiteral("app/language"), QStringLiteral("system")).toString();
    if (dialog.language() != oldLang) {
        settings.setValue(QStringLiteral("app/language"), dialog.language());
        m_toast->show(tr("Restart Feather PDF to apply the new language."));
    }
}

void MainWindow::applyViewDefaults() {
    QSettings settings;
    // Only act on settings the user has actually chosen, so the out-of-the-box
    // open behaviour is unchanged until Preferences is used.
    if (settings.contains(QStringLiteral("view/defaultLayout"))) {
        const QString layout = settings.value(QStringLiteral("view/defaultLayout")).toString();
        if (layout == QLatin1String("single"))
            m_viewport->setLayoutMode(PageView::LayoutMode::SinglePage);
        else if (layout == QLatin1String("twoup"))
            m_viewport->setLayoutMode(PageView::LayoutMode::TwoUp);
        else
            m_viewport->setLayoutMode(PageView::LayoutMode::Continuous);
    }
    if (settings.contains(QStringLiteral("view/defaultZoom"))) {
        const QString zoom = settings.value(QStringLiteral("view/defaultZoom")).toString();
        if (zoom == QLatin1String("fitpage"))
            m_viewport->fitWholePage();
        else if (zoom == QLatin1String("actual"))
            m_viewport->zoomActualSize();
        else
            m_viewport->fitToWidth();
    }
}

void MainWindow::placeFormField(int slot, const QRectF& normRect) {
    if (m_placingLink) { // the same drag mode is reused for placing links
        finishAddLink(slot, normRect);
        return;
    }
    if (m_placingStamp) { // and for placing stamps
        finishAddStamp(slot, normRect);
        return;
    }
    if (!m_placingField || !hasActiveDoc())
        return;
    m_placingField = false;
    m_viewport->setFieldPlacementMode(false);

    const QString out = QFileDialog::getSaveFileName(this, tr("Save form"), m_doc->filePath(),
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty()) {
        m_movingField.clear();
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    bool ok;
    QString message;
    if (!m_movingField.isEmpty()) {
        const QString name = m_movingField;
        m_movingField.clear();
        ok = FormEditor::setFieldRect(m_doc->filePath(), out, name, normRect, &error);
        message = tr("Moved “%1”").arg(name);
    } else {
        // The drawn rect is normalized to the displayed page; the field lands on
        // that page's original index so an in-session reorder still points at the
        // right page on disk.
        const int page = std::max(0, m_doc->originalPageAt(slot));
        if (m_pendingField.type == FormEditor::Type::Radio) {
            ok = FormEditor::addRadioGroup(m_doc->filePath(), out, m_pendingField.name,
                                           m_pendingField.options, page, normRect, &error);
        } else {
            m_pendingField.page = page;
            m_pendingField.rect = normRect;
            ok = FormEditor::addField(m_doc->filePath(), out, m_pendingField, &error);
        }
        message = tr("Added “%1” to %2").arg(m_pendingField.name, QFileInfo(out).fileName());
    }
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't save field"), error);
        return;
    }
    m_toast->show(message);
    openPath(out); // surface the document with its new field
    m_forms->reload();
}

bool MainWindow::saveActiveAs() {
    if (!hasActiveDoc())
        return false;
    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-edited.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Export PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return false;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = PdfEditor::saveArrangement(m_doc->filePath(), out, m_doc->pageOrder(),
                                               m_doc->rotations(), &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't export"), error);
        return false;
    }
    m_toast->show(tr("Exported to %1").arg(QFileInfo(out).fileName()));
    return true;
}

void MainWindow::setRedactionMode(bool on) {
    if (on && !hasActiveDoc())
        return;
    if (on) {
        setHighlightMode(false); // the edit modes are mutually exclusive
        setMeasureMode(false);
    }
    m_viewport->setRedactionMode(on);
    m_redactionBar->setVisible(on);
    if (on) {
        m_findBar->hide();
        m_redactionBar->setCount(m_viewport->redactionCount());
    } else {
        m_viewport->clearRedactions(); // leaving the mode discards unapplied marks
    }
}

void MainWindow::setHighlightMode(bool on) {
    if (on && !hasActiveDoc())
        return;
    if (on) {
        setRedactionMode(false);
        setMeasureMode(false);
    }
    m_viewport->setHighlightMode(on);
    m_annotationBar->setVisible(on);
    if (on) {
        m_findBar->hide();
        m_annotationBar->setCount(m_viewport->highlightCount() + m_viewport->noteCount() +
                                  m_viewport->inkCount());
    } else {
        m_viewport->clearAnnotations();
    }
}

void MainWindow::setMeasureMode(bool on) {
    if (on && !hasActiveDoc())
        return;
    if (on) {
        setRedactionMode(false); // the edit/measure modes are mutually exclusive
        setHighlightMode(false);
    }
    m_viewport->setMeasureMode(on);
    m_measureBar->setVisible(on);
    if (on)
        m_findBar->hide();
    else
        m_viewport->clearMeasure(); // leaving the mode discards the measurement
}

void MainWindow::applyAnnotations() {
    if (!hasActiveDoc())
        return;
    const QHash<int, QList<QPair<QRectF, QColor>>> hiMarks = m_viewport->highlightMarks();
    const QHash<int, QList<QPair<QPointF, QString>>> noteMarks = m_viewport->noteMarks();
    const QHash<int, QList<QPair<QPolygonF, QColor>>> inkMarks = m_viewport->inkMarks();
    const QHash<int, QList<PageView::ShapeMark>> shapeMarks = m_viewport->shapeMarks();
    if (hiMarks.isEmpty() && noteMarks.isEmpty() && inkMarks.isEmpty() && shapeMarks.isEmpty())
        return;

    // Map a normalized point from displayed (rotated) page space back to the
    // unrotated page space the annotation backend uses.
    const auto mapPoint = [](const QPointF& p, int rot) -> QPointF {
        switch (((rot % 360) + 360) % 360) {
        case 90: return {p.y(), 1.0 - p.x()};
        case 180: return {1.0 - p.x(), 1.0 - p.y()};
        case 270: return {1.0 - p.y(), p.x()};
        default: return p;
        }
    };
    const auto toPageRect = [&](const QRectF& d, int rot) {
        const QPointF a = mapPoint(d.topLeft(), rot);
        const QPointF b = mapPoint(d.bottomRight(), rot);
        return QRectF(QPointF(std::min(a.x(), b.x()), std::min(a.y(), b.y())),
                     QPointF(std::max(a.x(), b.x()), std::max(a.y(), b.y())));
    };

    QList<Annotator::Highlight> highlights;
    for (auto it = hiMarks.constBegin(); it != hiMarks.constEnd(); ++it) {
        const int orig = m_doc->originalPageAt(it.key());
        if (orig < 0)
            continue;
        const int rot = m_doc->rotation(it.key());
        for (const auto& mark : it.value())
            highlights.append(Annotator::Highlight{orig, toPageRect(mark.first, rot), mark.second});
    }
    QList<Annotator::Note> notes;
    const QColor noteColor(255, 214, 0);
    for (auto it = noteMarks.constBegin(); it != noteMarks.constEnd(); ++it) {
        const int orig = m_doc->originalPageAt(it.key());
        if (orig < 0)
            continue;
        const int rot = m_doc->rotation(it.key());
        for (const auto& nm : it.value())
            notes.append(Annotator::Note{orig, mapPoint(nm.first, rot), nm.second, noteColor});
    }
    QList<Annotator::Ink> inks;
    for (auto it = inkMarks.constBegin(); it != inkMarks.constEnd(); ++it) {
        const int orig = m_doc->originalPageAt(it.key());
        if (orig < 0)
            continue;
        const int rot = m_doc->rotation(it.key());
        for (const auto& stroke : it.value()) {
            QPolygonF mapped;
            mapped.reserve(stroke.first.size());
            for (const QPointF& p : stroke.first)
                mapped.append(mapPoint(p, rot));
            inks.append(Annotator::Ink{orig, {mapped}, stroke.second});
        }
    }
    QList<Annotator::Shape> shapes;
    using AT = PageView::AnnotTool;
    using SK = Annotator::Shape::Kind;
    const auto toShapeKind = [](AT t) {
        switch (t) {
        case AT::Underline: return SK::Underline;
        case AT::StrikeOut: return SK::StrikeOut;
        case AT::Line: return SK::Line;
        case AT::Arrow: return SK::Arrow;
        case AT::TextBox: return SK::TextBox;
        default: return SK::Rectangle;
        }
    };
    for (auto it = shapeMarks.constBegin(); it != shapeMarks.constEnd(); ++it) {
        const int orig = m_doc->originalPageAt(it.key());
        if (orig < 0)
            continue;
        const int rot = m_doc->rotation(it.key());
        for (const PageView::ShapeMark& sm : it.value()) {
            Annotator::Shape s;
            s.page = orig;
            s.kind = toShapeKind(sm.kind);
            s.color = sm.color;
            if (sm.kind == AT::Line || sm.kind == AT::Arrow) {
                s.a = mapPoint(sm.a, rot);
                s.b = mapPoint(sm.b, rot);
            } else {
                s.rect = toPageRect(sm.rect, rot);
                s.text = sm.text;
            }
            shapes.append(s);
        }
    }
    if (highlights.isEmpty() && notes.isEmpty() && inks.isEmpty() && shapes.isEmpty())
        return;

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-annotated.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save annotated PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = Annotator::saveAnnotations(m_doc->filePath(), out, highlights, notes, inks,
                                               shapes, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't save annotations"), error);
        return;
    }
    m_toast->show(tr("Saved annotated copy to %1").arg(QFileInfo(out).fileName()));
    setHighlightMode(false);
    openPath(out);
}

void MainWindow::applyRedactions() {
    if (!hasActiveDoc())
        return;
    const QHash<int, QList<QRectF>> marks = m_viewport->redactionMarks();
    if (marks.isEmpty())
        return;

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-redacted.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save redacted PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QPdfDocument* pdf = m_doc->pdf();
    QHash<int, PdfEditor::RedactedImage> redacted;
    constexpr double kDpi = 200.0;
    const double scale = kDpi / 72.0;

    for (auto it = marks.constBegin(); it != marks.constEnd(); ++it) {
        const int slot = it.key();
        const int orig = m_doc->originalPageAt(slot);
        if (orig < 0)
            continue;
        const int rot = m_doc->rotation(slot);
        QSizeF pt = pdf->pagePointSize(orig);
        if (rot == 90 || rot == 270)
            pt.transpose(); // effective (displayed) size - what the marks are relative to
        if (pt.width() <= 0 || pt.height() <= 0)
            continue;

        const QSize px(qRound(pt.width() * scale), qRound(pt.height() * scale));
        QPdfDocumentRenderOptions opts;
        opts.setRenderFlags(QPdfDocumentRenderOptions::RenderFlag::Annotations);
        switch (rot) {
        case 90: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise90); break;
        case 180: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise180); break;
        case 270: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise270); break;
        default: break;
        }

        QImage flat(px, QImage::Format_RGB888);
        flat.fill(Qt::white);
        QPainter pnt(&flat);
        const QImage rendered = pdf->render(orig, px, opts);
        if (!rendered.isNull())
            pnt.drawImage(0, 0, rendered);
        pnt.setPen(Qt::NoPen);
        for (const QRectF& nrm : it.value())
            pnt.fillRect(QRectF(nrm.x() * px.width(), nrm.y() * px.height(),
                                nrm.width() * px.width(), nrm.height() * px.height()),
                         Qt::black);
        pnt.end();

        QByteArray jpeg;
        QBuffer buffer(&jpeg);
        buffer.open(QIODevice::WriteOnly);
        flat.save(&buffer, "JPEG", 92);
        redacted.insert(slot, PdfEditor::RedactedImage{jpeg, px.width(), px.height(), pt.width(),
                                                       pt.height()});
    }

    QString error;
    const bool ok = PdfEditor::saveRedacted(m_doc->filePath(), out, m_doc->pageOrder(),
                                            m_doc->rotations(), redacted, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't redact"), error);
        return;
    }
    m_toast->show(tr("Saved redacted copy to %1").arg(QFileInfo(out).fileName()));
    setRedactionMode(false);
    openPath(out); // open the flattened result
}

namespace {
// Map a normalized rect from a page's natural orientation into the displayed
// orientation after an in-session clockwise rotation of `deg` (0/90/180/270),
// staying within the unit square.
QRectF rotateNormRect(const QRectF& r, int deg) {
    switch (((deg % 360) + 360) % 360) {
    case 90: return QRectF(1.0 - r.y() - r.height(), r.x(), r.height(), r.width());
    case 180: return QRectF(1.0 - r.x() - r.width(), 1.0 - r.y() - r.height(), r.width(), r.height());
    case 270: return QRectF(r.y(), 1.0 - r.x() - r.width(), r.height(), r.width());
    default: return r;
    }
}
}

void MainWindow::findAndRedact() {
    if (!hasActiveDoc())
        return;

    FindRedactDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!dialog.hasPattern()) {
        m_toast->show(tr("Choose at least one pattern to search for."));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    int total = 0;
    QString error;
    const QHash<int, QList<QRectF>> byOrig =
        PatternRedactor::findMatches(m_doc->filePath(), dialog.pattern(), &total, &error);
    QApplication::restoreOverrideCursor();

    if (byOrig.isEmpty()) {
        if (!error.isEmpty())
            QMessageBox::warning(this, tr("Couldn't search"), error);
        else
            m_toast->show(tr("No matching text found."));
        return;
    }

    // Found rects are in each page's natural orientation; place them on the
    // matching display slots, honouring any in-session rotation.
    QHash<int, QList<QRectF>> bySlot;
    for (int slot = 0; slot < m_doc->pageCount(); ++slot) {
        const auto it = byOrig.constFind(m_doc->originalPageAt(slot));
        if (it == byOrig.constEnd())
            continue;
        const int rot = m_doc->rotation(slot);
        QList<QRectF>& dst = bySlot[slot];
        for (const QRectF& r : it.value())
            dst.push_back(rot ? rotateNormRect(r, rot) : r);
    }

    setRedactionMode(true);
    m_viewport->addRedactions(bySlot);
    m_toast->show(tr("Found %n match(es): review the marks, then Apply.", "", total));
}

void MainWindow::sanitizeDocument() {
    if (!hasActiveDoc())
        return;

    SanitizeDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!dialog.anyChecked()) {
        m_toast->show(tr("Choose at least one thing to remove."));
        return;
    }

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-clean.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save cleaned PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    Sanitizer::Report report;
    QString error;
    const bool ok =
        Sanitizer::sanitize(m_doc->filePath(), out, dialog.options(), &report, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't clean the document"), error);
        return;
    }
    if (report.total() == 0)
        m_toast->show(tr("Nothing hidden to remove: saved a clean copy."));
    else
        m_toast->show(tr("Removed %n hidden item(s): saved %1", "", report.total())
                          .arg(QFileInfo(out).fileName()));
    openPath(out);
}

void MainWindow::convertToPdfA() {
    if (!hasActiveDoc())
        return;
    // The dialog owns the convert + preflight work and shows the report inline.
    PdfADialog dialog(m_doc->filePath(), this);
    dialog.exec();
}

void MainWindow::runBatch() {
    // The wizard runs over a list of files, not the active document, so it does
    // not require an open doc — but if one is open, seed the queue with it.
    QStringList seed;
    if (hasActiveDoc())
        seed << m_doc->filePath();
    BatchDialog dialog(seed, this);
    dialog.exec();
}

void MainWindow::addLink() {
    if (!hasActiveDoc())
        return;
    m_placingLink = true;
    setRedactionMode(false); // the drag modes are mutually exclusive
    setHighlightMode(false);
    setMeasureMode(false);
    m_viewport->setFieldPlacementMode(true);
    m_toast->show(tr("Draw the area the link should cover."));
}

void MainWindow::finishAddLink(int slot, const QRectF& normRect) {
    m_placingLink = false;
    m_viewport->setFieldPlacementMode(false);
    if (!hasActiveDoc())
        return;

    LinkUrlDialog dialog(QString(), this);
    if (dialog.exec() != QDialog::Accepted || dialog.url().isEmpty()) {
        m_toast->show(tr("Link cancelled."));
        return;
    }

    const QString out = QFileDialog::getSaveFileName(this, tr("Save with link"), m_doc->filePath(),
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    const int page = std::max(0, m_doc->originalPageAt(slot));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok =
        LinkEditor::addUriLink(m_doc->filePath(), out, page, normRect, dialog.url(), &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't add link"), error);
        return;
    }
    m_toast->show(tr("Added link to %1").arg(QFileInfo(out).fileName()));
    openPath(out);
}

void MainWindow::editLinks() {
    if (!hasActiveDoc())
        return;

    const QList<LinkEditor::Link> links = LinkEditor::read(m_doc->filePath());
    LinksDialog dialog(links, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QList<LinkEditor::Edit> edits = dialog.edits();
    if (edits.isEmpty()) {
        m_toast->show(tr("No link changes to apply."));
        return;
    }

    const QString out = QFileDialog::getSaveFileName(this, tr("Save with edited links"),
                                                     m_doc->filePath(),
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = LinkEditor::applyEdits(m_doc->filePath(), out, edits, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't update links"), error);
        return;
    }
    m_toast->show(tr("Updated %n link(s): saved %1", "", int(edits.size()))
                      .arg(QFileInfo(out).fileName()));
    openPath(out);
}

void MainWindow::addStamp() {
    if (!hasActiveDoc())
        return;

    StampDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // Remember the chosen stamp; the date is resolved now so the caption matches
    // what the preview showed. Then let the user drag where it goes.
    m_pendingStampPreset = dialog.preset();
    m_pendingStampDate =
        dialog.includeDate() ? QDate::currentDate().toString(Qt::ISODate) : QString();
    m_pendingStampName = dialog.name();
    m_placingStamp = true;

    setRedactionMode(false); // the drag modes are mutually exclusive
    setHighlightMode(false);
    setMeasureMode(false);
    m_viewport->setFieldPlacementMode(true);
    m_toast->show(tr("Draw where the stamp goes."));
}

void MainWindow::finishAddStamp(int slot, const QRectF& normRect) {
    m_placingStamp = false;
    m_viewport->setFieldPlacementMode(false);
    if (!hasActiveDoc())
        return;

    const QString out = QFileDialog::getSaveFileName(this, tr("Save with stamp"), m_doc->filePath(),
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    const int page = std::max(0, m_doc->originalPageAt(slot));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = StampLibrary::addStamp(m_doc->filePath(), out, page, normRect,
                                           m_pendingStampPreset, m_pendingStampDate,
                                           m_pendingStampName, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't add stamp"), error);
        return;
    }
    m_toast->show(tr("Added stamp to %1").arg(QFileInfo(out).fileName()));
    openPath(out);
}

void MainWindow::activateTool(const QString& id) {
    // Route a Tools entry (from the pane or the Tools menu) to its action. Only
    // "edit" (M8 text editing) is still a placeholder.
    if (id == QLatin1String("export")) {
        saveActiveAs();
    } else if (id == QLatin1String("organize")) {
        if (hasActiveDoc())
            m_rail->setCurrentPanel(NavigationRail::Panel::Thumbnails);
    } else if (id == QLatin1String("combine")) {
        combineDocuments();
    } else if (id == QLatin1String("split")) {
        splitDocument();
    } else if (id == QLatin1String("compare")) {
        compareDocuments();
    } else if (id == QLatin1String("optimize")) {
        optimizeDocument();
    } else if (id == QLatin1String("cmyk")) {
        convertToCmyk();
    } else if (id == QLatin1String("watermark")) {
        watermarkDocument();
    } else if (id == QLatin1String("bates")) {
        batesNumber();
    } else if (id == QLatin1String("protect")) {
        protectDocument();
    } else if (id == QLatin1String("flatten")) {
        flattenDocument();
    } else if (id == QLatin1String("redact")) {
        if (hasActiveDoc())
            setRedactionMode(!m_viewport->redactionMode());
    } else if (id == QLatin1String("comment")) {
        if (hasActiveDoc())
            setHighlightMode(!m_viewport->highlightMode());
    } else if (id == QLatin1String("snapshot")) {
        startSnapshot();
    } else if (id == QLatin1String("measure")) {
        if (hasActiveDoc())
            setMeasureMode(!m_viewport->measureMode());
    } else if (id == QLatin1String("sign")) {
        signDocument();
    } else if (id == QLatin1String("ltv")) {
        addLongTermValidation();
    } else if (id == QLatin1String("stamp")) {
        addStamp();
    } else if (id == QLatin1String("edit")) {
        editTextBoxes();
    } else if (id == QLatin1String("forms")) {
        if (hasActiveDoc())
            m_rail->setCurrentPanel(NavigationRail::Panel::Forms);
    } else if (id == QLatin1String("prepare-form")) {
        if (hasActiveDoc()) {
            m_rail->setCurrentPanel(NavigationRail::Panel::Forms);
            prepareForm();
        }
    } else if (id == QLatin1String("read-aloud")) {
        readAloud();
    } else if (id == QLatin1String("ocr")) {
        recognizeText();
    } else if (id == QLatin1String("pdfa")) {
        convertToPdfA();
    } else if (id == QLatin1String("batch")) {
        runBatch();
    } else if (id == QLatin1String("create")) {
        createPdf();
    } else if (id == QLatin1String("scan")) {
        scanDocument();
    } else if (id == QLatin1String("export")) {
        exportDocument();
    } else {
        notImplemented(id.left(1).toUpper() + id.mid(1));
    }
}

void MainWindow::createPdf() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Create PDF from files"), QString(),
        tr("Documents and images (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp *.doc *.docx "
           "*.odt *.rtf *.txt *.xls *.xlsx *.ods *.csv *.ppt *.pptx *.odp);;All files (*)"));
    if (files.isEmpty())
        return;

    bool allImages = true;
    for (const QString& f : files)
        if (!Converter::isImage(f)) {
            allImages = false;
            break;
        }

    if (!allImages && files.size() != 1) {
        QMessageBox::information(
            this, tr("Create PDF"),
            tr("Select images to combine into one PDF, or a single document to convert."));
        return;
    }

    const QFileInfo first(files.first());
    const QString suggested =
        first.dir().filePath(first.completeBaseName() + QStringLiteral(".pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    if (allImages) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString error;
        const bool ok = Converter::imagesToPdf(files, out, &error);
        QApplication::restoreOverrideCursor();
        if (!ok) {
            QMessageBox::warning(this, tr("Couldn't create PDF"), error);
            return;
        }
        m_toast->show(tr("Created %1").arg(QFileInfo(out).fileName()));
        openPath(out);
        return;
    }

    if (!Converter::hasOfficeConverter()) {
        QMessageBox::information(
            this, tr("Create PDF"),
            tr("Converting this document needs LibreOffice, which isn't installed."));
        return;
    }

    // LibreOffice can take several seconds - convert off the UI thread.
    m_toast->show(tr("Converting…"));
    const QString input = files.first();
    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, out] {
        const bool ok = watcher->result();
        watcher->deleteLater();
        if (!ok) {
            QMessageBox::warning(this, tr("Couldn't create PDF"),
                                 tr("The document couldn't be converted."));
            return;
        }
        m_toast->show(tr("Created %1").arg(QFileInfo(out).fileName()));
        openPath(out);
    });
    watcher->setFuture(QtConcurrent::run([input, out] {
        QString error;
        return Converter::officeToPdf(input, out, &error);
    }));
}

void MainWindow::scanDocument() {
    if (!Scanner::isAvailable()) {
        m_toast->show(tr("Scanning needs SANE (install the “sane-backends” package)."));
        return;
    }

    ScanDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString suggested =
        QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
            .filePath(QStringLiteral("scan.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save scan"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        QMessageBox::warning(this, tr("Couldn't scan"),
                             tr("Couldn't create a temporary working folder."));
        return;
    }

    m_toast->show(tr("Scanning…"));
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QString error;
    const QStringList images = Scanner::scanPages(dialog.deviceName(), dialog.resolution(),
                                                  dialog.mode(), dialog.pageCount(), tmp.path(),
                                                  &error);
    if (images.isEmpty()) {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Couldn't scan"),
                             error.isEmpty() ? tr("No pages were scanned.") : error);
        return;
    }

    if (!Converter::imagesToPdf(images, out, &error)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Couldn't scan"), error);
        return;
    }

    if (dialog.runOcr()) {
        m_toast->show(tr("Recognizing text…"));
        QApplication::processEvents();
        Ocr::Options opts; // default clean-up (deskew/despeckle/binarize)
        QString ocrError;
        if (!Ocr::addTextLayer(out, out, dialog.ocrLanguage(), opts, &ocrError)) {
            // The scan still succeeded; OCR is a best-effort extra.
            m_toast->show(tr("Scanned, but OCR failed: %1").arg(ocrError));
        }
    }

    QApplication::restoreOverrideCursor();
    m_toast->show(tr("Scanned %n page(s) into %1.", "", images.size())
                      .arg(QFileInfo(out).fileName()));
    openPath(out);
}

void MainWindow::exportDocument() {
    if (!hasActiveDoc())
        return;
    if (!Converter::hasOfficeConverter()) {
        QMessageBox::information(
            this, tr("Export"),
            tr("Exporting to an editable document needs LibreOffice, which isn't installed."));
        return;
    }

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral(".docx"));
    const QString out = QFileDialog::getSaveFileName(
        this, tr("Export to editable document"), suggested,
        tr("Word document (*.docx);;OpenDocument Text (*.odt);;Rich Text (*.rtf);;"
           "Plain text (*.txt)"));
    if (out.isEmpty())
        return;

    // LibreOffice can take several seconds - convert off the UI thread.
    m_toast->show(tr("Exporting…"));
    const QString input = m_doc->filePath();
    const bool isText = QFileInfo(out).suffix().compare(QStringLiteral("txt"),
                                                        Qt::CaseInsensitive) == 0;
    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, out, isText] {
        const bool ok = watcher->result();
        watcher->deleteLater();
        if (!ok) {
            QMessageBox::warning(this, tr("Couldn't export"),
                                 tr("The document couldn't be exported."));
            return;
        }
        // Word/ODT come from LibreOffice's PDF import, which approximates layout
        // and can repeat text - say so once so the result isn't a surprise.
        m_toast->show(isText ? tr("Exported %1").arg(QFileInfo(out).fileName())
                             : tr("Exported %1: check the layout; conversion is approximate")
                                   .arg(QFileInfo(out).fileName()));
    });
    watcher->setFuture(QtConcurrent::run([input, out] {
        QString error;
        return Converter::pdfToOffice(input, out, &error);
    }));
}

void MainWindow::exportPagesAsImages() {
    if (!hasActiveDoc())
        return;

    ExportImageDialog dialog(m_doc->pageCount(), m_viewport->currentPage() + 1, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QVector<int> pageSlots = dialog.selectedPages();
    if (pageSlots.isEmpty()) {
        m_toast->show(tr("No pages in that range to export."));
        return;
    }

    const QFileInfo info(m_doc->filePath());
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a folder for the images"),
                                                          info.absolutePath());
    if (dir.isEmpty())
        return;

    // Map display slots back to source pages + session rotation, so the images
    // honour any in-session rotate/delete/reorder (mirrors Extract Pages).
    QVector<ImageExporter::PageSpec> specs;
    specs.reserve(pageSlots.size());
    for (int slot : pageSlots)
        specs.push_back({m_doc->originalPageAt(slot), m_doc->rotation(slot), slot + 1});

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const int n = ImageExporter::renderPages(m_doc->pdf(), specs, dir, info.completeBaseName(),
                                              dialog.format(), dialog.dpi(), &error);
    QApplication::restoreOverrideCursor();

    if (n == 0) {
        QMessageBox::warning(this, tr("Couldn't export images"), error);
        return;
    }
    m_toast->show(tr("Exported %n image(s) to %1", "", n).arg(QDir(dir).dirName()));
}

void MainWindow::startSnapshot() {
    if (!hasActiveDoc())
        return;
    setRedactionMode(false); // the drag modes are mutually exclusive
    setHighlightMode(false);
    setMeasureMode(false);
    m_viewport->setSnapshotMode(true);
    m_toast->show(tr("Drag to select a region to copy as an image."));
}

void MainWindow::snapshotRegion(int slot, const QRectF& normRect) {
    m_viewport->setSnapshotMode(false); // one-shot: leave the mode after a capture
    if (!hasActiveDoc())
        return;

    // Map the display slot back to its source page + session rotation, so the
    // snapshot honours any in-session rotate/reorder (mirrors Export as Images).
    const int source = m_doc->originalPageAt(slot);
    const int rotation = m_doc->rotation(slot);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    // 200 DPI gives a crisp clipboard image and a usable PNG.
    const QImage image =
        SnapshotExporter::renderRegion(m_doc->pdf(), source, rotation, normRect, 200, &error);
    QApplication::restoreOverrideCursor();

    if (image.isNull()) {
        QMessageBox::warning(this, tr("Couldn't snapshot"), error);
        return;
    }

    // Always copy to the clipboard; then offer to save a PNG as well.
    QApplication::clipboard()->setImage(image);

    SnapshotDialog dialog(image, this);
    if (dialog.exec() != QDialog::Accepted) {
        m_toast->show(tr("Snapshot copied to the clipboard."));
        return;
    }

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-snapshot.png"));
    const QString out =
        QFileDialog::getSaveFileName(this, tr("Save snapshot"), suggested, tr("PNG image (*.png)"));
    if (out.isEmpty()) {
        m_toast->show(tr("Snapshot copied to the clipboard."));
        return;
    }
    if (!image.save(out, "PNG")) {
        QMessageBox::warning(this, tr("Couldn't save snapshot"),
                             tr("The image couldn't be written to that location."));
        return;
    }
    m_toast->show(tr("Saved snapshot to %1").arg(QFileInfo(out).fileName()));
}

void MainWindow::extractEmbeddedImages() {
    if (!hasActiveDoc())
        return;
    if (!ImageExporter::hasImageExtractor()) {
        QMessageBox::information(
            this, tr("Extract Embedded Images"),
            tr("Extracting embedded images needs the pdfimages tool (from Poppler), which "
               "isn't installed."));
        return;
    }

    const QFileInfo info(m_doc->filePath());
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a folder for the images"),
                                                          info.absolutePath());
    if (dir.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const int n = ImageExporter::extractEmbedded(m_doc->filePath(), dir, info.completeBaseName(),
                                                 &error);
    QApplication::restoreOverrideCursor();

    if (n == 0) {
        if (!error.isEmpty())
            QMessageBox::warning(this, tr("Couldn't extract images"), error);
        else
            m_toast->show(tr("This PDF has no embedded images."));
        return;
    }
    m_toast->show(tr("Extracted %n image(s) to %1", "", n).arg(QDir(dir).dirName()));
}

void MainWindow::optimizeDocument() {
    if (!hasActiveDoc())
        return;

    // Audit first so the dialog can show where the bytes live.
    Optimizer::Audit audit;
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool audited = Optimizer::audit(m_doc->filePath(), &audit, &error);
    QApplication::restoreOverrideCursor();
    if (!audited) {
        QMessageBox::warning(this, tr("Couldn't optimize"), error);
        return;
    }

    OptimizeDialog dialog(audit, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-optimized.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save optimized PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    Optimizer::Report report;
    const bool ok = Optimizer::optimize(m_doc->filePath(), out, dialog.options(), &report, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't optimize"), error);
        return;
    }
    const qint64 before = report.beforeBytes, after = report.afterBytes;
    const double pct = before > 0 ? 100.0 * (before - after) / before : 0.0;
    m_toast->show(tr("Optimized: %1 → %2 (%3% smaller)")
                      .arg(locale().formattedDataSize(before),
                           locale().formattedDataSize(after))
                      .arg(pct, 0, 'f', 1));
    openPath(out);
}

void MainWindow::convertToCmyk() {
    if (!hasActiveDoc())
        return;
    if (!CmykConverter::isAvailable()) {
        QMessageBox::information(
            this, tr("RGB to CMYK"),
            tr("This needs Ghostscript, which isn't installed. Install the 'ghostscript' "
               "package and try again."));
        return;
    }
    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-cmyk.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save CMYK PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    const QString input = m_doc->filePath();
    m_toast->show(tr("Converting to CMYK…"));
    auto* watcher = new QFutureWatcher<bool>(this);
    auto* err = new QString;
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, out, err] {
        const bool ok = watcher->result();
        const QString message = *err;
        watcher->deleteLater();
        delete err;
        if (!ok) {
            QMessageBox::warning(this, tr("Couldn't convert to CMYK"), message);
            return;
        }
        m_toast->show(tr("Converted to CMYK"));
        openPath(out);
    });
    watcher->setFuture(QtConcurrent::run([input, out, err] {
        return CmykConverter::toCmyk(input, out, err);
    }));
}

void MainWindow::compareDocuments() {
    if (!hasActiveDoc())
        return;
    const QString other = QFileDialog::getOpenFileName(
        this, tr("Compare current document with…"), QFileInfo(m_doc->filePath()).absolutePath(),
        tr("PDF documents (*.pdf)"));
    if (other.isEmpty())
        return;

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-comparison.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save comparison PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    // The current document is the newer one; the picked file is the baseline.
    const QString baseline = other;
    const QString current = m_doc->filePath();
    m_toast->show(tr("Comparing…"));
    auto* watcher = new QFutureWatcher<bool>(this);
    auto* changed = new int(0);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, out, changed] {
        const bool ok = watcher->result();
        const int n = *changed;
        watcher->deleteLater();
        delete changed;
        if (!ok) {
            QMessageBox::warning(this, tr("Couldn't compare"), tr("The comparison failed."));
            return;
        }
        m_toast->show(n == 0 ? tr("No visual differences found")
                             : tr("%n page(s) changed", "", n));
        openPath(out);
    });
    watcher->setFuture(QtConcurrent::run([baseline, current, out, changed] {
        QString error;
        return Comparer::compare(baseline, current, out, changed, &error);
    }));
}

void MainWindow::compareText() {
    if (!hasActiveDoc())
        return;
    const QString other = QFileDialog::getOpenFileName(
        this, tr("Compare text with…"), QFileInfo(m_doc->filePath()).absolutePath(),
        tr("PDF documents (*.pdf)"));
    if (other.isEmpty())
        return;

    // The current document is the newer one; the picked file is the baseline.
    const QString baseline = other;
    const QString current = m_doc->filePath();
    m_toast->show(tr("Comparing text…"));
    auto* watcher = new QFutureWatcher<TextComparer::Result>(this);
    connect(watcher, &QFutureWatcher<TextComparer::Result>::finished, this,
            [this, watcher, baseline, current] {
                const TextComparer::Result result = watcher->result();
                watcher->deleteLater();
                if (!result.changed() && result.pageCount == 0) {
                    QMessageBox::warning(this, tr("Couldn't compare"),
                                         tr("One of the PDFs couldn't be read for comparison."));
                    return;
                }
                CompareReportDialog dialog(result, QFileInfo(baseline).fileName(),
                                           QFileInfo(current).fileName(), this);
                dialog.exec();
            });
    watcher->setFuture(QtConcurrent::run([baseline, current] {
        QString error;
        return TextComparer::compare(baseline, current, &error);
    }));
}

void MainWindow::watermarkDocument() {
    if (!hasActiveDoc())
        return;
    WatermarkDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (dialog.useImage() ? dialog.imagePath().isEmpty() : dialog.text().isEmpty())
        return;

    const QFileInfo info(m_doc->filePath());
    const QString out = QFileDialog::getSaveFileName(
        this, tr("Save watermarked PDF"),
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-watermarked.pdf")),
        tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    bool ok;
    if (dialog.useImage()) {
        Watermarker::ImageWatermarkOptions opts;
        opts.imagePath = dialog.imagePath();
        opts.opacity = dialog.opacity();
        opts.scale = dialog.scale();
        opts.rotationDeg = dialog.rotation();
        ok = Watermarker::addImageWatermark(m_doc->filePath(), out, opts, &error);
    } else {
        Watermarker::WatermarkOptions opts;
        opts.text = dialog.text();
        opts.color = dialog.color();
        opts.opacity = dialog.opacity();
        opts.fontSize = dialog.fontSize();
        opts.rotationDeg = dialog.rotation();
        ok = Watermarker::addWatermark(m_doc->filePath(), out, opts, &error);
    }
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't add watermark"), error);
        return;
    }
    m_toast->show(tr("Watermark added"));
    openPath(out);
}

void MainWindow::batesNumber() {
    if (!hasActiveDoc())
        return;
    BatesDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QFileInfo info(m_doc->filePath());
    const QString out = QFileDialog::getSaveFileName(
        this, tr("Save numbered PDF"),
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-bates.pdf")),
        tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = Watermarker::addBates(m_doc->filePath(), out, dialog.options(), &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't add numbering"), error);
        return;
    }
    m_toast->show(tr("Bates numbering added"));
    openPath(out);
}

void MainWindow::addHeaderFooter() {
    if (!hasActiveDoc())
        return;
    HeaderFooterDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!dialog.anyText()) {
        m_toast->show(tr("Enter some header or footer text."));
        return;
    }

    const QFileInfo info(m_doc->filePath());
    const QString out = QFileDialog::getSaveFileName(
        this, tr("Save with header & footer"),
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-headerfooter.pdf")),
        tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = Watermarker::addHeaderFooter(m_doc->filePath(), out, dialog.options(), &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't add header & footer"), error);
        return;
    }
    m_toast->show(tr("Header & footer added"));
    openPath(out);
}

void MainWindow::flattenDocument() {
    if (!hasActiveDoc())
        return;
    FlattenDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QFileInfo info(m_doc->filePath());
    const QString out = QFileDialog::getSaveFileName(
        this, tr("Save flattened PDF"),
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-flattened.pdf")),
        tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = dialog.raster()
                        ? Flattener::flattenRaster(m_doc->filePath(), out, dialog.dpi(), &error)
                        : Flattener::flattenLossless(m_doc->filePath(), out, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't flatten"), error);
        return;
    }
    m_toast->show(tr("Flattened"));
    openPath(out);
}

void MainWindow::splitDocument() {
    if (!hasActiveDoc())
        return;
    SplitDialog dialog(m_doc->originalPageCount(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Choose a folder for the split files"),
        QFileInfo(m_doc->filePath()).absolutePath());
    if (dir.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    int files = 0;
    QString error;
    const bool ok = Splitter::split(m_doc->filePath(), dir,
                                    QFileInfo(m_doc->filePath()).completeBaseName(), dialog.mode(),
                                    dialog.everyN(), dialog.ranges(), &files, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't split"), error);
        return;
    }
    m_toast->show(tr("Split into %n file(s)", "", files));
}

void MainWindow::combineDocuments() {
    // Seed the list with the current document (if any), then let the user add
    // more and reorder. The merge is lossless (QPDF foreign-page copy).
    CombineDialog dialog(hasActiveDoc() ? m_doc->filePath() : QString(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QStringList inputs = dialog.paths();
    if (inputs.size() < 2)
        return;

    const QString suggested =
        QFileInfo(inputs.first()).dir().filePath(QStringLiteral("combined.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save combined PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = PdfEditor::combine(inputs, out, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't combine"), error);
        return;
    }
    m_toast->show(tr("Combined %1 files").arg(inputs.size()));
    openPath(out); // open the merged result in a new tab
}

void MainWindow::signDocument() {
    if (!hasActiveDoc())
        return;
    QStringList certs = Signer::availableCertificates();
    if (certs.isEmpty()) {
        // Dead end unless we offer a way in: a hardware token's certificate only
        // appears after its PKCS#11 module is registered as a security device.
        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(tr("No signing certificate"));
        box.setText(
            tr("No signing certificate was found in your certificate store. Import a certificate "
               "into the NSS database, or register a hardware token (smartcard / YubiKey) as a "
               "security device."));
        QAbstractButton* devicesBtn = nullptr;
        if (Signer::hasSecurityDeviceTools())
            devicesBtn = box.addButton(tr("Security devices…"), QMessageBox::ActionRole);
        box.addButton(QMessageBox::Close);
        box.exec();
        if (!devicesBtn || box.clickedButton() != devicesBtn)
            return;
        SecurityDevicesDialog(this).exec();
        certs = Signer::availableCertificates();
        if (certs.isEmpty())
            return;
    }

    SignDialog dialog(certs, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // Place the signature near the bottom-left of the current page.
    const int slot = m_viewport->currentPage();
    const int orig = m_doc->originalPageAt(slot);
    if (orig < 0)
        return;
    const QSizeF pt = m_doc->pdf()->pagePointSize(orig);
    const double w = std::min(240.0, pt.width() - 96.0);
    const QRectF rect(48, 40, std::max(160.0, w), 56);

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-signed.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save signed PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = Signer::sign(m_doc->filePath(), out, dialog.certificate(), dialog.password(),
                                 dialog.reason(), dialog.location(), orig, rect,
                                 dialog.imagePath(), &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't sign"), error);
        return;
    }

    // Optionally pin the signed file in time with an RFC 3161 trusted timestamp.
    if (dialog.wantsTimestamp()) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString tsError;
        const QString token = out + QStringLiteral(".tsr");
        const bool stamped = Signer::timestamp(out, dialog.tsaUrl(), token, &tsError);
        QApplication::restoreOverrideCursor();
        if (stamped)
            m_toast->show(tr("Signed, and timestamped to %1").arg(QFileInfo(token).fileName()));
        else
            QMessageBox::warning(this, tr("Signed, but timestamping failed"), tsError);
    } else {
        m_toast->show(tr("Signed and saved to %1").arg(QFileInfo(out).fileName()));
    }
    openPath(out);
    // Show the verification result for the freshly signed copy, offering to lock in
    // long-term validation data while we're here.
    SignaturesDialog sigs(Signer::verify(out), this, /*offerLtv=*/true);
    sigs.exec();
    if (sigs.ltvRequested())
        addLongTermValidation();
}

void MainWindow::viewSignatures() {
    if (!hasActiveDoc())
        return;
    SignaturesDialog dialog(Signer::verify(m_doc->filePath()), this, /*offerLtv=*/true);
    dialog.exec();
    if (dialog.ltvRequested())
        addLongTermValidation();
}

void MainWindow::addLongTermValidation() {
    if (!hasActiveDoc())
        return;

    // Nothing to enhance unless the document is actually signed.
    if (Signer::verify(m_doc->filePath()).isEmpty()) {
        QMessageBox::information(
            this, tr("Long-Term Validation"),
            tr("This document isn't signed yet. Sign it first, then add long-term validation so "
               "the signature keeps validating after the certificate expires."));
        return;
    }

    // Offer the PAdES-LTA archive timestamp too. Embedding it covers the whole
    // document (including the validation data) and renews trust over time, but it
    // needs a reachable Time Stamp Authority, so make it an explicit opt-in.
    QMessageBox ask(this);
    ask.setIcon(QMessageBox::Question);
    ask.setWindowTitle(tr("Long-Term Validation"));
    ask.setText(tr("Add the certificate chain so the signature keeps validating after the "
                   "certificate expires?"));
    ask.setInformativeText(tr("Optionally also embed a trusted archive timestamp (PAdES-LTA). "
                              "That covers the whole document and needs a reachable timestamp "
                              "authority."));
    QPushButton* withTs = ask.addButton(tr("Add with timestamp"), QMessageBox::AcceptRole);
    QPushButton* dssOnly = ask.addButton(tr("Add without timestamp"), QMessageBox::AcceptRole);
    ask.addButton(QMessageBox::Cancel);
    withTs->setObjectName(QStringLiteral("Share"));
    ask.exec();
    const bool wantTimestamp = ask.clickedButton() == withTs;
    if (ask.clickedButton() != withTs && ask.clickedButton() != dssOnly)
        return;

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-ltv.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save with long-term validation"),
                                                     suggested, tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    LtvSigner::Result res;
    const bool ok = LtvSigner::addValidationInfo(m_doc->filePath(), out, &error, &res);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't add long-term validation"), error);
        return;
    }

    // Report what got embedded; revocation data is only present when the
    // certificates advertise it and the network is reachable.
    QStringList parts;
    parts << tr("%n certificate(s)", nullptr, res.certs);
    if (res.ocsps > 0)
        parts << tr("%n OCSP response(s)", nullptr, res.ocsps);
    if (res.crls > 0)
        parts << tr("%n CRL(s)", nullptr, res.crls);

    if (wantTimestamp) {
        QSettings settings;
        const QString tsaUrl = settings.value(QStringLiteral("sign/tsaUrl"),
                                              QStringLiteral("https://freetsa.org/tsr"))
                                   .toString();
        const QString tmp = out + QStringLiteral(".ts.tmp");
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString tsError;
        const bool stamped = LtvSigner::addDocumentTimestamp(out, tmp, tsaUrl, &tsError);
        QApplication::restoreOverrideCursor();
        if (stamped && QFile::remove(out) && QFile::rename(tmp, out)) {
            m_toast->show(tr("Long-term validation added (%1) with an archive timestamp")
                              .arg(parts.join(QStringLiteral(", "))));
        } else {
            QFile::remove(tmp);
            QMessageBox::warning(this, tr("Validation added, but the timestamp failed"),
                                 tsError.isEmpty() ? tr("The archive timestamp couldn't be written.")
                                                   : tsError);
        }
    } else {
        m_toast->show(tr("Long-term validation added (%1)").arg(parts.join(QStringLiteral(", "))));
    }
    openPath(out);
}

void MainWindow::recognizeText() {
    if (!hasActiveDoc())
        return;
    if (!Ocr::isAvailable()) {
        QMessageBox::information(
            this, tr("Recognize Text"),
            tr("Text recognition needs Tesseract, which isn't installed."));
        return;
    }
    const QStringList langs = Ocr::languages();
    if (langs.isEmpty()) {
        QMessageBox::information(this, tr("Recognize Text"),
                                 tr("No Tesseract language data is installed."));
        return;
    }

    OcrDialog dialog(langs, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString language = dialog.language();
    Ocr::Options ocrOptions;
    ocrOptions.deskew = dialog.deskew();
    ocrOptions.despeckle = dialog.despeckle();
    ocrOptions.binarize = dialog.binarize();
    ocrOptions.autoLanguage = dialog.autoLanguage();

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-ocr.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save recognized PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    // OCR is slow (render + Tesseract per page) - run it off the UI thread.
    m_toast->show(tr("Recognizing text… this may take a while."));
    const QString input = m_doc->filePath();
    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, out] {
        const bool ok = watcher->result();
        watcher->deleteLater();
        if (!ok) {
            QMessageBox::warning(this, tr("Couldn't recognize text"),
                                 tr("Text recognition failed."));
            return;
        }
        m_toast->show(tr("Recognized text - saved to %1").arg(QFileInfo(out).fileName()));
        openPath(out);
    });
    watcher->setFuture(QtConcurrent::run([input, out, language, ocrOptions] {
        QString error;
        return Ocr::addTextLayer(input, out, language, ocrOptions, &error);
    }));
}

void MainWindow::protectDocument() {
    if (!hasActiveDoc())
        return;

    ProtectDialog dialog(m_doc->fileName(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString password = dialog.password();
    const PdfEditor::Permissions perms{dialog.allowPrinting(), dialog.allowCopying(),
                                       dialog.allowEditing()};

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-protected.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save protected PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    // Apply the current edit arrangement first (rotation/delete/reorder), then
    // encrypt - so the protected copy reflects what's on screen. Two-step: write
    // the arrangement to the chosen path, then encrypt it in place.
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    bool ok = PdfEditor::saveArrangement(m_doc->filePath(), out, m_doc->pageOrder(),
                                         m_doc->rotations(), &error);
    if (ok)
        ok = PdfEditor::protect(out, out, password, perms, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't protect"), error);
        return;
    }
    // The result is encrypted; the viewer can't reopen it without password
    // support yet, so just confirm rather than loading it into a tab.
    m_toast->show(tr("Saved protected copy to %1").arg(QFileInfo(out).fileName()));
}

void MainWindow::removeProtection() {
    if (!hasActiveDoc() || !m_doc->isEncrypted())
        return;

    const QFileInfo info(m_doc->filePath());
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-unprotected.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save unprotected PDF"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok =
        PdfEditor::removeProtection(m_doc->filePath(), out, m_doc->password(), &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't remove password"), error);
        return;
    }
    m_toast->show(tr("Saved unprotected copy to %1").arg(QFileInfo(out).fileName()));
    openPath(out); // the result opens without a prompt
}

bool MainWindow::saveActive() {
    if (!hasActiveDoc())
        return false;
    Session* s = session(m_activeId);
    if (s && s->undo && s->undo->isClean())
        return true; // nothing to write

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = PdfEditor::saveArrangement(m_doc->filePath(), m_doc->filePath(),
                                               m_doc->pageOrder(), m_doc->rotations(), &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Couldn't save"), error);
        return false;
    }
    m_doc->markSaved();
    if (s && s->undo)
        s->undo->setClean();
    m_toast->show(tr("Saved"));
    return true;
}

void MainWindow::printActive() {
    if (!hasActiveDoc())
        return;

    // Custom dialog: opens instantly, discovers printers off-thread, shows a
    // preview, and supports odd/even subsets - none of which the native dialog
    // does without freezing the UI on open.
    PrintDialog dialog(m_doc->pdf(), m_doc->pageOrder(), m_doc->rotations(), m_doc->fileName(),
                       m_viewport->currentPage(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QList<int> pages = dialog.selectedSlots();
    if (pages.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QPrinter printer(QPrinter::HighResolution);
    const QString printerName = dialog.selectedPrinter();
    if (printerName.isEmpty()) {
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(dialog.outputFile());
    } else {
        printer.setPrinterName(printerName); // loads the PPD (brief, expected)
    }
    printer.setCopyCount(dialog.copies());
    printer.setDocName(m_doc->fileName());

    printDocument(printer, pages, dialog.grayscale());
    QApplication::restoreOverrideCursor();
    m_toast->show(printerName.isEmpty()
                      ? tr("Saved to %1").arg(QFileInfo(dialog.outputFile()).fileName())
                      : tr("Sent to %1").arg(printerName));
}

void MainWindow::printDocument(QPrinter& printer, const QList<int>& pages, bool grayscale) {
    QPdfDocument* pdf = m_doc->pdf();
    QPainter painter;
    if (!painter.begin(&printer))
        return;

    // Cap the render resolution so high-DPI printers don't make gigantic images;
    // the painter scales each page to fill the sheet.
    const int dpi = std::min(300, printer.resolution());

    bool firstPage = true;
    for (int slot : pages) {
        const int orig = m_doc->originalPageAt(slot);
        if (orig < 0)
            continue;
        if (!firstPage)
            printer.newPage();
        firstPage = false;

        const int rotation = m_doc->rotation(slot);
        QPdfDocumentRenderOptions opts;
        switch (rotation) {
        case 90: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise90); break;
        case 180: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise180); break;
        case 270: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise270); break;
        default: break;
        }

        QSizeF pt = pdf->pagePointSize(orig); // unrotated, in points
        if (rotation == 90 || rotation == 270)
            pt.transpose();
        const QSize imageSize(qRound(pt.width() / 72.0 * dpi), qRound(pt.height() / 72.0 * dpi));
        QImage image = pdf->render(orig, imageSize, opts);
        if (image.isNull())
            continue;
        if (grayscale)
            image = image.convertToFormat(QImage::Format_Grayscale8);

        // Fit the page onto the printable area, preserving aspect and centering.
        const QRect sheet = printer.pageLayout().paintRectPixels(printer.resolution());
        const QSize drawn = image.size().scaled(sheet.size(), Qt::KeepAspectRatio);
        const QRect target(sheet.x() + (sheet.width() - drawn.width()) / 2,
                           sheet.y() + (sheet.height() - drawn.height()) / 2, drawn.width(),
                           drawn.height());
        painter.drawImage(target, image);
    }

    painter.end();
}

void MainWindow::openFind() {
    if (!hasActiveDoc())
        return;
    m_findBar->activate();
    // Re-run the current query so the count reflects the active document.
    if (!m_findBar->query().isEmpty()) {
        m_searchIndex = 0;
        m_viewport->search(m_findBar->query());
        m_viewport->showSearchResult(0);
        updateFindCount();
    }
}

void MainWindow::stepFind(int delta) {
    const int count = m_viewport->searchResultCount();
    if (count <= 0)
        return;
    m_searchIndex += delta;
    m_viewport->showSearchResult(m_searchIndex);
    updateFindCount();
}

void MainWindow::updateFindCount() {
    const int count = m_viewport->searchResultCount();
    const int current = count > 0 ? (((m_searchIndex % count) + count) % count) + 1 : 0;
    m_findBar->setResult(current, count);
}

void MainWindow::nextPage() {
    if (m_viewport->hasDocument())
        m_viewport->goToPage(m_viewport->currentPage() + 1);
}

void MainWindow::previousPage() {
    if (m_viewport->hasDocument())
        m_viewport->goToPage(m_viewport->currentPage() - 1);
}

bool MainWindow::openPath(const QString& path) {
    const QString absolute = QFileInfo(path).absoluteFilePath();

    // Already open? Just focus that tab instead of opening a duplicate.
    for (const Session& s : m_sessions) {
        if (s.path == absolute) {
            activateSession(s.id);
            return true;
        }
    }

    auto* doc = new FeatherDocument(this);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    FeatherDocument::LoadResult result = doc->load(absolute);
    QApplication::restoreOverrideCursor();

    // Encrypted file: ask for the password and retry until it opens or the user
    // cancels. QtPdf reports a wrong/absent password on an AES-256 file as an
    // "unsupported scheme", so we ask QPDF whether the file actually needs a
    // password before prompting (and to avoid looping on a truly unknown scheme).
    using LR = FeatherDocument::LoadResult;
    const auto needsPassword = [](LR r) {
        return r == LR::PasswordRequired || r == LR::Unsupported;
    };
    bool cancelled = false;
    if (needsPassword(result) && PdfEditor::isPasswordProtected(absolute)) {
        PasswordDialog dialog(QFileInfo(absolute).fileName(), this);
        while (needsPassword(result)) {
            if (dialog.exec() != QDialog::Accepted) {
                cancelled = true;
                break;
            }
            QApplication::setOverrideCursor(Qt::WaitCursor);
            result = doc->load(absolute, dialog.password());
            QApplication::restoreOverrideCursor();
            if (needsPassword(result))
                dialog.setError(tr("Incorrect password. Try again."));
        }
    }

    if (result != FeatherDocument::LoadResult::Ok) {
        if (!cancelled) // cancelling the password prompt is not an error
            QMessageBox::warning(this, tr("Couldn't open document"),
                                 FeatherDocument::describe(result));
        doc->deleteLater();
        return false;
    }
    doc->setEncrypted(!doc->password().isEmpty() || PdfEditor::isPasswordProtected(absolute));

    addRecentFile(absolute);
    rebuildRecentMenu();

    // A new session gets its own tab and its own undo history.
    Session s;
    s.doc = doc;
    s.path = absolute;
    s.id = m_tabStrip->addDocument(doc->title());
    s.undo = new QUndoStack(this);
    m_undoGroup->addStack(s.undo);
    // The unsaved-changes dot follows this document's clean state.
    const int sid = s.id;
    connect(s.undo, &QUndoStack::cleanChanged, this,
            [this, sid](bool clean) { m_tabStrip->setDocumentDirty(sid, !clean); });
    // Keep the thumbnail panel in step with edits to this document while active.
    connect(doc, &FeatherDocument::pageEdited, this, [this, doc](int slot) {
        if (m_doc == doc)
            m_thumbnails->setRotation(slot, doc->rotation(slot));
    });
    connect(doc, &FeatherDocument::arrangementChanged, this, [this, doc] {
        if (m_doc == doc) {
            m_thumbnails->setArrangement(doc->pageOrder(), doc->rotations());
            updatePageIndicator();
        }
    });
    m_sessions.append(s);
    activateSession(s.id);
    applyViewDefaults(); // a freshly opened document gets the preferred layout/zoom
    return true;
}

MainWindow::Session* MainWindow::session(int id) {
    for (Session& s : m_sessions) {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

void MainWindow::activateSession(int id) {
    Session* target = session(id);
    if (!target)
        return;

    // Remember where the outgoing document was before we swap it out.
    if (Session* current = session(m_activeId); current && m_viewport->hasDocument())
        current->lastPage = m_viewport->currentPage();

    m_activeId = id;
    m_doc = target->doc;
    if (target->undo)
        m_undoGroup->setActiveStack(target->undo);
    m_findBar->hide(); // search is per-document; setDocument clears the query
    m_readAloudBar->stopAndHide(); // reading is per-document too
    m_searchIndex = 0;
    m_viewport->setDocument(m_doc);
    m_viewport->goToPage(target->lastPage);
    m_thumbnails->setDocument(m_doc->pdf());
    m_thumbnails->setArrangement(m_doc->pageOrder(), m_doc->rotations());
    m_thumbnails->setCurrentPage(target->lastPage);
    m_outline->setDocument(m_doc->pdf());
    // The semantic panels read the file on disk via Poppler; they rebuild lazily
    // the next time each is shown.
    const QString path = m_doc->filePath();
    m_annotations->setDocumentPath(path);
    m_attachments->setDocumentPath(path);
    m_layers->setDocumentPath(path);
    m_forms->setDocumentPath(path);

    m_tabStrip->setActiveDocument(id);
    updateWindowTitle();
    updateChromeState();
    showDocument();
}

void MainWindow::closeSession(int id) {
    const int idx = [this, id] {
        for (int i = 0; i < m_sessions.size(); ++i)
            if (m_sessions[i].id == id)
                return i;
        return -1;
    }();
    if (idx < 0)
        return;

    FeatherDocument* doc = m_sessions[idx].doc;
    QUndoStack* stack = m_sessions[idx].undo;
    m_sessions.remove(idx);
    m_tabStrip->removeDocument(id);
    if (stack) {
        m_undoGroup->removeStack(stack);
        stack->deleteLater();
    }

    const bool wasActive = (id == m_activeId);
    if (wasActive) {
        m_activeId = -1;
        m_doc = nullptr;
        m_readAloudBar->stopAndHide();
        m_viewport->clear();
        m_thumbnails->clear();
        m_outline->clear();
        m_annotations->clear();
        m_attachments->clear();
        m_layers->clear();
        m_forms->clear();
    }
    if (doc)
        doc->deleteLater();

    if (wasActive) {
        if (!m_sessions.isEmpty())
            activateSession(m_sessions.last().id); // focus the next remaining tab
        else {
            updateWindowTitle();
            updateChromeState();
            showHome();
        }
    }
}

bool MainWindow::hasActiveDoc() const {
    return m_doc != nullptr && m_doc->isLoaded();
}

void MainWindow::openFileDialog() {
    const QString start = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open PDF"), start, tr("PDF documents (*.pdf);;All files (*)"));
    if (!path.isEmpty())
        openPath(path);
}

void MainWindow::closeDocument() {
    if (m_activeId != -1)
        closeTab(m_activeId);
}

void MainWindow::closeTab(int id) {
    if (maybeSaveSession(id))
        closeSession(id);
}

bool MainWindow::maybeSaveSession(int id) {
    Session* s = session(id);
    if (!s || !s->undo || s->undo->isClean())
        return true; // nothing unsaved

    activateSession(id); // show the document the prompt is about
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Unsaved changes"));
    box.setText(tr("Save changes to “%1” before closing?").arg(s->doc->title()));
    box.setInformativeText(tr("Your changes will be lost if you don't save them."));
    box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Save);
    switch (box.exec()) {
    case QMessageBox::Save:
        return saveActive(); // abort the close if the save fails
    case QMessageBox::Discard:
        return true;
    default:
        return false; // Cancel
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Collect tabs with unsaved changes.
    QVector<int> dirty;
    for (const Session& s : m_sessions) {
        if (s.undo && !s.undo->isClean())
            dirty.append(s.id);
    }

    if (!dirty.isEmpty()) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(tr("Unsaved changes"));
        box.setText(tr("%n document(s) have unsaved changes.", "", dirty.size()));
        box.setInformativeText(tr("Save them before closing Feather PDF?"));
        box.setStandardButtons(QMessageBox::SaveAll | QMessageBox::Discard | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::SaveAll);
        switch (box.exec()) {
        case QMessageBox::SaveAll:
            for (int id : dirty) {
                activateSession(id);
                if (!saveActive()) { // a failed/cancelled save aborts the quit
                    event->ignore();
                    return;
                }
            }
            break;
        case QMessageBox::Discard:
            break;
        default:
            event->ignore();
            return;
        }
    } else if (m_sessions.size() > 1) {
        // No unsaved work, but several tabs are open - confirm the bulk close.
        const auto choice = QMessageBox::question(
            this, tr("Close Feather PDF"),
            tr("%n tab(s) are open. Close them all?", "", m_sessions.size()),
            QMessageBox::Close | QMessageBox::Cancel, QMessageBox::Close);
        if (choice != QMessageBox::Close) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void MainWindow::updateWindowTitle() {
    if (hasActiveDoc())
        setWindowTitle(tr("%1 | Feather PDF").arg(m_doc->title()));
    else
        setWindowTitle(tr("Feather PDF"));
}

void MainWindow::updateChromeState() {
    const bool loaded = hasActiveDoc();

    m_saveAct->setEnabled(loaded);
    m_saveAsAct->setEnabled(loaded);
    m_protectAct->setEnabled(loaded);
    m_exportAct->setEnabled(loaded);
    m_exportImagesAct->setEnabled(loaded);
    m_extractImagesAct->setEnabled(loaded);
    m_removeProtectionAct->setEnabled(loaded && m_doc && m_doc->isEncrypted());
    m_printAct->setEnabled(loaded);
    m_rotateLeftAct->setEnabled(loaded);
    m_rotateRightAct->setEnabled(loaded);
    m_deletePageAct->setEnabled(loaded);
    m_extractPagesAct->setEnabled(loaded);
    m_insertPagesAct->setEnabled(loaded);
    m_cropPagesAct->setEnabled(loaded);
    m_addFieldAct->setEnabled(loaded);
    m_exportXfdfAct->setEnabled(loaded);
    m_importXfdfAct->setEnabled(loaded);
    m_closeAct->setEnabled(loaded);
    m_zoomInAct->setEnabled(loaded);
    m_zoomOutAct->setEnabled(loaded);
    m_zoomActualAct->setEnabled(loaded);
    m_fitWidthAct->setEnabled(loaded);
    m_fitPageAct->setEnabled(loaded);
    m_singlePageAct->setEnabled(loaded);
    m_continuousAct->setEnabled(loaded);
    m_twoUpAct->setEnabled(loaded);
    m_immersiveAct->setEnabled(loaded);

    m_commandBar->setDocumentLoaded(loaded);

    updatePageIndicator();
    updateZoomIndicator();
}

void MainWindow::updatePageIndicator() {
    if (!hasActiveDoc()) {
        m_commandBar->setPageText("- / -");
        m_pill->setPageText("- / -");
        return;
    }
    const QString text =
        tr("%1 / %2").arg(m_viewport->currentPage() + 1).arg(m_viewport->pageCount());
    m_commandBar->setPageText(text);
    m_pill->setPageText(text);
}

void MainWindow::updateZoomIndicator() {
    if (!hasActiveDoc()) {
        m_commandBar->setZoomText("-");
        m_pill->setZoomText("-");
        return;
    }
    const QString text = tr("%1%").arg(qRound(m_viewport->zoomFactor() * 100.0));
    m_commandBar->setZoomText(text);
    m_pill->setZoomText(text);
}

void MainWindow::showAbout() {
    const auto& pal = Theme::instance().palette();

    // Frameless so there is no title-bar "Feather PDF" duplicating the name
    // inside - a clean floating card with a shadow instead.
    QDialog dlg(this);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground);

    auto* outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(18, 18, 18, 18); // room for the shadow

    auto* card = new QFrame(&dlg);
    card->setObjectName("AboutCard");
    card->setStyleSheet(QStringLiteral("#AboutCard { background:%1; border:1px solid %2;"
                                       " border-radius:16px; }")
                            .arg(pal.surface.name(), pal.hairline.name(QColor::HexArgb)));
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(44);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 90));
    card->setGraphicsEffect(shadow);
    outer->addWidget(card);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(40, 32, 40, 22);
    v->setSpacing(0);
    auto centered = [&](QWidget* w) { v->addWidget(w, 0, Qt::AlignHCenter); };

    auto* logo = new QLabel(card);
    logo->setPixmap(Theme::instance().brandLogo(96));
    centered(logo);
    v->addSpacing(16);

    auto* title = new QLabel(tr("Feather PDF"), &dlg);
    title->setStyleSheet(
        QStringLiteral("color:%1; font-size:24px; font-weight:700;").arg(pal.text.name()));
    centered(title);
    v->addSpacing(2);

    auto* ver = new QLabel(tr("Version %1").arg(QStringLiteral(FEATHERPDF_VERSION)), &dlg);
    ver->setStyleSheet(QStringLiteral("color:%1; font-family:'Source Code Pro',monospace;"
                                      " font-size:12px;")
                           .arg(pal.dim.name()));
    centered(ver);
    v->addSpacing(12);

    auto* tagline = new QLabel(tr("Light on the system, full-featured on PDF."), &dlg);
    tagline->setStyleSheet(QStringLiteral("color:%1; font-size:13px;").arg(pal.text.name()));
    centered(tagline);
    v->addSpacing(4);

    auto* desc = new QLabel(
        tr("A native, open-source PDF tool for Linux, licensed under the GPLv3."), &dlg);
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignHCenter);
    desc->setFixedWidth(300);
    desc->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(pal.dim.name()));
    centered(desc);
    v->addSpacing(18);

    auto* links = new QLabel(&dlg);
    links->setTextFormat(Qt::RichText);
    links->setOpenExternalLinks(true);
    const QString a = QStringLiteral("color:%1; text-decoration:none; font-weight:600;")
                          .arg(pal.accent.name());
    links->setText(
        tr("<a style='%1' href='https://github.com/s4rt4/featherpdf-linux'>GitHub</a>"
           "&nbsp;&nbsp;·&nbsp;&nbsp;"
           "<a style='%1' href='https://www.gnu.org/licenses/gpl-3.0.html'>License</a>")
            .arg(a));
    centered(links);
    v->addSpacing(20);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    v->addWidget(buttons);

    dlg.exec();
}

void MainWindow::showProperties() {
    if (!hasActiveDoc())
        return;
    QPdfDocument* pdf = m_doc->pdf();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Document properties"));
    auto* form = new QFormLayout(&dialog);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);

    auto addRow = [&](const QString& label, const QString& value, bool mono = false) {
        if (value.trimmed().isEmpty())
            return;
        auto* v = new QLabel(value, &dialog);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (mono) {
            QFont f = v->font();
            f.setFamily(QStringLiteral("monospace"));
            v->setFont(f);
        }
        form->addRow(new QLabel(label, &dialog), v);
    };
    auto meta = [&](QPdfDocument::MetaDataField f) { return pdf->metaData(f).toString(); };

    addRow(tr("Title"), meta(QPdfDocument::MetaDataField::Title));
    addRow(tr("Author"), meta(QPdfDocument::MetaDataField::Author));
    addRow(tr("Subject"), meta(QPdfDocument::MetaDataField::Subject));
    addRow(tr("Keywords"), meta(QPdfDocument::MetaDataField::Keywords));
    addRow(tr("Creator"), meta(QPdfDocument::MetaDataField::Creator));
    addRow(tr("Producer"), meta(QPdfDocument::MetaDataField::Producer));

    const QString created =
        pdf->metaData(QPdfDocument::MetaDataField::CreationDate).toDateTime().toString(Qt::TextDate);
    addRow(tr("Created"), created);

    const int original = m_doc->originalPageCount();
    QString pages = QString::number(m_doc->pageCount());
    if (m_doc->pageCount() != original)
        pages += tr(" (was %1)").arg(original);
    addRow(tr("Pages"), pages, /*mono=*/true);

    const QSizeF sz = pdf->pagePointSize(m_doc->originalPageAt(m_viewport->currentPage()));
    addRow(tr("Page size"), tr("%1 × %2 pt").arg(qRound(sz.width())).arg(qRound(sz.height())), true);

    const QFileInfo fi(m_doc->filePath());
    addRow(tr("File size"), QLocale().formattedDataSize(fi.size()), true);
    addRow(tr("Location"), fi.absoluteFilePath(), true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    form->addRow(buttons);

    dialog.exec();
}

void MainWindow::notImplemented(const QString& feature) {
    m_toast->show(tr("%1 arrives in a later milestone.").arg(feature));
}

QStringList MainWindow::recentFiles() const {
    QSettings settings;
    return settings.value(kRecentFilesKey).toStringList();
}

void MainWindow::addRecentFile(const QString& path) {
    QSettings settings;
    QStringList files = settings.value(kRecentFilesKey).toStringList();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > kMaxRecentFiles)
        files.removeLast();
    settings.setValue(kRecentFilesKey, files);
}

void MainWindow::rebuildRecentMenu() {
    m_recentMenu->clear();
    const QStringList files = recentFiles();
    if (files.isEmpty()) {
        QAction* empty = m_recentMenu->addAction(tr("No recent documents"));
        empty->setEnabled(false);
        return;
    }
    for (const QString& path : files) {
        QAction* act = m_recentMenu->addAction(QFileInfo(path).fileName());
        act->setData(path);
        act->setToolTip(path);
        connect(act, &QAction::triggered, this, [this, path] { openPath(path); });
    }
    m_recentMenu->addSeparator();
    QAction* clear = m_recentMenu->addAction(tr("Clear list"));
    connect(clear, &QAction::triggered, this, [this] {
        QSettings().remove(kRecentFilesKey);
        rebuildRecentMenu();
    });
}
