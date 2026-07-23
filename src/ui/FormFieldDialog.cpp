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

#include "ui/FormFieldDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

FormFieldDialog::FormFieldDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Add Form Field"));

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
                       "QLabel, QCheckBox { color:%2; }"
                       "QLineEdit, QComboBox { background:%3; border:1px solid %4;"
                       " border-radius:8px; padding:5px 8px; color:%2; }"
                       "QLineEdit:focus, QComboBox:focus { border:1px solid %5; }")
            .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder), css(p.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(10);

    auto* hint = new QLabel(
        tr("The field is placed on the page you're viewing. You can fill it in the Forms panel."),
        this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    m_type = new QComboBox(this);
    m_type->addItem(tr("Text"), int(FormEditor::Type::Text));
    m_type->addItem(tr("Check box"), int(FormEditor::Type::CheckBox));
    m_type->addItem(tr("Dropdown"), int(FormEditor::Type::Dropdown));
    m_type->addItem(tr("Radio group"), int(FormEditor::Type::Radio));
    m_type->addItem(tr("Push button"), int(FormEditor::Type::PushButton));
    form->addRow(tr("Type"), m_type);

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("e.g. full_name"));
    form->addRow(tr("Name"), m_name);

    m_default = new QLineEdit(this);
    m_defaultLabel = new QLabel(tr("Default"), this);
    form->addRow(m_defaultLabel, m_default);

    m_options = new QLineEdit(this);
    m_options->setPlaceholderText(tr("comma-separated, e.g. Yes, No, Maybe"));
    m_optionsLabel = new QLabel(tr("Options"), this);
    form->addRow(m_optionsLabel, m_options);

    m_format = new QComboBox(this);
    m_format->addItem(tr("None"), int(FormEditor::Format::None));
    m_format->addItem(tr("Number"), int(FormEditor::Format::Number));
    m_format->addItem(tr("Currency"), int(FormEditor::Format::Currency));
    m_format->addItem(tr("Percentage"), int(FormEditor::Format::Percent));
    m_format->addItem(tr("Date"), int(FormEditor::Format::Date));
    m_format->addItem(tr("Time"), int(FormEditor::Format::Time));
    m_formatLabel = new QLabel(tr("Format"), this);
    form->addRow(m_formatLabel, m_format);

    m_checked = new QCheckBox(tr("Checked by default"), this);
    form->addRow(QString(), m_checked);

    root->addLayout(form);
    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* add = buttons->addButton(tr("Add Field"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    add->setObjectName(QStringLiteral("Share")); // accent-filled primary
    add->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    connect(m_type, &QComboBox::currentIndexChanged, this, &FormFieldDialog::syncRows);
    syncRows();

    resize(420, 0);
    m_name->setFocus();
}

void FormFieldDialog::syncRows() {
    const FormEditor::Type t = fieldType();
    const bool isCheck = t == FormEditor::Type::CheckBox;
    const bool isDrop = t == FormEditor::Type::Dropdown;
    const bool isRadio = t == FormEditor::Type::Radio;
    const bool isButton = t == FormEditor::Type::PushButton;

    // Default value applies to Text (initial text) and Push button (caption).
    const bool showDefault = t == FormEditor::Type::Text || isButton;
    m_defaultLabel->setVisible(showDefault);
    m_default->setVisible(showDefault);
    m_defaultLabel->setText(isButton ? tr("Caption") : tr("Default"));

    // Options drive both a dropdown's choices and a radio group's buttons.
    m_optionsLabel->setVisible(isDrop || isRadio);
    m_options->setVisible(isDrop || isRadio);
    m_optionsLabel->setText(isRadio ? tr("Buttons") : tr("Options"));

    // Input format / validation applies only to a Text field.
    const bool isText = t == FormEditor::Type::Text;
    m_formatLabel->setVisible(isText);
    m_format->setVisible(isText);

    m_checked->setVisible(isCheck);
}

FormEditor::Type FormFieldDialog::fieldType() const {
    return static_cast<FormEditor::Type>(m_type->currentData().toInt());
}

QString FormFieldDialog::fieldName() const {
    return m_name->text().trimmed();
}

QString FormFieldDialog::defaultValue() const {
    return m_default->text();
}

bool FormFieldDialog::checked() const {
    return m_checked->isChecked();
}

QStringList FormFieldDialog::options() const {
    QStringList out;
    for (const QString& part : m_options->text().split(',', Qt::SkipEmptyParts)) {
        const QString t = part.trimmed();
        if (!t.isEmpty())
            out << t;
    }
    return out;
}

FormEditor::Format FormFieldDialog::format() const {
    return static_cast<FormEditor::Format>(m_format->currentData().toInt());
}
