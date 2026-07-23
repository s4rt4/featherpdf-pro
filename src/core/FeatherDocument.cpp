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

#include "core/FeatherDocument.h"

#include <QFileInfo>
#include <QPdfDocument>

FeatherDocument::FeatherDocument(QObject* parent)
    : QObject(parent), m_pdf(new QPdfDocument(this)) {}

FeatherDocument::~FeatherDocument() = default;

FeatherDocument::LoadResult FeatherDocument::load(const QString& path, const QString& password) {
    m_pdf->setPassword(password); // ignored for unencrypted files
    const QPdfDocument::Error err = m_pdf->load(path);

    LoadResult result;
    switch (err) {
    case QPdfDocument::Error::None:
        result = LoadResult::Ok;
        break;
    case QPdfDocument::Error::FileNotFound:
        result = LoadResult::FileNotFound;
        break;
    case QPdfDocument::Error::InvalidFileFormat:
        result = LoadResult::InvalidFormat;
        break;
    case QPdfDocument::Error::IncorrectPassword:
        result = LoadResult::PasswordRequired;
        break;
    case QPdfDocument::Error::UnsupportedSecurityScheme:
        result = LoadResult::Unsupported;
        break;
    case QPdfDocument::Error::DataNotYetAvailable:
    case QPdfDocument::Error::Unknown:
    default:
        result = LoadResult::UnknownError;
        break;
    }

    if (result == LoadResult::Ok) {
        m_filePath = path;
        m_password = password;
        const int n = m_pdf->pageCount();
        m_order.resize(n);
        for (int i = 0; i < n; ++i)
            m_order[i] = i; // identity arrangement to start
        m_rot.assign(n, 0);
        setModified(false);
        emit loaded();
    } else {
        m_filePath.clear();
    }
    return result;
}

int FeatherDocument::originalPageCount() const {
    return m_pdf->pageCount();
}

int FeatherDocument::rotation(int displayIndex) const {
    return (displayIndex >= 0 && displayIndex < m_rot.size()) ? m_rot[displayIndex] : 0;
}

int FeatherDocument::originalPageAt(int displayIndex) const {
    return (displayIndex >= 0 && displayIndex < m_order.size()) ? m_order[displayIndex] : -1;
}

void FeatherDocument::rotatePage(int displayIndex, int deltaDegrees) {
    if (displayIndex < 0 || displayIndex >= m_rot.size())
        return;
    int next = (m_rot[displayIndex] + deltaDegrees) % 360;
    if (next < 0)
        next += 360;
    if (next == m_rot[displayIndex])
        return;
    m_rot[displayIndex] = next;
    setModified(true);
    emit pageEdited(displayIndex);
}

void FeatherDocument::removePage(int displayIndex, int* originalOut, int* rotationOut) {
    if (displayIndex < 0 || displayIndex >= m_order.size())
        return;
    if (originalOut)
        *originalOut = m_order[displayIndex];
    if (rotationOut)
        *rotationOut = m_rot[displayIndex];
    m_order.remove(displayIndex);
    m_rot.remove(displayIndex);
    setModified(true);
    emit arrangementChanged();
}

void FeatherDocument::insertPage(int displayIndex, int originalPage, int rotation) {
    displayIndex = qBound(0, displayIndex, m_order.size());
    m_order.insert(displayIndex, originalPage);
    m_rot.insert(displayIndex, ((rotation % 360) + 360) % 360);
    setModified(true);
    emit arrangementChanged();
}

void FeatherDocument::movePage(int from, int to) {
    if (from < 0 || from >= m_order.size() || to < 0 || to >= m_order.size() || from == to)
        return;
    m_order.move(from, to);
    m_rot.move(from, to);
    setModified(true);
    emit arrangementChanged();
}

void FeatherDocument::markSaved() {
    setModified(false);
}

void FeatherDocument::setModified(bool modified) {
    if (m_modified == modified)
        return;
    m_modified = modified;
    emit modifiedChanged(m_modified);
}

void FeatherDocument::close() {
    if (!isLoaded())
        return;
    m_pdf->close();
    m_filePath.clear();
    m_password.clear();
    m_encrypted = false;
    m_order.clear();
    m_rot.clear();
    setModified(false);
    emit closed();
}

bool FeatherDocument::isLoaded() const {
    return m_pdf->status() == QPdfDocument::Status::Ready;
}

QString FeatherDocument::fileName() const {
    return m_filePath.isEmpty() ? QString() : QFileInfo(m_filePath).fileName();
}

QString FeatherDocument::title() const {
    const QString meta = m_pdf->metaData(QPdfDocument::MetaDataField::Title).toString().trimmed();
    return meta.isEmpty() ? fileName() : meta;
}

int FeatherDocument::pageCount() const {
    return m_order.size(); // current (display) count after edits
}

QString FeatherDocument::describe(LoadResult result) {
    switch (result) {
    case LoadResult::Ok:
        return QObject::tr("The document opened successfully.");
    case LoadResult::FileNotFound:
        return QObject::tr("That file could not be found. It may have been moved or deleted.");
    case LoadResult::InvalidFormat:
        return QObject::tr("This file isn't a valid PDF, or it is damaged.");
    case LoadResult::PasswordRequired:
        return QObject::tr("This PDF is password-protected. Enter the password to open it.");
    case LoadResult::Unsupported:
        return QObject::tr("This PDF uses a security scheme Feather can't open yet.");
    case LoadResult::UnknownError:
        break;
    }
    return QObject::tr("The document couldn't be opened.");
}
