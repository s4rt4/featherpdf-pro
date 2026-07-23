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

#include "backends/Xfdf.h"

#include "backends/FormFiller.h"

#include <QFile>
#include <QHash>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {
// The string an XFDF <value> should carry for a field's current state.
QString valueOf(const FormFiller::Field& f) {
    switch (f.kind) {
    case FormFiller::Kind::CheckBox:
    case FormFiller::Kind::Radio:
        return f.checked ? QStringLiteral("On") : QStringLiteral("Off");
    case FormFiller::Kind::ComboBox:
    case FormFiller::Kind::ListBox:
        return f.currentChoice;
    default:
        return f.textValue;
    }
}
} // namespace

bool Xfdf::exportFields(const QString& pdfPath, const QString& xfdfPath, QString* error) {
    const QList<FormFiller::Field> fields = FormFiller::read(pdfPath);

    QFile file(xfdfPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Couldn't open the XFDF file for writing.");
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("xfdf"));
    xml.writeDefaultNamespace(QStringLiteral("http://ns.adobe.com/xfdf/"));
    xml.writeStartElement(QStringLiteral("fields"));
    for (const FormFiller::Field& f : fields) {
        if (f.name.isEmpty())
            continue;
        xml.writeStartElement(QStringLiteral("field"));
        xml.writeAttribute(QStringLiteral("name"), f.name);
        xml.writeTextElement(QStringLiteral("value"), valueOf(f));
        xml.writeEndElement(); // field
    }
    xml.writeEndElement(); // fields
    xml.writeEndElement(); // xfdf
    xml.writeEndDocument();
    file.close();
    return true;
}

bool Xfdf::importFields(const QString& pdfPath, const QString& xfdfPath, const QString& outPath,
                        QString* error) {
    QFile file(xfdfPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Couldn't open the XFDF file.");
        return false;
    }

    // Parse <field name="…"><value>…</value></field> into name → value.
    QHash<QString, QString> byName;
    QXmlStreamReader xml(&file);
    QString currentName;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QLatin1String("field"))
                currentName = xml.attributes().value(QStringLiteral("name")).toString();
            else if (xml.name() == QLatin1String("value") && !currentName.isEmpty())
                byName.insert(currentName, xml.readElementText());
        } else if (xml.isEndElement() && xml.name() == QLatin1String("field")) {
            currentName.clear();
        }
    }
    if (xml.hasError()) {
        if (error)
            *error = QStringLiteral("This file isn't valid XFDF: %1").arg(xml.errorString());
        return false;
    }
    if (byName.isEmpty()) {
        if (error)
            *error = QStringLiteral("The XFDF file has no field values.");
        return false;
    }

    // Map names to the document's field ids and coerce values to the right type.
    const QList<FormFiller::Field> fields = FormFiller::read(pdfPath);
    QHash<int, QVariant> values;
    for (const FormFiller::Field& f : fields) {
        const auto it = byName.constFind(f.name);
        if (it == byName.constEnd())
            continue;
        const QString v = it.value();
        switch (f.kind) {
        case FormFiller::Kind::CheckBox:
        case FormFiller::Kind::Radio: {
            const QString low = v.toLower();
            values.insert(f.id, !(low.isEmpty() || low == "off" || low == "0" || low == "false" ||
                                  low == "no"));
            break;
        }
        default:
            values.insert(f.id, v);
            break;
        }
    }
    if (values.isEmpty()) {
        if (error)
            *error = QStringLiteral("None of the XFDF fields match this document.");
        return false;
    }
    return FormFiller::save(pdfPath, outPath, values, error);
}
