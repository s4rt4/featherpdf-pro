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

#include "backends/PdfIncrement.h"

#include <QSet>

#include <algorithm>
#include <cstdio>

namespace PdfIncrement {

QByteArray makeObj(int id, int gen, const QByteArray& body) {
    QByteArray o = QByteArray::number(id) + ' ' + QByteArray::number(gen) + " obj\n";
    o += body;
    o += "\nendobj\n";
    return o;
}

QByteArray makeStreamObj(int id, const QByteArray& data, const QByteArray& extraDictEntries) {
    QByteArray body = "<< /Length " + QByteArray::number(data.size());
    if (!extraDictEntries.isEmpty())
        body += ' ' + extraDictEntries;
    body += " >>\nstream\n";
    body += data;
    body += "\nendstream";
    return makeObj(id, 0, body);
}

qint64 lastStartxref(const QByteArray& bytes) {
    const int sx = bytes.lastIndexOf("startxref");
    if (sx < 0)
        return -1;
    int i = sx + 9;
    while (i < bytes.size() && (bytes[i] == '\r' || bytes[i] == '\n' || bytes[i] == ' '))
        ++i;
    qint64 off = 0;
    bool any = false;
    while (i < bytes.size() && bytes[i] >= '0' && bytes[i] <= '9') {
        off = off * 10 + (bytes[i] - '0');
        any = true;
        ++i;
    }
    return any ? off : -1;
}

QByteArray writeIncrementalUpdate(const QByteArray& original, qint64 prevXref, QList<Obj> objs,
                                  int rootId, int rootGen, int newSize,
                                  const QByteArray& idArray) {
    QByteArray out = original;
    if (!out.endsWith('\n'))
        out += '\n';

    std::sort(objs.begin(), objs.end(), [](const Obj& a, const Obj& b) { return a.id < b.id; });

    QList<qint64> offsets;
    offsets.reserve(objs.size());
    for (const Obj& o : objs) {
        offsets << out.size();
        out += o.serialized;
    }

    const qint64 xrefPos = out.size();
    QByteArray xref = "xref\n";
    int i = 0;
    while (i < objs.size()) {
        int j = i;
        while (j + 1 < objs.size() && objs[j + 1].id == objs[j].id + 1)
            ++j;
        xref += QByteArray::number(objs[i].id) + ' ' + QByteArray::number(j - i + 1) + '\n';
        for (int k = i; k <= j; ++k) {
            char line[32];
            std::snprintf(line, sizeof(line), "%010lld %05d n\r\n",
                          static_cast<long long>(offsets[k]), objs[k].gen);
            xref += line;
        }
        i = j + 1;
    }
    out += xref;

    QByteArray trailer = "trailer\n<< /Size " + QByteArray::number(newSize) + " /Root "
        + QByteArray::number(rootId) + ' ' + QByteArray::number(rootGen) + " R /Prev "
        + QByteArray::number(prevXref);
    if (!idArray.isEmpty())
        trailer += " /ID " + idArray;
    trailer += " >>\n";
    out += trailer;
    out += "startxref\n" + QByteArray::number(xrefPos) + "\n%%EOF\n";
    return out;
}

QByteArray rebuildDict(QPDFObjectHandle dict, const QMap<QByteArray, QByteArray>& overrides) {
    QByteArray out = "<<";
    QSet<QByteArray> written;
    for (const std::string& k : dict.getKeys()) {
        const QByteArray key = QByteArray::fromStdString(k);
        out += ' ' + key + ' ';
        out += overrides.contains(key) ? overrides.value(key)
                                       : QByteArray::fromStdString(dict.getKey(k).unparse());
        written.insert(key);
    }
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it)
        if (!written.contains(it.key()))
            out += ' ' + it.key() + ' ' + it.value();
    out += " >>";
    return out;
}

QByteArray arrayWithAppended(QPDFObjectHandle array, int newId) {
    QByteArray a = "[";
    if (array.isArray())
        for (int i = 0; i < array.getArrayNItems(); ++i)
            a += ' ' + QByteArray::fromStdString(array.getArrayItem(i).unparse());
    a += ' ' + QByteArray::number(newId) + " 0 R ]";
    return a;
}

} // namespace PdfIncrement
