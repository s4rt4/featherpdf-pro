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

#include "ui/PrepareFormDialog.h"

#include "ui/Theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
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

// Column order in the review table.
enum Column { ColInclude = 0, ColName, ColType, ColFormat, ColCount };

QComboBox* makeFormatCombo(QWidget* parent) {
    auto* c = new QComboBox(parent);
    c->addItem(PrepareFormDialog::tr("None"), int(FormEditor::Format::None));
    c->addItem(PrepareFormDialog::tr("Number"), int(FormEditor::Format::Number));
    c->addItem(PrepareFormDialog::tr("Currency"), int(FormEditor::Format::Currency));
    c->addItem(PrepareFormDialog::tr("Percentage"), int(FormEditor::Format::Percent));
    c->addItem(PrepareFormDialog::tr("Date"), int(FormEditor::Format::Date));
    c->addItem(PrepareFormDialog::tr("Time"), int(FormEditor::Format::Time));
    return c;
}

int indexForData(const QComboBox* combo, int value) {
    const int i = combo->findData(value);
    return i < 0 ? 0 : i;
}
} // namespace

PrepareFormDialog::PrepareFormDialog(const QList<FormEditor::NewField>& detected, QWidget* parent)
    : QDialog(parent), m_fields(detected) {
    setWindowTitle(tr("Prepare Form"));

    const Theme::Palette& pal = Theme::instance().palette();
    QColor ctlBorder = pal.text;
    ctlBorder.setAlpha(46);
    setStyleSheet(
        QStringLiteral(
            "QLabel#Hint { color:%2; font-size:12px; }"
            "QTableWidget { background:%1; border:1px solid %3; border-radius:8px;"
            " gridline-color:%3; color:%4; outline:0; }"
            "QTableWidget::item { padding:2px 4px; }"
            "QTableWidget::item:selected { background:%5; color:%4; }"
            "QHeaderView::section { background:transparent; color:%2; border:none;"
            " border-bottom:1px solid %3; padding:6px 4px; font-weight:600; }"
            "QTableCornerButton::section { background:transparent; border:none; }"
            "QLineEdit { background:%1; border:1px solid %3; border-radius:7px; padding:4px 7px;"
            " color:%4; selection-background-color:%6; selection-color:#FFFFFF; }"
            "QLineEdit:focus { border:1px solid %6; }"
            "QComboBox { background:%1; border:1px solid %3; border-radius:7px; padding:4px 7px;"
            " color:%4; }"
            "QComboBox:focus { border:1px solid %6; }"
            "QComboBox:disabled { color:%2; }")
            .arg(cssColor(pal.surface), cssColor(pal.dim), cssColor(ctlBorder), cssColor(pal.text),
                 cssColor(pal.accentTint), cssColor(pal.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(10);

    auto* hint = new QLabel(
        tr("These fillable areas were detected from the page layout. Untick any you don't want, "
           "rename them, and adjust the type or format before adding them to the document."),
        this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_table = new QTableWidget(m_fields.size(), ColCount, this);
    m_table->setHorizontalHeaderLabels(
        {tr("Add"), tr("Name"), tr("Type"), tr("Format")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColInclude, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColType, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColFormat, QHeaderView::ResizeToContents);
    m_table->setMinimumSize(560, 280);

    for (int row = 0; row < m_fields.size(); ++row) {
        const FormEditor::NewField& f = m_fields[row];

        auto* include = new QTableWidgetItem;
        include->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        include->setCheckState(Qt::Checked);
        include->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, ColInclude, include);

        auto* name = new QLineEdit(f.name, m_table);
        m_table->setCellWidget(row, ColName, name);

        auto* type = new QComboBox(m_table);
        type->addItem(tr("Text"), int(FormEditor::Type::Text));
        type->addItem(tr("Check box"), int(FormEditor::Type::CheckBox));
        type->setCurrentIndex(indexForData(type, int(f.type)));
        type->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        type->setMinimumWidth(110);
        m_table->setCellWidget(row, ColType, type);

        auto* format = makeFormatCombo(m_table);
        format->setCurrentIndex(indexForData(format, int(f.format)));
        format->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        format->setMinimumWidth(120);
        m_table->setCellWidget(row, ColFormat, format);

        // Format only makes sense for a Text field; grey it out otherwise.
        const auto syncFormat = [type, format] {
            const auto t = static_cast<FormEditor::Type>(type->currentData().toInt());
            format->setEnabled(t == FormEditor::Type::Text);
        };
        syncFormat();
        connect(type, &QComboBox::currentIndexChanged, format, syncFormat);
    }
    root->addWidget(m_table, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* add = buttons->addButton(tr("Add Fields"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    add->setObjectName(QStringLiteral("Share")); // accent-filled primary
    add->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(640, 460);
}

QList<FormEditor::NewField> PrepareFormDialog::selectedFields() const {
    QList<FormEditor::NewField> out;
    for (int row = 0; row < m_fields.size(); ++row) {
        const QTableWidgetItem* include = m_table->item(row, ColInclude);
        if (!include || include->checkState() != Qt::Checked)
            continue;

        FormEditor::NewField f = m_fields[row]; // keep page + rect from detection
        if (auto* name = qobject_cast<QLineEdit*>(m_table->cellWidget(row, ColName)))
            f.name = name->text().trimmed();
        if (auto* type = qobject_cast<QComboBox*>(m_table->cellWidget(row, ColType)))
            f.type = static_cast<FormEditor::Type>(type->currentData().toInt());
        if (f.type == FormEditor::Type::Text) {
            if (auto* fmt = qobject_cast<QComboBox*>(m_table->cellWidget(row, ColFormat)))
                f.format = static_cast<FormEditor::Format>(fmt->currentData().toInt());
        } else {
            f.format = FormEditor::Format::None;
        }
        out.append(f);
    }
    return out;
}
