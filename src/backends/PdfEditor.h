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

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

// The lossless structure backend (QPDF). It copies the original page objects
// rather than re-rendering, so saved files keep their fidelity. M1 starts with
// applying per-page rotation; deletes and reordering join the same writer.
class PdfEditor {
public:
    // Write `inputPath` to `outputPath` as the arrangement described by `order`
    // (display slot → original 0-based page index) and `rotations` (per slot,
    // degrees clockwise relative to the page's original orientation). Pages not
    // present in `order` are dropped; repeats/reordering are honoured. Writing
    // over the input is handled safely (temp file + atomic rename). Returns true
    // on success; on failure fills *error with a friendly message.
    static bool saveArrangement(const QString& inputPath, const QString& outputPath,
                                const QVector<int>& order, const QVector<int>& rotations,
                                QString* error);

    // One bookmark in the document outline: a title, the 0-based page it jumps
    // to, and any nested children (sub-bookmarks).
    struct OutlineItem {
        QString title;
        int page = 0;
        QVector<OutlineItem> children;
    };

    // Replace the document's outline (/Outlines) with `items`, then write to
    // `outputPath`. An empty list removes the outline entirely. Destinations use
    // /Fit (whole page). Lossless apart from the outline; page content is copied
    // untouched. Temp file + atomic rename, so `outputPath` may equal `inputPath`.
    // Returns true on success; on failure fills *error with a friendly message.
    static bool setOutline(const QString& inputPath, const QString& outputPath,
                           const QVector<OutlineItem>& items, QString* error);

    // Margins to trim off each edge of a page, in points, expressed in the
    // page's *displayed* orientation (what the user sees, after rotation).
    struct CropMargins {
        double left = 0;
        double right = 0;
        double top = 0;
        double bottom = 0;
    };

    // Write the base document's current arrangement (`order`/`rotations`, as
    // saveArrangement) with a tighter /CropBox on the chosen pages. `applyToSlots`
    // lists the display slots to crop (empty = every slot). `margins` are insets
    // from each displayed edge; they are mapped into the page's unrotated space
    // (honouring both the page's own /Rotate and the per-slot rotation) and
    // applied relative to each page's current crop/media box. The visible content
    // is clipped, not removed (lossless — the full page data stays). Temp file +
    // atomic rename, so `outputPath` may equal `inputPath`. Returns true on
    // success; on failure fills *error with a friendly message.
    static bool cropPages(const QString& inputPath, const QString& outputPath,
                          const QVector<int>& order, const QVector<int>& rotations,
                          const QVector<int>& applyToSlots, const CropMargins& margins,
                          QString* error);

    // Build a new document at `outputPath` from the base document's current
    // arrangement (`order`/`rotations`, exactly as saveArrangement consumes them)
    // with pages from `insertPath` spliced in. `insertPages` lists the 0-based
    // pages of `insertPath` to take, in that order; they are inserted at display
    // slot `atSlot` (clamped to 0..order.size()). Base pages are copied losslessly
    // with their per-slot rotation; inserted pages keep their own orientation.
    // Temp file + atomic rename, so `outputPath` may equal `inputPath`. Returns
    // true on success; on failure fills *error with a friendly message.
    static bool insertPages(const QString& inputPath, const QString& outputPath,
                            const QVector<int>& order, const QVector<int>& rotations,
                            const QString& insertPath, const QVector<int>& insertPages, int atSlot,
                            QString* error);

    // Merge `inputPaths` into a single PDF at `outputPath`, concatenating all
    // pages of each input in list order. Pages are copied losslessly (foreign
    // object copy, no re-render). The write goes through a temp file then an
    // atomic rename, so `outputPath` may safely equal one of the inputs. Returns
    // true on success; on failure fills *error with a friendly message.
    static bool combine(const QStringList& inputPaths, const QString& outputPath, QString* error);

    // What a recipient who opens the document (with the open password, or no
    // password) is allowed to do. Restricting any of these is only enforced via
    // a separate owner password, which protect() generates automatically.
    struct Permissions {
        bool allowPrinting = true;
        bool allowCopying = true; // extract text/graphics
        bool allowEditing = true; // modify, assemble, annotate, fill forms
    };

    // Write `inputPath` to `outputPath` encrypted with AES-256 (the PDF 2.0 R6
    // scheme). `openPassword` (may be empty) is required to open the document;
    // `perms` restrict what the opener may do. Lossless - the page content is
    // copied, only an encryption layer is added. Temp file + atomic rename, so
    // `outputPath` may equal `inputPath`. Returns true on success; on failure
    // fills *error with a friendly message.
    static bool protect(const QString& inputPath, const QString& outputPath,
                        const QString& openPassword, const Permissions& perms, QString* error);

    // Rewrite `inputPath` to `outputPath` more compactly: pack objects into
    // object streams, (re)compress streams. Lossless - content is unchanged.
    // *beforeBytes/*afterBytes (optional) report the file sizes. Temp file +
    // atomic rename. Returns true on success; on failure fills *error.
    static bool optimize(const QString& inputPath, const QString& outputPath, qint64* beforeBytes,
                         qint64* afterBytes, QString* error);

    // True if `path` is an encrypted PDF that needs a user password to open.
    // (QtPdf reports AES-256 files as an "unsupported scheme" until the right
    // password is set, so the viewer asks QPDF whether to prompt for one.)
    static bool isPasswordProtected(const QString& path);

    // Write `inputPath` (opened with `password`) to `outputPath` with its
    // encryption removed. Lossless. Temp file + atomic rename, so `outputPath`
    // may equal `inputPath`. Returns true on success; fills *error otherwise.
    static bool removeProtection(const QString& inputPath, const QString& outputPath,
                                 const QString& password, QString* error);

    // A page that has been flattened to a redacted image. `jpeg` is JPEG-encoded
    // RGB (the page rendered with its redaction boxes painted opaque), `px*` its
    // pixel size, `pt*` the page box in points it should fill.
    struct RedactedImage {
        QByteArray jpeg;
        int pxWidth = 0;
        int pxHeight = 0;
        double ptWidth = 0;
        double ptHeight = 0;
    };

    // Save the arrangement (`order`/`rotations`, as saveArrangement) but replace
    // the display slots present in `redactedBySlot` with image-only pages built
    // from the supplied flattened renders - so the redacted content is physically
    // gone, not merely covered. Non-redacted pages are copied losslessly. Temp
    // file + atomic rename. Returns true on success; fills *error otherwise.
    static bool saveRedacted(const QString& inputPath, const QString& outputPath,
                             const QVector<int>& order, const QVector<int>& rotations,
                             const QHash<int, RedactedImage>& redactedBySlot, QString* error);
};
