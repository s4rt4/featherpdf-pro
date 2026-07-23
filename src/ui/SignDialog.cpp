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

#include "ui/SignDialog.h"

#include "backends/Signer.h"
#include "ui/SecurityDevicesDialog.h"
#include "ui/Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QVBoxLayout>

SignDialog::SignDialog(const QStringList& certificates, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Sign Document"));

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
    const QString chevDown = Theme::instance().symbolicIconPath(QStringLiteral("chevron-down"), p.dim);
    setStyleSheet(
        QStringLiteral(
            "QLabel#Hint { color:%2; }"
            "QLineEdit, QComboBox { background:%1; border:1px solid %3; border-radius:8px;"
            " padding:6px 9px; color:%4; selection-background-color:%5; selection-color:#FFFFFF; }"
            "QLineEdit:focus, QComboBox:focus { border:1px solid %5; }"
            "QComboBox::drop-down { border:none; width:26px; }"
            "QComboBox::down-arrow { image:url(%6); width:13px; height:13px; }"
            "QComboBox QAbstractItemView { background:%1; border:1px solid %3; border-radius:8px;"
            " padding:4px; outline:0; selection-background-color:%7; selection-color:%4; }"
            "QRadioButton, QCheckBox { color:%4; spacing:7px; }"
            "QRadioButton:disabled, QCheckBox:disabled, QLineEdit:disabled { color:%2; }"
            "QPushButton#Browse { background:%1; border:1px solid %3; border-radius:8px;"
            " padding:6px 12px; color:%4; }"
            "QPushButton#Browse:hover { border:1px solid %5; }")
            .arg(css(p.surface), css(p.dim), css(ctlBorder), css(p.text), css(p.accent), chevDown,
                 css(p.accentTint)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(14);

    auto* hint = new QLabel(
        tr("Sign with a certificate from your system. The signature is placed on the current "
           "page; anyone can then verify the document is intact."),
        this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_cert = new QComboBox(this);
    m_cert->addItems(certificates);
    m_cert->setMinimumWidth(280);
    m_reason = new QLineEdit(this);
    m_reason->setPlaceholderText(tr("e.g. Approved"));
    m_location = new QLineEdit(this);
    m_location->setPlaceholderText(tr("e.g. Jakarta"));
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Leave empty if the key has no password"));
    // The certificate, plus a way to register a PKCS#11 hardware token whose
    // certificates then appear in this list.
    auto* devicesBtn = new QPushButton(tr("Security devices…"), this);
    devicesBtn->setObjectName(QStringLiteral("Browse"));
    devicesBtn->setCursor(Qt::PointingHandCursor);
    auto* certRow = new QHBoxLayout;
    certRow->setSpacing(8);
    certRow->addWidget(m_cert, 1);
    certRow->addWidget(devicesBtn);
    form->addRow(tr("Certificate"), certRow);
    connect(devicesBtn, &QPushButton::clicked, this, [this] {
        SecurityDevicesDialog(this).exec();
        // A newly registered token may have added certificates; refresh the list.
        const QString current = m_cert->currentText();
        m_cert->clear();
        m_cert->addItems(Signer::availableCertificates());
        const int idx = m_cert->findText(current);
        if (idx >= 0)
            m_cert->setCurrentIndex(idx);
    });
    form->addRow(tr("Reason"), m_reason);
    form->addRow(tr("Location"), m_location);
    form->addRow(tr("Password"), m_password);

    // Appearance: the default text block, or a graphical (image) signature.
    m_apprText = new QRadioButton(tr("Text"), this);
    m_apprImage = new QRadioButton(tr("Image"), this);
    m_apprText->setChecked(true);
    auto* apprGroup = new QButtonGroup(this);
    apprGroup->addButton(m_apprText);
    apprGroup->addButton(m_apprImage);
    auto* apprRow = new QHBoxLayout;
    apprRow->setSpacing(16);
    apprRow->addWidget(m_apprText);
    apprRow->addWidget(m_apprImage);
    apprRow->addStretch(1);
    form->addRow(tr("Appearance"), apprRow);

    m_imagePath = new QLineEdit(this);
    m_imagePath->setPlaceholderText(tr("Choose a PNG or JPEG of your signature"));
    m_imagePath->setReadOnly(true);
    auto* browse = new QPushButton(tr("Browse…"), this);
    browse->setObjectName(QStringLiteral("Browse"));
    browse->setCursor(Qt::PointingHandCursor);
    auto* imgRow = new QHBoxLayout;
    imgRow->setSpacing(8);
    imgRow->addWidget(m_imagePath, 1);
    imgRow->addWidget(browse);
    form->addRow(QString(), imgRow);

    // Trusted timestamp (RFC 3161) — a detached .tsr sidecar next to the signed file.
    QSettings settings;
    m_timestamp = new QCheckBox(tr("Add a trusted timestamp (RFC 3161)"), this);
    m_timestamp->setChecked(
        settings.value(QStringLiteral("sign/timestamp"), false).toBool());
    form->addRow(QString(), m_timestamp);

    m_tsaUrl = new QLineEdit(this);
    m_tsaUrl->setText(settings
                          .value(QStringLiteral("sign/tsaUrl"),
                                 QStringLiteral("https://freetsa.org/tsr"))
                          .toString());
    m_tsaUrl->setPlaceholderText(tr("https://your-tsa.example/tsr"));
    form->addRow(tr("TSA URL"), m_tsaUrl);

    root->addLayout(form);

    // Image fields follow the Image radio; the TSA URL follows the checkbox.
    const auto syncImage = [this] {
        const bool on = m_apprImage->isChecked();
        m_imagePath->setEnabled(on);
    };
    connect(m_apprText, &QRadioButton::toggled, this, syncImage);
    connect(m_apprImage, &QRadioButton::toggled, this, syncImage);
    syncImage();
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(
            this, tr("Choose signature image"), QString(),
            tr("Images (*.png *.jpg *.jpeg)"));
        if (!f.isEmpty()) {
            m_imagePath->setText(f);
            m_apprImage->setChecked(true);
        }
    });

    const auto syncTsa = [this] { m_tsaUrl->setEnabled(m_timestamp->isChecked()); };
    connect(m_timestamp, &QCheckBox::toggled, this, syncTsa);
    syncTsa();

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* sign = buttons->addButton(tr("Sign"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    sign->setObjectName(QStringLiteral("Share"));
    sign->setCursor(Qt::PointingHandCursor);
    sign->setEnabled(!certificates.isEmpty());
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        // Remember the timestamp choice and TSA URL for next time.
        QSettings settings;
        settings.setValue(QStringLiteral("sign/timestamp"), m_timestamp->isChecked());
        settings.setValue(QStringLiteral("sign/tsaUrl"), m_tsaUrl->text().trimmed());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    // Qt leaves stylesheet padding out of a line edit's size hint, so in a packed
    // form the fields compress until their placeholder text clips. Pin a
    // font-scaled height on every field so the text always has room.
    const int fieldHeight = fontMetrics().height() + 14;
    for (QLineEdit* le : findChildren<QLineEdit*>())
        le->setMinimumHeight(fieldHeight);

    resize(440, 0);
}

QString SignDialog::certificate() const { return m_cert->currentText(); }
QString SignDialog::reason() const { return m_reason->text().trimmed(); }
QString SignDialog::location() const { return m_location->text().trimmed(); }
QString SignDialog::password() const { return m_password->text(); }

QString SignDialog::imagePath() const {
    if (!m_apprImage->isChecked())
        return QString();
    const QString p = m_imagePath->text().trimmed();
    return QFileInfo::exists(p) ? p : QString();
}

bool SignDialog::wantsTimestamp() const { return m_timestamp->isChecked(); }
QString SignDialog::tsaUrl() const { return m_tsaUrl->text().trimmed(); }
