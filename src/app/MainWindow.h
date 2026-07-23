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

#pragma once

#include <QHash>
#include <QMainWindow>
#include <QPair>
#include <QRectF>
#include <QStringList>
#include <QVector>

#include "backends/FormEditor.h"
#include "backends/PdfEditor.h"
#include "backends/StampLibrary.h"

class FeatherDocument;
class Viewport;
class HomeView;
class DocsView;
class ThumbnailPanel;
class OutlinePanel;
class AnnotationsPanel;
class AttachmentsPanel;
class LayersPanel;
class FormPanel;
class TabStrip;
class CommandBar;
class FindBar;
class RedactionBar;
class AnnotationBar;
class MeasureBar;
class ReadAloudBar;
class NavigationRail;
class ToolsPane;
class FloatingPill;
class Toast;
class QAction;
class QAbstractAnimation;
class QLabel;
class QMenu;
class QPrinter;
class QStackedWidget;
class QUndoGroup;
class QUndoStack;

// The application shell (ui-guidelines §5): a tabbed Acrobat-style workspace -
// menu bar, tab strip, command toolbar, then the body (left navigation rail ·
// center viewport with its floating pill · right Tools pane). M0 is a single
// document; the multi-document tab workspace builds on this same shell.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Opens a PDF and shows it. Reports failures to the user. Returns success.
    bool openPath(const QString& path);

public slots:
    void openFileDialog();
    void closeDocument();
    void closeTab(int id); // prompts to save if the tab is dirty, then closes it

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    // Returns true if it is OK to proceed (saved or discarded); false to cancel.
    bool maybeSaveSession(int id);

    void buildActions();
    void buildMenus();
    void buildShell();
    void wireSignals();

    void nextPage();
    void previousPage();

    // Page edits (M1).
    void rotateActivePage(int deltaDegrees);
    void deleteActivePage();
    void extractActivePages(); // save a chosen page subset to a new PDF (QPDF)
    void insertPagesIntoActive(); // splice pages from another PDF, open the result
    void cropActivePages();       // trim page margins (/CropBox), open the result
    void saveOutline(const QVector<PdfEditor::OutlineItem>& items); // write /Outlines, open result
    void addFormField(); // author a new AcroForm field: configure, then place
    void prepareForm();  // auto-detect fillable areas, review, then add them all
    void readAloud();    // speak the document via speech-dispatcher (Read Aloud bar)
    void placeFormField(int slot, const QRectF& normRect); // drawn rect → create/move field
    void moveFormField(const QString& name);   // start repositioning an existing field
    void deleteFormField(const QString& name); // remove an existing field
    void exportFormData(); // write the document's form values to an XFDF file
    void importFormData(); // apply an XFDF file's values, open the result
    void editTextBoxes();  // M8(a): edit/delete the text boxes you've added
    void editInLibreOffice(); // M8 fallback: open the PDF in LibreOffice Draw
    void showPreferences();   // the Preferences dialog
    void applyViewDefaults(); // apply the saved default layout/zoom to the viewport
    bool saveActiveAs();      // export the edited document to a chosen path (QPDF)
    bool saveActive();        // write edits back to the current file (QPDF)
    void startSnapshot();           // enter the drag-to-snapshot region mode
    void snapshotRegion(int slot, const QRectF& normRect); // region drawn → clip + offer PNG
    void setRedactionMode(bool on); // enter/leave the draw-to-redact mode
    void applyRedactions();         // flatten marked pages, save, open the result
    void findAndRedact();           // find pattern matches, mark them for redaction (review then apply)
    void sanitizeDocument();        // strip metadata/attachments/scripts, save, open the result
    void convertToPdfA();           // tag toward PDF/A-1b + optional veraPDF preflight
    void runBatch();                // open the batch / action wizard over many files
    void addLink();                 // draw a rectangle, then make it a URL hyperlink
    void finishAddLink(int slot, const QRectF& normRect); // rect drawn → prompt URL → write
    void editLinks();               // list/edit/delete the document's hyperlinks
    void addStamp();                // pick a stamp preset, then draw where it goes
    void finishAddStamp(int slot, const QRectF& normRect); // rect drawn → write the stamp
    void setHighlightMode(bool on); // enter/leave the annotation-authoring mode
    void applyAnnotations();        // write highlight + note annotations, save, open result
    void setMeasureMode(bool on);   // enter/leave the click-to-measure mode
    void activateTool(const QString& id); // route a Tools entry (pane or menu) to its action
    void createPdf();         // make a PDF from images (native) or an office doc (LibreOffice)
    void scanDocument();      // scan from a SANE device into a new PDF (optional OCR)
    void exportDocument();    // export the PDF to an editable office doc (LibreOffice)
    void exportPagesAsImages(); // render selected pages to PNG/JPEG/TIFF (QPdfDocument)
    void extractEmbeddedImages(); // pull the raster images embedded in the PDF (pdfimages)
    void optimizeDocument();  // rewrite more compactly (QPDF)
    void convertToCmyk();     // convert all colors to process CMYK (Ghostscript)
    void compareDocuments();  // visual diff against another PDF
    void compareText();       // word-level text diff against another PDF
    void watermarkDocument(); // stamp a text watermark on every page
    void batesNumber();       // stamp sequential Bates numbers
    void addHeaderFooter();   // stamp header/footer text + page numbers
    void flattenDocument();   // bake annotations/forms in (lossless or raster)
    void splitDocument();     // split into several files
    void combineDocuments();  // merge several PDFs into one (QPDF)
    void signDocument();      // digitally sign with a certificate (Poppler/NSS)
    void viewSignatures();    // show/verify the document's existing signatures
    void addLongTermValidation(); // embed a DSS so signatures validate long term (LTV)
    void recognizeText();     // OCR - add a searchable text layer (Tesseract)
    void protectDocument();   // write a password-encrypted copy (QPDF, AES-256)
    void removeProtection();  // write a decrypted (password-free) copy (QPDF)
    void printActive();       // custom print dialog (async printers + preview)
    void printDocument(QPrinter& printer, const QList<int>& pages, bool grayscale);

    // Find: open the find bar, drive matches, and report the running count.
    void openFind();
    void stepFind(int delta);
    void updateFindCount();

    // One open document = one tab. Each session owns its FeatherDocument and
    // remembers the page it was last on so switching tabs restores the spot.
    struct Session {
        int id = -1;
        FeatherDocument* doc = nullptr;
        QString path;
        int lastPage = 0;
        QUndoStack* undo = nullptr; // this document's edit history
    };
    Session* session(int id);
    void activateSession(int id);
    void closeSession(int id);
    bool hasActiveDoc() const;

    // Switch the center between the Home start screen and the document workspace,
    // toggling the document-only chrome (rail · command bar · Tools pane · pill).
    void showHome();
    void showDocument();
    void openDocs();  // open/activate the Documentation tab
    void showDocs();   // switch the workspace to the docs view
    void closeDocs();  // close the docs tab and return to home/document

    // Immersive reading: hide all chrome, leaving the page alone with its pill
    // (ui-guidelines §1 - the signature moment). F11 toggles, Esc exits.
    void setImmersive(bool on);
    void animateChrome(bool collapse);                 // slide the chrome in/out
    QVector<QPair<QWidget*, bool>> chromeItems() const; // {widget, vertical}

    void updateWindowTitle();
    void updateChromeState();
    void updatePageIndicator();
    void updateZoomIndicator();

    void addRecentFile(const QString& path);
    void rebuildRecentMenu();
    QStringList recentFiles() const;

    void showProperties(); // Document ▸ Properties - metadata dialog
    void showAbout();      // Help ▸ About - branded dialog with links

    // Honest placeholder for features that arrive in later milestones.
    void notImplemented(const QString& feature);

    QVector<Session> m_sessions;
    int m_activeId = -1;
    FeatherDocument* m_doc = nullptr; // the active session's document, or nullptr

    Viewport* m_viewport = nullptr;
    HomeView* m_home = nullptr;
    DocsView* m_docsView = nullptr;
    QStackedWidget* m_centerStack = nullptr;
    TabStrip* m_tabStrip = nullptr;

    // Left rail expandable panel (one at a time).
    QWidget* m_panelHost = nullptr;
    QStackedWidget* m_panelStack = nullptr;
    QLabel* m_panelHead = nullptr;
    ThumbnailPanel* m_thumbnails = nullptr;
    OutlinePanel* m_outline = nullptr;
    AnnotationsPanel* m_annotations = nullptr;
    AttachmentsPanel* m_attachments = nullptr;
    LayersPanel* m_layers = nullptr;
    FormPanel* m_forms = nullptr;
    CommandBar* m_commandBar = nullptr;
    FindBar* m_findBar = nullptr;
    RedactionBar* m_redactionBar = nullptr;
    AnnotationBar* m_annotationBar = nullptr;
    MeasureBar* m_measureBar = nullptr;
    ReadAloudBar* m_readAloudBar = nullptr;
    int m_searchIndex = 0;
    NavigationRail* m_rail = nullptr;
    ToolsPane* m_toolsPane = nullptr;
    FloatingPill* m_pill = nullptr;
    Toast* m_toast = nullptr;

    // Actions (centralized; menus and shortcuts use these).
    QAction* m_openAct = nullptr;
    QAction* m_saveAct = nullptr;
    QAction* m_saveAsAct = nullptr;
    QAction* m_protectAct = nullptr;
    QAction* m_exportAct = nullptr;
    QAction* m_exportImagesAct = nullptr;
    QAction* m_extractImagesAct = nullptr;
    QAction* m_removeProtectionAct = nullptr;
    QAction* m_printAct = nullptr;
    QAction* m_closeAct = nullptr;
    QAction* m_quitAct = nullptr;
    QAction* m_undoAct = nullptr;
    QAction* m_redoAct = nullptr;
    QAction* m_rotateLeftAct = nullptr;
    QAction* m_rotateRightAct = nullptr;
    QAction* m_deletePageAct = nullptr;
    QAction* m_extractPagesAct = nullptr;
    QAction* m_insertPagesAct = nullptr;
    QAction* m_cropPagesAct = nullptr;
    QAction* m_addFieldAct = nullptr;
    QAction* m_exportXfdfAct = nullptr;
    QAction* m_importXfdfAct = nullptr;
    FormEditor::NewField m_pendingField; // field being placed (after the dialog)
    bool m_placingField = false;         // viewport is in field-placement mode
    QString m_movingField;               // non-empty: placing repositions this field
    bool m_placingLink = false;          // viewport drag is placing a new hyperlink
    bool m_placingStamp = false;         // viewport drag is placing a new stamp
    StampLibrary::Preset m_pendingStampPreset = StampLibrary::Preset::Approved;
    QString m_pendingStampDate;          // resolved date add-on (empty = none)
    QString m_pendingStampName;          // name add-on (empty = none)
    QUndoGroup* m_undoGroup = nullptr;
    QAction* m_zoomInAct = nullptr;
    QAction* m_zoomOutAct = nullptr;
    QAction* m_zoomActualAct = nullptr;
    QAction* m_fitWidthAct = nullptr;
    QAction* m_fitPageAct = nullptr;
    QAction* m_singlePageAct = nullptr;
    QAction* m_continuousAct = nullptr;
    QAction* m_twoUpAct = nullptr;
    QAction* m_toggleThemeAct = nullptr;
    QAction* m_immersiveAct = nullptr;
    QAction* m_aboutAct = nullptr;

    bool m_immersive = false;
    bool m_panelWasVisible = false;
    QAbstractAnimation* m_chromeAnim = nullptr;
    QHash<QWidget*, int> m_chromeExtent;

    QMenu* m_recentMenu = nullptr;

    static constexpr int kMaxRecentFiles = 10;
};
