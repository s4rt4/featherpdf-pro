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
#include <QList>
#include <QMap>

#include <qpdf/QPDFObjectHandle.hh>

// Helpers for writing classic PDF incremental updates: serialize raw indirect
// objects, append them with a fresh xref subsection and a trailer chained via
// /Prev, and rebuild dictionaries/arrays while preserving indirect references.
// Used by the signing stack (Signer, LtvSigner), where the original bytes —
// and therefore every existing signature — must never move.
namespace PdfIncrement {

struct Obj {
    int id = 0;
    int gen = 0;
    QByteArray serialized; // complete "<id> <gen> obj ... endobj\n"
};

// A raw indirect object: "<id> <gen> obj\n<body>\nendobj\n".
QByteArray makeObj(int id, int gen, const QByteArray& body);

// A stream object holding `data` verbatim (DER blobs, appearance streams, …).
QByteArray makeStreamObj(int id, const QByteArray& data,
                         const QByteArray& extraDictEntries = QByteArray());

// The byte offset of the file's most recent cross-reference section, read from
// the last "startxref" pointer. -1 if it can't be found.
qint64 lastStartxref(const QByteArray& bytes);

// Append `objs` to `original` as a classic incremental update: a fresh xref
// subsection per run of consecutive ids, a trailer chaining back via /Prev,
// then startxref + %%EOF.
QByteArray writeIncrementalUpdate(const QByteArray& original, qint64 prevXref, QList<Obj> objs,
                                  int rootId, int rootGen, int newSize,
                                  const QByteArray& idArray);

// Serialise a dictionary, substituting `overrides[key]` for those keys and
// keeping every other key's value as-is (indirect references preserved). Keys
// present in `overrides` but not in the dictionary are appended.
QByteArray rebuildDict(QPDFObjectHandle dict, const QMap<QByteArray, QByteArray>& overrides);

// "[ a 0 R b 0 R ... ]" for an existing array's items plus one freshly added ref.
QByteArray arrayWithAppended(QPDFObjectHandle array, int newId);

} // namespace PdfIncrement
