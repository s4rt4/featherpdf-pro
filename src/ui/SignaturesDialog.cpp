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

#include "ui/SignaturesDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SignaturesDialog::SignaturesDialog(const QList<Signer::SignatureInfo>& signatures, QWidget* parent,
                                   bool offerLtv)
    : QDialog(parent) {
    setWindowTitle(tr("Signatures"));

    const Theme::Palette& p = Theme::instance().palette();
    const auto css = [](const QColor& c) { return c.name(QColor::HexRgb); };
    setStyleSheet(QStringLiteral("QLabel#Signer { color:%1; font-weight:600; }"
                                 "QLabel#Meta { color:%2; font-size:12px; }"
                                 "QLabel#Valid { color:%3; font-size:12px; font-weight:600; }"
                                 "QLabel#Invalid { color:%4; font-size:12px; font-weight:600; }"
                                 "#SigCard { background:%5; border:1px solid %6; border-radius:10px; }")
                      .arg(css(p.text), css(p.dim), css(p.success), css(p.destructive),
                           css(p.surface), css(p.canvas)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    if (signatures.isEmpty()) {
        auto* none = new QLabel(tr("This document has no digital signatures."), this);
        none->setObjectName(QStringLiteral("Meta"));
        none->setWordWrap(true);
        root->addWidget(none);
    } else {
        for (const Signer::SignatureInfo& s : signatures) {
            auto* card = new QFrame(this);
            card->setObjectName(QStringLiteral("SigCard"));
            auto* cv = new QVBoxLayout(card);
            cv->setContentsMargins(14, 12, 14, 12);
            cv->setSpacing(4);

            auto* signer = new QLabel(s.signer.isEmpty() ? tr("Unknown signer") : s.signer, card);
            signer->setObjectName(QStringLiteral("Signer"));
            cv->addWidget(signer);

            auto* status = new QLabel(s.status, card);
            status->setObjectName(s.valid ? QStringLiteral("Valid") : QStringLiteral("Invalid"));
            status->setWordWrap(true);
            cv->addWidget(status);

            const QString meta =
                [&] {
                    QStringList parts;
                    if (!s.time.isEmpty())
                        parts << tr("Signed %1").arg(s.time);
                    if (!s.reason.isEmpty())
                        parts << tr("Reason: %1").arg(s.reason);
                    if (!s.location.isEmpty())
                        parts << tr("Location: %1").arg(s.location);
                    return parts.join(QStringLiteral("  ·  "));
                }();
            if (!meta.isEmpty()) {
                auto* m = new QLabel(meta, card);
                m->setObjectName(QStringLiteral("Meta"));
                m->setWordWrap(true);
                cv->addWidget(m);
            }
            root->addWidget(card);
        }
    }

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    // Long-term validation only makes sense for a document that has signatures.
    if (offerLtv && !signatures.isEmpty()) {
        auto* ltv = buttons->addButton(tr("Add long-term validation"),
                                       QDialogButtonBox::ActionRole);
        ltv->setObjectName(QStringLiteral("Share"));
        ltv->setToolTip(tr("Embed the certificate chain (and revocation data when available) so "
                           "the signature keeps validating after the certificate expires."));
        connect(ltv, &QPushButton::clicked, this, [this] {
            m_ltvRequested = true;
            accept();
        });
    }
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);

    resize(420, 0);
}
