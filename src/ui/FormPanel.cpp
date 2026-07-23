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

#include "ui/FormPanel.h"

#include "backends/FormFiller.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

FormPanel::FormPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("FormPanel"));

    auto applyTheme = [this] {
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
            QStringLiteral("#FormPanel QLabel#FieldName { color:%1; font-size:11px; }"
                           "#FormPanel QLineEdit, #FormPanel QComboBox { background:%2;"
                           " border:1px solid %3; border-radius:7px; padding:5px 7px; color:%4; }"
                           "#FormPanel QLineEdit:focus, #FormPanel QComboBox:focus {"
                           " border:1px solid %5; }"
                           "#FormPanel QCheckBox { color:%4; spacing:8px; }")
                .arg(css(p.dim), css(p.surface), css(ctlBorder), css(p.text), css(p.accent)));
    };
    applyTheme();
    connect(&Theme::instance(), &Theme::changed, this, applyTheme);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    m_stack = new QStackedWidget(this);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_form = new QWidget(scroll);
    new QVBoxLayout(m_form); // populated per rebuild
    scroll->setWidget(m_form);

    m_empty = new QLabel(tr("This document has no form fields."), this);
    m_empty->setObjectName(QStringLiteral("OutlineEmpty"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setContentsMargins(16, 16, 16, 16);

    m_stack->addWidget(scroll);   // 0
    m_stack->addWidget(m_empty);  // 1
    col->addWidget(m_stack, 1);

    // Icon-only footer (the rail is narrow); tooltips name each action.
    const auto makeBtn = [this](const QString& tip) {
        auto* b = new QToolButton(this);
        b->setToolTip(tip);
        b->setAccessibleName(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setAutoRaise(true);
        b->setFixedSize(30, 30);
        b->setIconSize(QSize(17, 17));
        return b;
    };
    m_add = makeBtn(tr("Add a field"));
    m_prepare = makeBtn(tr("Prepare Form: detect fillable areas"));
    m_save = makeBtn(tr("Save the form"));
    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(12, 8, 12, 12);
    footer->setSpacing(8);
    footer->addWidget(m_add);
    footer->addWidget(m_prepare);
    footer->addStretch(1);
    footer->addWidget(m_save);
    col->addLayout(footer);

    const auto applyIcons = [this] {
        const Theme::Palette& p = Theme::instance().palette();
        m_add->setIcon(Theme::instance().icon(QStringLiteral("plus"), p.text));
        m_prepare->setIcon(Theme::instance().icon(QStringLiteral("form"), p.text));
        m_save->setIcon(Theme::instance().icon(QStringLiteral("save"), p.accent)); // primary
    };
    applyIcons();
    connect(&Theme::instance(), &Theme::changed, this, applyIcons);

    connect(m_save, &QToolButton::clicked, this, [this] { emit saveRequested(m_values); });
    connect(m_add, &QToolButton::clicked, this, [this] { emit addFieldRequested(); });
    connect(m_prepare, &QToolButton::clicked, this, [this] { emit prepareFormRequested(); });
}

void FormPanel::setDocumentPath(const QString& path) {
    if (path == m_path)
        return;
    m_path = path;
    m_dirty = true;
    if (isVisible())
        rebuild();
}

void FormPanel::clear() {
    m_path.clear();
    m_dirty = false;
    m_values.clear();
    rebuild();
}

void FormPanel::reload() {
    m_dirty = true;
    if (isVisible())
        rebuild();
}

void FormPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (m_dirty)
        rebuild();
}

void FormPanel::rebuild() {
    m_dirty = false;
    m_values.clear();

    // Replace the field host wholesale (deletes the old editors).
    auto* layout = static_cast<QVBoxLayout*>(m_form->layout());
    if (m_fieldHost) {
        layout->removeWidget(m_fieldHost);
        m_fieldHost->deleteLater();
        m_fieldHost = nullptr;
    }
    m_fieldHost = new QWidget(m_form);
    auto* fl = new QVBoxLayout(m_fieldHost);
    fl->setContentsMargins(14, 12, 14, 12);
    fl->setSpacing(12);
    layout->addWidget(m_fieldHost);
    layout->addStretch(1);

    m_add->setEnabled(!m_path.isEmpty()); // can author even on an empty form
    m_prepare->setEnabled(!m_path.isEmpty());

    const QList<FormFiller::Field> fields = m_path.isEmpty() ? QList<FormFiller::Field>()
                                                             : FormFiller::read(m_path);
    if (fields.isEmpty()) {
        m_save->setEnabled(false);
        m_stack->setCurrentWidget(m_empty);
        return;
    }

    // Per-field authoring controls: Move repositions, ✕ deletes. Only shown for
    // named fields (we address fields by their fully-qualified name).
    const auto addAuthorButtons = [this](QHBoxLayout* head, const QString& fieldName) {
        if (fieldName.isEmpty())
            return;
        auto* move = new QToolButton(m_fieldHost);
        move->setText(tr("Move"));
        move->setCursor(Qt::PointingHandCursor);
        move->setToolTip(tr("Draw a new position for this field"));
        auto* del = new QToolButton(m_fieldHost);
        del->setText(QStringLiteral("✕"));
        del->setCursor(Qt::PointingHandCursor);
        del->setToolTip(tr("Delete this field"));
        head->addWidget(move);
        head->addWidget(del);
        connect(move, &QToolButton::clicked, this,
                [this, fieldName] { emit moveFieldRequested(fieldName); });
        connect(del, &QToolButton::clicked, this,
                [this, fieldName] { emit deleteFieldRequested(fieldName); });
    };

    for (const FormFiller::Field& f : fields) {
        const int id = f.id;
        const QString label = f.name.isEmpty() ? tr("(unnamed field)") : f.name;

        if (f.kind == FormFiller::Kind::CheckBox || f.kind == FormFiller::Kind::Radio) {
            auto* head = new QHBoxLayout;
            head->setSpacing(6);
            auto* cb = new QCheckBox(label, m_fieldHost);
            cb->setChecked(f.checked);
            cb->setEnabled(!f.readOnly);
            m_values.insert(id, f.checked);
            connect(cb, &QCheckBox::toggled, this, [this, id](bool on) { m_values[id] = on; });
            head->addWidget(cb, 1);
            addAuthorButtons(head, f.name);
            fl->addLayout(head);
            continue;
        }

        auto* head = new QHBoxLayout;
        head->setSpacing(6);
        auto* name = new QLabel(label, m_fieldHost);
        name->setObjectName(QStringLiteral("FieldName"));
        name->setWordWrap(true);
        head->addWidget(name, 1);
        addAuthorButtons(head, f.name);
        fl->addLayout(head);

        if (f.kind == FormFiller::Kind::ComboBox) {
            auto* combo = new QComboBox(m_fieldHost);
            combo->addItems(f.options);
            combo->setCurrentText(f.currentChoice);
            combo->setEnabled(!f.readOnly);
            m_values.insert(id, f.currentChoice);
            connect(combo, &QComboBox::currentTextChanged, this,
                    [this, id](const QString& t) { m_values[id] = t; });
            fl->addWidget(combo);
        } else { // Text (and ListBox falls back to a line edit)
            auto* edit = new QLineEdit(f.textValue, m_fieldHost);
            edit->setEnabled(!f.readOnly);
            m_values.insert(id, f.textValue);
            connect(edit, &QLineEdit::textChanged, this,
                    [this, id](const QString& t) { m_values[id] = t; });
            fl->addWidget(edit);
        }
    }

    m_save->setEnabled(true);
    m_stack->setCurrentWidget(m_stack->widget(0));
}
