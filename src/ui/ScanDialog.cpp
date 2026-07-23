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

#include "ui/ScanDialog.h"

#include "backends/Ocr.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QString cssColor(const QColor& c) {
    return c.alpha() == 255 ? c.name(QColor::HexRgb)
                            : QStringLiteral("rgba(%1,%2,%3,%4)")
                                  .arg(c.red())
                                  .arg(c.green())
                                  .arg(c.blue())
                                  .arg(QString::number(c.alphaF(), 'f', 3));
}
} // namespace

ScanDialog::ScanDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Scan"));

    const Theme::Palette& p = Theme::instance().palette();
    QColor ctlBorder = p.text;
    ctlBorder.setAlpha(46);
    setStyleSheet(
        QStringLiteral("QLabel#Hint { color:%1; font-size:12px; }"
                       "QLabel, QCheckBox { color:%2; }"
                       "QLineEdit, QComboBox, QSpinBox { background:%3; border:1px solid %4;"
                       " border-radius:8px; padding:5px 8px; color:%2; }"
                       "QComboBox:focus, QSpinBox:focus { border:1px solid %5; }"
                       "QPushButton#Ghost { background:transparent; border:1px solid %4;"
                       " border-radius:8px; padding:5px 12px; color:%2; }"
                       "QPushButton#Ghost:hover { border:1px solid %5; color:%5; }")
            .arg(cssColor(p.dim), cssColor(p.text), cssColor(p.surface), cssColor(ctlBorder),
                 cssColor(p.accent)));
    setStyleSheet(styleSheet() + Theme::instance().spinBoxArrowQss());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(10);

    auto* hint = new QLabel(
        tr("Scan from a connected device into a new PDF. Pages are captured with SANE; you can add "
           "a searchable text layer with OCR."),
        this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    // Device row: combo + refresh button.
    auto* deviceRow = new QHBoxLayout;
    deviceRow->setSpacing(8);
    m_device = new QComboBox(this);
    m_device->setMinimumWidth(240);
    m_refresh = new QPushButton(tr("Refresh"), this);
    m_refresh->setObjectName(QStringLiteral("Ghost"));
    m_refresh->setCursor(Qt::PointingHandCursor);
    deviceRow->addWidget(m_device, 1);
    deviceRow->addWidget(m_refresh);
    form->addRow(tr("Scanner"), deviceRow);

    m_resolution = new QComboBox(this);
    for (int dpi : {150, 200, 300, 600})
        m_resolution->addItem(tr("%1 DPI").arg(dpi), dpi);
    m_resolution->setCurrentIndex(2); // 300 DPI
    form->addRow(tr("Resolution"), m_resolution);

    m_mode = new QComboBox(this);
    m_mode->addItem(tr("Color"), int(Scanner::Mode::Color));
    m_mode->addItem(tr("Grayscale"), int(Scanner::Mode::Gray));
    form->addRow(tr("Mode"), m_mode);

    m_pages = new QSpinBox(this);
    m_pages->setRange(1, 200);
    m_pages->setValue(1);
    m_pages->setToolTip(tr("More than one page needs a document feeder (ADF)."));
    form->addRow(tr("Pages"), m_pages);

    m_ocr = new QCheckBox(tr("Make it searchable with OCR"), this);
    form->addRow(QString(), m_ocr);

    m_ocrLang = new QComboBox(this);
    const QStringList langs = Ocr::languages();
    for (const QString& code : langs)
        m_ocrLang->addItem(code, code);
    form->addRow(tr("OCR language"), m_ocrLang);

    root->addLayout(form);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("Hint"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto* buttons = new QDialogButtonBox(this);
    m_scan = qobject_cast<QPushButton*>(
        buttons->addButton(tr("Scan"), QDialogButtonBox::AcceptRole));
    buttons->addButton(QDialogButtonBox::Cancel);
    m_scan->setObjectName(QStringLiteral("Share")); // accent-filled primary
    m_scan->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    connect(m_refresh, &QPushButton::clicked, this, &ScanDialog::refreshDevices);

    // OCR controls are only meaningful when Tesseract is installed.
    const bool ocrOk = Ocr::isAvailable() && !langs.isEmpty();
    m_ocr->setEnabled(ocrOk);
    if (!ocrOk)
        m_ocr->setToolTip(tr("Install Tesseract to add a searchable text layer."));
    const auto syncOcr = [this] { m_ocrLang->setEnabled(m_ocr->isChecked()); };
    connect(m_ocr, &QCheckBox::toggled, this, syncOcr);
    syncOcr();

    resize(460, 0);
    refreshDevices();
}

void ScanDialog::refreshDevices() {
    m_device->clear();
    m_status->clear();
    m_refresh->setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QString error;
    const QList<Scanner::Device> devices = Scanner::devices(&error);

    QApplication::restoreOverrideCursor();
    m_refresh->setEnabled(true);

    for (const Scanner::Device& d : devices)
        m_device->addItem(d.label(), d.name);

    const bool any = !devices.isEmpty();
    m_device->setEnabled(any);
    m_scan->setEnabled(any);
    if (!any) {
        m_status->setText(error.isEmpty()
                              ? tr("No scanners found. Connect one and choose Refresh.")
                              : tr("Couldn't list scanners: %1").arg(error));
    }
}

QString ScanDialog::deviceName() const {
    return m_device->currentData().toString();
}

int ScanDialog::resolution() const {
    return m_resolution->currentData().toInt();
}

Scanner::Mode ScanDialog::mode() const {
    return static_cast<Scanner::Mode>(m_mode->currentData().toInt());
}

int ScanDialog::pageCount() const {
    return m_pages->value();
}

bool ScanDialog::runOcr() const {
    return m_ocr->isEnabled() && m_ocr->isChecked();
}

QString ScanDialog::ocrLanguage() const {
    return m_ocrLang->currentData().toString();
}
