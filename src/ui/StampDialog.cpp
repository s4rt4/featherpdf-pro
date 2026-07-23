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

#include "ui/StampDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

StampDialog::StampDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Stamp"));

    const Theme::Palette& p = Theme::instance().palette();
    QColor ctlBorder = p.text;
    ctlBorder.setAlpha(46);
    const auto css = [](const QColor& c) {
        return c.alpha() == 255 ? c.name(QColor::HexRgb)
                                : QStringLiteral("rgba(%1,%2,%3,%4)")
                                      .arg(c.red())
                                      .arg(c.green())
                                      .arg(c.blue())
                                      .arg(QString::number(c.alphaF(), 'f', 3));
    };
    setStyleSheet(
        QStringLiteral("QLabel#Hint { color:%1; font-size:12px; }"
                       "QLabel#Preview { color:%2; font-weight:600; font-size:14px;"
                       " background:%3; border:1px solid %4; border-radius:8px; padding:9px 11px; }"
                       "QLabel, QCheckBox { color:%2; }"
                       "QLineEdit, QComboBox { background:%3; border:1px solid %4;"
                       " border-radius:8px; padding:5px 8px; color:%2; }"
                       "QLineEdit:focus, QComboBox:focus { border:1px solid %5; }")
            .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder), css(p.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(10);

    auto* hint = new QLabel(tr("Pick a stamp, then drag on the page to place it."), this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    m_preset = new QComboBox(this);
    m_preset->addItem(tr("Approved"), int(StampLibrary::Preset::Approved));
    m_preset->addItem(tr("Draft"), int(StampLibrary::Preset::Draft));
    m_preset->addItem(tr("Confidential"), int(StampLibrary::Preset::Confidential));
    m_preset->addItem(tr("For Review"), int(StampLibrary::Preset::ForReview));
    form->addRow(tr("Stamp"), m_preset);

    m_date = new QCheckBox(tr("Include today's date"), this);
    form->addRow(QString(), m_date);

    m_useName = new QCheckBox(tr("Include a name"), this);
    form->addRow(QString(), m_useName);

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("e.g. Vinvan"));
    m_nameLabel = new QLabel(tr("Name"), this);
    form->addRow(m_nameLabel, m_name);

    root->addLayout(form);

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("Preview"));
    m_preview->setAlignment(Qt::AlignCenter);
    root->addWidget(m_preview);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* place = buttons->addButton(tr("Place Stamp"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    place->setObjectName(QStringLiteral("Share")); // accent-filled primary
    place->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    connect(m_preset, &QComboBox::currentIndexChanged, this, &StampDialog::refreshPreview);
    connect(m_date, &QCheckBox::toggled, this, &StampDialog::refreshPreview);
    connect(m_useName, &QCheckBox::toggled, this, [this] {
        syncRows();
        refreshPreview();
    });
    connect(m_name, &QLineEdit::textChanged, this, &StampDialog::refreshPreview);

    syncRows();
    refreshPreview();
    resize(420, 0);
}

void StampDialog::syncRows() {
    const bool on = m_useName->isChecked();
    m_nameLabel->setVisible(on);
    m_name->setVisible(on);
}

void StampDialog::refreshPreview() {
    const QString date = includeDate() ? QDate::currentDate().toString(Qt::ISODate) : QString();
    m_preview->setText(StampLibrary::caption(preset(), date, name()));
    const QColor c = StampLibrary::color(preset());
    m_preview->setStyleSheet(QStringLiteral("color:%1;").arg(c.name(QColor::HexRgb)));
}

StampLibrary::Preset StampDialog::preset() const {
    return static_cast<StampLibrary::Preset>(m_preset->currentData().toInt());
}

bool StampDialog::includeDate() const {
    return m_date->isChecked();
}

QString StampDialog::name() const {
    return m_useName->isChecked() ? m_name->text().trimmed() : QString();
}
