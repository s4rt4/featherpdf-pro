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

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// The batch / action engine. An "action" is an ordered list of steps; each step
// is one of the document operations (the same backends the app and CLI use),
// parameterized by a QVariantMap. runFile() pipes a document through the steps
// via temporary files, so several operations compose into one output. The GUI
// wizard iterates this over many files; the engine itself is headless.
class BatchRunner {
public:
    enum class Op {
        Optimize,     // dpi:int, unembedFonts:bool, compress:bool
        Watermark,    // text:str, opacity:double, fontSize:double, angle:double
        Bates,        // prefix:str, suffix:str, start:int, digits:int
        HeaderFooter, // headerLeft/Center/Right, footerLeft/Center/Right:str, startNumber:int
        Sanitize,     // metadata:bool, attachments:bool, scripts:bool
        Encrypt,      // password:str, allowPrint:bool, allowCopy:bool, allowEdit:bool
        Rotate,       // angle:int (90/180/270), applied to every page
        Flatten,      // raster:bool, dpi:int
        PdfA,         // (no params)
        Ocr,          // language:str, deskew/despeckle/binarize/autoLanguage:bool
    };

    struct Step {
        Op op;
        QVariantMap params;
    };

    // Apply `steps` to `inputPath` in order, writing the final result to
    // `outputPath`. Intermediate results go through a temp directory. With no
    // steps, the input is copied. Returns false and fills *error (prefixed with
    // the failing step's name) on the first failure.
    static bool runFile(const QString& inputPath, const QString& outputPath,
                        const QList<Step>& steps, QString* error);

    // Persist an action (its ordered steps) to a JSON file and read one back.
    // Format is stable and human-editable: {"feather-action":1,"steps":[{"op":
    // "<id>","params":{…}}]}. loadAction seeds each step from defaultParams and
    // overlays the saved values, so files survive new parameters being added.
    static bool saveAction(const QString& path, const QList<Step>& steps, QString* error);
    static bool loadAction(const QString& path, QList<Step>* steps, QString* error);

    // Map a stable op id (see opId) back to its Op. Returns false for unknown ids.
    static bool opFromId(const QString& id, Op* op);

    // Catalog helpers for the UI: every op's stable id, its human label, and the
    // default parameters to seed a new step with.
    static QString opId(Op op);
    static QString opLabel(Op op);
    static QVariantMap defaultParams(Op op);
    static QList<Op> allOps();
    // A short, human-readable summary of a configured step ("Watermark “DRAFT”").
    static QString summarize(const Step& step);
};
