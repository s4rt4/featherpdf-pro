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

#include "backends/FormEditor.h"

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <exception>
#include <set>
#include <vector>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

namespace {
// Field flag bits (PDF 32000-1, table 226/227/230), 1-based in the spec.
constexpr int kFfPushButton = 1 << 16; // /Btn: pushbutton (no value)
constexpr int kFfCombo = 1 << 17;      // /Ch: dropdown rather than list box

// The page's MediaBox as [x0,y0,x1,y1], normalized so x0<x1, y0<y1.
bool mediaBox(QPDFObjectHandle page, double out[4]) {
    QPDFObjectHandle b = page.getKey("/MediaBox");
    if (!b.isArray() || b.getArrayNItems() != 4)
        return false;
    double v[4];
    for (int i = 0; i < 4; ++i) {
        QPDFObjectHandle e = b.getArrayItem(i);
        if (!e.isNumber())
            return false;
        v[i] = e.getNumericValue();
    }
    out[0] = std::min(v[0], v[2]);
    out[1] = std::min(v[1], v[3]);
    out[2] = std::max(v[0], v[2]);
    out[3] = std::max(v[1], v[3]);
    return true;
}

// Baked page rotation (/Rotate), normalized to 0/90/180/270.
int pageRotate(QPDFObjectHandle page) {
    QPDFObjectHandle r = page.getKey("/Rotate");
    const int v = r.isInteger() ? r.getIntValueAsInt() : 0;
    return ((v % 360) + 360) % 360;
}

// Map a rect given in displayed (top-left origin) page fractions to the page's
// unrotated fractions, for a clockwise display rotation `rot`.
QRectF toUnrotatedFraction(const QRectF& r, int rot) {
    const auto m = [&](double u, double v) -> QPointF {
        switch (rot) {
        case 90:  return {v, 1.0 - u};
        case 180: return {1.0 - u, 1.0 - v};
        case 270: return {1.0 - v, u};
        default:  return {u, v};
        }
    };
    const QPointF a = m(r.left(), r.top());
    const QPointF b = m(r.right(), r.bottom());
    return QRectF(QPointF(std::min(a.x(), b.x()), std::min(a.y(), b.y())),
                  QPointF(std::max(a.x(), b.x()), std::max(a.y(), b.y())));
}

// Map a normalized (top-left origin) rect to a PDF /Rect array on a page whose
// MediaBox is `mb`; optionally report the rect's width/height in points.
QPDFObjectHandle rectToPdf(const double mb[4], const QRectF& r, double* wOut, double* hOut) {
    const double W = mb[2] - mb[0], H = mb[3] - mb[1];
    const double x0 = mb[0] + r.left() * W, x1 = mb[0] + r.right() * W;
    const double yTop = mb[3] - r.top() * H, yBot = mb[3] - r.bottom() * H;
    const double loX = std::min(x0, x1), hiX = std::max(x0, x1);
    const double loY = std::min(yTop, yBot), hiY = std::max(yTop, yBot);
    if (wOut)
        *wOut = hiX - loX;
    if (hOut)
        *hOut = hiY - loY;
    QPDFObjectHandle a = QPDFObjectHandle::newArray();
    a.appendItem(QPDFObjectHandle::newReal(loX, 2));
    a.appendItem(QPDFObjectHandle::newReal(loY, 2));
    a.appendItem(QPDFObjectHandle::newReal(hiX, 2));
    a.appendItem(QPDFObjectHandle::newReal(hiY, 2));
    return a;
}

std::string num(double v) {
    return QByteArray::number(v, 'f', 2).toStdString();
}

// Escape a string for a PDF literal-string operand in a content stream.
std::string litStr(const QString& s) {
    std::string o = "(";
    const QByteArray latin1 = s.toLatin1();
    for (char c : latin1) {
        if (c == '(' || c == ')' || c == '\\')
            o += '\\';
        o += c;
    }
    o += ")";
    return o;
}

// A 0.5pt grey border just inside the [0 0 w h] box.
std::string borderOps(double w, double h) {
    return "0.55 0.55 0.55 RG 0.5 w 0.5 0.5 " + num(w - 1) + " " + num(h - 1) + " re S\n";
}

// A Form XObject sized [0 0 w h] with `ops` as its content. When `withFont`, a
// Helvetica resource is attached so the ops may draw text with /Helv.
QPDFObjectHandle makeAppearance(QPDF& qpdf, double w, double h, const std::string& ops,
                                bool withFont) {
    QPDFObjectHandle xobj = QPDFObjectHandle::newStream(&qpdf, ops);
    QPDFObjectHandle d = xobj.getDict();
    d.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    d.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
    QPDFObjectHandle res = QPDFObjectHandle::newDictionary();
    if (withFont) {
        QPDFObjectHandle helv = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        helv.replaceKey("/Type", QPDFObjectHandle::newName("/Font"));
        helv.replaceKey("/Subtype", QPDFObjectHandle::newName("/Type1"));
        helv.replaceKey("/BaseFont", QPDFObjectHandle::newName("/Helvetica"));
        QPDFObjectHandle fonts = QPDFObjectHandle::newDictionary();
        fonts.replaceKey("/Helv", helv);
        res.replaceKey("/Font", fonts);
    }
    d.replaceKey("/Resources", res);
    QPDFObjectHandle bbox = QPDFObjectHandle::newArray();
    bbox.appendItem(QPDFObjectHandle::newInteger(0));
    bbox.appendItem(QPDFObjectHandle::newInteger(0));
    bbox.appendItem(QPDFObjectHandle::newReal(w, 2));
    bbox.appendItem(QPDFObjectHandle::newReal(h, 2));
    d.replaceKey("/BBox", bbox);
    return xobj;
}

// Border plus a single line of left-aligned Helvetica text (used by Text and
// Dropdown). Empty text yields just the border.
QPDFObjectHandle textAppearance(QPDF& qpdf, double w, double h, const QString& text) {
    std::string ops = borderOps(w, h);
    if (!text.isEmpty()) {
        const double size = std::max(6.0, std::min(12.0, h - 4));
        const double baseline = (h - size) / 2 + size * 0.25;
        ops += "0 g BT /Helv " + num(size) + " Tf 3 " + num(baseline) + " Td " + litStr(text) +
               " Tj ET\n";
    }
    return makeAppearance(qpdf, w, h, ops, /*withFont=*/true);
}

// The /AP /N "on" mark for a check box: border + a bold X.
QPDFObjectHandle checkOnAppearance(QPDF& qpdf, double w, double h) {
    const double m = std::min(w, h) * 0.22;
    std::string ops = borderOps(w, h) + "0 g " + num(std::min(w, h) * 0.12) + " w " + num(m) + " " +
                      num(m) + " m " + num(w - m) + " " + num(h - m) + " l " + num(m) + " " +
                      num(h - m) + " m " + num(w - m) + " " + num(m) + " l S\n";
    return makeAppearance(qpdf, w, h, ops, /*withFont=*/false);
}

// The /AP /N "on" mark for a radio button: border + a filled centre dot.
QPDFObjectHandle radioOnAppearance(QPDF& qpdf, double w, double h) {
    const double r = std::min(w, h) * 0.22;
    const double cx = w / 2, cy = h / 2;
    std::string ops = borderOps(w, h) + "0 g " + num(cx - r) + " " + num(cy - r) + " " + num(2 * r) +
                      " " + num(2 * r) + " re f\n";
    return makeAppearance(qpdf, w, h, ops, /*withFont=*/false);
}

// A push button: light fill, border, and a left-aligned caption.
QPDFObjectHandle buttonAppearance(QPDF& qpdf, double w, double h, const QString& caption) {
    const double size = std::max(7.0, std::min(12.0, h - 6));
    const double baseline = (h - size) / 2 + size * 0.25;
    std::string ops = "0.92 0.92 0.92 rg 0 0 " + num(w) + " " + num(h) + " re f\n" +
                      borderOps(w, h);
    if (!caption.isEmpty())
        ops += "0 g BT /Helv " + num(size) + " Tf 6 " + num(baseline) + " Td " + litStr(caption) +
               " Tj ET\n";
    return makeAppearance(qpdf, w, h, ops, /*withFont=*/true);
}

// A PDF-name-safe export token for a radio option (unique within `used`).
std::string exportToken(const QString& option, int index, QStringList& used) {
    QString t;
    for (QChar c : option)
        if (c.isLetterOrNumber() || c == '_')
            t.append(c);
    if (t.isEmpty() || used.contains(t))
        t = QStringLiteral("Choice_%1").arg(index);
    while (used.contains(t))
        t += QStringLiteral("_%1").arg(index);
    used << t;
    return ("/" + t).toStdString();
}

// Index of the top-level field named `name` in a /Fields array, or -1.
int findFieldIndex(QPDFObjectHandle fields, const QString& name) {
    for (int i = 0; i < fields.getArrayNItems(); ++i) {
        QPDFObjectHandle t = fields.getArrayItem(i).getKey("/T");
        if (t.isString() && QString::fromStdString(t.getUTF8Value()) == name)
            return i;
    }
    return -1;
}

// (Re)generate the /AP for a single-widget field node from its type and value.
// Radio kids are handled separately. No-op for unknown types.
void applyAppearance(QPDF& qpdf, QPDFObjectHandle node, double w, double h) {
    QPDFObjectHandle ft = node.getKey("/FT");
    const std::string t = ft.isName() ? ft.getName() : std::string();
    const auto utf8 = [](QPDFObjectHandle o) {
        return o.isString() ? QString::fromStdString(o.getUTF8Value()) : QString();
    };
    if (t == "/Tx" || t == "/Ch") {
        QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
        ap.replaceKey("/N", textAppearance(qpdf, w, h, utf8(node.getKey("/V"))));
        node.replaceKey("/AP", ap);
    } else if (t == "/Btn") {
        const int ff = node.getKey("/Ff").isInteger() ? node.getKey("/Ff").getIntValueAsInt() : 0;
        if (ff & kFfPushButton) {
            QString cap;
            QPDFObjectHandle mk = node.getKey("/MK");
            if (mk.isDictionary())
                cap = utf8(mk.getKey("/CA"));
            QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
            ap.replaceKey("/N", buttonAppearance(qpdf, w, h, cap));
            node.replaceKey("/AP", ap);
        } else {
            QPDFObjectHandle apN = QPDFObjectHandle::newDictionary();
            apN.replaceKey("/Yes", checkOnAppearance(qpdf, w, h));
            apN.replaceKey("/Off", makeAppearance(qpdf, w, h, borderOps(w, h), false));
            QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
            ap.replaceKey("/N", apN);
            node.replaceKey("/AP", ap);
        }
    }
}

// Ensure the catalog has an /AcroForm with a Helvetica default resource and a
// default appearance, and return it (and its /Fields array via the out-param).
QPDFObjectHandle ensureAcroForm(QPDF& qpdf, QPDFObjectHandle& fieldsOut) {
    QPDFObjectHandle root = qpdf.getRoot();
    QPDFObjectHandle acro = root.getKey("/AcroForm");
    if (!acro.isDictionary()) {
        acro = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        root.replaceKey("/AcroForm", acro);
    }
    QPDFObjectHandle fields = acro.getKey("/Fields");
    if (!fields.isArray()) {
        fields = QPDFObjectHandle::newArray();
        acro.replaceKey("/Fields", fields);
    }
    // Regenerate appearances on open/save (so the new field is actually drawn).
    acro.replaceKey("/NeedAppearances", QPDFObjectHandle::newBool(true));
    if (!acro.getKey("/DA").isString())
        acro.replaceKey("/DA", QPDFObjectHandle::newString("/Helv 0 Tf 0 g"));

    QPDFObjectHandle dr = acro.getKey("/DR");
    if (!dr.isDictionary()) {
        dr = QPDFObjectHandle::newDictionary();
        acro.replaceKey("/DR", dr);
    }
    QPDFObjectHandle fonts = dr.getKey("/Font");
    if (!fonts.isDictionary()) {
        fonts = QPDFObjectHandle::newDictionary();
        dr.replaceKey("/Font", fonts);
    }
    if (!fonts.getKey("/Helv").isDictionary()) {
        QPDFObjectHandle helv = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        helv.replaceKey("/Type", QPDFObjectHandle::newName("/Font"));
        helv.replaceKey("/Subtype", QPDFObjectHandle::newName("/Type1"));
        helv.replaceKey("/BaseFont", QPDFObjectHandle::newName("/Helvetica"));
        fonts.replaceKey("/Helv", helv);
    }
    fieldsOut = fields;
    return acro;
}

// A /JavaScript action dict carrying `js` as its /JS program.
QPDFObjectHandle jsAction(const std::string& js) {
    QPDFObjectHandle a = QPDFObjectHandle::newDictionary();
    a.replaceKey("/S", QPDFObjectHandle::newName("/JavaScript"));
    a.replaceKey("/JS", QPDFObjectHandle::newString(js)); // QPDF escapes on write
    return a;
}

// Build the /AA (additional-actions) dict for a Text field's input format, using
// the standard Acrobat AF* field-format helpers (/F format + /K keystroke).
// Returns false for Format::None (no actions).
bool formatActions(FormEditor::Format fmt, QPDFObjectHandle& aaOut) {
    std::string f, k;
    switch (fmt) {
    case FormEditor::Format::None:
        return false;
    case FormEditor::Format::Number:
        f = "AFNumber_Format(2, 0, 0, 0, \"\", true)";
        k = "AFNumber_Keystroke(2, 0, 0, 0, \"\", true)";
        break;
    case FormEditor::Format::Currency:
        f = "AFNumber_Format(2, 0, 0, 0, \"$\", true)";
        k = "AFNumber_Keystroke(2, 0, 0, 0, \"$\", true)";
        break;
    case FormEditor::Format::Percent:
        f = "AFPercent_Format(2, 0)";
        k = "AFPercent_Keystroke(2, 0)";
        break;
    case FormEditor::Format::Date:
        f = "AFDate_FormatEx(\"mm/dd/yyyy\")";
        k = "AFDate_KeystrokeEx(\"mm/dd/yyyy\")";
        break;
    case FormEditor::Format::Time:
        f = "AFTime_Format(0)";
        k = "AFTime_Keystroke(0)";
        break;
    }
    QPDFObjectHandle aa = QPDFObjectHandle::newDictionary();
    aa.replaceKey("/F", jsAction(f));
    aa.replaceKey("/K", jsAction(k));
    aaOut = aa;
    return true;
}

// Create one field/widget node from `field`, attach it to its page's /Annots and
// to the AcroForm `fields` array. Radio is rejected (use addRadioGroup). Shared
// by addField (single) and addFields (batch). Fills *error and returns false on
// a bad field; an empty name is the caller's responsibility to pre-check.
bool placeOneField(QPDF& qpdf, std::vector<QPDFPageObjectHelper>& pages,
                   const FormEditor::NewField& field, QPDFObjectHandle fields, QString* error) {
    using Type = FormEditor::Type;
    const int n = static_cast<int>(pages.size());
    const int pg = std::max(0, std::min(field.page, n - 1));
    QPDFObjectHandle pageObj = pages[pg].getObjectHandle();

    double mb[4];
    if (!mediaBox(pageObj, mb)) {
        if (error)
            *error = QStringLiteral("The target page has no media box.");
        return false;
    }
    const int rot = pageRotate(pageObj);
    const QRectF ur = toUnrotatedFraction(field.rect, rot);
    double w = 0, h = 0;
    QPDFObjectHandle rect = rectToPdf(mb, ur, &w, &h);

    QPDFObjectHandle node = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
    node.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
    node.replaceKey("/Subtype", QPDFObjectHandle::newName("/Widget"));
    node.replaceKey("/T", QPDFObjectHandle::newUnicodeString(field.name.toStdString()));
    node.replaceKey("/P", pageObj);
    node.replaceKey("/F", QPDFObjectHandle::newInteger(4)); // Print
    node.replaceKey("/Rect", rect);

    const auto singleAppearance = [&](QPDFObjectHandle xobj) {
        QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
        ap.replaceKey("/N", xobj);
        node.replaceKey("/AP", ap);
    };

    switch (field.type) {
    case Type::Text:
        node.replaceKey("/FT", QPDFObjectHandle::newName("/Tx"));
        node.replaceKey("/DA", QPDFObjectHandle::newString("/Helv 0 Tf 0 g"));
        if (!field.defaultValue.isEmpty()) {
            node.replaceKey("/V",
                            QPDFObjectHandle::newUnicodeString(field.defaultValue.toStdString()));
            node.replaceKey("/DV",
                            QPDFObjectHandle::newUnicodeString(field.defaultValue.toStdString()));
        }
        singleAppearance(textAppearance(qpdf, w, h, field.defaultValue));
        break;
    case Type::CheckBox: {
        node.replaceKey("/FT", QPDFObjectHandle::newName("/Btn"));
        const char* state = field.checked ? "/Yes" : "/Off";
        node.replaceKey("/V", QPDFObjectHandle::newName(state));
        node.replaceKey("/AS", QPDFObjectHandle::newName(state));
        QPDFObjectHandle apN = QPDFObjectHandle::newDictionary();
        apN.replaceKey("/Yes", checkOnAppearance(qpdf, w, h));
        apN.replaceKey("/Off", makeAppearance(qpdf, w, h, borderOps(w, h), false));
        QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
        ap.replaceKey("/N", apN);
        node.replaceKey("/AP", ap);
        break;
    }
    case Type::Dropdown: {
        node.replaceKey("/FT", QPDFObjectHandle::newName("/Ch"));
        node.replaceKey("/Ff", QPDFObjectHandle::newInteger(kFfCombo));
        node.replaceKey("/DA", QPDFObjectHandle::newString("/Helv 0 Tf 0 g"));
        QPDFObjectHandle opt = QPDFObjectHandle::newArray();
        for (const QString& o : field.options)
            opt.appendItem(QPDFObjectHandle::newUnicodeString(o.toStdString()));
        node.replaceKey("/Opt", opt);
        const QString def = !field.defaultValue.isEmpty()
                                ? field.defaultValue
                                : (!field.options.isEmpty() ? field.options.first() : QString());
        if (!def.isEmpty())
            node.replaceKey("/V", QPDFObjectHandle::newUnicodeString(def.toStdString()));
        singleAppearance(textAppearance(qpdf, w, h, def));
        break;
    }
    case Type::PushButton: {
        node.replaceKey("/FT", QPDFObjectHandle::newName("/Btn"));
        node.replaceKey("/Ff", QPDFObjectHandle::newInteger(kFfPushButton));
        const QString caption = !field.defaultValue.isEmpty() ? field.defaultValue : field.name;
        QPDFObjectHandle mk = QPDFObjectHandle::newDictionary();
        mk.replaceKey("/CA", QPDFObjectHandle::newString(caption.toStdString()));
        node.replaceKey("/MK", mk);
        singleAppearance(buttonAppearance(qpdf, w, h, caption));
        break;
    }
    case Type::Radio:
        if (error)
            *error = QStringLiteral("Use addRadioGroup for a radio set.");
        return false;
    }

    // Input format / validation actions (Text and Dropdown carry a typed value).
    if (field.type == Type::Text || field.type == Type::Dropdown) {
        QPDFObjectHandle aa;
        if (formatActions(field.format, aa))
            node.replaceKey("/AA", aa);
    }

    QPDFObjectHandle annots = pageObj.getKey("/Annots");
    if (!annots.isArray()) {
        annots = QPDFObjectHandle::newArray();
        pageObj.replaceKey("/Annots", annots);
    }
    annots.appendItem(node);
    fields.appendItem(node);
    return true;
}
} // namespace

bool FormEditor::addField(const QString& inputPath, const QString& outputPath,
                          const NewField& field, QString* error) {
    if (field.name.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("Give the field a name.");
        return false;
    }

    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFPageDocumentHelper dh(qpdf);
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        const int n = static_cast<int>(pages.size());
        if (n == 0) {
            if (error)
                *error = QStringLiteral("The document has no pages.");
            if (inPlace)
                QFile::remove(target);
            return false;
        }
        QPDFObjectHandle fields;
        ensureAcroForm(qpdf, fields);
        if (!placeOneField(qpdf, pages, field, fields, error)) {
            if (inPlace)
                QFile::remove(target);
            return false;
        }

        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        writer.write();
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        if (inPlace)
            QFile::remove(target);
        return false;
    }

    if (inPlace) {
        QFile::remove(outputPath);
        if (!QFile::rename(target, outputPath)) {
            if (error)
                *error = QStringLiteral("The file with the new field couldn't replace the "
                                        "original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

bool FormEditor::addFields(const QString& inputPath, const QString& outputPath,
                           const QList<NewField>& newFields, int* count, QString* error) {
    if (count)
        *count = 0;
    if (newFields.isEmpty()) {
        if (error)
            *error = QStringLiteral("No fields to add.");
        return false;
    }

    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    int written = 0;
    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFPageDocumentHelper dh(qpdf);
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        if (pages.empty()) {
            if (error)
                *error = QStringLiteral("The document has no pages.");
            if (inPlace)
                QFile::remove(target);
            return false;
        }

        QPDFObjectHandle fields;
        ensureAcroForm(qpdf, fields);
        for (const NewField& f : newFields) {
            if (f.name.trimmed().isEmpty() || f.type == Type::Radio)
                continue; // skip unnamed and radio (needs addRadioGroup)
            QString stepError;
            if (placeOneField(qpdf, pages, f, fields, &stepError))
                ++written;
        }
        if (written == 0) {
            if (error)
                *error = QStringLiteral("None of the fields could be added.");
            if (inPlace)
                QFile::remove(target);
            return false;
        }

        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        writer.write();
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        if (inPlace)
            QFile::remove(target);
        return false;
    }

    if (inPlace) {
        QFile::remove(outputPath);
        if (!QFile::rename(target, outputPath)) {
            if (error)
                *error = QStringLiteral("The file with the new fields couldn't replace the "
                                        "original.");
            QFile::remove(target);
            return false;
        }
    }
    if (count)
        *count = written;
    return true;
}

bool FormEditor::addRadioGroup(const QString& inputPath, const QString& outputPath,
                               const QString& name, const QStringList& options, int page,
                               const QRectF& firstRect, QString* error) {
    if (name.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("Give the field a name.");
        return false;
    }
    if (options.size() < 2) {
        if (error)
            *error = QStringLiteral("A radio group needs at least two options.");
        return false;
    }

    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFPageDocumentHelper dh(qpdf);
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        const int n = static_cast<int>(pages.size());
        if (n == 0) {
            if (error)
                *error = QStringLiteral("The document has no pages.");
            if (inPlace)
                QFile::remove(target);
            return false;
        }
        const int pg = std::max(0, std::min(page, n - 1));
        QPDFObjectHandle pageObj = pages[pg].getObjectHandle();
        double mb[4];
        if (!mediaBox(pageObj, mb)) {
            if (error)
                *error = QStringLiteral("The target page has no media box.");
            if (inPlace)
                QFile::remove(target);
            return false;
        }

        // Parent /Btn field carrying the radio flag (bit 16) and no exported value.
        QPDFObjectHandle parent = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        parent.replaceKey("/FT", QPDFObjectHandle::newName("/Btn"));
        parent.replaceKey("/Ff", QPDFObjectHandle::newInteger(1 << 15)); // Radio
        parent.replaceKey("/T", QPDFObjectHandle::newUnicodeString(name.toStdString()));
        parent.replaceKey("/V", QPDFObjectHandle::newName("/Off"));
        QPDFObjectHandle kids = QPDFObjectHandle::newArray();
        parent.replaceKey("/Kids", kids);

        QPDFObjectHandle annots = pageObj.getKey("/Annots");
        if (!annots.isArray()) {
            annots = QPDFObjectHandle::newArray();
            pageObj.replaceKey("/Annots", annots);
        }

        const int rot = pageRotate(pageObj);
        const double step = firstRect.height() * 1.8; // vertical gap between buttons
        QStringList usedTokens;
        for (int i = 0; i < options.size(); ++i) {
            QRectF r = firstRect;
            r.moveTop(std::min(firstRect.top() + i * step, 1.0 - firstRect.height()));
            double w = 0, h = 0;
            QPDFObjectHandle rect = rectToPdf(mb, toUnrotatedFraction(r, rot), &w, &h);
            const std::string on = exportToken(options[i], i, usedTokens);

            QPDFObjectHandle kid = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
            kid.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
            kid.replaceKey("/Subtype", QPDFObjectHandle::newName("/Widget"));
            kid.replaceKey("/Parent", parent);
            kid.replaceKey("/P", pageObj);
            kid.replaceKey("/F", QPDFObjectHandle::newInteger(4)); // Print
            kid.replaceKey("/Rect", rect);
            kid.replaceKey("/AS", QPDFObjectHandle::newName("/Off"));

            // /AP /N defines the allowed states (the export value + /Off), each
            // with a visible mark.
            QPDFObjectHandle apN = QPDFObjectHandle::newDictionary();
            apN.replaceKey(on, radioOnAppearance(qpdf, w, h));
            apN.replaceKey("/Off", makeAppearance(qpdf, w, h, borderOps(w, h), false));
            QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
            ap.replaceKey("/N", apN);
            kid.replaceKey("/AP", ap);

            kids.appendItem(kid);
            annots.appendItem(kid);
        }

        QPDFObjectHandle fields;
        ensureAcroForm(qpdf, fields);
        fields.appendItem(parent);

        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        writer.write();
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        if (inPlace)
            QFile::remove(target);
        return false;
    }

    if (inPlace) {
        QFile::remove(outputPath);
        if (!QFile::rename(target, outputPath)) {
            if (error)
                *error = QStringLiteral("The file with the new field couldn't replace the "
                                        "original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

bool FormEditor::deleteField(const QString& inputPath, const QString& outputPath,
                             const QString& name, QString* error) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFObjectHandle acro = qpdf.getRoot().getKey("/AcroForm");
        QPDFObjectHandle fields =
            acro.isDictionary() ? acro.getKey("/Fields") : QPDFObjectHandle::newNull();
        const int idx = fields.isArray() ? findFieldIndex(fields, name) : -1;
        if (idx < 0) {
            if (error)
                *error = QStringLiteral("That field is no longer in the document.");
            if (inPlace)
                QFile::remove(target);
            return false;
        }

        // The widget annotations to unlink: the field itself, or each radio kid.
        QPDFObjectHandle f = fields.getArrayItem(idx);
        std::set<QPDFObjGen> widgets;
        QPDFObjectHandle kids = f.getKey("/Kids");
        if (kids.isArray())
            for (int i = 0; i < kids.getArrayNItems(); ++i)
                widgets.insert(kids.getArrayItem(i).getObjGen());
        else
            widgets.insert(f.getObjGen());

        fields.eraseItem(idx);

        QPDFPageDocumentHelper dh(qpdf);
        for (QPDFPageObjectHelper& page : dh.getAllPages()) {
            QPDFObjectHandle annots = page.getObjectHandle().getKey("/Annots");
            if (!annots.isArray())
                continue;
            for (int j = annots.getArrayNItems() - 1; j >= 0; --j)
                if (widgets.count(annots.getArrayItem(j).getObjGen()))
                    annots.eraseItem(j);
        }

        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        writer.write();
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        if (inPlace)
            QFile::remove(target);
        return false;
    }

    if (inPlace) {
        QFile::remove(outputPath);
        if (!QFile::rename(target, outputPath)) {
            if (error)
                *error = QStringLiteral("The edited file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

bool FormEditor::setFieldRect(const QString& inputPath, const QString& outputPath,
                              const QString& name, const QRectF& normRect, QString* error) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFObjectHandle acro = qpdf.getRoot().getKey("/AcroForm");
        QPDFObjectHandle fields =
            acro.isDictionary() ? acro.getKey("/Fields") : QPDFObjectHandle::newNull();
        const int idx = fields.isArray() ? findFieldIndex(fields, name) : -1;
        if (idx < 0) {
            if (error)
                *error = QStringLiteral("That field is no longer in the document.");
            if (inPlace)
                QFile::remove(target);
            return false;
        }
        QPDFObjectHandle node = fields.getArrayItem(idx);
        if (node.getKey("/Kids").isArray()) {
            if (error)
                *error = QStringLiteral("A radio group can't be moved as one field yet.");
            if (inPlace)
                QFile::remove(target);
            return false;
        }

        // Locate the field's page: /P if present, else the page that lists it.
        QPDFObjectHandle pageObj = node.getKey("/P");
        QPDFPageDocumentHelper dh(qpdf);
        if (!pageObj.isDictionary()) {
            for (QPDFPageObjectHelper& page : dh.getAllPages()) {
                QPDFObjectHandle annots = page.getObjectHandle().getKey("/Annots");
                if (!annots.isArray())
                    continue;
                for (int j = 0; j < annots.getArrayNItems(); ++j)
                    if (annots.getArrayItem(j).getObjGen() == node.getObjGen()) {
                        pageObj = page.getObjectHandle();
                        break;
                    }
                if (pageObj.isDictionary())
                    break;
            }
        }
        double mb[4];
        if (!pageObj.isDictionary() || !mediaBox(pageObj, mb)) {
            if (error)
                *error = QStringLiteral("Couldn't find the field's page.");
            if (inPlace)
                QFile::remove(target);
            return false;
        }

        double w = 0, h = 0;
        QPDFObjectHandle rect =
            rectToPdf(mb, toUnrotatedFraction(normRect, pageRotate(pageObj)), &w, &h);
        node.replaceKey("/Rect", rect);
        applyAppearance(qpdf, node, w, h);

        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        writer.write();
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        if (inPlace)
            QFile::remove(target);
        return false;
    }

    if (inPlace) {
        QFile::remove(outputPath);
        if (!QFile::rename(target, outputPath)) {
            if (error)
                *error = QStringLiteral("The edited file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}
