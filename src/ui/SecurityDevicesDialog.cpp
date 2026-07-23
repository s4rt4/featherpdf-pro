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

#include "ui/SecurityDevicesDialog.h"

#include "backends/Signer.h"
#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

SecurityDevicesDialog::SecurityDevicesDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Security Devices"));

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
        QStringLiteral(
            "QLabel#Hint { color:%2; }"
            "QLabel#Status { color:%2; }"
            "QListWidget, QLineEdit { background:%1; border:1px solid %3; border-radius:8px;"
            " padding:6px 9px; color:%4; selection-background-color:%5; selection-color:#FFFFFF; }"
            "QLineEdit:focus { border:1px solid %5; }"
            "QPushButton#Browse { background:%1; border:1px solid %3; border-radius:8px;"
            " padding:6px 12px; color:%4; } QPushButton#Browse:hover { border:1px solid %5; }"
            "QPushButton#Browse:disabled { color:%2; }")
            .arg(css(p.surface), css(p.dim), css(ctlBorder), css(p.text), css(p.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(12);

    auto* hint = new QLabel(
        tr("Register a PKCS#11 module for a smartcard or USB token (e.g. "
           "/usr/lib64/opensc-pkcs11.so). Certificates on the inserted token then appear in the "
           "Sign dialog; enter the token PIN as the certificate password."),
        this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setMinimumSize(360, 130);
    root->addWidget(m_list, 1);

    auto* removeBtn = new QPushButton(tr("Remove selected"), this);
    removeBtn->setObjectName(QStringLiteral("Browse"));
    removeBtn->setCursor(Qt::PointingHandCursor);
    auto* removeRow = new QHBoxLayout;
    removeRow->addStretch(1);
    removeRow->addWidget(removeBtn);
    root->addLayout(removeRow);

    // Add-a-device row: a name, the module path (with a file picker), and Add.
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("Name, e.g. YubiKey"));
    m_module = new QLineEdit(this);
    m_module->setPlaceholderText(tr("PKCS#11 module (.so)"));
    auto* browse = new QPushButton(tr("Browse…"), this);
    browse->setObjectName(QStringLiteral("Browse"));
    browse->setCursor(Qt::PointingHandCursor);
    auto* addBtn = new QPushButton(tr("Add device"), this);
    addBtn->setObjectName(QStringLiteral("Share")); // accent-filled primary
    addBtn->setCursor(Qt::PointingHandCursor);

    auto* addRow = new QHBoxLayout;
    addRow->addWidget(m_name, 2);
    addRow->addWidget(m_module, 3);
    addRow->addWidget(browse);
    addRow->addWidget(addBtn);
    root->addLayout(addRow);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("Status"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Select PKCS#11 module"), QStringLiteral("/usr/lib64"),
            tr("Shared libraries (*.so *.so.*)"));
        if (!path.isEmpty())
            m_module->setText(path);
    });
    connect(addBtn, &QPushButton::clicked, this, &SecurityDevicesDialog::addDevice);
    connect(removeBtn, &QPushButton::clicked, this, &SecurityDevicesDialog::removeSelected);

    if (!Signer::hasSecurityDeviceTools()) {
        m_name->setEnabled(false);
        m_module->setEnabled(false);
        browse->setEnabled(false);
        addBtn->setEnabled(false);
        removeBtn->setEnabled(false);
        m_status->setText(tr("Install nss-tools (modutil) to manage security devices."));
    }
    refresh();
}

void SecurityDevicesDialog::refresh() {
    m_list->clear();
    const QStringList devices = Signer::securityDevices();
    if (devices.isEmpty())
        m_list->addItem(tr("(no security devices registered)"));
    else
        m_list->addItems(devices);
    m_list->setEnabled(!devices.isEmpty());
}

void SecurityDevicesDialog::addDevice() {
    const QString name = m_name->text().trimmed();
    const QString module = m_module->text().trimmed();
    if (name.isEmpty() || module.isEmpty()) {
        m_status->setText(tr("Give the device a name and pick its module."));
        return;
    }
    QString err;
    if (!Signer::addSecurityDevice(name, module, &err)) {
        m_status->setText(err);
        return;
    }
    m_name->clear();
    m_module->clear();
    m_status->setText(tr("Added %1.").arg(name));
    refresh();
}

void SecurityDevicesDialog::removeSelected() {
    const QListWidgetItem* item = m_list->currentItem();
    if (!item || !m_list->isEnabled()) {
        m_status->setText(tr("Select a device to remove."));
        return;
    }
    const QString name = item->text();
    QString err;
    if (!Signer::removeSecurityDevice(name, &err)) {
        m_status->setText(err);
        return;
    }
    m_status->setText(tr("Removed %1.").arg(name));
    refresh();
}
