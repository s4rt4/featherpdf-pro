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

#include <QDialog>

#include "backends/Signer.h"

// Shows the signatures present in a document and their validity. When `offerLtv`
// is set and the document has signatures, it also offers to embed long-term
// validation data; the caller checks ltvRequested() after exec() to act on it.
class SignaturesDialog : public QDialog {
    Q_OBJECT

public:
    SignaturesDialog(const QList<Signer::SignatureInfo>& signatures, QWidget* parent = nullptr,
                     bool offerLtv = false);

    // True if the user asked to add long-term validation (the dialog was accepted
    // via the "Add long-term validation" button).
    bool ltvRequested() const { return m_ltvRequested; }

private:
    bool m_ltvRequested = false;
};
