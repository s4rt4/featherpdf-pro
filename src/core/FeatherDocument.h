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

#include <QObject>
#include <QString>
#include <QVector>

class QPdfDocument;

// FeatherDocument is the single façade over the PDF backends. Modules talk to
// this object, never to a backend directly - so page indexing and (later)
// coordinate conversion live in exactly one place. For M0 it wraps the QtPdf /
// PDFium document used for rendering; Poppler (semantic layer) and QPDF
// (lossless writer) are folded in behind this same interface in later milestones.
class FeatherDocument : public QObject {
    Q_OBJECT

public:
    explicit FeatherDocument(QObject* parent = nullptr);
    ~FeatherDocument() override;

    enum class LoadResult {
        Ok,
        FileNotFound,
        InvalidFormat,
        PasswordRequired,
        Unsupported,
        UnknownError,
    };

    // Loads a PDF from disk, replacing any currently open document. For an
    // encrypted file, pass the password; an empty password is fine for a normal
    // file and yields LoadResult::PasswordRequired for a protected one.
    LoadResult load(const QString& path, const QString& password = QString());
    void close();

    bool isLoaded() const;
    QString filePath() const { return m_filePath; }
    // The password the file opened with (empty for an unencrypted document).
    QString password() const { return m_password; }
    // True if this document is encrypted (set by the opener after a successful
    // load - QtPdf alone can't report this reliably, QPDF confirms it).
    bool isEncrypted() const { return m_encrypted; }
    void setEncrypted(bool encrypted) { m_encrypted = encrypted; }
    QString fileName() const;       // base name, e.g. "Annual-Report.pdf"
    QString title() const;          // metadata title, falls back to file name
    int pageCount() const;          // CURRENT (display) page count after edits
    int originalPageCount() const;  // pages in the file on disk

    // The rendering backend handle. The viewport (QtPdf) renders from this.
    // This accessor is the *one* sanctioned exception to "modules never touch a
    // backend" - only the render layer may use it.
    QPdfDocument* pdf() const { return m_pdf; }

    // Human-readable message for a load failure, ready for a dialog/toast.
    static QString describe(LoadResult result);

    // ── Page edits (M1) ──────────────────────────────────────────────────────
    // The façade owns the editable page arrangement as parallel per-slot arrays:
    // m_order[i] is the original (on-disk) page shown in display slot i, and
    // m_rot[i] its clockwise rotation. Rotate edits one slot; delete/insert/move
    // change the arrangement. Edits apply at render time and are made lossless on
    // save (QPDF copies the original page objects in this order).

    int rotation(int displayIndex) const;     // 0, 90, 180, or 270
    int originalPageAt(int displayIndex) const; // -1 if out of range
    const QVector<int>& pageOrder() const { return m_order; }
    const QVector<int>& rotations() const { return m_rot; }

    void rotatePage(int displayIndex, int deltaDegrees);
    // Removes a slot; reports its original page + rotation so it can be restored.
    void removePage(int displayIndex, int* originalOut = nullptr, int* rotationOut = nullptr);
    void insertPage(int displayIndex, int originalPage, int rotation);
    void movePage(int from, int to);

    bool isModified() const { return m_modified; }
    void markSaved(); // clears the modified flag (after a successful save)

signals:
    void loaded();
    void closed();
    void pageEdited(int displayIndex); // a slot's rotation changed
    void arrangementChanged();         // pages added/removed/reordered
    void modifiedChanged(bool modified);

private:
    void setModified(bool modified);

    QPdfDocument* m_pdf;
    QString m_filePath;
    QString m_password;   // the password the document opened with (may be empty)
    bool m_encrypted = false;
    QVector<int> m_order; // display slot → original page index
    QVector<int> m_rot;   // display slot → rotation (degrees clockwise)
    bool m_modified = false;
};
